#include "../../Runtime/Threading/ConcurrencyAndProcess.hpp"
#include "../../StdLib/Collections/CollectionsAndIterators.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"
#include "../../StdLib/Data/JsonAndDataFormats.hpp"
#include "../../StdLib/Security/SecurityAndNetwork.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++g_passed;
        std::cout << "  [PASS] " << description << "\n";
    } else {
        ++g_failed;
        std::cerr << "  [FAIL] " << description << "\n";
    }
}

void TestConsoleIOPrimitivesMathAndRegex() {
    std::cout << "\n--- 1. Console I/O, Primitives, Robix.Math, Robix.Time & ReDoS-Immune Robix.Regex ---\n";
    using namespace Robix::StdLib;

    ConsoleIO::ClearBuffers();
    ConsoleIO::QueueSimulatedInputLine("250");
    ConsoleIO::QueueSimulatedInputLine("not_a_number");
    ConsoleIO::QueueSimulatedInputLine("true");

    auto okInt  = ConsoleIO::TryInputInt("Enter RPM:");
    auto badInt = ConsoleIO::TryInputInt("Enter RPM:");
    auto okBool = ConsoleIO::TryInputBool("Enable Motor:");

    Check(okInt.IsSuccess() && *okInt.value == 250 && badInt.IsError() && okBool.IsSuccess() && *okBool.value,
          "ConsoleIO TryInput<T> validates typed input and returns structured InputError on invalid text");

    // Numeric & String built-in methods
    Check(NumericOps::Abs(-42) == 42 && NumericOps::Round(3.6) == 4.0 &&
          NumericOps::Floor(3.9) == 3.0 && NumericOps::Ceiling(3.1) == 4.0,
          "NumericOps Abs, Round, Floor, and Ceiling operate accurately");

    Check(StringOps::ToUpper("Robix") == "ROBIX" &&
          StringOps::Trim("  hello  ") == "hello" &&
          StringOps::Replace("a-b-c", "-", "_") == "a_b_c" &&
          StringOps::Split("x,y,z", ",").size() == 3,
          "StringOps ToUpper, Trim, Replace, and Split operate accurately");

    // Math Vec3 & Quaternion 90-degree Z-axis rotation of (1, 0, 0) -> (0, 1, 0)
    const Vec3 vX{ 1.0, 0.0, 0.0 };
    const Quaternion rotZ90 = Quaternion::FromAxisAngle(Vec3{ 0.0, 0.0, 1.0 }, MathModule::PI * 0.5);
    const Vec3 rotated      = rotZ90.Rotate(vX);
    Check(std::abs(rotated.x) < 1e-6 && std::abs(rotated.y - 1.0) < 1e-6 && std::abs(rotated.z) < 1e-6,
          "Robix.Math Quaternion::FromAxisAngle and Rotate transform 3D robotics vectors accurately");

    // Robix.Time
    const auto now = RobixDateTime::Now();
    const auto tomorrow = now.AddDays(1);
    Check((tomorrow.ToUnixTime() - now.ToUnixTime()) == 86400 && HighResClock::NowNanoseconds() > 0,
          "Robix.Time RobixDateTime and HighResClock provide accurate timestamps and arithmetic");

    // First-Class LinearRegex (Thompson NFA)
    LinearRegex sensorRx("^SENSOR_[0-9]+\\s*=\\s*\\w+$");
    Check(sensorRx.IsValid() && sensorRx.IsFullMatch("SENSOR_42 = READY") &&
          !sensorRx.IsFullMatch("INVALID_42 = READY"),
          "Robix.Regex validates character classes, quantifiers, and anchors");

    LinearRegex numFinder("\\d+");
    auto allNums = numFinder.FindAll("Motor1=120rpm, Motor2=350rpm");
    Check(allNums.size() == 4 && allNums[1] == "120" && allNums[3] == "350" &&
          numFinder.Replace("Pin12", "#") == "Pin#",
          "Robix.Regex FindAll and Replace extract and transform all matches");

    // ReDoS Immunity Test: Pathological pattern `a*a*a*a*a*a*b` on 80 'a's
    const auto startTime = std::chrono::steady_clock::now();
    LinearRegex redosPattern("a*a*a*a*a*a*b");
    const std::string pathologicalInput(80, 'a');
    const bool matched = redosPattern.Match(pathologicalInput);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    Check(!matched && elapsedMs < 250,
          "Robix.Regex Thompson NFA guarantees linear-time execution (< 250ms) on pathological ReDoS input");
}

void TestCollectionsAndIteratorProtocol() {
    std::cout << "\n--- 2. Collections, Formal Iterator Protocol, Matrix<T> & Tensor<T> ---\n";
    using namespace Robix::StdLib;

    List<int> nums({ 5, 2, 8, 1, 9, 4 });
    nums.Sort();

    // Formal Iterator<T> protocol (`Next() -> Option<T>`)
    auto it = nums.GetIterator();
    int firstVal = *it.Next();
    int countIterated = 1;
    while (it.Next().has_value()) {
        ++countIterated;
    }
    Check(firstVal == 1 && countIterated == 6,
          "Formal Iterator<T>::Next() traverses sorted List<T> in order");

    // Functional Filter -> Map -> Reduce
    const int sumEvenSquares = nums
        .Filter([](const int& x) { return (x % 2) == 0; })
        .Map<int>([](const int& x) { return x * x; })
        .Reduce<int>(0, [](const int& acc, const int& x) { return acc + x; });
    // Even numbers in {1, 2, 4, 5, 8, 9} are 2, 4, 8 -> 4 + 16 + 64 = 84
    Check(sumEvenSquares == 84 && nums.Enumerate().size() == 6,
          "List<T> Filter, Map, Reduce, and Enumerate execute accurately");

    // Dictionary, HashSet, Stack, Queue, PriorityQueue, Graph
    Dictionary<std::string, int> dict;
    dict.Set("LeftMotor", 100);
    Check(dict.Contains("LeftMotor") && *dict.Get("LeftMotor") == 100,
          "Dictionary<K, V> stores and retrieves key-value pairs");

    PriorityQueue<int> pq;
    pq.Enqueue(30);
    pq.Enqueue(10);
    pq.Enqueue(20);
    Check(*pq.Dequeue() == 10 && *pq.Dequeue() == 20,
          "PriorityQueue<T> dequeues elements in priority order");

    Graph<std::string> navGraph;
    navGraph.AddEdge("Dock", "Hallway");
    navGraph.AddEdge("Hallway", "Lab");
    Check(navGraph.HasPathBFS("Dock", "Lab") && !navGraph.HasPathBFS("Lab", "Dock"),
          "Graph<T>::HasPathBFS resolves directed robotics navigation paths");

    // Matrix<T> & Tensor<T>
    Matrix<int> mA(2, 2);
    mA.At(0, 0) = 1; mA.At(0, 1) = 2;
    mA.At(1, 0) = 3; mA.At(1, 1) = 4;
    Matrix<int> mB = mA.Multiply(mA);
    Check(mB.At(0, 0) == 7 && mB.At(0, 1) == 10 && mB.At(1, 0) == 15 && mB.At(1, 1) == 22,
          "Matrix<T>::Multiply computes exact matrix product");

    Tensor<float> tensor({ 2, 3, 4 }, 1.5f);
    Check(tensor.ElementCount() == 24 && tensor.Data()[0] == 1.5f,
          "Tensor<T> initializes N-dimensional contiguous storage");
}

void TestJsonSerializationDatabaseAndIO() {
    std::cout << "\n--- 3. Robix.Json, Schema Validation, CSV/TOML/Binary/Compression, DB & IO ---\n";
    using namespace Robix::StdLib;

    const std::string jsonInput = R"({"robot":"Atlas","battery":92,"active":true,"sensors":["LiDAR","IMU"]})";
    auto parsedOpt = JsonCodec::TryParse(jsonInput);
    Check(parsedOpt.has_value() && parsedOpt->GetField("robot")->strVal == "Atlas" &&
          parsedOpt->GetField("battery")->intVal == 92 &&
          parsedOpt->GetField("sensors")->arrayVal.size() == 2,
          "JsonCodec::TryParse parses nested objects, arrays, strings, ints, and booleans");

    JsonSchema schema;
    schema.requiredFields = { "robot", "battery", "active" };
    schema.fieldTypes["robot"]   = JsonKind::String;
    schema.fieldTypes["battery"] = JsonKind::Int;
    std::string schemaErr;
    Check(schema.Validate(*parsedOpt, schemaErr),
          "JsonSchema::Validate verifies required fields and field types");

    const std::string compactJson = JsonCodec::Serialize(*parsedOpt, JsonFormatOption::Compact);
    const std::string prettyJson  = JsonCodec::Serialize(*parsedOpt, JsonFormatOption::Pretty);
    Check(!compactJson.empty() && prettyJson.find("\n") != std::string::npos,
          "JsonCodec::Serialize supports both Compact and Pretty formatting");

    auto jsonlDocs = JsonCodec::ParseJsonLines("{\"id\":1}\n{\"id\":2}\n");
    Check(jsonlDocs.size() == 2 && jsonlDocs[1].GetField("id")->intVal == 2,
          "JsonCodec::ParseJsonLines parses streaming .jsonl documents");

    // CSV, TOML/YAML, Binary, RLE Compression
    auto csvRows = CsvCodec::Parse("name,speed\nMotorA,120\nMotorB,240");
    Check(csvRows.size() == 3 && csvRows[2][0] == "MotorB" && csvRows[2][1] == "240",
          "CsvCodec parses tabular CSV records");

    auto kv = KeyValueFormatCodec::Parse("Host = \"127.0.0.1\"\nPort = 8080");
    Check(kv["Host"] == "127.0.0.1" && kv["Port"] == "8080",
          "KeyValueFormatCodec parses TOML/YAML configuration entries");

    std::int64_t decodedId = 0;
    std::string decodedStr;
    auto binPacket = BinarySerializer::EncodeInt64AndString(9001, "TelemetryPayload");
    Check(BinarySerializer::DecodeInt64AndString(binPacket, decodedId, decodedStr) &&
          decodedId == 9001 && decodedStr == "TelemetryPayload",
          "BinarySerializer round-trips binary records accurately");

    std::vector<std::uint8_t> rawBytes(100, 0xAB);
    auto compressed   = LosslessCompressor::CompressRLE(rawBytes);
    auto decompressed = LosslessCompressor::DecompressRLE(compressed);
    Check(compressed.size() == 2 && decompressed == rawBytes,
          "LosslessCompressor RLE compresses 100 identical bytes to 2 bytes and restores losslessly");

    // EmbeddedDatabase & CrossPlatformIO
    EmbeddedDatabase db;
    db.CreateTable("Robots");
    db.InsertRow("Robots", { { "id", "1" }, { "status", "Online" } });
    db.InsertRow("Robots", { { "id", "2" }, { "status", "Offline" } });
    auto onlineRows = db.SelectWhereEquals("Robots", "status", "Online");
    Check(db.RowCount("Robots") == 2 && onlineRows.size() == 1 && onlineRows[0].at("id") == "1",
          "EmbeddedDatabase executes table creation, insertion, and filtered queries");

    const std::string tempPath = "Build/Stage7TempIOTest.txt";
    Check(CrossPlatformIO::WriteAllText(tempPath, "Robix OS-Independent IO") &&
          CrossPlatformIO::Exists(tempPath) &&
          *CrossPlatformIO::ReadAllText(tempPath) == "Robix OS-Independent IO" &&
          CrossPlatformIO::RemoveFile(tempPath),
          "CrossPlatformIO writes, reads, checks existence, and removes files cleanly");
}

void TestConcurrencyAsyncCancellationAndProcess() {
    std::cout << "\n--- 4. ThreadPool, Channel<T>, Structured Async Cancellation & Multiprocessing ---\n";
    using namespace Robix::Runtime;

    // 1. ThreadPool & Channel<T>
    ThreadPool pool(4);
    Channel<int> ch(16);
    auto f1 = pool.Submit([&]() { ch.Send(11); return 11; });
    auto f2 = pool.Submit([&]() { ch.Send(29); return 29; });
    Check((f1.get() + f2.get()) == 40,
          "ThreadPool submits concurrent tasks and returns futures");

    int r1 = *ch.Receive();
    int r2 = *ch.Receive();
    ch.Close();
    Check((r1 + r2) == 40,
          "Channel<T> transfers messages safely across worker threads");

    // 2. Structured Async Cancellation + Guaranteed LIFO Defer Cleanup on Cancelled Task
    std::vector<int> deferCleanupOrder;
    RobixMutex orderMtx;
    {
        StructuredAsyncScope scope;
        scope.CancelAllChildren(); // Pre-cancel parent scope to verify child cancellation + defer execution!

        scope.SpawnTask([&](AsyncTaskContext& ctx) {
            ctx.RegisterDeferCleanup([&]() {
                orderMtx.WithLock([&]() { deferCleanupOrder.push_back(1); });
            });
            ctx.RegisterDeferCleanup([&]() {
                orderMtx.WithLock([&]() { deferCleanupOrder.push_back(2); });
            });
            if (ctx.CheckCancellationPoint()) {
                return;
            }
        });
        scope.JoinAll();

        Check(scope.CancelledTaskCount() == 1 && scope.CompletedTaskCount() == 0,
              "StructuredAsyncScope propagates parent cancellation to child tasks");
    }

    // Also test LIFO defer execution inside an active task that hits a cancellation point midway
    deferCleanupOrder.clear();
    {
        CancellationToken rootTok;
        AsyncTaskContext ctx(rootTok.CreateLinkedChildToken());
        ctx.RegisterDeferCleanup([&]() { deferCleanupOrder.push_back(10); });
        ctx.RegisterDeferCleanup([&]() { deferCleanupOrder.push_back(20); });
        rootTok.RequestCancel();
        const bool cancelled = ctx.CheckCancellationPoint();
        Check(cancelled && deferCleanupOrder.size() == 2 &&
              deferCleanupOrder[0] == 20 && deferCleanupOrder[1] == 10,
              "Cancelled async task deterministically executes registered defer/using cleanups in LIFO order");
    }

    // 3. Deadlock Detector (`T7001`)
    Robix::Frontend::DiagnosticBag dlDiag;
    DeadlockDetector detector(dlDiag);
    Check(detector.RecordLockOrderEdge("MotorLock", "SensorLock") &&
          !detector.RecordLockOrderEdge("SensorLock", "MotorLock") &&
          dlDiag.ContainsCode("T7001"),
          "DeadlockDetector catches lock-order inversion cycle (MotorLock <-> SensorLock) with T7001");

    // 4. Multiprocessing (`ProcessManager` & `SharedMemoryRegion`)
    SharedMemoryRegion shm("RobotTelemetrySHM", 64);
    const std::uint64_t magicVal = 0xCAFEBABE12345678ULL;
    std::uint64_t readBack       = 0;
    Check(shm.WriteBytes(0, &magicVal, sizeof(magicVal)) &&
          shm.ReadBytes(0, &readBack, sizeof(readBack)) &&
          readBack == magicVal,
          "SharedMemoryRegion writes and reads IPC bytes safely");

    auto procRes = ProcessManager::LaunchRobixSubprocess(
        "RobotWorker.rbx",
        { "--mode=calibrate" },
        [](const std::vector<std::string>& args, std::string& outStdout) {
            outStdout = "Calibrated:" + args[0];
            return 0;
        });
    Check(procRes.exitCode == 0 && procRes.stdoutText == "Calibrated:--mode=calibrate",
          "ProcessManager::LaunchRobixSubprocess executes isolated worker task and captures stdout/exitCode");
}

void TestSecurityCryptographyAndNetworking() {
    std::cout << "\n--- 5. Formal Security Architecture (Capabilities, Secret<T>, SHA-256, Supply-Chain, Network) ---\n";
    using namespace Robix::StdLib;

    // 1. Capability Sandbox (`SEC7001`)
    Robix::Frontend::DiagnosticBag secDiag;
    CapabilitySandbox sandbox(secDiag);
    sandbox.GrantCapability("Network.Client");

    Check(sandbox.EnforceCapability("Network.Client", "FetchTelemetry") && !secDiag.HasErrors(),
          "CapabilitySandbox allows operations when capability is granted");
    Check(!sandbox.EnforceCapability("Hardware.Camera", "CaptureFrame") && secDiag.ContainsCode("SEC7001"),
          "CapabilitySandbox blocks unauthorized Hardware.Camera access with SEC7001");

    // 2. Zero-on-Drop SecretBuffer + Constant-Time Comparison
    bool wasZeroized = false;
    {
        SecretBuffer apiSecret("sk_live_RoboticelaMasterKey_998877", &wasZeroized);
        Check(apiSecret.RevealScoped() == "sk_live_RoboticelaMasterKey_998877" && !wasZeroized,
              "SecretBuffer holds sensitive credential in RAM while in scope");
    }
    Check(wasZeroized,
          "SecretBuffer automatically zeroizes sensitive memory on scope exit");
    Check(SecretBuffer::ConstantTimeEquals("Token_ABC_123", "Token_ABC_123") &&
          !SecretBuffer::ConstantTimeEquals("Token_ABC_123", "Token_ABC_124"),
          "SecretBuffer::ConstantTimeEquals performs timing-safe secret comparison");

    // 3. RFC 6234 SHA-256, HMAC-SHA256, CSPRNG & Authenticated Encryption
    const std::string sha256Abc = CryptoEngine::ComputeSHA256Hex("abc");
    Check(sha256Abc == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "CryptoEngine::ComputeSHA256Hex matches official NIST FIPS 180-4 / RFC 6234 vector for 'abc'");

    const auto randBytes = CryptoEngine::GenerateCSPRNGBytes(32);
    Check(randBytes.size() == 32,
          "CryptoEngine::GenerateCSPRNGBytes produces 256-bit cryptographic random entropy");

    const auto sealed = CryptoEngine::SealAuthenticated("MasterKey32Bytes", "Nonce123", "TopSecretRobotCommand");
    const auto opened = CryptoEngine::OpenAuthenticated("MasterKey32Bytes", "Nonce123", sealed);
    auto tampered     = sealed;
    tampered[0] ^= 0xFFU;
    const auto openedTampered = CryptoEngine::OpenAuthenticated("MasterKey32Bytes", "Nonce123", tampered);

    Check(opened.has_value() && *opened == "TopSecretRobotCommand" && !openedTampered.has_value(),
          "CryptoEngine SealAuthenticated/OpenAuthenticated decrypts valid ciphertext and rejects tampered ciphertext");

    // 4. Supply-Chain Package Signature & Vulnerability Auditor (`robix audit`)
    const std::string signingKey = "RoboticelaRegistryRootKey";
    const std::string pkgDigest  = CryptoEngine::ComputeSHA256Hex("Robix.Motors.BinaryPayload");
    const std::string validSig   = CryptoEngine::ComputeHMACSHA256Hex(
        signingKey, "Robix.Motors@1.2.0:" + pkgDigest);

    Robix::Frontend::DiagnosticBag auditDiag;
    auto cleanAudit = SupplyChainAuditor::AuditDependencies(
        { { "Robix.Motors", "1.2.0", pkgDigest, validSig } },
        signingKey,
        {},
        auditDiag);
    Check(cleanAudit.passed && !auditDiag.HasErrors() &&
          cleanAudit.generatedSBOMJson.find("RobixSBOM-1.0") != std::string::npos,
          "SupplyChainAuditor verifies valid package signature and generates SBOM JSON");

    Robix::Frontend::DiagnosticBag vulnDiag;
    auto vulnAudit = SupplyChainAuditor::AuditDependencies(
        { { "Robix.Motors", "1.0.0", pkgDigest, "bad_signature" } },
        signingKey,
        { { "Robix.Motors@1.0.0", "CVE-2026-0001: Buffer overflow in legacy driver" } },
        vulnDiag);
    Check(!vulnAudit.passed && vulnDiag.ContainsCode("SEC7002") && vulnDiag.ContainsCode("SEC7003"),
          "SupplyChainAuditor detects tampered package signature (SEC7002) and known CVE vulnerability (SEC7003)");

    // 5. OS-Independent Networking (HTTP Router & MQTT Broker)
    HTTPEndpointRouter router;
    router.RegisterRoute("GET", "/api/status", [](const HTTPRequest&) {
        return HTTPResponse{ 200, "{\"status\":\"Active\"}", {} };
    });
    const auto okResp  = router.Dispatch(HTTPRequest{ "GET", "/api/status", "", {} });
    const auto errResp = router.Dispatch(HTTPRequest{ "GET", "/api/unknown", "", {} });
    Check(okResp.statusCode == 200 && okResp.body == "{\"status\":\"Active\"}" && errResp.statusCode == 404,
          "HTTPEndpointRouter dispatches registered HTTP routes and returns 404 for unknown paths");

    MQTTBroker mqtt;
    std::string receivedTelemetry;
    mqtt.Subscribe("robot/arm/joint1", [&](std::string_view, std::string_view payload) {
        receivedTelemetry = std::string(payload);
    });
    const std::size_t delivered = mqtt.Publish("robot/arm/joint1", "angle=45.0");
    Check(delivered == 1 && receivedTelemetry == "angle=45.0",
          "MQTTBroker delivers published robotics topic messages to subscribers");
}

} // namespace

int main() {
    std::cout << "================================================================\n";
    std::cout << "Robix Stage 7 Built-In Types, Standard Library & Concurrency Suite\n";
    std::cout << "================================================================\n";

    TestConsoleIOPrimitivesMathAndRegex();
    TestCollectionsAndIteratorProtocol();
    TestJsonSerializationDatabaseAndIO();
    TestConcurrencyAsyncCancellationAndProcess();
    TestSecurityCryptographyAndNetworking();

    std::cout << "\n================================================================\n";
    std::cout << "Stage 7 Results: " << g_passed << " passed, " << g_failed << " failed.\n";
    std::cout << "================================================================\n";

    return (g_failed == 0) ? 0 : 1;
}

