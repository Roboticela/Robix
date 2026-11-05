#include <iostream>
#include <cstdint>

class MandelbrotRenderer {
public:
    static int64_t ComputePoint(double cr, double ci, int64_t maxIter) {
        double zr = 0.0;
        double zi = 0.0;
        for (int64_t iter = 0; iter < maxIter; ++iter) {
            double zr2 = zr * zr;
            double zi2 = zi * zi;
            if (zr2 + zi2 > 4.0) {
                return iter;
            }
            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
        }
        return maxIter;
    }
};

int main() {
    int64_t width = 500;
    int64_t height = 500;
    int64_t totalEscaped = 0;

    double rMin = -2.0;
    double rMax = 1.0;
    double iMin = -1.5;
    double iMax = 1.5;

    double rStep = (rMax - rMin) / 500.0;
    double iStep = (iMax - iMin) / 500.0;

    double ci = iMin;
    for (int64_t y = 0; y < height; ++y) {
        double cr = rMin;
        for (int64_t x = 0; x < width; ++x) {
            int64_t iters = MandelbrotRenderer::ComputePoint(cr, ci, 100);
            if (iters < 100) {
                ++totalEscaped;
            }
            cr += rStep;
        }
        ci += iStep;
    }

    std::cout << "Mandelbrot Escaped: " << totalEscaped << "\n";
    return 0;
}

