/* W184: isolate the actual numerical owner from the evaluator and transport.
   Compile with the same CORE-MATH sin objects and floating-point policy as
   the application. Observational timings; fingerprint equality is checked
   by the runner, never a speed threshold. */
#include "xpp_math.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
// Enough calls to amortize timer overhead without making a laptop run hot.
constexpr int Iterations = 2'000'000;
// One warm-up, then five samples to expose variance rather than best-case time.
constexpr int Samples = 5;
// FNV-1a mixes every result bit, including sign-zero, into a stable fingerprint.
constexpr std::uint64_t FingerprintOffset = 14695981039346656037ULL;
constexpr std::uint64_t FingerprintPrime = 1099511628211ULL;
double arithmetic(double x) { return (x * x - x) * 0.125 + 0.5; }
double sine(double x) { return xpp::math::sin(x); }
double fused(double x) { return std::fma(x, 0x1.0000000000001p0, -x); }
}

int main()
{
    // A rounding witness: separate multiplication/addition is not exact FMA.
    constexpr double WitnessA = 1.0 + 0x1p-27;
    constexpr double WitnessB = 1.0 - 0x1p-27;
    std::cout << "fma_witness fused=" << std::hexfloat << std::fma(WitnessA, WitnessB, -1.0)
              << " unfused=" << WitnessA * WitnessB - 1.0 << std::defaultfloat << '\n';
    for (const auto &[name, function] : std::array{
             std::pair{"arithmetic", arithmetic}, std::pair{"sin", sine}, std::pair{"fma", fused}}) {
        std::array<double, Samples> elapsed;
        std::uint64_t fingerprint = FingerprintOffset;
        for (int sample = -1; sample < Samples; ++sample) {
            fingerprint = FingerprintOffset;
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < Iterations; ++i) {
                // Binary-exact inputs in [0,1); the same sequence on every target.
                const double x = (i & 1023) * 0x1p-10;
                fingerprint = (fingerprint ^ std::bit_cast<std::uint64_t>(function(x))) * FingerprintPrime;
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (sample >= 0) elapsed[sample] = ms;
        }
        std::cout << name << " fingerprint=" << std::hex << fingerprint << std::dec << " samples_ms=";
        for (double ms : elapsed) std::cout << ms << ',';
        std::sort(elapsed.begin(), elapsed.end());
        std::cout << " median_ms=" << elapsed[Samples / 2] << '\n';
    }
}
