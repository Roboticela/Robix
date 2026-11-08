#include "../../Runtime/Memory/LowLevelSystems.hpp"
#include "../../Runtime/Memory/UnifiedMemory.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_passedTests = 0;
int g_failedTests = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++g_passedTests;
    } else {
        ++g_failedTests;
        std::cerr << "[FAIL] " << description << "\n";
    }
}

void TestAutomaticRCWeakHandlesAndCycleCollection() {
    Robix::Frontend::DiagnosticBag diags;
    Robix::Runtime::UnifiedMemoryManager mem(diags);

    std::vector<std::string> destructionOrder;
    Robix::Runtime::WeakHandle weakObserver;

    {
        auto objA = mem.AllocateManagedObject("SensorNodeA", 128, [&]() {
            destructionOrder.emplace_back("SensorNodeA");
        });
        auto objB = mem.AllocateManagedObject("SensorNodeB", 128, [&]() {
            destructionOrder.emplace_back("SensorNodeB");
        });

        weakObserver = Robix::Runtime::WeakHandle(objA);
        Check(!weakObserver.IsExpired(), "Weak<T> is active while strong handle is alive");

        auto upgraded = weakObserver.Upgrade();
        Check(upgraded.has_value() && upgraded->StrongCount() == 2,
              "Weak<T>.Upgrade() returns valid ManagedHandle and increments strongCount to 2");
    }

    // objB was declared second so C++ / Robix reverse local destruction destroys objB before objA
    Check(destructionOrder.size() == 2 &&
          destructionOrder[0] == "SensorNodeB" &&
          destructionOrder[1] == "SensorNodeA",
          "Automatic RC objects are deterministically destroyed in reverse declaration order");
    Check(weakObserver.IsExpired() && !weakObserver.Upgrade().has_value(),
          "Weak<T> expires and Upgrade() returns nullopt after target object is destroyed");

    // Test isolated reference cycle detection (CycleNode1 <-> CycleNode2)
    int cycleDestructorsRun = 0;
    {
        auto node1 = mem.AllocateManagedObject("CycleNode1", 64, [&]() { ++cycleDestructorsRun; });
        auto node2 = mem.AllocateManagedObject("CycleNode2", 64, [&]() { ++cycleDestructorsRun; });

        // Create mutual strong reference cycle: node1 -> node2 and node2 -> node1
        node1.AddOutgoingReference(node2);
        node2.AddOutgoingReference(node1);
        Check(node1.StrongCount() == 2 && node2.StrongCount() == 2,
              "Cyclic objects have strongCount == 2 before scope exit");
    }
    // External handles node1 and node2 exited scope, leaving strongCount == 1 on each in an isolated cycle
    Check(cycleDestructorsRun == 0, "Isolated reference cycle remains alive before cycle collector runs");

    std::size_t collected = mem.CollectReferenceCycles();
    Check(collected == 2 && cycleDestructorsRun == 2,
          "Trial-deletion cycle collector detects and reclaims both nodes in isolated reference cycle");
}

void TestArenaPoolAndDMARegions() {
    // 1. Scoped ArenaAllocator
    Robix::Runtime::ArenaAllocator arena(4096, 1);
    void* p1 = arena.Allocate(128, 16);
    void* p2 = arena.Allocate(256, 64);
    Check(p1 != nullptr && p2 != nullptr && arena.OwnsAddress(p1) && arena.OwnsAddress(p2),
          "ArenaAllocator performs O(1) aligned bump-pointer allocations within capacity");
    Check((reinterpret_cast<std::uintptr_t>(p2) % 64) == 0, "ArenaAllocator honors 64-byte alignment");
    arena.Reset();
    Check(arena.UsedBytes() == 0, "ArenaAllocator.Reset() reclaims entire arena in O(1)");

    // 2. Deterministic Real-Time PoolAllocator
    Robix::Runtime::PoolAllocator pool(64, 4);
    Check(pool.AvailableBlocks() == 4, "PoolAllocator initializes with 4 available fixed-size blocks");
    void* b1 = pool.AcquireBlock();
    void* b2 = pool.AcquireBlock();
    Check(b1 != nullptr && b2 != nullptr && pool.AvailableBlocks() == 2,
          "PoolAllocator.AcquireBlock() allocates in O(1)");
    bool released = pool.ReleaseBlock(b1);
    Check(released && pool.AvailableBlocks() == 3, "PoolAllocator.ReleaseBlock() returns block to free list in O(1)");

    // 3. Hardware-aligned DMABufferRegion
    Robix::Runtime::DMABufferRegion dmaPage(8192, 4096);
    Check(dmaPage.Data() != nullptr && dmaPage.IsProperlyAligned(),
          "DMABufferRegion allocates 4096-byte page-aligned zeroed DMA buffer");
}

void TestManualMemoryCoexistenceOwnershipTransferAndBudgets() {
    Robix::Frontend::DiagnosticBag diags;
    // Configure 4 KB heap budget and 1 KB stack budget
    Robix::Runtime::UnifiedMemoryManager mem(diags, 4096, 1024);

    Robix::Runtime::ArenaAllocator arena(1024, 2);
    mem.RegisterArena(&arena);
    void* arenaPtr = arena.Allocate(64, 8);

    // 1. Cross-tier violation: calling ManualFree on an Arena pointer -> M5002
    Check(!mem.ManualFree(arenaPtr) && diags.ContainsCode("M5002"),
          "Calling Memory.Free on an Arena-allocated pointer is rejected with M5002");

    // 2. Cross-tier violation: calling ManualFree on an Automatic RC object without IntoRaw -> M5002
    bool rcDestroyed = false;
    {
        auto rcObj = mem.AllocateManagedObject("MotorDriver", 128, [&]() { rcDestroyed = true; });
        Robix::Frontend::DiagnosticBag crossDiags;
        Robix::Runtime::UnifiedMemoryManager crossMem(crossDiags);
        auto managed2 = crossMem.AllocateManagedObject("Servo", 64);
        Check(!crossMem.ManualFree(managed2.RawPayload()) && crossDiags.ContainsCode("M5002"),
              "Calling Memory.Free on an automatically managed RC object without IntoRaw emits M5002");
    }
    Check(rcDestroyed, "Untransferred RC object is still cleaned up automatically at scope exit");

    // 3. Explicit Ownership Transfer: Memory.IntoRaw(managed) -> ManualFree
    bool transferredDestroyedAuto = false;
    void* rawFromManaged = nullptr;
    {
        auto managedObj = mem.AllocateManagedObject("TransferredBuffer", 256, [&]() {
            transferredDestroyedAuto = true;
        });
        rawFromManaged = mem.IntoRaw(managedObj);
        Check(rawFromManaged != nullptr && !managedObj.IsValid(),
              "Memory.IntoRaw transfers ownership to raw pointer and invalidates automatic handle");
    }
    Check(!transferredDestroyedAuto,
          "Scope exit does NOT automatically free object after Memory.IntoRaw transfer");
    Check(mem.ManualFree(rawFromManaged),
          "Manual Memory.Free succeeds on pointer obtained via Memory.IntoRaw");

    // 4. Double-Free detection -> M5001
    {
        Robix::Frontend::DiagnosticBag dfDiags;
        Robix::Runtime::UnifiedMemoryManager dfMem(dfDiags);
        void* raw = dfMem.ManualAllocate(128, 16);
        Check(dfMem.ManualFree(raw), "First ManualFree succeeds");
        Check(!dfMem.ManualFree(raw) && dfDiags.ContainsCode("M5001"),
              "Second ManualFree on the same pointer is caught as Double-Free (M5001)");
    }

    // 5. Explicit Ownership Transfer: ManualAllocate -> Memory.FromRaw -> Automatic cleanup
    bool fromRawDestroyed = false;
    {
        void* manualBlock = mem.ManualAllocate(128, 16);
        Check(mem.GetActiveManualAllocationCount() == 1, "Manual allocation tracked before FromRaw");
        auto adoptedHandle = mem.FromRaw(manualBlock, "AdoptedObject", [&]() {
            fromRawDestroyed = true;
        });
        Check(adoptedHandle.IsValid() && mem.GetActiveManualAllocationCount() == 0,
              "Memory.FromRaw transfers manual block into automatic RC management");
    }
    Check(fromRawDestroyed, "Object adopted via Memory.FromRaw is automatically destroyed at scope exit");

    // 6. Stack & Heap Budget Enforcement
    {
        Robix::Frontend::DiagnosticBag budgetDiags;
        Robix::Runtime::UnifiedMemoryManager tightMem(budgetDiags, 512, 256);

        Check(tightMem.EnterStackFrame(128), "Stack frame within 256-byte budget succeeds");
        Check(!tightMem.EnterStackFrame(200) && budgetDiags.ContainsCode("M5005"),
              "Exceeding stack frame budget emits M5005");
        tightMem.LeaveStackFrame(128);

        auto tryOk = tightMem.TryManualAllocate(256, 8);
        Check(tryOk.IsSuccess(), "TryManualAllocate within 512-byte heap budget succeeds");
        auto tryExceed = tightMem.TryManualAllocate(512, 8);
        Check(!tryExceed.IsSuccess() &&
              tryExceed.error == Robix::Runtime::MemoryErrorKind::HeapBudgetExceeded,
              "TryManualAllocate exceeding heap budget returns HeapBudgetExceeded without panicking");
        (void)tightMem.ManualFree(tryOk.ptr);
    }
}

void TestSpanRawPointerInlineASMAndCFFIBoundary() {
    // 1. Safe Bounds-Checked Span<T>
    {
        Robix::Frontend::DiagnosticBag spanDiags;
        int data[4] = { 10, 20, 30, 40 };
        Robix::Runtime::Span<int> span(data, 4);

        int* elem2 = span.At(2, spanDiags);
        Check(elem2 != nullptr && *elem2 == 30 && !spanDiags.HasErrors(),
              "Span<Int>.At(2) returns pointer to element 30");

        auto sub = span.SubSpan(1, 2, spanDiags);
        Check(sub.Length() == 2 && sub[0] == 20 && sub[1] == 30,
              "Span<Int>.SubSpan(1, 2) creates valid sub-slice [20, 30]");

        int* oob = span.At(10, spanDiags);
        Check(oob == nullptr && spanDiags.ContainsCode("M5003"),
              "Out-of-bounds Span<Int>.At(10) is caught with M5003");
    }

    // 2. Typed RawPointer<T> arithmetic & null check
    {
        Robix::Frontend::DiagnosticBag ptrDiags;
        std::int32_t words[3] = { 100, 200, 300 };
        Robix::Runtime::RawPointer<std::int32_t> p0(&words[0]);
        auto p2 = p0.AddOffset(2);
        Check(*p2.Dereference(ptrDiags) == 300 && p2.Diff(p0) == 2,
              "RawPointer arithmetic AddOffset(2) and Diff() work accurately");

        Robix::Runtime::RawPointer<std::int32_t> nullPtr(nullptr);
        Check(nullPtr.Dereference(ptrDiags) == nullptr && ptrDiags.ContainsCode("M5004"),
              "Dereferencing null RawPointer emits M5004");
    }

    // 3. Target-Checked Inline Assembly Validator
    {
        Robix::Frontend::DiagnosticBag asmDiags;
        Robix::Runtime::InlineASMStatementDesc x86Asm;
        x86Asm.instructionText   = "RDTSC; MOV RAX, RDX";
        x86Asm.outputConstraints = { "=r" };
        x86Asm.clobbers          = { "rax", "rdx", "memory" };

        Check(Robix::Runtime::InlineASMValidator::ValidateForTarget(
                  x86Asm, Robix::Spec::TargetArch::X86_64_Windows, asmDiags),
              "x86-64 RDTSC/RAX assembly validates cleanly for X86_64_Windows target");

        Check(!Robix::Runtime::InlineASMValidator::ValidateForTarget(
                  x86Asm, Robix::Spec::TargetArch::ARM64, asmDiags) &&
              asmDiags.ContainsCode("A5001"),
              "x86-64 RDTSC/RAX assembly is rejected with A5001 when targeting ARM64");

        Robix::Frontend::DiagnosticBag armDiags;
        Robix::Runtime::InlineASMStatementDesc armAsm{ "DMB SY", {}, {}, { "memory" }, true, {} };
        Check(Robix::Runtime::InlineASMValidator::ValidateForTarget(
                  armAsm, Robix::Spec::TargetArch::ARM64, armDiags),
              "ARM64 'DMB SY' memory barrier validates cleanly for ARM64 target");
    }

    // 4. Memory-Safe C FFI Boundary Guard
    {
        Robix::Frontend::DiagnosticBag ffiDiags;
        Robix::Runtime::FFIBoundaryGuard ffi(ffiDiags);

        std::uint8_t packet[64]{};
        Check(!ffi.ValidateCFFIPointerArg(packet, false, true, "c_send_packet") &&
              ffiDiags.ContainsCode("F5001"),
              "Passing unpinned Robix buffer to C FFI function is rejected with F5001");

        ffi.PinBufferForFFI(packet, sizeof(packet));
        Check(ffi.ValidateCFFIPointerArg(packet, false, true, "c_send_packet"),
              "Pinned Robix buffer passes C FFI boundary verification");
        ffi.UnpinBufferAfterFFI(packet);

        auto isolatedFault = ffi.ExecuteIsolatedCCall("c_faulty_driver", []() -> std::int64_t {
            throw std::runtime_error("Simulated C library fault");
        });
        Check(!isolatedFault.succeeded && ffiDiags.ContainsCode("F5001"),
              "FFI boundary isolates C callback fault without crashing the Robix runtime");
    }
}

} // namespace

int main() {
    TestAutomaticRCWeakHandlesAndCycleCollection();
    TestArenaPoolAndDMARegions();
    TestManualMemoryCoexistenceOwnershipTransferAndBudgets();
    TestSpanRawPointerInlineASMAndCFFIBoundary();

    std::cout << "========================================================\n";
    std::cout << "Robix Stage 5 Unified Memory & Low-Level Systems Tests: "
              << g_passedTests << " passed, "
              << g_failedTests << " failed.\n";
    std::cout << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}

