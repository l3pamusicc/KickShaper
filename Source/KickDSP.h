#pragma once
#include <cmath>
#include <algorithm>

// Yhteinen matematiikka: kick-kayrat, saturaatio ja suotimet.
// Ei riipu JUCEsta, joten sita kayttaa sekä aanenkasittely etta kayttoliittyma.
namespace Kick
{
    constexpr double twoPi = 6.283185307179586;
    constexpr double kPi   = 3.141592653589793;

    // ------------------------------------------------------------ Kick-muoto
    struct Shape
    {
        double pStart = 1800.0, pMid = 180.0, pEnd = 48.0, pMidT = 0.03;   // pitch-kayra
        double aMidT = 0.35, aMidL = 0.55, aCurve = 1.6;                    // amp-kayra

        // f = 0..1 (kohta kick-aanen kokonaispituudesta)
        double freqAt (double f) const
        {
            f = std::min (1.0, std::max (0.0, f));
            const double l0 = std::log2 (pStart), l1 = std::log2 (pMid), l2 = std::log2 (pEnd);
            double lf;
            if (f < pMidT) lf = l0 + (l1 - l0) * (f / pMidT);
            else           lf = l1 + (l2 - l1) * ((f - pMidT) / (1.0 - pMidT));
            return std::pow (2.0, lf);
        }

        double ampAt (double f) const
        {
            f = std::min (1.0, std::max (0.0, f));
            if (f < aMidT) return 1.0 + (aMidL - 1.0) * (f / aMidT);
            const double u = (f - aMidT) / (1.0 - aMidT);
            return aMidL * std::pow (1.0 - u, aCurve);
        }
    };

    // ------------------------------------------------------------ Saturaatio (ADAA)
    inline double logCosh (double x)
    {
        const double ax = std::fabs (x);
        return ax + std::log1p (std::exp (-2.0 * ax)) - 0.6931471805599453;
    }

    inline double shaper (double x, int type)
    {
        switch (type)
        {
            case 0:  return std::tanh (x);
            case 1:  return std::tanh (x + 0.35) - 0.3364;
            case 2:  return std::max (-1.0, std::min (1.0, x));
            default: return std::sin (x);
        }
    }

    // Shaperin integraalifunktio (anti-aliasointia varten)
    inline double shaperAD (double x, int type)
    {
        switch (type)
        {
            case 0:  return logCosh (x);
            case 1:  return logCosh (x + 0.35) - 0.3364 * x;
            case 2:  { const double ax = std::fabs (x); return ax <= 1.0 ? 0.5 * x * x : ax - 0.5; }
            default: return -std::cos (x);
        }
    }

    // ------------------------------------------------------------ Biquad-suotimet (RBJ)
    struct BQ      { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };
    struct BQState { double z1 = 0.0, z2 = 0.0; };

    inline double bqProcess (const BQ& c, BQState& s, double x)
    {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    inline BQ makeBQ (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BQ r;
        r.b0 = b0 / a0; r.b1 = b1 / a0; r.b2 = b2 / a0;
        r.a1 = a1 / a0; r.a2 = a2 / a0;
        return r;
    }

    inline BQ lowShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * f / fs, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s * 0.5 * std::sqrt (2.0);
        const double t = 2.0 * std::sqrt (A) * alpha;
        return makeBQ (A * ((A + 1.0) - (A - 1.0) * c + t),
                       2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                       A * ((A + 1.0) - (A - 1.0) * c - t),
                       (A + 1.0) + (A - 1.0) * c + t,
                       -2.0 * ((A - 1.0) + (A + 1.0) * c),
                       (A + 1.0) + (A - 1.0) * c - t);
    }

    inline BQ highShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * f / fs, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s * 0.5 * std::sqrt (2.0);
        const double t = 2.0 * std::sqrt (A) * alpha;
        return makeBQ (A * ((A + 1.0) + (A - 1.0) * c + t),
                       -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                       A * ((A + 1.0) + (A - 1.0) * c - t),
                       (A + 1.0) - (A - 1.0) * c + t,
                       2.0 * ((A - 1.0) - (A + 1.0) * c),
                       (A + 1.0) - (A - 1.0) * c - t);
    }

    inline BQ peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * f / fs, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s / (2.0 * q);
        return makeBQ (1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A,
                       1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
    }

    inline BQ bandpass (double fs, double f, double q)
    {
        const double w0 = twoPi * f / fs, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s / (2.0 * q);
        return makeBQ (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

    inline BQ highpass (double fs, double f, double q)
    {
        const double w0 = twoPi * f / fs, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s / (2.0 * q);
        return makeBQ ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }
}
