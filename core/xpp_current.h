#ifndef XPP_CURRENT_H
#define XPP_CURRENT_H
/* The current one of a kind of object the core has one of at a time: the
   Model (model.h) and the Session (session.h). C++ only.

   Until W47d passes them explicitly, xpp::model() and xpp::session() read
   them through here: a slot that is constant-initialised (no guard on a
   read) and filled on first use, changed only by a load. */

namespace xpp::detail {

template <class T>
inline T *&current_slot() noexcept
{
  static constinit T *current=nullptr;
  return current;
}

/* the first one, made on first use. The objects are kept until a load
   replaces them and the last one for the program's life, never destroyed,
   so nothing that runs at exit (an atexit handler, a static's destructor)
   reads one after it is gone; LeakSanitizer sees it as reachable. */
template <class T>
[[gnu::noinline]] T *first_current()
{
  T *&current=current_slot<T>();
  if(!current)current=new T();
  return current;
}

/* Inline: the integrator's right-hand side reads the current Model and
   Session on every step, and a call per read slowed it measurably. */
template <class T>
inline T &current()
{
  T *p=current_slot<T>();
  if(!p)[[unlikely]]p=first_current<T>();
  return *p;
}

}

#endif
