#ifndef _derived_h
#define _derived_h

#include <string_view>

namespace xpp {

struct Session; /* session.h */

/* the derived quantities of the Session s: every one compiled (1 when
   one does not; the load, once), worked out again (after a parameter
   changed), one added (name = rhs) */
int compile_derived(Session &s);
void evaluate_derived(Session &s);
int add_derived(Session &s, std::string_view name, std::string_view rhs);

} // namespace xpp
#endif
