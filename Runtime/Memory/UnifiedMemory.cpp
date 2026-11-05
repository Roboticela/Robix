#include "UnifiedMemory.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace Robix::Runtime {

namespace {

void* AlignedMallocRaw(std::size_t sizeBytes, std::size_t alignment) noexcept {
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr; // Alignment must be power of two
    }
    const std::size_t actualSize = (sizeBytes > 0) ? sizeBytes : 1;
#if defined(_WIN32)
    return _aligned_malloc(actualSize, alignment);
#else
    const std::size_t rounded = ((actualSize + alignment - 1) / alignment) * alignment;
    return std::aligned_alloc(alignment, rounded);
#endif
}

void AlignedFreeRaw(void* ptr) noexcept {
    if (!ptr) return;
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

} // namespace

// ============================================================================
// 1. MANAGEDHANDLE & WEAKHANDLE IMPLEMENTATION
// ============================================================================

ManagedHandle::ManagedHandle(RCControlBlock* block) noexcept : m_block(block) {}

ManagedHandle::ManagedHandle(const ManagedHandle& other) noexcept : m_block(other.m_block) {
    Retain();
}

ManagedHandle::ManagedHandle(ManagedHandle&& other) noexcept : m_block(other.m_block) {
    other.m_block = nullptr;
}

ManagedHandle& ManagedHandle::operator=(const ManagedHandle& other) noexcept {
    if (this != &other) {
        Release();
        m_block = other.m_block;
        Retain();
    }
    return *this;
}

ManagedHandle& ManagedHandle::operator=(ManagedHandle&& other) noexcept {
    if (this != &other) {
        Release();
        m_block       = other.m_block;
        other.m_block = nullptr;
    }
    return *this;
}

ManagedHandle::~ManagedHandle() {
    Release();
}

void ManagedHandle::AddOutgoingReference(const ManagedHandle& target) {
    if (!m_block || !target.m_block || m_block->isDestroyed || target.m_block->isDestroyed) {
        return;
    }
    m_block->outgoingRefs.push_back(target.m_block);
    ++target.m_block->strongCount;
}

void ManagedHandle::Reset() noexcept {
    Release();
}

void ManagedHandle::Retain() noexcept {
    if (m_block && !m_block->isDestroyed && !m_block->isTransferredToManual) {
        ++m_block->strongCount;
    }
}

void ManagedHandle::Release() noexcept {
    if (!m_block) {
        return;
    }
    RCControlBlock* b = m_block;
    m_block           = nullptr;

    if (b->isTransferredToManual || b->isDestroyed) {
        return;
    }

    if (b->strongCount > 0) {
        --b->strongCount;
    }

    if (b->strongCount == 0) {
        b->isDestroyed = true;
        if (b->destructorCallback) {
            b->destructorCallback();
        }
        // Release outgoing references in reverse declaration order
        for (auto it = b->outgoingRefs.rbegin(); it != b->outgoingRefs.rend(); ++it) {
            RCControlBlock* child = *it;
            if (child && !child->isDestroyed && child->strongCount > 0) {
                --child->strongCount;
                if (child->strongCount == 0) {
                    child->isDestroyed = true;
                    if (child->destructorCallback) {
                        child->destructorCallback();
                    }
                    AlignedFreeRaw(child->payloadPtr);
                    child->payloadPtr = nullptr;
                }
            }
        }
        b->outgoingRefs.clear();
        AlignedFreeRaw(b->payloadPtr);
        b->payloadPtr = nullptr;
    }
}

WeakHandle::WeakHandle(const ManagedHandle& strong) noexcept : m_block(strong.m_block) {
    RetainWeak();
}

WeakHandle::WeakHandle(const WeakHandle& other) noexcept : m_block(other.m_block) {
    RetainWeak();
}

WeakHandle::WeakHandle(WeakHandle&& other) noexcept : m_block(other.m_block) {
    other.m_block = nullptr;
}

WeakHandle& WeakHandle::operator=(const WeakHandle& other) noexcept {
    if (this != &other) {
        ReleaseWeak();
        m_block = other.m_block;
        RetainWeak();
    }
    return *this;
}

WeakHandle& WeakHandle::operator=(WeakHandle&& other) noexcept {
    if (this != &other) {
        ReleaseWeak();
        m_block       = other.m_block;
        other.m_block = nullptr;
    }
    return *this;
}

WeakHandle::~WeakHandle() {
    ReleaseWeak();
}

bool WeakHandle::IsExpired() const noexcept {
    return m_block == nullptr || m_block->isDestroyed || m_block->strongCount == 0;
}

std::optional<ManagedHandle> WeakHandle::Upgrade() const noexcept {
    if (IsExpired()) {
        return std::nullopt;
    }
    ++m_block->strongCount;
    return ManagedHandle(m_block);
}

void WeakHandle::RetainWeak() noexcept {
    if (m_block) {
        ++m_block->weakCount;
    }
}

void WeakHandle::ReleaseWeak() noexcept {
    if (m_block) {
        if (m_block->weakCount > 0) {
            --m_block->weakCount;
        }
        m_block = nullptr;
    }
}

// ============================================================================
// 2. ARENA, POOL & DMA BUFFER ALLOCATORS
// ============================================================================

ArenaAllocator::ArenaAllocator(std::size_t capacityBytes, std::uint32_t arenaId)
    : m_capacity(capacityBytes), m_offset(0), m_arenaId(arenaId) {
    m_buffer = static_cast<std::uint8_t*>(AlignedMallocRaw(capacityBytes, 64));
}

ArenaAllocator::~ArenaAllocator() {
    AlignedFreeRaw(m_buffer);
}

void* ArenaAllocator::Allocate(std::size_t sizeBytes, std::size_t alignment) noexcept {
    if (!m_buffer || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }
    const std::uintptr_t baseAddr    = reinterpret_cast<std::uintptr_t>(m_buffer);
    const std::uintptr_t currentAddr = baseAddr + m_offset;
    const std::uintptr_t alignedAddr = (currentAddr + (alignment - 1)) & ~(alignment - 1);
    const std::size_t newOffset      = static_cast<std::size_t>(alignedAddr - baseAddr) + sizeBytes;

    if (newOffset > m_capacity) {
        return nullptr;
    }
    m_offset = newOffset;
    return reinterpret_cast<void*>(alignedAddr);
}

void ArenaAllocator::Reset() noexcept {
    m_offset = 0;
}

bool ArenaAllocator::OwnsAddress(const void* ptr) const noexcept {
    if (!m_buffer || !ptr) return false;
    const auto* bytePtr = static_cast<const std::uint8_t*>(ptr);
    return bytePtr >= m_buffer && bytePtr < (m_buffer + m_capacity);
}

PoolAllocator::PoolAllocator(std::size_t blockSizeBytes, std::size_t blockCount)
    : m_blockSize(blockSizeBytes < sizeof(void*) ? sizeof(void*) : blockSizeBytes)
    , m_blockCount(blockCount) {
    m_buffer = static_cast<std::uint8_t*>(AlignedMallocRaw(m_blockSize * m_blockCount, 64));
    m_freeList.reserve(m_blockCount);
    for (std::size_t i = 0; i < m_blockCount; ++i) {
        m_freeList.push_back(m_buffer + ( (m_blockCount - 1 - i) * m_blockSize ));
    }
}

PoolAllocator::~PoolAllocator() {
    AlignedFreeRaw(m_buffer);
}

void* PoolAllocator::AcquireBlock() noexcept {
    if (m_freeList.empty()) {
        return nullptr;
    }
    void* blk = m_freeList.back();
    m_freeList.pop_back();
    return blk;
}

bool PoolAllocator::ReleaseBlock(void* blockPtr) noexcept {
    if (!blockPtr || !m_buffer) return false;
    const auto* bytePtr = static_cast<const std::uint8_t*>(blockPtr);
    if (bytePtr < m_buffer || bytePtr >= (m_buffer + m_blockSize * m_blockCount)) {
        return false;
    }
    m_freeList.push_back(blockPtr);
    return true;
}

DMABufferRegion::DMABufferRegion(std::size_t sizeBytes, std::size_t alignmentBytes)
    : m_sizeBytes(sizeBytes), m_alignment(alignmentBytes < 64 ? 64 : alignmentBytes) {
    m_alignedPtr = AlignedMallocRaw(m_sizeBytes, m_alignment);
    if (m_alignedPtr) {
        std::memset(m_alignedPtr, 0, m_sizeBytes);
    }
}

DMABufferRegion::~DMABufferRegion() {
    AlignedFreeRaw(m_alignedPtr);
}

// ============================================================================
// 3. UNIFIED MEMORY MANAGER IMPLEMENTATION
// ============================================================================

UnifiedMemoryManager::UnifiedMemoryManager(Frontend::DiagnosticBag& diagnostics,
                                           std::size_t maxHeapBudgetBytes,
                                           std::size_t maxStackBudgetBytes)
    : m_diagnostics(diagnostics)
    , m_maxHeapBudgetBytes(maxHeapBudgetBytes)
    , m_maxStackBudgetBytes(maxStackBudgetBytes) {}

UnifiedMemoryManager::~UnifiedMemoryManager() {
    for (auto& [ptr, rec] : m_allocations) {
        if (!rec.isFreed && rec.tier == MemoryTier::ManualUnsafe && rec.ptr) {
            AlignedFreeRaw(rec.ptr);
        }
    }
    for (RCControlBlock* blk : m_allRCBlocks) {
        if (blk) {
            if (!blk->isDestroyed && !blk->isTransferredToManual && blk->payloadPtr) {
                AlignedFreeRaw(blk->payloadPtr);
            }
            delete blk;
        }
    }
}

bool UnifiedMemoryManager::EnterStackFrame(std::size_t frameSizeBytes, Frontend::SourceLocation loc) {
    if (m_currentStackBytes + frameSizeBytes > m_maxStackBudgetBytes) {
        m_diagnostics.ReportError(
            "M5005",
            "Stack budget limit exceeded (" + std::to_string(m_currentStackBytes + frameSizeBytes) +
                " bytes > " + std::to_string(m_maxStackBudgetBytes) + " bytes).",
            loc,
            "Reduce stack frame allocation size or move large buffers to an Arena or Pool.");
        return false;
    }
    m_currentStackBytes += frameSizeBytes;
    return true;
}

void UnifiedMemoryManager::LeaveStackFrame(std::size_t frameSizeBytes) noexcept {
    if (m_currentStackBytes >= frameSizeBytes) {
        m_currentStackBytes -= frameSizeBytes;
    } else {
        m_currentStackBytes = 0;
    }
}

ManagedHandle UnifiedMemoryManager::AllocateManagedObject(std::string typeName,
                                                          std::size_t sizeBytes,
                                                          std::function<void()> destructorCallback,
                                                          bool isAtomic) {
    void* payload = AlignedMallocRaw(sizeBytes, 16);
    if (payload) {
        std::memset(payload, 0, sizeBytes);
    }

    auto* block               = new RCControlBlock();
    block->strongCount        = 1;
    block->weakCount          = 0;
    block->isAtomic           = isAtomic;
    block->payloadSizeBytes   = sizeBytes;
    block->typeName           = std::move(typeName);
    block->payloadPtr         = payload;
    block->destructorCallback = std::move(destructorCallback);
    m_allRCBlocks.push_back(block);

    AllocationRecord rec;
    rec.ptr       = payload;
    rec.sizeBytes = sizeBytes;
    rec.alignment = 16;
    rec.tier      = MemoryTier::AutomaticRC;
    rec.rcBlock   = block;
    m_allocations[payload] = rec;
    m_currentHeapBytes += sizeBytes;

    return ManagedHandle(block);
}

std::size_t UnifiedMemoryManager::CollectReferenceCycles() {
    // Trial-deletion algorithm over live RCControlBlocks:
    // For each live block, compute internal references coming solely from other live blocks.
    // If a strongly connected subgraph has strongCount == internalIncomingCount (no external roots),
    // all blocks in that isolated cycle are unreachable and can be deterministically collected!
    std::unordered_map<RCControlBlock*, std::uint32_t> internalRefCounts;
    for (RCControlBlock* blk : m_allRCBlocks) {
        if (!blk || blk->isDestroyed || blk->isTransferredToManual) continue;
        internalRefCounts.try_emplace(blk, 0);
        for (RCControlBlock* target : blk->outgoingRefs) {
            if (target && !target->isDestroyed && !target->isTransferredToManual) {
                ++internalRefCounts[target];
            }
        }
    }

    // Propagate external liveness from roots (blocks where strongCount > internalRefCounts[blk])
    std::unordered_set<RCControlBlock*> externallyReachable;
    std::vector<RCControlBlock*> worklist;
    for (const auto& [blk, intCount] : internalRefCounts) {
        if (blk->strongCount > intCount) {
            externallyReachable.insert(blk);
            worklist.push_back(blk);
        }
    }

    while (!worklist.empty()) {
        RCControlBlock* curr = worklist.back();
        worklist.pop_back();
        for (RCControlBlock* child : curr->outgoingRefs) {
            if (child && !child->isDestroyed && externallyReachable.insert(child).second) {
                worklist.push_back(child);
            }
        }
    }

    // Any live block NOT in externallyReachable is part of an isolated reference cycle
    std::size_t collectedCount = 0;
    for (const auto& [blk, _] : internalRefCounts) {
        if (externallyReachable.find(blk) == externallyReachable.end() && !blk->isDestroyed) {
            blk->isDestroyed = true;
            blk->strongCount = 0;
            blk->outgoingRefs.clear();
            if (blk->destructorCallback) {
                blk->destructorCallback();
            }
            if (blk->payloadPtr) {
                m_allocations[blk->payloadPtr].isFreed = true;
                if (m_currentHeapBytes >= blk->payloadSizeBytes) {
                    m_currentHeapBytes -= blk->payloadSizeBytes;
                }
                AlignedFreeRaw(blk->payloadPtr);
                blk->payloadPtr = nullptr;
            }
            ++collectedCount;
        }
    }

    return collectedCount;
}

void UnifiedMemoryManager::RegisterArena(const ArenaAllocator* arena) {
    if (arena) {
        m_arenas.push_back(arena);
    }
}

MemoryAllocResult UnifiedMemoryManager::TryManualAllocate(std::size_t sizeBytes, std::size_t alignment) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return { nullptr, MemoryErrorKind::InvalidAlignment };
    }
    if (m_currentHeapBytes + sizeBytes > m_maxHeapBudgetBytes) {
        return { nullptr, MemoryErrorKind::HeapBudgetExceeded };
    }
    void* ptr = AlignedMallocRaw(sizeBytes, alignment);
    if (!ptr) {
        return { nullptr, MemoryErrorKind::OutOfMemory };
    }
    std::memset(ptr, 0, sizeBytes);

    AllocationRecord rec;
    rec.ptr       = ptr;
    rec.sizeBytes = sizeBytes;
    rec.alignment = alignment;
    rec.tier      = MemoryTier::ManualUnsafe;
    rec.isFreed   = false;
    m_allocations[ptr] = rec;
    m_currentHeapBytes += sizeBytes;

    return { ptr, MemoryErrorKind::None };
}

void* UnifiedMemoryManager::ManualAllocate(std::size_t sizeBytes,
                                           std::size_t alignment,
                                           Frontend::SourceLocation loc) {
    MemoryAllocResult res = TryManualAllocate(sizeBytes, alignment);
    if (!res.IsSuccess()) {
        m_diagnostics.ReportError(
            "M5005",
            "Manual memory allocation of " + std::to_string(sizeBytes) + " bytes failed (budget or alignment error).",
            loc,
            "Use 'Memory.TryAllocate' or increase the heap allocation budget.");
        return nullptr;
    }
    return res.ptr;
}

bool UnifiedMemoryManager::ManualFree(void* ptr, Frontend::SourceLocation loc) {
    if (!ptr) {
        return false;
    }

    // Check if ptr belongs to a registered Arena (Cross-Tier Free Violation!)
    for (const ArenaAllocator* arena : m_arenas) {
        if (arena && arena->OwnsAddress(ptr)) {
            m_diagnostics.ReportError(
                "M5002",
                "Cross-tier memory violation: Cannot call 'Memory.Free' on an Arena-allocated pointer.",
                loc,
                "Arena memory is freed automatically in bulk when the Arena resets or exits scope.");
            return false;
        }
    }

    auto it = m_allocations.find(ptr);
    if (it == m_allocations.end()) {
        m_diagnostics.ReportError(
            "M5002",
            "Invalid pointer passed to 'Memory.Free': address was not allocated by ManualAllocate or IntoRaw.",
            loc);
        return false;
    }

    AllocationRecord& rec = it->second;
    if (rec.tier == MemoryTier::AutomaticRC) {
        m_diagnostics.ReportError(
            "M5002",
            "Cross-tier memory violation: Cannot manually 'Memory.Free' an automatically managed object without 'Memory.IntoRaw'.",
            loc,
            "Call 'let rawPtr = Memory.IntoRaw(obj);' to explicitly transfer ownership before manual deallocation.");
        return false;
    }

    if (rec.isFreed) {
        m_diagnostics.ReportError(
            "M5001",
            "Double-free detected: Manual memory block was already freed.",
            loc,
            "Ensure each manual pointer is freed at most once.");
        return false;
    }

    rec.isFreed = true;
    if (m_currentHeapBytes >= rec.sizeBytes) {
        m_currentHeapBytes -= rec.sizeBytes;
    }
    if (rec.rcBlock) {
        rec.rcBlock->isDestroyed = true;
        rec.rcBlock->payloadPtr  = nullptr;
    }
    AlignedFreeRaw(rec.ptr);
    return true;
}

void* UnifiedMemoryManager::IntoRaw(ManagedHandle& managed, Frontend::SourceLocation loc) {
    RCControlBlock* blk = managed.ControlBlock();
    if (!blk || blk->isDestroyed || !blk->payloadPtr) {
        m_diagnostics.ReportError(
            "M5002",
            "'Memory.IntoRaw' called on an invalid or already destroyed managed handle.",
            loc);
        return nullptr;
    }

    blk->isTransferredToManual = true;
    void* rawPtr = blk->payloadPtr;
    m_allocations[rawPtr].tier = MemoryTier::ManualUnsafe;
    managed.m_block = nullptr;
    return rawPtr;
}

ManagedHandle UnifiedMemoryManager::FromRaw(void* rawPtr,
                                            std::string typeName,
                                            std::function<void()> destructorCallback,
                                            Frontend::SourceLocation loc) {
    auto it = m_allocations.find(rawPtr);
    if (it == m_allocations.end() || it->second.isFreed || it->second.tier != MemoryTier::ManualUnsafe) {
        m_diagnostics.ReportError(
            "M5002",
            "'Memory.FromRaw' requires a live, manually owned pointer.",
            loc,
            "Only pass live pointers from 'Memory.Allocate' or 'Memory.IntoRaw' into 'Memory.FromRaw'.");
        return ManagedHandle{};
    }

    AllocationRecord& rec = it->second;
    rec.tier = MemoryTier::AutomaticRC;

    RCControlBlock* blk = rec.rcBlock;
    if (!blk) {
        blk               = new RCControlBlock();
        blk->payloadPtr   = rawPtr;
        blk->payloadSizeBytes = rec.sizeBytes;
        m_allRCBlocks.push_back(blk);
        rec.rcBlock = blk;
    }
    blk->strongCount           = 1;
    blk->isTransferredToManual = false;
    blk->isDestroyed           = false;
    blk->typeName              = std::move(typeName);
    if (destructorCallback) {
        blk->destructorCallback = std::move(destructorCallback);
    }

    return ManagedHandle(blk);
}

std::size_t UnifiedMemoryManager::GetActiveManualAllocationCount() const noexcept {
    std::size_t count = 0;
    for (const auto& [ptr, rec] : m_allocations) {
        if (!rec.isFreed && rec.tier == MemoryTier::ManualUnsafe) {
            ++count;
        }
    }
    return count;
}

} // namespace Robix::Runtime

