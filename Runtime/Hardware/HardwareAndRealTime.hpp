#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../Memory/UnifiedMemory.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Robix::Runtime {

// ============================================================================
// 1. HARDWARE BACKEND MODE & VOLATILE MMIO / CPU REGISTER ACCESS
// ============================================================================
// Mandatory Rule 13 & 14: Never pretend physical hardware is present when it
// is not. Attempting PhysicalOSOrBareMetal without a real hardware board
// reports HW9001 unless SimulationBackend is explicitly initialized.

enum class HardwareBackendKind : std::uint8_t {
    Unconfigured,
    PhysicalOSOrBareMetal,
    SimulationBackend
};

enum class GPIOPinMode : std::uint8_t {
    Input,
    Output,
    InputPullUp,
    InputPullDown
};

enum class GPIOPinState : std::uint8_t {
    Low  = 0,
    High = 1
};

enum class InterruptTrigger : std::uint8_t {
    RisingEdge,
    FallingEdge,
    BothEdges
};

struct CANFrame {
    std::uint32_t id      = 0;
    bool isExtended       = false;
    std::uint8_t dlc      = 0;
    std::uint8_t data[8]  = { 0, 0, 0, 0, 0, 0, 0, 0 };
};

// Volatile Memory-Mapped I/O (MMIO) 32-bit Register Block
class VolatileMMIORegister32 {
public:
    explicit VolatileMMIORegister32(volatile std::uint32_t* mappedAddress) noexcept
        : m_reg(mappedAddress) {}

    void Write(std::uint32_t value) noexcept {
        if (m_reg) {
            *m_reg = value;
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }
    }

    [[nodiscard]] std::uint32_t Read() const noexcept {
        if (!m_reg) return 0;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        return *m_reg;
    }

    void SetBits(std::uint32_t mask) noexcept {
        Write(Read() | mask);
    }

    void ClearBits(std::uint32_t mask) noexcept {
        Write(Read() & ~mask);
    }

private:
    volatile std::uint32_t* m_reg = nullptr;
};

// ============================================================================
// 2. PORTABLE HARDWARE ABSTRACTION LAYER (Robix.Hardware & Robix.Embedded)
// ============================================================================

class HardwareHAL {
public:
    explicit HardwareHAL(Frontend::DiagnosticBag& diagnostics) noexcept;

    bool InitializeBackend(HardwareBackendKind kind,
                           bool physicalBoardPresent = false,
                           Frontend::SourceLocation loc = {});

    [[nodiscard]] HardwareBackendKind ActiveBackend() const noexcept { return m_backend; }
    [[nodiscard]] bool IsSimulationMode() const noexcept { return m_backend == HardwareBackendKind::SimulationBackend; }

    // --- GPIO ---
    bool ConfigureGPIO(std::uint32_t pin, GPIOPinMode mode, Frontend::SourceLocation loc = {});
    bool WriteGPIO(std::uint32_t pin, GPIOPinState state, Frontend::SourceLocation loc = {});
    [[nodiscard]] std::optional<GPIOPinState> ReadGPIO(std::uint32_t pin, Frontend::SourceLocation loc = {});

    // --- PWM ---
    bool ConfigurePWM(std::uint32_t channel, double frequencyHz, double dutyCycleZeroToOne, Frontend::SourceLocation loc = {});
    [[nodiscard]] double GetPWMDutyCycle(std::uint32_t channel) const noexcept;
    [[nodiscard]] double GetPWMFrequencyHz(std::uint32_t channel) const noexcept;

    // --- UART ---
    bool ConfigureUART(std::uint32_t port, std::uint32_t baudRate, Frontend::SourceLocation loc = {});
    bool UARTWrite(std::uint32_t port, std::string_view payload, Frontend::SourceLocation loc = {});
    void InjectSimulatedUARTRx(std::uint32_t port, std::string_view rxBytes);
    [[nodiscard]] std::string UARTReadAvailable(std::uint32_t port, Frontend::SourceLocation loc = {});

    // --- SPI ---
    bool SPITransfer(std::uint32_t bus,
                     const std::vector<std::uint8_t>& txBytes,
                     std::vector<std::uint8_t>& rxBytesOut,
                     Frontend::SourceLocation loc = {});

    // --- I2C ---
    bool I2CWriteRegister(std::uint32_t bus,
                          std::uint8_t deviceAddr7Bit,
                          std::uint8_t regAddr,
                          std::uint8_t value,
                          Frontend::SourceLocation loc = {});
    [[nodiscard]] std::optional<std::uint8_t> I2CReadRegister(std::uint32_t bus,
                                                              std::uint8_t deviceAddr7Bit,
                                                              std::uint8_t regAddr,
                                                              Frontend::SourceLocation loc = {});

    // --- CAN Bus ---
    bool CANSendFrame(std::uint32_t bus, const CANFrame& frame, Frontend::SourceLocation loc = {});
    [[nodiscard]] std::optional<CANFrame> CANReceiveFrame(std::uint32_t bus, Frontend::SourceLocation loc = {});

    // --- DMA Controller (Aligned DMA Transfer) ---
    bool ExecuteDMATransfer(const DMABufferRegion& srcDMA,
                            DMABufferRegion& dstDMA,
                            std::size_t byteCount,
                            Frontend::SourceLocation loc = {});

    // --- Interrupt Controller (ISR Registration & Dispatch) ---
    bool AttachInterrupt(std::uint32_t irqLine,
                         InterruptTrigger trigger,
                         std::function<void()> isrHandler,
                         Frontend::SourceLocation loc = {});
    bool TriggerSimulatedInterrupt(std::uint32_t irqLine, Frontend::SourceLocation loc = {});

private:
    bool EnsureBackendReady(std::string_view subsystem, Frontend::SourceLocation loc);

    struct PinStateRecord {
        GPIOPinMode mode   = GPIOPinMode::Input;
        GPIOPinState state = GPIOPinState::Low;
    };

    struct PWMChannelRecord {
        double frequencyHz = 0.0;
        double dutyCycle   = 0.0;
    };

    Frontend::DiagnosticBag& m_diagnostics;
    HardwareBackendKind m_backend = HardwareBackendKind::Unconfigured;
    std::unordered_map<std::uint32_t, PinStateRecord> m_gpioPins;
    std::unordered_map<std::uint32_t, PWMChannelRecord> m_pwmChannels;
    std::unordered_map<std::uint32_t, std::string> m_uartRxBuffers;
    std::unordered_map<std::uint32_t, std::string> m_uartTxBuffers;
    std::unordered_map<std::uint16_t, std::uint8_t> m_i2cRegisters; // key: (addr7 << 8) | reg
    std::unordered_map<std::uint32_t, std::vector<CANFrame>> m_canQueues;
    std::unordered_map<std::uint32_t, std::function<void()>> m_isrHandlers;
};

// ============================================================================
// 3. REAL-TIME EXECUTION PROFILE (Robix.RealTime)
// ============================================================================
// Supports @RealTime, @Priority(1..99), @Period(us), No-GC enforcement (RT9001),
// lock-free SPSC ring buffer, watchdog timer (RT9003), and WCET / deadline
// miss tracking (RT9002).

struct RealTimeTaskConfig {
    std::string taskName;
    int priority            = 50;   // 1 (lowest) .. 99 (highest RT priority)
    std::uint64_t periodUs  = 1000; // e.g., 1000 us = 1 kHz control loop
    std::uint64_t deadlineUs = 1000;
    bool forbidGCAndDynamicHeap = true;
};

struct RealTimeWCETReport {
    std::string taskName;
    std::uint64_t invocations          = 0;
    std::uint64_t minExecutionTimeUs   = 0;
    std::uint64_t maxExecutionTimeUs   = 0; // Measured WCET
    std::uint64_t totalExecutionTimeUs = 0;
    std::uint64_t deadlineMisses       = 0;
};

// Wait-Free / Lock-Free Single-Producer Single-Consumer (SPSC) Ring Queue for ISR & Real-Time Tasks
template <typename T, std::size_t Capacity>
class LockFreeSPSCQueue {
    static_assert(Capacity >= 2, "LockFreeSPSCQueue Capacity must be at least 2");
public:
    bool TryPush(const T& item) noexcept {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) % Capacity;
        if (next == m_tail.load(std::memory_order_acquire)) {
            return false; // Full
        }
        m_buffer[head] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> TryPop() noexcept {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return std::nullopt; // Empty
        }
        T item = m_buffer[tail];
        m_tail.store((tail + 1) % Capacity, std::memory_order_release);
        return item;
    }

    [[nodiscard]] bool IsEmpty() const noexcept {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

private:
    T m_buffer[Capacity]{};
    std::atomic<std::size_t> m_head{ 0 };
    std::atomic<std::size_t> m_tail{ 0 };
};

// Hardware / Real-Time Watchdog Timer
class WatchdogTimer {
public:
    explicit WatchdogTimer(std::uint64_t timeoutUs) noexcept
        : m_timeoutUs(timeoutUs) {}

    void Kick(std::uint64_t currentTimestampUs) noexcept {
        m_lastKickUs = currentTimestampUs;
        m_tripped    = false;
    }

    bool CheckExpired(std::uint64_t currentTimestampUs,
                      Frontend::DiagnosticBag& diagnostics,
                      Frontend::SourceLocation loc = {});

    [[nodiscard]] bool IsTripped() const noexcept { return m_tripped; }

private:
    std::uint64_t m_timeoutUs  = 10000;
    std::uint64_t m_lastKickUs = 0;
    bool m_tripped             = false;
};

class RealTimeExecutor {
public:
    explicit RealTimeExecutor(Frontend::DiagnosticBag& diagnostics) noexcept;

    bool RegisterTask(const RealTimeTaskConfig& config, Frontend::SourceLocation loc = {});

    // Executes one deterministic real-time cycle of the task while enforcing No-GC
    // and measuring execution time against deadlineUs.
    bool ExecuteTaskCycle(std::string_view taskName,
                          const std::function<void()>& stepFn,
                          std::uint64_t simulatedDurationUs,
                          Frontend::SourceLocation loc = {});

    // Called by the allocator if a GC or non-deterministic heap allocation is attempted inside an @RealTime task
    bool CheckAllocationAllowedInCurrentContext(Frontend::SourceLocation loc = {});

    [[nodiscard]] std::optional<RealTimeWCETReport> GetWCETReport(std::string_view taskName) const;
    [[nodiscard]] bool IsInRealTimeNoGCScope() const noexcept { return m_inRealTimeNoGCScope; }

private:
    Frontend::DiagnosticBag& m_diagnostics;
    std::unordered_map<std::string, RealTimeTaskConfig> m_configs;
    std::unordered_map<std::string, RealTimeWCETReport> m_reports;
    bool m_inRealTimeNoGCScope = false;
};

} // namespace Robix::Runtime
