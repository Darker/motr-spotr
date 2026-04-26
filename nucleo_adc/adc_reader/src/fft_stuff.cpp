#include "fft_stuff.h"
#include <cmath>
#include <complex>
#include <vector>

constexpr double MATH_PI = 3.14159265358979323846;

// -----------------------------
// Radix-2 FFT (in-place)
// -----------------------------
void fft(std::vector<std::complex<double>> &a)
{
    const size_t n = a.size();
    if (n <= 1)
        return;

    size_t j = 0;
    for (size_t i = 1; i < n; i++)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * MATH_PI / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));

        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; k++)
            {
                auto u = a[i + k];
                auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// -----------------------------
// High-quality spectrum extractor
// -----------------------------
std::vector<double> computeSpectrum(const std::vector<double> &samples,
                                    double fs, double fmin, double fmax,
                                    int Nout)
{
    // 1. Determine FFT size (next power of two)
    size_t N = samples.size();
    size_t Nfft = 1;
    while (Nfft < N)
        Nfft <<= 1;

    // 2. Copy samples into complex buffer (zero-padded)
    std::vector<std::complex<double>> buf(Nfft);
    for (size_t i = 0; i < N; i++)
        buf[i] = std::complex<double>(samples[i], 0.0);

    // 3. Apply a Hann window (quality!)
    for (size_t i = 0; i < N; i++)
    {
        double w = 0.5 * (1.0 - std::cos(2.0 * MATH_PI * i / (N - 1)));
        buf[i] *= w;
    }

    // 4. FFT
    fft(buf);

    // 5. Magnitude spectrum (only positive freqs)
    size_t Nh = Nfft / 2;
    std::vector<double> mag(Nh);
    for (size_t i = 0; i < Nh; i++)
        mag[i] = std::abs(buf[i]);

    // 6. Output bucket mapping
    std::vector<double> out(Nout, 0.0);

    double df = fs / Nfft; // FFT bin spacing

    for (int i = 0; i < Nout; i++)
    {
        double f = fmin + (fmax - fmin) * (double(i) / (Nout - 1));
        double bin = f / df;

        size_t i0 = std::floor(bin);
        size_t i1 = std::min(i0 + 1, Nh - 1);
        double t = bin - i0;
        if (bin >= Nh - 1)
        {
            out[i] = mag[i1];
            continue;
        }

        out[i] = (1.0 - t) * mag[i0] + t * mag[i1];
    }

    return out;
}

void generateMockSignal(std::vector<double> &out, double fs,
                        const std::vector<FakeTone> tones)
{
    const size_t N = out.size();

    // Example tones
    // struct Tone
    // {
    //     double freq;
    //     double amp;
    // };
    // std::vector<Tone> tones = {
    //     {440.0, 0.8},  // A4
    //     {1200.0, 0.4}, // mid tone
    //     {5000.0, 0.2}, // high tone
    //     {9000.0, 0.1}  // very high tone
    // };

    for (size_t i = 0; i < N; i++)
    {
        double t = double(i) / fs;
        double v = 0.0;

        for (auto &tone : tones)
        {
            v += tone.amp * std::sin(2.0 * MATH_PI * tone.freq * t) + (std::sin(2.0 * MATH_PI * tone.fuzzFreq * t)*0.03);
        }
        out[i] = v;
    }
}
