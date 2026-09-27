/* The current xpp::Model and a load's swap (model.h). */
#include "model.h"

namespace xpp {

ModelLoad::ModelLoad()
  : previous(detail::current_slot<Model>())
{
  detail::current_slot<Model>()=new Model();
}

ModelLoad::~ModelLoad()
{
  if(committed)return;
  delete detail::current_slot<Model>();
  detail::current_slot<Model>()=previous;
}

void ModelLoad::commit()
{
  committed=true;
  delete previous;
  previous=nullptr;
}

}
