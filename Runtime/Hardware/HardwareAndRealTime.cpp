#include "HardwareAndRealTime.hpp"

#include <algorithm>
#include <cstring>

namespace Robix::Runtime {

// ============================================================================
// 1. PORTABLE HARDWARE ABSTRACTION LAYER IMPLEMENTATION
// ============================================================================

HardwareHAL::HardwareHAL(Frontend::DiagnosticBag& diagnostics) noexcept
    : m_diagnostics(diagnostics) {}

bool HardwareHAL::InitializeBackend(HardwareBackendKind kind,
                                    bool physicalBoardPresent,
                                    Frontend::SourceLocation loc) {
    if (kind == HardwareBackendKind::PhysicalOSOrBareMetal && !physicalBoardPresent) {
        m_diagnostics.ReportError(
            "HW9001",
            "Physical hardware backend requested, but no physical GPIO/bus controller board was detected on this host.",
            loc,
            "Connect target robotics hardware or explicitly initialize HardwareBackendKind::SimulationBackend for host simulation.");
        m_backend = HardwareBackendKind::Unconfigured;
        return false;
    }
    m_backend = kind;
    return m_backend != HardwareBackendKind::Unconfigured;
}

bool HardwareHAL::EnsureBackendReady(std::string_view subsystem, Frontend::SourceLocation loc) {
    if (m_backend == HardwareBackendKind::Unconfigured) {
        m_diagnostics.ReportError(
            "HW9001",
            "Hardware subsystem '" + std::string(subsystem) +
                "' accessed before initializing a valid hardware or simulation backend.",
            loc,
            "Call InitializeBackend(HardwareBackendKind::SimulationBackend) or connect target hardware.");
        return false;
    }
    return true;
}

bool HardwareHAL::ConfigureGPIO(std::uint32_t pin, GPIOPinMode mode, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("GPIO", loc)) return false;
    m_gpioPins[pin].mode = mode;
    return true;
}

bool HardwareHAL::WriteGPIO(std::uint32_t pin, GPIOPinState state, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("GPIO", loc)) return false;
    auto it = m_gpioPins.find(pin);
    if (it == m_gpioPins.end() || it->second.mode != GPIOPinMode::Output) {
        m_diagnostics.ReportError(
            "HW9002",
            "Cannot write to GPIO pin " + std::to_string(pin) + " because it is not configured as GPIOPinMode::Output.",
            loc,
            "Call ConfigureGPIO(pin, GPIOPinMode::Output) before WriteGPIO.");
        return false;
    }
    it->second.state = state;
    return true;
}

std::optional<GPIOPinState> HardwareHAL::ReadGPIO(std::uint32_t pin, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("GPIO", loc)) return std::nullopt;
    auto it = m_gpioPins.find(pin);
    if (it == m_gpioPins.end()) {
        m_diagnostics.ReportError(
            "HW9002",
            "Cannot read unconfigured GPIO pin " + std::to_string(pin) + ".",
            loc,
            "Configure the GPIO pin before reading.");
        return std::nullopt;
    }
    return it->second.state;
}

bool HardwareHAL::ConfigurePWM(std::uint32_t channel,
                               double frequencyHz,
                               double dutyCycleZeroToOne,
                               Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("PWM", loc)) return false;
    if (frequencyHz <= 0.0 || dutyCycleZeroToOne < 0.0 || dutyCycleZeroToOne > 1.0) {
        m_diagnostics.ReportError(
            "HW9003",
            "Invalid PWM configuration on channel " + std::to_string(channel) +
                ": frequency must be > 0 Hz and duty cycle in [0.0, 1.0].",
            loc);
        return false;
    }
    m_pwmChannels[channel] = PWMChannelRecord{ frequencyHz, dutyCycleZeroToOne };
    return true;
}

double HardwareHAL::GetPWMDutyCycle(std::uint32_t channel) const noexcept {
    auto it = m_pwmChannels.find(channel);
    return (it != m_pwmChannels.end()) ? it->second.dutyCycle : 0.0;
}

double HardwareHAL::GetPWMFrequencyHz(std::uint32_t channel) const noexcept {
    auto it = m_pwmChannels.find(channel);
    return (it != m_pwmChannels.end()) ? it->second.frequencyHz : 0.0;
}

bool HardwareHAL::ConfigureUART(std::uint32_t port, std::uint32_t baudRate, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("UART", loc)) return false;
    if (baudRate == 0) {
        m_diagnostics.ReportError("HW9004", "UART baudRate must be non-zero.", loc);
        return false;
    }
    m_uartRxBuffers[port];
    m_uartTxBuffers[port];
    return true;
}

bool HardwareHAL::UARTWrite(std::uint32_t port, std::string_view payload, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("UART", loc)) return false;
    m_uartTxBuffers[port].append(payload);
    return true;
}

void HardwareHAL::InjectSimulatedUARTRx(std::uint32_t port, std::string_view rxBytes) {
    m_uartRxBuffers[port].append(rxBytes);
}

std::string HardwareHAL::UARTReadAvailable(std::uint32_t port, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("UART", loc)) return {};
    std::string out = std::move(m_uartRxBuffers[port]);
    m_uartRxBuffers[port].clear();
    return out;
}

bool HardwareHAL::SPITransfer(std::uint32_t /*bus*/,
                              const std::vector<std::uint8_t>& txBytes,
                              std::vector<std::uint8_t>& rxBytesOut,
                              Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("SPI", loc)) return false;
    // Full-duplex loopback in simulation mode
    rxBytesOut = txBytes;
    return true;
}

bool HardwareHAL::I2CWriteRegister(std::uint32_t /*bus*/,
                                   std::uint8_t deviceAddr7Bit,
                                   std::uint8_t regAddr,
                                   std::uint8_t value,
                                   Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("I2C", loc)) return false;
    if (deviceAddr7Bit > 0x7F) {
        m_diagnostics.ReportError("HW9005", "I2C 7-bit address exceeds 0x7F.", loc);
        return false;
    }
    const std::uint16_t key = static_cast<std::uint16_t>((static_cast<std::uint16_t>(deviceAddr7Bit) << 8) | regAddr);
    m_i2cRegisters[key] = value;
    return true;
}

std::optional<std::uint8_t> HardwareHAL::I2CReadRegister(std::uint32_t /*bus*/,
                                                         std::uint8_t deviceAddr7Bit,
                                                         std::uint8_t regAddr,
                                                         Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("I2C", loc)) return std::nullopt;
    const std::uint16_t key = static_cast<std::uint16_t>((static_cast<std::uint16_t>(deviceAddr7Bit) << 8) | regAddr);
    auto it = m_i2cRegisters.find(key);
    if (it == m_i2cRegisters.end()) return std::uint8_t{ 0 };
    return it->second;
}

bool HardwareHAL::CANSendFrame(std::uint32_t bus, const CANFrame& frame, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("CAN", loc)) return false;
    if (frame.dlc > 8) {
        m_diagnostics.ReportError("HW9006", "CAN 2.0 frame DLC cannot exceed 8 bytes.", loc);
        return false;
    }
    m_canQueues[bus].push_back(frame);
    return true;
}

std::optional<CANFrame> HardwareHAL::CANReceiveFrame(std::uint32_t bus, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("CAN", loc)) return std::nullopt;
    auto& q = m_canQueues[bus];
    if (q.empty()) return std::nullopt;
    CANFrame front = q.front();
    q.erase(q.begin());
    return front;
}

bool HardwareHAL::ExecuteDMATransfer(const DMABufferRegion& srcDMA,
                                     DMABufferRegion& dstDMA,
                                     std::size_t byteCount,
                                     Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("DMA", loc)) return false;
    if (!srcDMA.Data() || !dstDMA.Data() ||
        byteCount > srcDMA.SizeBytes() || byteCount > dstDMA.SizeBytes()) {
        m_diagnostics.ReportError(
            "HW9007",
            "DMA transfer bounds violation or invalid DMA buffer descriptor.",
            loc);
        return false;
    }
    if (!srcDMA.IsProperlyAligned() || !dstDMA.IsProperlyAligned()) {
        m_diagnostics.ReportError(
            "HW9007",
            "DMA buffer physical/virtual address violates hardware alignment constraints.",
            loc);
        return false;
    }
    std::memcpy(dstDMA.Data(), srcDMA.Data(), byteCount);
    return true;
}

bool HardwareHAL::AttachInterrupt(std::uint32_t irqLine,
                                  InterruptTrigger /*trigger*/,
                                  std::function<void()> isrHandler,
                                  Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("InterruptController", loc)) return false;
    if (!isrHandler) return false;
    m_isrHandlers[irqLine] = std::move(isrHandler);
    return true;
}

bool HardwareHAL::TriggerSimulatedInterrupt(std::uint32_t irqLine, Frontend::SourceLocation loc) {
    if (!EnsureBackendReady("InterruptController", loc)) return false;
    auto it = m_isrHandlers.find(irqLine);
    if (it == m_isrHandlers.end()) return false;
    it->second();
    return true;
}

// ============================================================================
// 2. REAL-TIME EXECUTION PROFILE IMPLEMENTATION
// ============================================================================

bool WatchdogTimer::CheckExpired(std::uint64_t currentTimestampUs,
                                 Frontend::DiagnosticBag& diagnostics,
                                 Frontend::SourceLocation loc) {
    if (currentTimestampUs > m_lastKickUs && (currentTimestampUs - m_lastKickUs) > m_timeoutUs) {
        m_tripped = true;
        diagnostics.ReportError(
            "RT9003",
            "Real-time Watchdog Timer expired (" + std::to_string(currentTimestampUs - m_lastKickUs) +
                " us elapsed > timeout " + std::to_string(m_timeoutUs) + " us).",
            loc,
            "Ensure real-time control loop kicks the watchdog within its bounded period.");
        return true;
    }
    return false;
}

RealTimeExecutor::RealTimeExecutor(Frontend::DiagnosticBag& diagnostics) noexcept
    : m_diagnostics(diagnostics) {}

bool RealTimeExecutor::RegisterTask(const RealTimeTaskConfig& config, Frontend::SourceLocation loc) {
    if (config.priority < 1 || config.priority > 99) {
        m_diagnostics.ReportError(
            "RT9004",
            "Real-time task '" + config.taskName + "' priority " + std::to_string(config.priority) +
                " is outside valid range [1..99].",
            loc);
        return false;
    }
    m_configs[config.taskName] = config;
    RealTimeWCETReport rep;
    rep.taskName = config.taskName;
    m_reports[config.taskName] = rep;
    return true;
}

bool RealTimeExecutor::ExecuteTaskCycle(std::string_view taskName,
                                        const std::function<void()>& stepFn,
                                        std::uint64_t simulatedDurationUs,
                                        Frontend::SourceLocation loc) {
    const std::string key(taskName);
    auto it = m_configs.find(key);
    if (it == m_configs.end()) return false;

    const bool prevNoGC = m_inRealTimeNoGCScope;
    m_inRealTimeNoGCScope = it->second.forbidGCAndDynamicHeap;
    if (stepFn) {
        stepFn();
    }
    m_inRealTimeNoGCScope = prevNoGC;

    auto& rep = m_reports[key];
    rep.invocations += 1;
    rep.totalExecutionTimeUs += simulatedDurationUs;
    if (rep.invocations == 1 || simulatedDurationUs < rep.minExecutionTimeUs) {
        rep.minExecutionTimeUs = simulatedDurationUs;
    }
    if (simulatedDurationUs > rep.maxExecutionTimeUs) {
        rep.maxExecutionTimeUs = simulatedDurationUs;
    }

    if (simulatedDurationUs > it->second.deadlineUs) {
        rep.deadlineMisses += 1;
        m_diagnostics.ReportError(
            "RT9002",
            "Real-time task '" + key + "' missed deadline: execution took " +
                std::to_string(simulatedDurationUs) + " us (deadline " +
                std::to_string(it->second.deadlineUs) + " us).",
            loc,
            "Optimize the @RealTime control loop or increase @Period/@Deadline.");
        return false;
    }
    return true;
}

bool RealTimeExecutor::CheckAllocationAllowedInCurrentContext(Frontend::SourceLocation loc) {
    if (m_inRealTimeNoGCScope) {
        m_diagnostics.ReportError(
            "RT9001",
            "Forbidden GC or unbounded heap allocation inside an @RealTime task.",
            loc,
            "Pre-allocate memory in a deterministic Arena or Pool before entering the @RealTime loop.");
        return false;
    }
    return true;
}

std::optional<RealTimeWCETReport> RealTimeExecutor::GetWCETReport(std::string_view taskName) const {
    auto it = m_reports.find(std::string(taskName));
    if (it == m_reports.end()) return std::nullopt;
    return it->second;
}

} // namespace Robix::Runtime
