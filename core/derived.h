#ifndef _derived_h
#define _derived_h
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}
/* the derived quantities of the Session s: every one compiled (1 when
   one does not; the load, once), worked out again (after a parameter
   changed), one added (name = rhs) */
int compile_derived(xpp::Session &s);
void evaluate_derived(xpp::Session &s);
int add_derived(xpp::Session &s, const char *name, const char *rhs);
#endif
#endif
