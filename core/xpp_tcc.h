#ifndef XPP_TCC_H
#define XPP_TCC_H

#include "xpp_error.h"
#include <memory>
#include <span>
#include <string>

namespace xpp::tcc {

struct Symbol {
    const char *name;
    const void *address;
};

/* Only trusted, generated C belongs here: libtcc is not a sandbox. External
   references must be supplied as symbols. The returned addresses live until
   the Program is destroyed; callers cast them to the generated signature.
   W254 tests this owner; W255 connects it to model compilation. */
class Program {
public:
    static Result<std::unique_ptr<Program>> compile(const std::string &source,
                                                   std::span<const Symbol> symbols,
                                                   Place place);
    ~Program();
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;
    Result<void *> function(const std::string &name) const;

private:
    struct State;
    explicit Program(Place place);
    std::unique_ptr<State> state_;
};

} // namespace xpp::tcc
#endif
