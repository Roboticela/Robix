#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../Collections/CollectionsAndIterators.hpp"
#include "../Core/CoreTypesAndRegex.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Robix::StdLib {

// ============================================================================
// 1. SIMD VECTORIZATION & ACCELERATED MATRIX / TENSOR KERNELS (Robix.GPU / SIMD)
// ============================================================================

struct alignas(32) SIMDVec4f {
    float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    constexpr SIMDVec4f() noexcept = default;
    constexpr SIMDVec4f(float v0, float v1, float v2, float v3) noexcept
        : v{ v0, v1, v2, v3 } {}

    [[nodiscard]] constexpr SIMDVec4f operator+(const SIMDVec4f& rhs) const noexcept {
        return SIMDVec4f(v[0] + rhs.v[0], v[1] + rhs.v[1], v[2] + rhs.v[2], v[3] + rhs.v[3]);
    }

    [[nodiscard]] constexpr SIMDVec4f operator*(const SIMDVec4f& rhs) const noexcept {
        return SIMDVec4f(v[0] * rhs.v[0], v[1] * rhs.v[1], v[2] * rhs.v[2], v[3] * rhs.v[3]);
    }

    [[nodiscard]] constexpr float HorizontalSum() const noexcept {
        return (v[0] + v[1]) + (v[2] + v[3]);
    }

    [[nodiscard]] static float DotProduct(const float* a, const float* b, std::size_t count) noexcept;
};

// ============================================================================
// 2. REVERSE-MODE AUTOMATIC DIFFERENTIATION (Robix.AI.Autodiff)
// ============================================================================

class AutodiffTape {
public:
    std::size_t CreateVariable(double initialValue);
    std::size_t Add(std::size_t lhsId, std::size_t rhsId);
    std::size_t Multiply(std::size_t lhsId, std::size_t rhsId);
    std::size_t ReLU(std::size_t inputId);

    void Backward(std::size_t outputId);

    [[nodiscard]] double Value(std::size_t id) const noexcept;
    [[nodiscard]] double Gradient(std::size_t id) const noexcept;

private:
    enum class OpKind : std::uint8_t { Leaf, Add, Mul, ReLU };
    struct TapeNode {
        OpKind op         = OpKind::Leaf;
        double value      = 0.0;
        double grad       = 0.0;
        std::size_t left  = 0;
        std::size_t right = 0;
    };
    std::vector<TapeNode> m_nodes;
};

// ============================================================================
// 3. NEURAL NETWORKS & MODEL INFERENCE (Robix.AI & Robix.NeuralNetworks)
// ============================================================================

enum class ActivationKind : std::uint8_t {
    Linear,
    ReLU,
    Sigmoid,
    Tanh,
    Softmax
};

class ActivationFunctions {
public:
    [[nodiscard]] static double ReLU(double x) noexcept { return x > 0.0 ? x : 0.0; }
    [[nodiscard]] static double Sigmoid(double x) noexcept { return 1.0 / (1.0 + std::exp(-x)); }
    [[nodiscard]] static double Tanh(double x) noexcept { return std::tanh(x); }
    [[nodiscard]] static std::vector<double> Apply(const std::vector<double>& input, ActivationKind kind);
};

class DenseLayer {
public:
    DenseLayer(std::size_t inputDim,
               std::size_t outputDim,
               Matrix<double> weights,
               std::vector<double> biases,
               ActivationKind activation);

    [[nodiscard]] std::size_t InputDim() const noexcept { return m_inputDim; }
    [[nodiscard]] std::size_t OutputDim() const noexcept { return m_outputDim; }

    [[nodiscard]] std::optional<std::vector<double>> Forward(
        const std::vector<double>& input,
        Frontend::DiagnosticBag& diagnostics,
        Frontend::SourceLocation loc = {}) const;

private:
    std::size_t m_inputDim  = 0;
    std::size_t m_outputDim = 0;
    Matrix<double> m_weights;
    std::vector<double> m_biases;
    ActivationKind m_activation = ActivationKind::Linear;
};

class SequentialNeuralModel {
public:
    explicit SequentialNeuralModel(std::string modelName) noexcept
        : m_modelName(std::move(modelName)) {}

    void AddLayer(DenseLayer layer);

    [[nodiscard]] std::optional<std::vector<double>> Predict(
        const std::vector<double>& input,
        Frontend::DiagnosticBag& diagnostics,
        Frontend::SourceLocation loc = {}) const;

    [[nodiscard]] std::future<std::optional<std::vector<double>>> PredictAsync(
        std::vector<double> input,
        Frontend::DiagnosticBag& diagnostics,
        Frontend::SourceLocation loc = {}) const;

    [[nodiscard]] const std::string& Name() const noexcept { return m_modelName; }
    [[nodiscard]] std::size_t LayerCount() const noexcept { return m_layers.size(); }

private:
    std::string m_modelName;
    std::vector<DenseLayer> m_layers;
};

// ============================================================================
// 4. COMPUTER VISION & AUDIO/SIGNAL PROCESSING (Robix.ComputerVision & Robix.Audio)
// ============================================================================

class GrayscaleImage2D {
public:
    GrayscaleImage2D(std::size_t width, std::size_t height, std::uint8_t initialPixel = 0);

    [[nodiscard]] std::size_t Width() const noexcept { return m_width; }
    [[nodiscard]] std::size_t Height() const noexcept { return m_height; }

    void SetPixel(std::size_t x, std::size_t y, std::uint8_t val) noexcept;
    [[nodiscard]] std::uint8_t GetPixel(std::size_t x, std::size_t y) const noexcept;

    // 3x3 Sobel Edge Magnitude Filter
    [[nodiscard]] GrayscaleImage2D ComputeSobelEdgeMagnitude() const;

private:
    std::size_t m_width  = 0;
    std::size_t m_height = 0;
    std::vector<std::uint8_t> m_pixels;
};

struct ComplexSample {
    double real = 0.0;
    double imag = 0.0;
    [[nodiscard]] double Magnitude() const noexcept { return std::sqrt(real * real + imag * imag); }
};

class AudioSignalProcessor {
public:
    // Discrete Fourier Transform (DFT) spectrum computation for sensor/audio signals
    [[nodiscard]] static std::vector<ComplexSample> ComputeDFT(const std::vector<double>& timeDomainSamples);

    // Root-Mean-Square (RMS) signal energy
    [[nodiscard]] static double ComputeRMS(const std::vector<double>& samples) noexcept;
};

} // namespace Robix::StdLib

