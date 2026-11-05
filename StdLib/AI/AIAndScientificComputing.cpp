#include "AIAndScientificComputing.hpp"

#include <algorithm>
#include <cmath>

namespace Robix::StdLib {

// ============================================================================
// 1. SIMD VECTORIZATION IMPLEMENTATION
// ============================================================================

float SIMDVec4f::DotProduct(const float* a, const float* b, std::size_t count) noexcept {
    if (!a || !b || count == 0) return 0.0f;

    SIMDVec4f acc(0.0f, 0.0f, 0.0f, 0.0f);
    std::size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        const SIMDVec4f va(a[i], a[i + 1], a[i + 2], a[i + 3]);
        const SIMDVec4f vb(b[i], b[i + 1], b[i + 2], b[i + 3]);
        acc = acc + (va * vb);
    }

    float sum = acc.HorizontalSum();
    for (; i < count; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============================================================================
// 2. REVERSE-MODE AUTOMATIC DIFFERENTIATION IMPLEMENTATION
// ============================================================================

std::size_t AutodiffTape::CreateVariable(double initialValue) {
    const std::size_t id = m_nodes.size();
    m_nodes.push_back(TapeNode{ OpKind::Leaf, initialValue, 0.0, 0, 0 });
    return id;
}

std::size_t AutodiffTape::Add(std::size_t lhsId, std::size_t rhsId) {
    const std::size_t id = m_nodes.size();
    const double val = m_nodes[lhsId].value + m_nodes[rhsId].value;
    m_nodes.push_back(TapeNode{ OpKind::Add, val, 0.0, lhsId, rhsId });
    return id;
}

std::size_t AutodiffTape::Multiply(std::size_t lhsId, std::size_t rhsId) {
    const std::size_t id = m_nodes.size();
    const double val = m_nodes[lhsId].value * m_nodes[rhsId].value;
    m_nodes.push_back(TapeNode{ OpKind::Mul, val, 0.0, lhsId, rhsId });
    return id;
}

std::size_t AutodiffTape::ReLU(std::size_t inputId) {
    const std::size_t id = m_nodes.size();
    const double val = m_nodes[inputId].value > 0.0 ? m_nodes[inputId].value : 0.0;
    m_nodes.push_back(TapeNode{ OpKind::ReLU, val, 0.0, inputId, 0 });
    return id;
}

void AutodiffTape::Backward(std::size_t outputId) {
    if (outputId >= m_nodes.size()) return;
    for (auto& node : m_nodes) {
        node.grad = 0.0;
    }
    m_nodes[outputId].grad = 1.0;

    for (std::size_t i = outputId + 1; i-- > 0;) {
        const TapeNode& cur = m_nodes[i];
        switch (cur.op) {
            case OpKind::Leaf:
                break;
            case OpKind::Add:
                m_nodes[cur.left].grad  += cur.grad;
                m_nodes[cur.right].grad += cur.grad;
                break;
            case OpKind::Mul:
                m_nodes[cur.left].grad  += cur.grad * m_nodes[cur.right].value;
                m_nodes[cur.right].grad += cur.grad * m_nodes[cur.left].value;
                break;
            case OpKind::ReLU:
                if (m_nodes[cur.left].value > 0.0) {
                    m_nodes[cur.left].grad += cur.grad;
                }
                break;
        }
    }
}

double AutodiffTape::Value(std::size_t id) const noexcept {
    return (id < m_nodes.size()) ? m_nodes[id].value : 0.0;
}

double AutodiffTape::Gradient(std::size_t id) const noexcept {
    return (id < m_nodes.size()) ? m_nodes[id].grad : 0.0;
}

// ============================================================================
// 3. NEURAL NETWORKS & MODEL INFERENCE IMPLEMENTATION
// ============================================================================

std::vector<double> ActivationFunctions::Apply(const std::vector<double>& input, ActivationKind kind) {
    std::vector<double> out(input.size(), 0.0);
    if (input.empty()) return out;

    if (kind == ActivationKind::Softmax) {
        const double maxVal = *std::max_element(input.begin(), input.end());
        double expSum = 0.0;
        for (std::size_t i = 0; i < input.size(); ++i) {
            out[i] = std::exp(input[i] - maxVal);
            expSum += out[i];
        }
        if (expSum > 1e-15) {
            for (double& v : out) {
                v /= expSum;
            }
        }
        return out;
    }

    for (std::size_t i = 0; i < input.size(); ++i) {
        switch (kind) {
            case ActivationKind::Linear:  out[i] = input[i]; break;
            case ActivationKind::ReLU:    out[i] = ReLU(input[i]); break;
            case ActivationKind::Sigmoid: out[i] = Sigmoid(input[i]); break;
            case ActivationKind::Tanh:    out[i] = Tanh(input[i]); break;
            case ActivationKind::Softmax: break;
        }
    }
    return out;
}

DenseLayer::DenseLayer(std::size_t inputDim,
                       std::size_t outputDim,
                       Matrix<double> weights,
                       std::vector<double> biases,
                       ActivationKind activation)
    : m_inputDim(inputDim),
      m_outputDim(outputDim),
      m_weights(std::move(weights)),
      m_biases(std::move(biases)),
      m_activation(activation) {}

std::optional<std::vector<double>> DenseLayer::Forward(
    const std::vector<double>& input,
    Frontend::DiagnosticBag& diagnostics,
    Frontend::SourceLocation loc) const {

    if (input.size() != m_inputDim ||
        m_weights.Rows() != m_outputDim ||
        m_weights.Cols() != m_inputDim ||
        m_biases.size() != m_outputDim) {
        diagnostics.ReportError(
            "AI9001",
            "Neural network DenseLayer dimension mismatch: expected input length " +
                std::to_string(m_inputDim) + ", got " + std::to_string(input.size()) + ".",
            loc,
            "Ensure input tensor dimensions match the layer's InputDim.");
        return std::nullopt;
    }

    std::vector<double> preActivation(m_outputDim, 0.0);
    for (std::size_t r = 0; r < m_outputDim; ++r) {
        double sum = m_biases[r];
        for (std::size_t c = 0; c < m_inputDim; ++c) {
            sum += m_weights.At(r, c) * input[c];
        }
        preActivation[r] = sum;
    }

    return ActivationFunctions::Apply(preActivation, m_activation);
}

void SequentialNeuralModel::AddLayer(DenseLayer layer) {
    m_layers.push_back(std::move(layer));
}

std::optional<std::vector<double>> SequentialNeuralModel::Predict(
    const std::vector<double>& input,
    Frontend::DiagnosticBag& diagnostics,
    Frontend::SourceLocation loc) const {

    if (m_layers.empty()) {
        diagnostics.ReportError(
            "AI9002",
            "Cannot run inference on empty SequentialNeuralModel '" + m_modelName + "'.",
            loc);
        return std::nullopt;
    }

    std::vector<double> activations = input;
    for (const auto& layer : m_layers) {
        auto nextOpt = layer.Forward(activations, diagnostics, loc);
        if (!nextOpt.has_value()) {
            return std::nullopt;
        }
        activations = std::move(*nextOpt);
    }
    return activations;
}

std::future<std::optional<std::vector<double>>> SequentialNeuralModel::PredictAsync(
    std::vector<double> input,
    Frontend::DiagnosticBag& diagnostics,
    Frontend::SourceLocation loc) const {

    return std::async(std::launch::async, [this, in = std::move(input), &diagnostics, loc]() {
        return this->Predict(in, diagnostics, loc);
    });
}

// ============================================================================
// 4. COMPUTER VISION & AUDIO/SIGNAL PROCESSING IMPLEMENTATION
// ============================================================================

GrayscaleImage2D::GrayscaleImage2D(std::size_t width, std::size_t height, std::uint8_t initialPixel)
    : m_width(width), m_height(height), m_pixels(width * height, initialPixel) {}

void GrayscaleImage2D::SetPixel(std::size_t x, std::size_t y, std::uint8_t val) noexcept {
    if (x < m_width && y < m_height) {
        m_pixels[y * m_width + x] = val;
    }
}

std::uint8_t GrayscaleImage2D::GetPixel(std::size_t x, std::size_t y) const noexcept {
    if (x < m_width && y < m_height) {
        return m_pixels[y * m_width + x];
    }
    return 0;
}

GrayscaleImage2D GrayscaleImage2D::ComputeSobelEdgeMagnitude() const {
    GrayscaleImage2D out(m_width, m_height, 0);
    if (m_width < 3 || m_height < 3) return out;

    for (std::size_t y = 1; y + 1 < m_height; ++y) {
        for (std::size_t x = 1; x + 1 < m_width; ++x) {
            const int p00 = GetPixel(x - 1, y - 1);
            const int p10 = GetPixel(x,     y - 1);
            const int p20 = GetPixel(x + 1, y - 1);
            const int p01 = GetPixel(x - 1, y);
            const int p21 = GetPixel(x + 1, y);
            const int p02 = GetPixel(x - 1, y + 1);
            const int p12 = GetPixel(x,     y + 1);
            const int p22 = GetPixel(x + 1, y + 1);

            const int gx = (-p00 + p20) + (-2 * p01 + 2 * p21) + (-p02 + p22);
            const int gy = (-p00 - 2 * p10 - p20) + (p02 + 2 * p12 + p22);

            const double mag = std::sqrt(static_cast<double>(gx * gx + gy * gy));
            const int clamped = static_cast<int>(std::clamp(mag, 0.0, 255.0));
            out.SetPixel(x, y, static_cast<std::uint8_t>(clamped));
        }
    }
    return out;
}

std::vector<ComplexSample> AudioSignalProcessor::ComputeDFT(const std::vector<double>& timeDomainSamples) {
    const std::size_t n = timeDomainSamples.size();
    std::vector<ComplexSample> spectrum(n);
    if (n == 0) return spectrum;

    const double twoPiOverN = (2.0 * MathModule::PI) / static_cast<double>(n);
    for (std::size_t k = 0; k < n; ++k) {
        double sumReal = 0.0;
        double sumImag = 0.0;
        for (std::size_t t = 0; t < n; ++t) {
            const double angle = twoPiOverN * static_cast<double>(k * t);
            sumReal += timeDomainSamples[t] * std::cos(angle);
            sumImag -= timeDomainSamples[t] * std::sin(angle);
        }
        spectrum[k] = ComplexSample{ sumReal, sumImag };
    }
    return spectrum;
}

double AudioSignalProcessor::ComputeRMS(const std::vector<double>& samples) noexcept {
    if (samples.empty()) return 0.0;
    double sqSum = 0.0;
    for (double s : samples) {
        sqSum += s * s;
    }
    return std::sqrt(sqSum / static_cast<double>(samples.size()));
}

} // namespace Robix::StdLib

