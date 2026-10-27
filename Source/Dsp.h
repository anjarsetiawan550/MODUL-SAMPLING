#pragma once
#include <algorithm>
#include <cmath>
#include <complex>

namespace sc
{
    // Biquad RBJ (Butterworth Q) -- dipakai dua tingkat berurutan = 24 dB/oct (Linkwitz-Riley 4)
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void reset() noexcept { z1 = z2 = 0; }

        void setHighPass (double fs, double fc, double q = 0.70710678118)
        {
            fc = std::clamp (fc, 1.0, fs * 0.49);
            const double w0 = 6.283185307179586 * fc / fs, c = std::cos (w0), s = std::sin (w0);
            const double alpha = s / (2.0 * q), a0 = 1.0 + alpha;
            b0 = (1.0 + c) * 0.5 / a0;  b1 = -(1.0 + c) / a0;  b2 = b0;
            a1 = -2.0 * c / a0;         a2 = (1.0 - alpha) / a0;
        }

        void setLowPass (double fs, double fc, double q = 0.70710678118)
        {
            fc = std::clamp (fc, 1.0, fs * 0.49);
            const double w0 = 6.283185307179586 * fc / fs, c = std::cos (w0), s = std::sin (w0);
            const double alpha = s / (2.0 * q), a0 = 1.0 + alpha;
            b0 = (1.0 - c) * 0.5 / a0;  b1 = (1.0 - c) / a0;  b2 = b0;
            a1 = -2.0 * c / a0;         a2 = (1.0 - alpha) / a0;
        }

        inline float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }

        double magnitude (double fs, double f) const
        {
            const double w = 6.283185307179586 * f / fs;
            const std::complex<double> e1 = std::polar (1.0, -w), e2 = std::polar (1.0, -2.0 * w);
            const auto num = b0 + b1 * e1 + b2 * e2;
            const auto den = 1.0 + a1 * e1 + a2 * e2;
            return std::abs (num) / std::max (std::abs (den), 1e-12);
        }
    };

    // Magnitude respons 2 tingkat biquad identik (untuk kurva di LCD)
    inline double lr4Magnitude (bool highPass, double fs, double fc, double f)
    {
        Biquad b;
        if (highPass) b.setHighPass (fs, fc); else b.setLowPass (fs, fc);
        const double m = b.magnitude (fs, f);
        return m * m;
    }
}
