/* The current xpp::Model and a load's swap (model.h). */
#include "model.h"

namespace xpp {

namespace detail {

Model *first_model()
{
  /* the Models are kept until a load replaces them and the last one for
     the program's life, never destroyed, so nothing that runs at exit (an
     atexit handler, a static's destructor) reads one after it is gone;
     LeakSanitizer sees it as reachable */
  Model *&current=current_model();
  if(!current)current=new Model();
  return current;
}

}

ModelLoad::ModelLoad()
  : previous(detail::current_model())
{
  detail::current_model()=new Model();
}

ModelLoad::~ModelLoad()
{
  if(committed)return;
  delete detail::current_model();
  detail::current_model()=previous;
}

void ModelLoad::commit()
{
  committed=true;
  delete previous;
  previous=nullptr;
}

}
