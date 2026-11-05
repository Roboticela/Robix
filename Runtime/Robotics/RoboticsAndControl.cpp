#include "RoboticsAndControl.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace Robix::Runtime {

// ============================================================================
// 1. SAFETY MONITOR & EMERGENCY STOP IMPLEMENTATION
// ============================================================================

void SafetyMonitor::TriggerEmergencyStop(std::string_view reason, Frontend::SourceLocation loc) {
    m_emergencyStopped = true;
    m_lastReason       = std::string(reason);
    m_diagnostics.ReportError(
        "ROB9001",
        "Emergency Stop (E-Stop) triggered: " + m_lastReason,
        loc,
        "Clear the physical hazard and call ResetEmergencyStop() before issuing actuator commands.");
}

bool SafetyMonitor::CheckActuatorCommandAllowed(std::string_view actuatorName, Frontend::SourceLocation loc) {
    if (m_emergencyStopped) {
        m_diagnostics.ReportError(
            "ROB9001",
            "Actuator '" + std::string(actuatorName) + "' command rejected while Emergency Stop (E-Stop) is active (" +
                m_lastReason + ").",
            loc,
            "Reset E-Stop after verifying robot workspace safety.");
        return false;
    }
    return true;
}

// ============================================================================
// 2. MOTOR CONTROLLER IMPLEMENTATION
// ============================================================================

MotorController::MotorController(std::string name,
                                 HardwareHAL& hal,
                                 SafetyMonitor& safety,
                                 std::uint32_t pwmChannel,
                                 std::uint32_t dirPin,
                                 double minCommand,
                                 double maxCommand) noexcept
    : m_name(std::move(name)),
      m_hal(hal),
      m_safety(safety),
      m_pwmChannel(pwmChannel),
      m_dirPin(dirPin),
      m_minCommand(minCommand),
      m_maxCommand(maxCommand) {
    m_hal.ConfigureGPIO(m_dirPin, GPIOPinMode::Output);
}

bool MotorController::SetCommand(double commandValue,
                                 Frontend::DiagnosticBag& diagnostics,
                                 Frontend::SourceLocation loc) {
    if (!m_safety.CheckActuatorCommandAllowed(m_name, loc)) {
        CoastStop();
        return false;
    }
    if (commandValue < m_minCommand || commandValue > m_maxCommand) {
        diagnostics.ReportError(
            "ROB9002",
            "Actuator '" + m_name + "' command " + std::to_string(commandValue) +
                " exceeds configured safety limits [" + std::to_string(m_minCommand) + ", " +
                std::to_string(m_maxCommand) + "].",
            loc,
            "Clamp actuator commands to the safe operating envelope.");
        return false;
    }

    m_currentCommand = commandValue;
    const GPIOPinState dirState = (commandValue >= 0.0) ? GPIOPinState::High : GPIOPinState::Low;
    m_hal.WriteGPIO(m_dirPin, dirState, loc);

    const double maxMag = std::max(std::abs(m_minCommand), std::abs(m_maxCommand));
    const double duty   = (maxMag > 1e-12) ? std::clamp(std::abs(commandValue) / maxMag, 0.0, 1.0) : 0.0;
    return m_hal.ConfigurePWM(m_pwmChannel, 20000.0, duty, loc);
}

void MotorController::CoastStop() noexcept {
    m_currentCommand = 0.0;
    Frontend::DiagnosticBag ignored;
    m_hal.ConfigurePWM(m_pwmChannel, 20000.0, 0.0);
}

// ============================================================================
// 3. PID & KALMAN FILTER IMPLEMENTATION
// ============================================================================

PIDController::PIDController(double kp, double ki, double kd, double minOutput, double maxOutput) noexcept
    : m_kp(kp), m_ki(ki), m_kd(kd), m_minOutput(minOutput), m_maxOutput(maxOutput) {}

double PIDController::Update(double setpoint, double measurement, double dtSeconds) noexcept {
    if (dtSeconds <= 1e-12) return 0.0;

    const double error = setpoint - measurement;
    m_integral += error * dtSeconds;

    // Anti-windup integral clamping
    if (m_ki > 1e-12) {
        const double maxInt = m_maxOutput / m_ki;
        const double minInt = m_minOutput / m_ki;
        m_integral = std::clamp(m_integral, minInt, maxInt);
    }

    const double derivative = m_firstUpdate ? 0.0 : ((error - m_prevError) / dtSeconds);
    m_firstUpdate = false;
    m_prevError   = error;

    const double rawOut = (m_kp * error) + (m_ki * m_integral) + (m_kd * derivative);
    return std::clamp(rawOut, m_minOutput, m_maxOutput);
}

void PIDController::Reset() noexcept {
    m_integral    = 0.0;
    m_prevError   = 0.0;
    m_firstUpdate = true;
}

ScalarKalmanFilter::ScalarKalmanFilter(double initialEstimate,
                                       double initialErrorCov,
                                       double processNoiseVar,
                                       double measurementNoiseVar) noexcept
    : m_x(initialEstimate),
      m_p(initialErrorCov),
      m_q(processNoiseVar),
      m_r(measurementNoiseVar) {}

double ScalarKalmanFilter::Update(double measurement, double controlDelta) noexcept {
    // Predict
    const double xPred = m_x + controlDelta;
    const double pPred = m_p + m_q;

    // Update
    const double kalmanGain = pPred / (pPred + m_r);
    m_x = xPred + kalmanGain * (measurement - xPred);
    m_p = (1.0 - kalmanGain) * pPred;
    return m_x;
}

// ============================================================================
// 4. KINEMATICS, TRAJECTORY PLANNING & A* NAVIGATION IMPLEMENTATION
// ============================================================================

StdLib::Vec3 Planar2LinkManipulator::ForwardKinematics(double theta1Rad, double theta2Rad) const noexcept {
    const double x = link1Length * std::cos(theta1Rad) + link2Length * std::cos(theta1Rad + theta2Rad);
    const double y = link1Length * std::sin(theta1Rad) + link2Length * std::sin(theta1Rad + theta2Rad);
    return StdLib::Vec3{ x, y, 0.0 };
}

std::optional<std::pair<double, double>> Planar2LinkManipulator::InverseKinematics(
    double targetX,
    double targetY,
    bool elbowUp,
    Frontend::DiagnosticBag& diagnostics,
    Frontend::SourceLocation loc) const {

    const double distSq = targetX * targetX + targetY * targetY;
    const double cosTheta2 = (distSq - link1Length * link1Length - link2Length * link2Length) /
                             (2.0 * link1Length * link2Length);

    if (cosTheta2 < -1.0 || cosTheta2 > 1.0) {
        diagnostics.ReportError(
            "ROB9003",
            "Inverse Kinematics target (" + std::to_string(targetX) + ", " + std::to_string(targetY) +
                ") is unreachable for link lengths L1=" + std::to_string(link1Length) +
                ", L2=" + std::to_string(link2Length) + ".",
            loc,
            "Verify target coordinates lie within the manipulator reachable workspace.");
        return std::nullopt;
    }

    const double sinMag = std::sqrt(std::max(0.0, 1.0 - cosTheta2 * cosTheta2));
    const double sinTheta2 = elbowUp ? -sinMag : sinMag;
    const double theta2    = std::atan2(sinTheta2, cosTheta2);

    const double k1     = link1Length + link2Length * cosTheta2;
    const double k2     = link2Length * sinTheta2;
    const double theta1 = std::atan2(targetY, targetX) - std::atan2(k2, k1);

    return std::make_pair(theta1, theta2);
}

double QuinticPolynomialTrajectory::EvaluatePosition(
    double pos0, double posT, double durationSeconds, double tSeconds) noexcept {
    if (durationSeconds <= 1e-12) return posT;
    const double tau = std::clamp(tSeconds / durationSeconds, 0.0, 1.0);
    const double tau3 = tau * tau * tau;
    const double tau4 = tau3 * tau;
    const double tau5 = tau4 * tau;
    const double s    = 10.0 * tau3 - 15.0 * tau4 + 6.0 * tau5;
    return pos0 + (posT - pos0) * s;
}

double QuinticPolynomialTrajectory::EvaluateVelocity(
    double pos0, double posT, double durationSeconds, double tSeconds) noexcept {
    if (durationSeconds <= 1e-12) return 0.0;
    const double tau = std::clamp(tSeconds / durationSeconds, 0.0, 1.0);
    const double tau2 = tau * tau;
    const double tau3 = tau2 * tau;
    const double tau4 = tau3 * tau;
    const double ds   = (30.0 * tau2 - 60.0 * tau3 + 30.0 * tau4) / durationSeconds;
    return (posT - pos0) * ds;
}

OccupancyGridNavigator::OccupancyGridNavigator(int rows, int cols)
    : m_rows(rows), m_cols(cols), m_grid(static_cast<std::size_t>(rows * cols), 0) {}

void OccupancyGridNavigator::SetObstacle(int row, int col, bool occupied) {
    if (row >= 0 && row < m_rows && col >= 0 && col < m_cols) {
        m_grid[static_cast<std::size_t>(row * m_cols + col)] = occupied ? 1 : 0;
    }
}

bool OccupancyGridNavigator::IsOccupied(int row, int col) const noexcept {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return true;
    return m_grid[static_cast<std::size_t>(row * m_cols + col)] != 0;
}

std::vector<GridCell2D> OccupancyGridNavigator::PlanPathAStar(GridCell2D start, GridCell2D goal) const {
    if (IsOccupied(start.row, start.col) || IsOccupied(goal.row, goal.col)) {
        return {};
    }

    auto toIndex = [this](GridCell2D c) { return c.row * m_cols + c.col; };
    auto heuristic = [](GridCell2D a, GridCell2D b) {
        return std::abs(a.row - b.row) + std::abs(a.col - b.col);
    };

    struct Node {
        int fScore = 0;
        int gScore = 0;
        GridCell2D cell{};
        bool operator>(const Node& other) const noexcept { return fScore > other.fScore; }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::unordered_map<int, int> bestG;
    std::unordered_map<int, GridCell2D> cameFrom;

    const int startIdx = toIndex(start);
    bestG[startIdx]    = 0;
    openSet.push(Node{ heuristic(start, goal), 0, start });

    constexpr int dRow[4] = { -1, 1, 0, 0 };
    constexpr int dCol[4] = { 0, 0, -1, 1 };

    while (!openSet.empty()) {
        const Node current = openSet.top();
        openSet.pop();

        if (current.cell == goal) {
            std::vector<GridCell2D> path;
            GridCell2D step = goal;
            while (!(step == start)) {
                path.push_back(step);
                step = cameFrom[toIndex(step)];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dir = 0; dir < 4; ++dir) {
            const GridCell2D neighbor{ current.cell.row + dRow[dir], current.cell.col + dCol[dir] };
            if (IsOccupied(neighbor.row, neighbor.col)) continue;

            const int tentativeG = current.gScore + 1;
            const int nIdx       = toIndex(neighbor);
            auto it = bestG.find(nIdx);
            if (it == bestG.end() || tentativeG < it->second) {
                bestG[nIdx]    = tentativeG;
                cameFrom[nIdx] = current.cell;
                openSet.push(Node{ tentativeG + heuristic(neighbor, goal), tentativeG, neighbor });
            }
        }
    }

    return {};
}

} // namespace Robix::Runtime

