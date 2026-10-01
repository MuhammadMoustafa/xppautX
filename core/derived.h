#ifndef _derived_h
#define _derived_h

#include <string_view>

namespace xpp {

struct Session; /* session.h */

/* the derived quantities of the Session s: every one compiled (the load,
   once: one that does not fails it at its line), worked out again (after a
   parameter changed), one added (name = rhs) */
void compile_derived(Session &s);
void evaluate_derived(Session &s);
int add_derived(Session &s, std::string_view name, std::string_view rhs);

} // namespace xpp
#endif
