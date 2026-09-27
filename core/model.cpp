/* The one xpp::Model (model.h). */
#include "model.h"

namespace xpp {

Model &model()
{
  /* kept for the program's life and never destroyed, so nothing that runs
     at exit (an atexit handler, a static's destructor) reads it after it
     is gone; LeakSanitizer sees it as reachable */
  static Model *const m = new Model();
  return *m;
}

}
