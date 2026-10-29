// Explicit stage-2 instrumentation only: no clock is read without the env key.
#pragma once
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>

template<class F> int factor_event(const char *kind, F call) noexcept {
    try {
    const char *path = std::getenv("W378_FACTOR_LOG");
    if (!path) return call();
    const auto wall = std::chrono::steady_clock::now();
    const auto cpu = std::clock();
    const int flag = call();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count();
    const double cpu_seconds = static_cast<double>(std::clock()-cpu)/CLOCKS_PER_SEC;
    std::ofstream out(path, std::ios::app);
    if (!out) throw std::runtime_error("cannot append factor log");
    out << std::setprecision(17) << "{\"kind\":\"" << kind << "\",\"wall\":" << seconds
        << ",\"cpu\":" << cpu_seconds << ",\"flag\":" << flag << "}\n";
    return flag;
    } catch(...) {return -1;} // no exception crosses a SUNDIALS C callback
}
