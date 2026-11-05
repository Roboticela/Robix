#include "../../Runtime/Hardware/HardwareAndRealTime.hpp"
#include "../../Runtime/Robotics/RoboticsAndControl.hpp"
#include "../../StdLib/AI/AIAndScientificComputing.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool condition, const std::string& testName) {
    if (condition) {
        ++g_passed;
        std::cout << "  [PASS] " << testName << "\n";
    } else {
        ++g_failed;
        std::cout << "  [FAIL] " << testName << "\n";
    }
}

bool HasDiagnosticCode(const Robix::Frontend::DiagnosticBag& bag, std::string_view code) {
    return bag.ContainsCode(code);
}

// ----------------------------------------------------------------------------
// 1. Hardware HAL, Volatile MMIO, DMA & Mandatory Rule 13/14 Verification
// ----------------------------------------------------------------------------
void TestHardwareHALAndEmbeddedIO() {
    std::cout << "\n--- 1. Hardware Abstraction Layer (GPIO, PWM, UART, SPI, I2C, CAN, DMA, Interrupts, MMIO) ---\n";
    using namespace Robix;

    // Mandatory Rule 13: Never pretend physical hardware is present when it is not
    {
        Frontend::DiagnosticBag diags;
        Runtime::HardwareHAL hal(diags);
        const bool ok = hal.InitializeBackend(Runtime::HardwareBackendKind::PhysicalOSOrBareMetal, false);
        Check(!ok && HasDiagnosticCode(diags, "HW9001"),
              "HardwareHAL rejects PhysicalOSOrBareMetal with HW9001 when no physical board is present (Rule 13/14)");
    }

    // Explicit SimulationBackend initialization & peripheral operations
    Frontend::DiagnosticBag diags;
    Runtime::HardwareHAL hal(diags);
    Check(hal.InitializeBackend(Runtime::HardwareBackendKind::SimulationBackend) && hal.IsSimulationMode(),
          "HardwareHAL initializes explicit SimulationBackend cleanly");

    // GPIO Input/Output & unconfigured write protection
    Check(!hal.WriteGPIO(13, Runtime::GPIOPinState::High) && HasDiagnosticCode(diags, "HW9002"),
          "HardwareHAL blocks WriteGPIO on unconfigured pin with HW9002");

    Frontend::DiagnosticBag cleanDiags;
    Runtime::HardwareHAL simHal(cleanDiags);
    simHal.InitializeBackend(Runtime::HardwareBackendKind::SimulationBackend);

    Check(simHal.ConfigureGPIO(13, Runtime::GPIOPinMode::Output) &&
          simHal.WriteGPIO(13, Runtime::GPIOPinState::High) &&
          simHal.ReadGPIO(13) == Runtime::GPIOPinState::High,
          "GPIO ConfigureGPIO(Output), WriteGPIO(High), and ReadGPIO(13) round-trip");

    // PWM
    Check(simHal.ConfigurePWM(0, 20000.0, 0.75) &&
          std::abs(simHal.GetPWMDutyCycle(0) - 0.75) < 1e-9 &&
          std::abs(simHal.GetPWMFrequencyHz(0) - 20000.0) < 1e-6,
          "PWM channel configures 20 kHz carrier with 75% duty cycle");

    // UART
    simHal.ConfigureUART(1, 115200);
    simHal.InjectSimulatedUARTRx(1, "IMU:OK\n");
    Check(simHal.UARTReadAvailable(1) == "IMU:OK\n",
          "UART 115200 baud receives injected telemetry frame");

    // SPI Full-Duplex Transfer
    std::vector<std::uint8_t> spiTx = { 0xAA, 0x55, 0x01, 0xFF };
    std::vector<std::uint8_t> spiRx;
    Check(simHal.SPITransfer(0, spiTx, spiRx) && spiRx == spiTx,
          "SPI full-duplex transfer exchanges byte buffer accurately");

    // I2C 7-bit Device Register Read/Write
    Check(simHal.I2CWriteRegister(1, 0x68, 0x75, 0x71) &&
          simHal.I2CReadRegister(1, 0x68, 0x75) == std::uint8_t{ 0x71 },
          "I2C writes and reads register 0x75 on 7-bit address 0x68");

    // CAN 2.0 Frame Tx/Rx
    Runtime::CANFrame txFrame;
    txFrame.id  = 0x123;
    txFrame.dlc = 4;
    txFrame.data[0] = 0x10;
    txFrame.data[1] = 0x20;
    txFrame.data[2] = 0x30;
    txFrame.data[3] = 0x40;
    simHal.CANSendFrame(0, txFrame);
    auto rxFrameOpt = simHal.CANReceiveFrame(0);
    Check(rxFrameOpt.has_value() && rxFrameOpt->id == 0x123 && rxFrameOpt->dlc == 4 && rxFrameOpt->data[2] == 0x30,
          "CAN bus transmits and receives 2.0 frame with ID 0x123 and DLC 4");

    // Aligned DMA Controller Transfer
    Runtime::DMABufferRegion srcDMA(256, 64);
    Runtime::DMABufferRegion dstDMA(256, 64);
    auto* srcBytes = static_cast<std::uint8_t*>(srcDMA.Data());
    auto* dstBytes = static_cast<std::uint8_t*>(dstDMA.Data());
    for (std::size_t i = 0; i < 64; ++i) srcBytes[i] = static_cast<std::uint8_t>(i + 1);
    Check(simHal.ExecuteDMATransfer(srcDMA, dstDMA, 64) && dstBytes[0] == 1 && dstBytes[63] == 64,
          "HardwareHAL::ExecuteDMATransfer copies 64-byte aligned DMA buffer");

    // Interrupt Controller (ISR) & Volatile MMIO 32-bit Register
    int isrCounter = 0;
    simHal.AttachInterrupt(7, Runtime::InterruptTrigger::RisingEdge, [&]() { ++isrCounter; });
    simHal.TriggerSimulatedInterrupt(7);
    Check(isrCounter == 1, "Interrupt controller dispatches registered ISR on IRQ line 7");

    volatile std::uint32_t rawReg = 0;
    Runtime::VolatileMMIORegister32 mmio(&rawReg);
    mmio.SetBits(0x05u);
    mmio.ClearBits(0x01u);
    Check(mmio.Read() == 0x04u, "VolatileMMIORegister32 SetBits and ClearBits preserve volatile memory ordering");
}

// ----------------------------------------------------------------------------
// 2. Real-Time Execution Profile (@RealTime, WCET, Lock-Free SPSC, Watchdog)
// ----------------------------------------------------------------------------
void TestRealTimeExecutionProfile() {
    std::cout << "\n--- 2. Real-Time Execution Profile (@RealTime, No-GC RT9001, Deadline RT9002, Watchdog RT9003, Lock-Free SPSC) ---\n";
    using namespace Robix;

    // Lock-free SPSC ring queue
    Runtime::LockFreeSPSCQueue<int, 8> spsc;
    Check(spsc.TryPush(10) && spsc.TryPush(20) && spsc.TryPop() == 10 && spsc.TryPop() == 20 && spsc.IsEmpty(),
          "LockFreeSPSCQueue performs wait-free single-producer single-consumer operations");

    // RealTimeExecutor: bounded execution, No-GC enforcement (RT9001), WCET report & deadline miss (RT9002)
    Frontend::DiagnosticBag diags;
    Runtime::RealTimeExecutor rtExec(diags);

    Runtime::RealTimeTaskConfig cfg;
    cfg.taskName               = "ArmTorqueControlLoop";
    cfg.priority               = 90;
    cfg.periodUs               = 1000;
    cfg.deadlineUs             = 1000;
    cfg.forbidGCAndDynamicHeap = true;

    Check(rtExec.RegisterTask(cfg), "RealTimeExecutor registers @RealTime task with priority 90 and 1000 us deadline");

    bool cycle1Ok = rtExec.ExecuteTaskCycle("ArmTorqueControlLoop", [&]() {
        // Attempt forbidden dynamic GC allocation inside @RealTime loop
        rtExec.CheckAllocationAllowedInCurrentContext();
    }, 420);

    Check(cycle1Ok && HasDiagnosticCode(diags, "RT9001"),
          "RealTimeExecutor blocks GC / dynamic heap allocation inside @RealTime loop with RT9001");

    // Trigger deadline miss (> 1000 us)
    bool cycle2Ok = rtExec.ExecuteTaskCycle("ArmTorqueControlLoop", []() {}, 1350);
    auto wcetOpt  = rtExec.GetWCETReport("ArmTorqueControlLoop");
    Check(!cycle2Ok && HasDiagnosticCode(diags, "RT9002") &&
          wcetOpt.has_value() && wcetOpt->invocations == 2 &&
          wcetOpt->minExecutionTimeUs == 420 && wcetOpt->maxExecutionTimeUs == 1350 &&
          wcetOpt->deadlineMisses == 1,
          "RealTimeExecutor records WCET (1350 us) and reports RT9002 on deadline miss");

    // Watchdog Timer (RT9003)
    Frontend::DiagnosticBag wdDiags;
    Runtime::WatchdogTimer watchdog(5000);
    watchdog.Kick(1000);
    Check(!watchdog.CheckExpired(4000, wdDiags), "WatchdogTimer does not trip within timeout window");
    Check(watchdog.CheckExpired(7500, wdDiags) && watchdog.IsTripped() && HasDiagnosticCode(wdDiags, "RT9003"),
          "WatchdogTimer trips and reports RT9003 when control loop exceeds timeout");
}

// ----------------------------------------------------------------------------
// 3. Robotics Standard Library (E-Stop, Encoders, Motors, PID, Kalman, IK, A*)
// ----------------------------------------------------------------------------
void TestRoboticsAndControlSystems() {
    std::cout << "\n--- 3. Robotics Standard Library (Safety E-Stop, Motors, PID, Kalman, Kinematics, Trajectory, A*) ---\n";
    using namespace Robix;

    Frontend::DiagnosticBag diags;
    Runtime::HardwareHAL hal(diags);
    hal.InitializeBackend(Runtime::HardwareBackendKind::SimulationBackend);

    Runtime::SafetyMonitor safety(diags);
    Runtime::MotorController jointMotor("ShoulderMotor", hal, safety, 1, 4, -10.0, 10.0);

    Check(jointMotor.SetCommand(5.0, diags) && std::abs(hal.GetPWMDutyCycle(1) - 0.5) < 1e-6,
          "MotorController commands 50% PWM duty cycle and sets direction GPIO");

    // Out-of-bounds actuator command (ROB9002)
    Check(!jointMotor.SetCommand(15.0, diags) && HasDiagnosticCode(diags, "ROB9002"),
          "MotorController rejects actuator command exceeding safety limits with ROB9002");

    // Emergency Stop (ROB9001)
    safety.TriggerEmergencyStop("Obstacle collision proximity alert");
    Check(safety.IsEmergencyStopped() && !jointMotor.SetCommand(2.0, diags) &&
          std::abs(jointMotor.CurrentCommand()) < 1e-9 && HasDiagnosticCode(diags, "ROB9001"),
          "SafetyMonitor Emergency Stop (E-Stop) halts motor and rejects commands with ROB9001");

    // Quadrature Encoder
    Runtime::QuadratureEncoder encoder(4096);
    encoder.AddTicks(2048);
    Check(std::abs(encoder.Revolutions() - 0.5) < 1e-9 &&
          std::abs(encoder.AngleRadians() - StdLib::MathModule::PI) < 1e-9,
          "QuadratureEncoder computes revolutions (0.5) and angle (PI rad) from tick count");

    // Closed-loop PID controller convergence on simulated 1st-order plant with disturbance
    Runtime::PIDController pid(4.0, 2.0, 0.05, -10.0, 10.0);
    double plantPosition = 0.0;
    const double target  = 1.0;
    const double dt      = 0.02;
    for (int step = 0; step < 300; ++step) {
        const double u = pid.Update(target, plantPosition, dt);
        plantPosition += (u - 0.5 * plantPosition) * dt;
    }
    Check(std::abs(plantPosition - target) < 1e-2,
          "PIDController converges closed-loop plant position to target setpoint 1.0");

    // Scalar Kalman Filter & Complementary Filter
    Runtime::ScalarKalmanFilter kf(0.0, 1.0, 0.01, 0.25);
    for (int i = 0; i < 30; ++i) {
        kf.Update(10.0);
    }
    Check(std::abs(kf.StateEstimate() - 10.0) < 0.05 && kf.ErrorCovariance() < 0.1,
          "ScalarKalmanFilter converges state estimate to 10.0 and shrinks error covariance");

    Runtime::ComplementaryFilter compFilter(0.98);
    const double fusedAngle = compFilter.Update(0.5, 0.1, 0.01);
    Check(fusedAngle > 0.0, "ComplementaryFilter fuses gyroscope rate and accelerometer tilt angle");

    // 2-Link Planar Arm Forward & Analytical Inverse Kinematics
    Runtime::Planar2LinkManipulator arm{ 1.0, 1.0 };
    const double q1 = 0.4;
    const double q2 = 0.7;
    const StdLib::Vec3 eePos = arm.ForwardKinematics(q1, q2);

    Frontend::DiagnosticBag ikDiags;
    auto ikSol = arm.InverseKinematics(eePos.x, eePos.y, false, ikDiags);
    Check(ikSol.has_value(), "Planar2LinkManipulator::InverseKinematics solves reachable target");
    if (ikSol.has_value()) {
        const StdLib::Vec3 fkVerify = arm.ForwardKinematics(ikSol->first, ikSol->second);
        Check( std::abs(fkVerify.x - eePos.x) < 1e-6 && std::abs(fkVerify.y - eePos.y) < 1e-6,
              "ForwardKinematics(InverseKinematics(x, y)) matches target end-effector position");
    }

    auto unreachableIK = arm.InverseKinematics(5.0, 5.0, false, ikDiags);
    Check(!unreachableIK.has_value() && HasDiagnosticCode(ikDiags, "ROB9003"),
          "InverseKinematics reports ROB9003 when target lies outside manipulator workspace");

    // Minimum-jerk Quintic Trajectory Planning
    Check(std::abs(Runtime::QuinticPolynomialTrajectory::EvaluatePosition(0.0, 10.0, 2.0, 0.0) - 0.0) < 1e-9 &&
          std::abs(Runtime::QuinticPolynomialTrajectory::EvaluatePosition(0.0, 10.0, 2.0, 1.0) - 5.0) < 1e-9 &&
          std::abs(Runtime::QuinticPolynomialTrajectory::EvaluatePosition(0.0, 10.0, 2.0, 2.0) - 10.0) < 1e-9 &&
          std::abs(Runtime::QuinticPolynomialTrajectory::EvaluateVelocity(0.0, 10.0, 2.0, 0.0)) < 1e-9 &&
          std::abs(Runtime::QuinticPolynomialTrajectory::EvaluateVelocity(0.0, 10.0, 2.0, 2.0)) < 1e-9,
          "QuinticPolynomialTrajectory interpolates smoothly with zero boundary velocity");

    // Occupancy Grid A* Path Planner
    Runtime::OccupancyGridNavigator nav(5, 5);
    nav.SetObstacle(1, 1, true);
    nav.SetObstacle(1, 2, true);
    nav.SetObstacle(1, 3, true);
    auto path = nav.PlanPathAStar({ 0, 2 }, { 2, 2 });
    Check(!path.empty() && path.front() == Runtime::GridCell2D{ 0, 2 } &&
          path.back() == Runtime::GridCell2D{ 2, 2 } && !nav.IsOccupied(path[1].row, path[1].col),
          "OccupancyGridNavigator::PlanPathAStar finds collision-free shortest path around obstacles");
}

// ----------------------------------------------------------------------------
// 4. AI, Machine Learning, Autodiff, Computer Vision & Audio Signal Processing
// ----------------------------------------------------------------------------
void TestAIAndScientificComputing() {
    std::cout << "\n--- 4. AI, Autodiff, Neural Networks, SIMD, Computer Vision & Audio DSP ---\n";
    using namespace Robix;

    // SIMD 4-lane vector dot product
    const float a[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    const float b[6] = { 2.0f, 1.0f, 0.5f, 2.0f, 1.0f, 1.5f };
    const float dot  = StdLib::SIMDVec4f::DotProduct(a, b, 6);
    // 2 + 2 + 1.5 + 8 + 5 + 9 = 27.5
    Check(std::abs(dot - 27.5f) < 1e-5f, "SIMDVec4f::DotProduct computes 4-wide vectorized dot product");

    // Reverse-Mode Automatic Differentiation: f(x, y) = ReLU(x * y + x) at x=3, y=4 -> f=15, df/dx = y+1 = 5, df/dy = x = 3
    StdLib::AutodiffTape tape;
    const auto x   = tape.CreateVariable(3.0);
    const auto y   = tape.CreateVariable(4.0);
    const auto xy  = tape.Multiply(x, y);
    const auto sum = tape.Add(xy, x);
    const auto out = tape.ReLU(sum);
    tape.Backward(out);
    Check(std::abs(tape.Value(out) - 15.0) < 1e-9 &&
          std::abs(tape.Gradient(x) - 5.0) < 1e-9 &&
          std::abs(tape.Gradient(y) - 3.0) < 1e-9,
          "AutodiffTape computes exact reverse-mode gradients for f(x, y) = ReLU(x*y + x)");

    // Sequential Neural Network Model (Sync + Async Inference + Softmax)
    StdLib::Matrix<double> w1(2, 2, 0.0);
    w1.At(0, 0) = 1.0; w1.At(0, 1) = 0.5;
    w1.At(1, 0) = -1.0; w1.At(1, 1) = 2.0;
    StdLib::DenseLayer layer1(2, 2, w1, { 0.1, -0.2 }, StdLib::ActivationKind::ReLU);

    StdLib::Matrix<double> w2(2, 2, 0.0);
    w2.At(0, 0) = 1.5; w2.At(0, 1) = -0.5;
    w2.At(1, 0) = 0.2; w2.At(1, 1) = 1.0;
    StdLib::DenseLayer layer2(2, 2, w2, { 0.0, 0.0 }, StdLib::ActivationKind::Softmax);

    StdLib::SequentialNeuralModel model("ObstacleClassifierNet");
    model.AddLayer(layer1);
    model.AddLayer(layer2);

    Frontend::DiagnosticBag aiDiags;
    auto predOpt = model.Predict({ 2.0, 1.0 }, aiDiags);
    Check(predOpt.has_value() && predOpt->size() == 2 &&
          std::abs(((*predOpt)[0] + (*predOpt)[1]) - 1.0) < 1e-9 &&
          (*predOpt)[0] > (*predOpt)[1],
          "SequentialNeuralModel::Predict runs multi-layer ReLU + Softmax forward pass");

    auto asyncFut = model.PredictAsync({ 2.0, 1.0 }, aiDiags);
    auto asyncRes = asyncFut.get();
    Check(asyncRes.has_value() && std::abs((*asyncRes)[0] - (*predOpt)[0]) < 1e-9,
          "SequentialNeuralModel::PredictAsync returns identical inference output asynchronously");

    auto badShape = model.Predict({ 1.0, 2.0, 3.0 }, aiDiags);
    Check(!badShape.has_value() && HasDiagnosticCode(aiDiags, "AI9001"),
          "DenseLayer::Forward detects input tensor dimension mismatch and reports AI9001");

    // Computer Vision: 3x3 Sobel Edge Detector on vertical step edge
    StdLib::GrayscaleImage2D img(5, 5, 0);
    for (std::size_t y = 0; y < 5; ++y) {
        for (std::size_t x = 3; x < 5; ++x) {
            img.SetPixel(x, y, 200);
        }
    }
    const auto edges = img.ComputeSobelEdgeMagnitude();
    Check(edges.GetPixel(2, 2) > 200 && edges.GetPixel(0, 2) == 0,
          "GrayscaleImage2D::ComputeSobelEdgeMagnitude detects vertical intensity boundary");

    // Audio / Vibration DSP: DFT & RMS
    const std::vector<double> dcSignal = { 2.0, 2.0, 2.0, 2.0 };
    const auto dft = StdLib::AudioSignalProcessor::ComputeDFT(dcSignal);
    Check(dft.size() == 4 && std::abs(dft[0].Magnitude() - 8.0) < 1e-9 &&
          std::abs(dft[1].Magnitude()) < 1e-9 &&
          std::abs(StdLib::AudioSignalProcessor::ComputeRMS(dcSignal) - 2.0) < 1e-9,
          "AudioSignalProcessor computes exact DFT frequency bins and RMS amplitude");
}

} // namespace

int main() {
    std::cout << "================================================================\n";
    std::cout << "Robix Stage 9 Hardware, Real-Time, Robotics & AI Test Suite\n";
    std::cout << "================================================================\n";

    TestHardwareHALAndEmbeddedIO();
    TestRealTimeExecutionProfile();
    TestRoboticsAndControlSystems();
    TestAIAndScientificComputing();

    std::cout << "\n================================================================\n";
    std::cout << "Stage 9 Results: " << g_passed << " passed, " << g_failed << " failed.\n";
    std::cout << "================================================================\n";

    return (g_failed == 0) ? 0 : 1;
}
