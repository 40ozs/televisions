#pragma once

// Minimal TAP-style test harness (ADR-012). Zero dependencies beyond the
// standard library; DSP measurement helpers included (Goertzel, RMS, CSV).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace vfatest
{

struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn)
    {
        registry().push_back ({ name, std::move (fn) });
    }
};

struct Failure
{
    std::string message;
};

inline int& failureCount() { static int c = 0; return c; }
inline int& checkCount()   { static int c = 0; return c; }
inline std::vector<std::string>& currentFailures()
{
    static std::vector<std::string> f;
    return f;
}

inline void reportCheck (bool ok, const char* expr, const char* file, int line,
                         const std::string& detail = {})
{
    ++checkCount();
    if (! ok)
    {
        ++failureCount();
        char buf[512];
        std::snprintf (buf, sizeof (buf), "  CHECK failed: %s (%s:%d) %s",
                       expr, file, line, detail.c_str());
        currentFailures().push_back (buf);
    }
}

#define VFA_TEST(NAME) \
    static void vfaTestFn_##NAME(); \
    static const ::vfatest::Registrar vfaTestReg_##NAME (#NAME, &vfaTestFn_##NAME); \
    static void vfaTestFn_##NAME()

#define CHECK(expr) ::vfatest::reportCheck ((expr), #expr, __FILE__, __LINE__)
#define CHECK_MSG(expr, msg) ::vfatest::reportCheck ((expr), #expr, __FILE__, __LINE__, (msg))
#define CHECK_NEAR(a, b, tol) \
    do { const double va_ = (a), vb_ = (b), vt_ = (tol); \
         char nb_[160]; std::snprintf (nb_, sizeof (nb_), "[%g vs %g, tol %g]", va_, vb_, vt_); \
         ::vfatest::reportCheck (std::abs (va_ - vb_) <= vt_, #a " ~= " #b, __FILE__, __LINE__, nb_); } while (0)
#define REQUIRE(expr) \
    do { const bool ok_ = (expr); \
         ::vfatest::reportCheck (ok_, #expr, __FILE__, __LINE__); \
         if (! ok_) throw ::vfatest::Failure { #expr }; } while (0)

// ------------------------------------------------------------ measurement
// Goertzel magnitude of a single frequency; returns linear magnitude of the
// sinusoidal component (amplitude, not power).
inline double goertzelAmplitude (const float* x, int n, double hz, double sampleRate)
{
    const double w = 2.0 * M_PI * hz / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i)
    {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double real = s1 - s2 * std::cos (w);
    const double imag = s2 * std::sin (w);
    return 2.0 * std::sqrt (real * real + imag * imag) / n;
}

inline double amplitudeToDb (double a) { return 20.0 * std::log10 (std::max (a, 1.0e-12)); }

inline double rms (const float* x, int n)
{
    double acc = 0;
    for (int i = 0; i < n; ++i) acc += (double) x[i] * x[i];
    return std::sqrt (acc / std::max (n, 1));
}

inline double rmsDb (const float* x, int n) { return amplitudeToDb (rms (x, n)); }

inline bool hasNanOrInf (const float* x, int n)
{
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (x[i])) return true;
    return false;
}

// THD from Goertzel bins: harmonics 2..maxH relative to fundamental.
inline double thdPercent (const float* x, int n, double f0, double sampleRate, int maxH = 8)
{
    const double fund = goertzelAmplitude (x, n, f0, sampleRate);
    double h = 0;
    for (int k = 2; k <= maxH; ++k)
    {
        const double fk = f0 * k;
        if (fk < sampleRate * 0.48)
        {
            const double a = goertzelAmplitude (x, n, fk, sampleRate);
            h += a * a;
        }
    }
    return fund > 1.0e-12 ? 100.0 * std::sqrt (h) / fund : 0.0;
}

// ------------------------------------------------------------------ output
inline std::filesystem::path testOutputDir()
{
    auto p = std::filesystem::path (VFA_REPO_ROOT) / "TestOutput";
    std::filesystem::create_directories (p);
    return p;
}

inline void writeCsv (const std::string& filename,
                      const std::vector<std::string>& header,
                      const std::vector<std::vector<double>>& rows)
{
    std::ofstream out (testOutputDir() / filename);
    for (size_t i = 0; i < header.size(); ++i)
        out << header[i] << (i + 1 < header.size() ? "," : "\n");
    for (const auto& row : rows)
        for (size_t i = 0; i < row.size(); ++i)
            out << row[i] << (i + 1 < row.size() ? "," : "\n");
}

// -------------------------------------------------------------------- main
inline int runAll (int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int index = 0, failures = 0, ran = 0;
    std::printf ("TAP version 13\n");
    std::vector<const TestCase*> selected;
    for (const auto& t : registry())
        if (filter == nullptr || std::strstr (t.name, filter) != nullptr)
            selected.push_back (&t);
    std::printf ("1..%d\n", (int) selected.size());
    for (const auto* t : selected)
    {
        ++index; ++ran;
        currentFailures().clear();
        const int before = failureCount();
        try
        {
            t->fn();
        }
        catch (const Failure&)      { /* counted by REQUIRE */ }
        catch (const std::exception& e)
        {
            ++failureCount();
            currentFailures().push_back (std::string ("  unhandled exception: ") + e.what());
        }
        const bool ok = failureCount() == before;
        std::printf ("%s %d - %s\n", ok ? "ok" : "not ok", index, t->name);
        if (! ok)
        {
            ++failures;
            for (const auto& f : currentFailures())
                std::printf ("# %s\n", f.c_str());
        }
        std::fflush (stdout);
    }
    std::printf ("# %d test(s), %d check(s), %d failing test(s)\n",
                 ran, checkCount(), failures);
    return failures == 0 ? 0 : 1;
}

} // namespace vfatest
