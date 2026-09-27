#include "edit_rhs.h"
#include "xpp_ui.h"
#include "parserslow.h"
#include "browse.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "load_eqn.h"
#include "form_ode.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

/* do_edit_box's fields: the names it shows and the values it edits in
   place (at most MAX_LEN_EBOX-1 characters each, what the front ends
   write at most) */
class EditBox {
public:
  void add(std::string name, const char *value)
  {
    names_.push_back(std::move(name));
    values_.emplace_back(std::string_view(value ? value : "").substr(0, MAX_LEN_EBOX - 1));
  }
  /* 0 on cancel */
  int show(const char *title)
  {
    std::vector<const char *> names;
    for (const std::string &n : names_) names.push_back(n.c_str());
    return do_edit_box(title, names.data(), values_);
  }
  const std::string &name(int i) const { return names_[i]; }
  const char *value(int i) const { return values_[i].c_str(); }
private:
  std::vector<std::string> names_;
  std::vector<std::string> values_;
};

/* the command add_expr compiles an expression into */
using Command = std::array<int, 200>;

} // namespace

void edit_rhs()
{
 int n=NEQ;
 if(NEQ>NEQMAXFOREDIT) return;
 EditBox box;
 for(int i=0;i<n;i++){
   std::string name;
   if(i>=NODE)name=uvar_names[i];
   else if(EqType[i]==1)name=xpp::format("{}(T)",uvar_names[i]);
   else if(METHOD==0)name=xpp::format("{}(n+1)",uvar_names[i]);
   else name=xpp::format("d{}/dT",uvar_names[i]);
   box.add(std::move(name),ode_names[i]);
 }
 if(box.show("Right Hand Sides")==0)return;
 for(int i=0;i<n;i++){
   if(i<NODE||(i>=(NODE+NMarkov))){
     Command command;
     int len;
     if(add_expr(box.value(i),command.data(),&len)==1)
       err_msg(xpp::format("Bad rhs:{}={}",box.name(i),box.value(i)).c_str());
     else {
       set_ode_name(i,box.value(i));
       int i0=i;
       if(i>=NODE)i0=i0+FIX_VAR-NMarkov;
       for(int j=0;j<len;j++)
         my_ode[i0][j]=command[j];
     }
   }
 }
}

void edit_functions()
{
 int n=NFUN;
 if(n==0||n>NEQMAXFOREDIT)return;
 EditBox box;
 for(int i=0;i<n;i++){
   std::string name;
   if(narg_fun[i]==0)
     name=xpp::format("{}()",ufun_names[i]);
   else if(narg_fun[i]==1)
     name=xpp::format("{}({})",ufun_names[i],ufun_arg[i].args[0]);
   else
     name=xpp::format("{}({},...,{})",ufun_names[i],ufun_arg[i].args[0],
                      ufun_arg[i].args[narg_fun[i]-1]);
   box.add(std::move(name),ufun_def[i]);
 }
 if(box.show("Functions")==0)return;
 for(int i=0;i<n;i++){
   Command command;
   int len;
   set_ufun_arg_names(i);
   int err=add_expr(box.value(i),command.data(),&len);
   set_old_arg_names(narg_fun[i]);
   if(err==1)
     err_msg(xpp::format("Bad func.:{}={}",box.name(i),box.value(i)).c_str());
   else {
     set_ufun_def(i,box.value(i));
     for(int j=0;j<=len;j++)
       ufun[i][j]=command[j];
     fixup_endfun(ufun[i].data(),len,narg_fun[i]);
   }
 }
}

int save_as()
{
  std::string filename=this_file;
  ping();
  if(!file_selector("Save As",filename,"*.ode"))return(-1);
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return(-1);
  double z;
  w.print("{}",NEQ);
  for(int i=0;i<NODE;i++){
    if(i%5==0)w.print("\nvariable ");
    w.print(" {}={:.16g} ",uvar_names[i],last_ic[i]);
  }
  w.print("\n");
  for(int i=NODE;i<NEQ;i++){
    if((i-NODE)%5==0)w.print("\naux ");
    w.print(" {} ",uvar_names[i]);
  }
  w.print("\n");
  for(int i=0;i<NUPAR;i++){
    if(i%5==0)w.print("\nparam  ");
    get_val(upar_names[i],&z);
    w.print(" {}={:.16g}   ",upar_names[i],z);
  }
  w.print("\n");
  for(int i=0;i<NFUN;i++)
    w.print("user {} {} {}\n",ufun_names[i],narg_fun[i],ufun_def[i]);
  for(int i=0;i<NODE;i++)
    w.print("{} {}\n",EqType[i]==1?"i":"o",ode_names[i]);
  for(int i=NODE;i<NEQ;i++)
    w.print("o {}\n",ode_names[i]);
  for(int i=0;i<NODE;i++)w.print("b {} \n",my_bc[i].string);
  w.print("done\n");
  return w.commit()?1:0;
}
