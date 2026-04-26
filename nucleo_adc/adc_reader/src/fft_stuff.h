#pragma once
#include <vector>

namespace hertz {

    // Base type for frequency values
    struct Hz {
        double value;
        constexpr Hz() : value(0.0) {}
        constexpr Hz(double hzValue) : value(hzValue) {}
        constexpr operator double() const { return value; }
    };

    // --- User-defined literals ---
    constexpr Hz operator""_Hz(long double v) {
        return Hz{ static_cast<double>(v) };
    }

    constexpr Hz operator""_kHz(long double v) {
        return Hz{ static_cast<double>(v) * 1'000.0 };
    }

    constexpr Hz operator""_MHz(long double v) {
        return Hz{ static_cast<double>(v) * 1'000'000.0 };
    }

    // Integer overloads
    constexpr Hz operator""_Hz(unsigned long long v) {
        return Hz{ static_cast<double>(v) };
    }

    constexpr Hz operator""_kHz(unsigned long long v) {
        return Hz{ static_cast<double>(v) * 1'000.0 };
    }

    constexpr Hz operator""_MHz(unsigned long long v) {
        return Hz{ static_cast<double>(v) * 1'000'000.0 };
    }

} // namespace hertz
struct FakeTone
{
    hertz::Hz freq;
    double amp;
    hertz::Hz fuzzFreq{0};
};

std::vector<double> computeSpectrum(const std::vector<double> &samples,
                                    double samplingFreq, double fmin,
                                    double fmax, int Nout);

void generateMockSignal(std::vector<double> &out, double fs, const std::vector<FakeTone> tones);



