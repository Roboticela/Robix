#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"
#include "../Hardware/HardwareAndRealTime.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Robix::Runtime {

// ============================================================================
// 1. SAFETY MONITOR & EMERGENCY STOP (E-STOP)
// ============================================================================
// Enforces Emergency Stop interlocks (ROB9001) and actuator position/velocity
// safety bounds (ROB9002).

class SafetyMonitor {
public:
    explicit SafetyMonitor(Frontend::DiagnosticBag& diagnostics) noexcept
        : m_diagnostics(diagnostics) {}

    void TriggerEmergencyStop(std::string_view reason, Frontend::SourceLocation loc = {});
    void ResetEmergencyStop() noexcept { m_emergencyStopped = false; }

    [[nodiscard]] bool IsEmergencyStopped() const noexcept { return m_emergencyStopped; }
    [[nodiscard]] const std::string& LastEStopReason() const noexcept { return m_lastReason; }

    bool CheckActuatorCommandAllowed(std::string_view actuatorName, Frontend::SourceLocation loc = {});

private:
    Frontend::DiagnosticBag& m_diagnostics;
    bool m_emergencyStopped = false;
    std::string m_lastReason;
};

// ============================================================================
// 2. SENSORS & MOTOR / ACTUATOR CONTROLLERS
// ============================================================================

struct IMUSample {
    StdLib::Vec3 accelMetersPerSec2{};
    StdLib::Vec3 gyroRadPerSec{};
};

struct LiDARScanPoint {
    double angleRad      = 0.0;
    double distanceMeters = 0.0;
};

class QuadratureEncoder {
public:
    explicit QuadratureEncoder(std::int64_t countsPerRevolution) noexcept
        : m_countsPerRev(countsPerRevolution > 0 ? countsPerRevolution : 1) {}

    void AddTicks(std::int64_t deltaTicks) noexcept { m_ticks += deltaTicks; }
    void Reset() noexcept { m_ticks = 0; }

    [[nodiscard]] std::int64_t RawTicks() const noexcept { return m_ticks; }
    [[nodiscard]] double Revolutions() const noexcept {
        return static_cast<double>(m_ticks) / static_cast<double>(m_countsPerRev);
    }
    [[nodiscard]] double AngleRadians() const noexcept {
        return Revolutions() * 2.0 * StdLib::MathModule::PI;
    }

private:
    std::int64_t m_countsPerRev = 4096;
    std::int64_t m_ticks        = 0;
};

class MotorController {
public:
    MotorController(std::string name,
                    HardwareHAL& hal,
                    SafetyMonitor& safety,
                    std::uint32_t pwmChannel,
                    std::uint32_t dirPin,
                    double minCommand,
                    double maxCommand) noexcept;

    bool SetCommand(double commandValue,
                    Frontend::DiagnosticBag& diagnostics,
                    Frontend::SourceLocation loc = {});

    void CoastStop() noexcept;

    [[nodiscard]] double CurrentCommand() const noexcept { return m_currentCommand; }

private:
    std::string m_name;
    HardwareHAL& m_hal;
    SafetyMonitor& m_safety;
    std::uint32_t m_pwmChannel = 0;
    std::uint32_t m_dirPin     = 0;
    double m_minCommand        = -1.0;
    double m_maxCommand        = 1.0;
    double m_currentCommand    = 0.0;
};

// ============================================================================
// 3. CONTROL SYSTEMS (PID, KALMAN FILTER & COMPLEMENTARY FILTER)
// ============================================================================

class PIDController {
public:
    PIDController(double kp,
                  double ki,
                  double kd,
                  double minOutput = -1.0,
                  double maxOutput = 1.0) noexcept;

    double Update(double setpoint, double measurement, double dtSeconds) noexcept;
    void Reset() noexcept;

    [[nodiscard]] double IntegralTerm() const noexcept { return m_integral; }

private:
    double m_kp        = 0.0;
    double m_ki        = 0.0;
    double m_kd        = 0.0;
    double m_minOutput = -1.0;
    double m_maxOutput = 1.0;
    double m_integral  = 0.0;
    double m_prevError = 0.0;
    bool m_firstUpdate = true;
};

// 1D Scalar Kalman Filter for Sensor Noise Filtering
class ScalarKalmanFilter {
public:
    ScalarKalmanFilter(double initialEstimate,
                       double initialErrorCov,
                       double processNoiseVar,
                       double measurementNoiseVar) noexcept;

    double Update(double measurement, double controlDelta = 0.0) noexcept;

    [[nodiscard]] double StateEstimate() const noexcept { return m_x; }
    [[nodiscard]] double ErrorCovariance() const noexcept { return m_p; }

private:
    double m_x = 0.0; // State estimate
    double m_p = 1.0; // Estimation error covariance
    double m_q = 0.01; // Process noise variance
    double m_r = 0.1;  // Measurement noise variance
};

// IMU Complementary Filter (Gyroscope + Accelerometer Tilt Fusion)
class ComplementaryFilter {
public:
    explicit ComplementaryFilter(double alpha = 0.98) noexcept
        : m_alpha(StdLib::MathModule::Clamp(alpha, 0.0, 1.0)) {}

    double Update(double gyroRateRadPerSec, double accelAngleRad, double dtSeconds) noexcept {
        m_angleRad = m_alpha * (m_angleRad + gyroRateRadPerSec * dtSeconds) +
                     (1.0 - m_alpha) * accelAngleRad;
        return m_angleRad;
    }

    [[nodiscard]] double AngleRadians() const noexcept { return m_angleRad; }

private:
    double m_alpha    = 0.98;
    double m_angleRad = 0.0;
};

// ============================================================================
// 4. KINEMATICS, TRAJECTORY PLANNING, COORDINATE FRAMES & NAVIGATION (A*)
// ============================================================================

struct Transform3D {
    StdLib::Vec3 translation{};
    StdLib::Quaternion rotation{};

    [[nodiscard]] StdLib::Vec3 TransformPoint(const StdLib::Vec3& localPoint) const noexcept {
        return rotation.Rotate(localPoint) + translation;
    }
};

struct Planar2LinkManipulator {
    double link1Length = 1.0;
    double link2Length = 1.0;

    // Forward Kinematics: (theta1, theta2) -> End-Effector (x, y)
    [[nodiscard]] StdLib::Vec3 ForwardKinematics(double theta1Rad, double theta2Rad) const noexcept;

    // Analytical Inverse Kinematics: Target (x, y) -> (theta1, theta2)
    // Reports ROB9003 if target is outside the workspace [|L1 - L2|, L1 + L2].
    [[nodiscard]] std::optional<std::pair<double, double>> InverseKinematics(
        double targetX,
        double targetY,
        bool elbowUp,
        Frontend::DiagnosticBag& diagnostics,
        Frontend::SourceLocation loc = {}) const;
};

struct QuinticPolynomialTrajectory {
    // Evaluates a minimum-jerk quintic polynomial trajectory from pos0 to posT over duration T
    // with zero initial/final velocity and acceleration.
    [[nodiscard]] static double EvaluatePosition(double pos0, double posT, double durationSeconds, double tSeconds) noexcept;
    [[nodiscard]] static double EvaluateVelocity(double pos0, double posT, double durationSeconds, double tSeconds) noexcept;
};

struct GridCell2D {
    int row = 0;
    int col = 0;
    constexpr bool operator==(const GridCell2D& other) const noexcept {
        return row == other.row && col == other.col;
    }
};

class OccupancyGridNavigator {
public:
    OccupancyGridNavigator(int rows, int cols);

    void SetObstacle(int row, int col, bool occupied = true);
    [[nodiscard]] bool IsOccupied(int row, int col) const noexcept;

    // 4-connected A* shortest collision-free path planning
    [[nodiscard]] std::vector<GridCell2D> PlanPathAStar(GridCell2D start, GridCell2D goal) const;

private:
    int m_rows = 0;
    int m_cols = 0;
    std::vector<std::uint8_t> m_grid;
};

} // namespace Robix::Runtime
