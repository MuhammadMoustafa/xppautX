#pragma once
/* the adapter's private state (xpp_mir.cpp), what its front ends reach: MIR's own header and Program::State */
#include "xpp_mir.h"
extern "C" {
#include "mir.h"
#include "mir-gen.h"
}

namespace xpp::mir {

struct Program::State {
    MIR_context_t context = nullptr;
    MIR_module_t module = nullptr;
    bool generator = false;
    void (*finish_front)(MIR_context_t) = nullptr; /* a front end's own cleanup (c2mir_finish) */
    std::string message;
    Place place;
    double front_ms = 0, generate_ms = 0;

    ~State();
    std::unexpected<Error> failure(const std::string &operation) const;
};

} // namespace xpp::mir
