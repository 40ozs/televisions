#include "../Harness/VfaTest.h"
#include "DSP/Core/Rng.h"

using vfa::dsp::Rng;

VFA_TEST (Rng_determinism_same_seed_bit_exact)
{
    Rng a (12345), b (12345);
    for (int i = 0; i < 10000; ++i)
        CHECK (a.nextU64() == b.nextU64());
}

VFA_TEST (Rng_different_seeds_diverge)
{
    Rng a (1), b (2);
    int same = 0;
    for (int i = 0; i < 1000; ++i)
        if (a.nextU64() == b.nextU64()) ++same;
    CHECK (same == 0);
}

VFA_TEST (Rng_stream_derivation_independent)
{
    const auto s1 = Rng::streamSeed (42, 1, 0, 0);
    const auto s2 = Rng::streamSeed (42, 1, 1, 0);
    const auto s3 = Rng::streamSeed (42, 2, 0, 0);
    CHECK (s1 != s2);
    CHECK (s1 != s3);
    CHECK (s2 != s3);
    CHECK (Rng::streamSeed (42, 1, 0, 0) == s1); // stable
}

VFA_TEST (Rng_uniform_range_and_moments)
{
    Rng r (7);
    double sum = 0, sumSq = 0;
    constexpr int n = 200000;
    for (int i = 0; i < n; ++i)
    {
        const float v = r.next01();
        CHECK_MSG (v >= 0.0f && v < 1.0f, "uniform out of range");
        sum += v; sumSq += v * v;
    }
    const double mean = sum / n;
    const double var = sumSq / n - mean * mean;
    CHECK_NEAR (mean, 0.5, 0.01);
    CHECK_NEAR (var, 1.0 / 12.0, 0.005);
}

VFA_TEST (Rng_gauss_bounded_and_centred)
{
    Rng r (99);
    double sum = 0;
    for (int i = 0; i < 100000; ++i)
    {
        const float g = r.nextGauss();
        CHECK_MSG (std::abs (g) <= 3.47f, "gauss out of bound");
        sum += g;
    }
    CHECK_NEAR (sum / 100000.0, 0.0, 0.02);
}
