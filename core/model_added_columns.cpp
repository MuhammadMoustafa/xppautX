#include "model_added_columns.h"

#include <cstddef>

#include "model.h"
#include "typed_values.h"
#include "xpp_text.h"

namespace xpp {

int data_column(const Model &m, std::span<const AddedColumn> added, std::string_view name)
{
  if(equal_ignoring_case("T",name))return 0;
  const int variable=find_user_name(m,2,name);
  if(variable>-1)return variable+1;
  for(std::size_t k=0;k<added.size();k++)
    if(equal_ignoring_case(added[k].name,name))return m.neq+1+static_cast<int>(k);
  return -1;
}

std::string data_column_name(const Model &m, std::span<const AddedColumn> added, int j)
{
  if(j==0)return "T";
  if(j>0&&j<=m.neq)return m.uvar_names[static_cast<std::size_t>(j-1)];
  if(j>m.neq){
    const std::size_t k=static_cast<std::size_t>(j-m.neq-1);
    if(k<added.size())return added[k].name;
  }
  return "";
}

} // namespace xpp
