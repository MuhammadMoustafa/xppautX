#pragma once

#include "../../core/xpp_error.h"
#include <memory>
#include <span>
#include <string>

/* W706 spike: the adapter to third_party/mir (MIR master dc2c763), the same shape as
   core/xpp_tcc.h so a later card can put one compile entry in front of either.
   It is not in the default build (Makefile target `mirspike`). */
namespace xpp::mir {

struct Symbol {
    const char *name;
    const void *address;
};

/* Only trusted, generated C belongs here: MIR is not a sandbox. The text is
   compiled with no preprocessor (no #include reaches the file system) and
   every external reference must be a supplied symbol. The returned addresses
   live until the Program is destroyed. */
class Program {
public:
    /* level: MIR's optimization level, 0 to 3 */
    static Result<std::unique_ptr<Program>> compile(const std::string &source,
                                                   std::span<const Symbol> symbols,
                                                   Place place, unsigned level);
    ~Program();
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;
    /* a function's code or a data item's storage */
    Result<void *> function(const std::string &name) const;

    struct State; /* public only for the file-local calls into MIR */

private:
    explicit Program(Place place);
    std::unique_ptr<State> state_;
};

} // namespace xpp::mir
