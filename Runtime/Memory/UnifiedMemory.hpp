#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Robix::Runtime {

// ============================================================================
// 1. ALLOCATION TIERS & ERROR CODES
// ============================================================================

enum class MemoryTier : std::uint8_t {
    StackZeroCost = 0,
    AutomaticRC   = 1,
    ArenaRegion   = 2,
    ManualUnsafe  = 3
};

enum class MemoryErrorKind : std::uint8_t {
    None = 0,
    OutOfMemory,
    HeapBudgetExceeded,
    StackBudgetExceeded,
    InvalidAlignment,
    DoubleFree,
    CrossTierFreeViolation,
    UseAfterFree,
    ArenaEscapeViolation
};

struct MemoryAllocResult {
    void* ptr = nullptr;
    MemoryErrorKind error = MemoryErrorKind::None;

    [[nodiscard]] bool IsSuccess() const noexcept { return ptr != nullptr && error == MemoryErrorKind::None; }
};

// ============================================================================
// 2. TIER 2: AUTOMATIC REFERENCE COUNTING, WEAK<T> & CYCLE COLLECTION
// ============================================================================

struct RCControlBlock {
    std::uint32_t strongCount     = 1;
    std::uint32_t weakCount       = 0;
    bool isAtomic                 = false;
    bool isTransferredToManual    = false;
    bool isDestroyed              = false;
    std::size_t payloadSizeBytes  = 0;
    std::string typeName;
    void* payloadPtr              = nullptr;
    std::function<void()> destructorCallback;
    std::vector<RCControlBlock*> outgoingRefs; // For deterministic cycle detection
};

class WeakHandle;

class ManagedHandle {
public:
    ManagedHandle() noexcept = default;
    explicit ManagedHandle(RCControlBlock* block) noexcept;
    ManagedHandle(const ManagedHandle& other) noexcept;
    ManagedHandle(ManagedHandle&& other) noexcept;
    ManagedHandle& operator=(const ManagedHandle& other) noexcept;
    ManagedHandle& operator=(ManagedHandle&& other) noexcept;
    ~ManagedHandle();

    [[nodiscard]] bool IsValid() const noexcept {
        return m_block != nullptr && !m_block->isDestroyed && !m_block->isTransferredToManual;
    }
    [[nodiscard]] std::uint32_t StrongCount() const noexcept { return m_block ? m_block->strongCount : 0; }
    [[nodiscard]] std::uint32_t WeakCount() const noexcept { return m_block ? m_block->weakCount : 0; }
    [[nodiscard]] void* RawPayload() const noexcept { return m_block ? m_block->payloadPtr : nullptr; }
    [[nodiscard]] RCControlBlock* ControlBlock() const noexcept { return m_block; }

    void AddOutgoingReference(const ManagedHandle& target);
    void Reset() noexcept;

private:
    friend class WeakHandle;
    friend class UnifiedMemoryManager;
    RCControlBlock* m_block = nullptr;

    void Retain() noexcept;
    void Release() noexcept;
};

class WeakHandle {
public:
    WeakHandle() noexcept = default;
    explicit WeakHandle(const ManagedHandle& strong) noexcept;
    WeakHandle(const WeakHandle& other) noexcept;
    WeakHandle(WeakHandle&& other) noexcept;
    WeakHandle& operator=(const WeakHandle& other) noexcept;
    WeakHandle& operator=(WeakHandle&& other) noexcept;
    ~WeakHandle();

    // Safely upgrades Weak<T> -> Option<ManagedHandle>
    [[nodiscard]] std::optional<ManagedHandle> Upgrade() const noexcept;
    [[nodiscard]] bool IsExpired() const noexcept;

private:
    RCControlBlock* m_block = nullptr;
    void RetainWeak() noexcept;
    void ReleaseWeak() noexcept;
};

// ============================================================================
// 3. TIER 3: SCOPED ARENA ALLOCATOR, POOL ALLOCATOR & DMA BUFFER REGION
// ============================================================================

class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacityBytes, std::uint32_t arenaId = 1);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // O(1) bump-pointer allocation
    [[nodiscard]] void* Allocate(std::size_t sizeBytes, std::size_t alignment = 8) noexcept;
    void Reset() noexcept;

    [[nodiscard]] std::size_t CapacityBytes() const noexcept { return m_capacity; }
    [[nodiscard]] std::size_t UsedBytes() const noexcept { return m_offset; }
    [[nodiscard]] std::uint32_t ArenaId() const noexcept { return m_arenaId; }
    [[nodiscard]] bool OwnsAddress(const void* ptr) const noexcept;

private:
    std::uint8_t* m_buffer  = nullptr;
    std::size_t m_capacity  = 0;
    std::size_t m_offset    = 0;
    std::uint32_t m_arenaId = 1;
};

class PoolAllocator {
public:
    PoolAllocator(std::size_t blockSizeBytes, std::size_t blockCount);
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // O(1) deterministic real-time block acquire and release
    [[nodiscard]] void* AcquireBlock() noexcept;
    bool ReleaseBlock(void* blockPtr) noexcept;

    [[nodiscard]] std::size_t AvailableBlocks() const noexcept { return m_freeList.size(); }
    [[nodiscard]] std::size_t TotalBlocks() const noexcept { return m_blockCount; }

private:
    std::uint8_t* m_buffer     = nullptr;
    std::size_t m_blockSize    = 0;
    std::size_t m_blockCount   = 0;
    std::vector<void*> m_freeList;
};

class DMABufferRegion {
public:
    DMABufferRegion(std::size_t sizeBytes, std::size_t alignmentBytes = 64);
    ~DMABufferRegion();

    DMABufferRegion(const DMABufferRegion&) = delete;
    DMABufferRegion& operator=(const DMABufferRegion&) = delete;

    [[nodiscard]] void* Data() const noexcept { return m_alignedPtr; }
    [[nodiscard]] std::size_t SizeBytes() const noexcept { return m_sizeBytes; }
    [[nodiscard]] std::size_t AlignmentBytes() const noexcept { return m_alignment; }
    [[nodiscard]] bool IsProperlyAligned() const noexcept {
        return (reinterpret_cast<std::uintptr_t>(m_alignedPtr) % m_alignment) == 0;
    }

private:
    void* m_alignedPtr      = nullptr;
    std::size_t m_sizeBytes = 0;
    std::size_t m_alignment = 64;
};

// ============================================================================
// 4. UNIFIED MEMORY MANAGER (COEXISTENCE, OWNERSHIP TRANSFER & CYCLE COLLECTOR)
// ============================================================================

struct AllocationRecord {
    void* ptr                  = nullptr;
    std::size_t sizeBytes      = 0;
    std::size_t alignment      = 8;
    MemoryTier tier            = MemoryTier::ManualUnsafe;
    bool isFreed               = false;
    RCControlBlock* rcBlock    = nullptr;
};

class UnifiedMemoryManager {
public:
    explicit UnifiedMemoryManager(Frontend::DiagnosticBag& diagnostics,
                                  std::size_t maxHeapBudgetBytes = 64ULL * 1024ULL * 1024ULL,
                                  std::size_t maxStackBudgetBytes = 1ULL * 1024ULL * 1024ULL);
    ~UnifiedMemoryManager();

    // Tier 1: Stack budget verification for embedded & safety-critical targets
    bool EnterStackFrame(std::size_t frameSizeBytes, Frontend::SourceLocation loc = {});
    void LeaveStackFrame(std::size_t frameSizeBytes) noexcept;

    // Tier 2: Automatic RC allocation & Cycle Collection
    ManagedHandle AllocateManagedObject(std::string typeName,
                                        std::size_t sizeBytes,
                                        std::function<void()> destructorCallback = {},
                                        bool isAtomic = false);

    // Detect and collect isolated reference-counted cycles (e.g. A <-> B)
    std::size_t CollectReferenceCycles();

    // Tier 3: Register an Arena so cross-tier Free(arenaPtr) is caught
    void RegisterArena(const ArenaAllocator* arena);

    // Tier 4: Full Manual Memory Control (`Unsafe`)
    void* ManualAllocate(std::size_t sizeBytes, std::size_t alignment = 8, Frontend::SourceLocation loc = {});
    MemoryAllocResult TryManualAllocate(std::size_t sizeBytes, std::size_t alignment = 8) noexcept;
    bool ManualFree(void* ptr, Frontend::SourceLocation loc = {});

    // Explicit Ownership Transfer between Tier 2 (Automatic) and Tier 4 (Manual)
    // Memory.IntoRaw(managed) -> raw pointer (prevents automatic RC from freeing it)
    void* IntoRaw(ManagedHandle& managed, Frontend::SourceLocation loc = {});
    // Memory.FromRaw(rawPtr) -> ManagedHandle (transfers manual memory into automatic RC)
    ManagedHandle FromRaw(void* rawPtr,
                          std::string typeName = "RawTransferred",
                          std::function<void()> destructorCallback = {},
                          Frontend::SourceLocation loc = {});

    // Inspection & Leak Auditing
    [[nodiscard]] std::size_t GetCurrentHeapBytes() const noexcept { return m_currentHeapBytes; }
    [[nodiscard]] std::size_t GetCurrentStackBytes() const noexcept { return m_currentStackBytes; }
    [[nodiscard]] std::size_t GetActiveManualAllocationCount() const noexcept;

private:
    Frontend::DiagnosticBag& m_diagnostics;
    std::size_t m_maxHeapBudgetBytes  = 0;
    std::size_t m_maxStackBudgetBytes = 0;
    std::size_t m_currentHeapBytes    = 0;
    std::size_t m_currentStackBytes   = 0;

    std::unordered_map<void*, AllocationRecord> m_allocations;
    std::vector<RCControlBlock*> m_allRCBlocks;
    std::vector<const ArenaAllocator*> m_arenas;
};

} // namespace Robix::Runtime

