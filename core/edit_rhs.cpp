#include "model.h"
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
 int n=xpp::model().neq;
 if(xpp::model().neq>NEQMAXFOREDIT) return;
 EditBox box;
 for(int i=0;i<n;i++){
   std::string name;
   if(i>=xpp::model().node)name=xpp::model().uvar_names[i];
   else if(xpp::model().eq_type[i]==1)name=xpp::format("{}(T)",xpp::model().uvar_names[i]);
   else if(METHOD==0)name=xpp::format("{}(n+1)",xpp::model().uvar_names[i]);
   else name=xpp::format("d{}/dT",xpp::model().uvar_names[i]);
   box.add(std::move(name),xpp::model().formulas[i].c_str());
 }
 if(box.show("Right Hand Sides")==0)return;
 for(int i=0;i<n;i++){
   if(i<xpp::model().node||(i>=(xpp::model().node+xpp::model().nmarkov))){
     Command command;
     int len;
     if(add_expr(box.value(i),command.data(),&len)==1)
       err_msg(xpp::format("Bad rhs:{}={}",box.name(i),box.value(i)).c_str());
     else {
       set_ode_name(i,box.value(i));
       int i0=i;
       if(i>=xpp::model().node)i0=i0+xpp::model().fix_var-xpp::model().nmarkov;
       for(int j=0;j<len;j++)
         xpp::model().programs[i0][j]=command[j];
     }
   }
 }
}

void edit_functions()
{
 int n=xpp::model().nfun;
 if(n==0||n>NEQMAXFOREDIT)return;
 EditBox box;
 for(int i=0;i<n;i++){
   std::string name;
   if(xpp::model().narg_fun[i]==0)
     name=xpp::format("{}()",xpp::model().ufun_names[i]);
   else if(xpp::model().narg_fun[i]==1)
     name=xpp::format("{}({})",xpp::model().ufun_names[i],xpp::model().ufun_args[i][0]);
   else
     name=xpp::format("{}({},...,{})",xpp::model().ufun_names[i],xpp::model().ufun_args[i][0],
                      xpp::model().ufun_args[i][xpp::model().narg_fun[i]-1]);
   box.add(std::move(name),xpp::model().ufun_defs[i].c_str());
 }
 if(box.show("Functions")==0)return;
 for(int i=0;i<n;i++){
   Command command;
   int len;
   set_ufun_arg_names(i);
   int err=add_expr(box.value(i),command.data(),&len);
   set_old_arg_names(xpp::model().narg_fun[i]);
   if(err==1)
     err_msg(xpp::format("Bad func.:{}={}",box.name(i),box.value(i)).c_str());
   else {
     set_ufun_def(i,box.value(i));
     for(int j=0;j<=len;j++)
       xpp::model().ufun_programs[i][j]=command[j];
     fixup_endfun(xpp::model().ufun_programs[i].data(),len,xpp::model().narg_fun[i]);
   }
 }
}

int save_as()
{
  std::string filename=xpp::model().this_file;
  ping();
  if(!file_selector("Save As",filename,"*.ode"))return(-1);
  xpp::Writer w=open_writer_asking(filename.c_str());
  if(!w)return(-1);
  double z;
  w.print("{}",xpp::model().neq);
  for(int i=0;i<xpp::model().node;i++){
    if(i%5==0)w.print("\nvariable ");
    w.print(" {}={:.16g} ",xpp::model().uvar_names[i],last_ic[i]);
  }
  w.print("\n");
  for(int i=xpp::model().node;i<xpp::model().neq;i++){
    if((i-xpp::model().node)%5==0)w.print("\naux ");
    w.print(" {} ",xpp::model().uvar_names[i]);
  }
  w.print("\n");
  for(int i=0;i<xpp::model().nupar;i++){
    if(i%5==0)w.print("\nparam  ");
    get_val(xpp::model().upar_names[i],&z);
    w.print(" {}={:.16g}   ",xpp::model().upar_names[i],z);
  }
  w.print("\n");
  for(int i=0;i<xpp::model().nfun;i++)
    w.print("user {} {} {}\n",xpp::model().ufun_names[i],xpp::model().narg_fun[i],xpp::model().ufun_defs[i]);
  for(int i=0;i<xpp::model().node;i++)
    w.print("{} {}\n",xpp::model().eq_type[i]==1?"i":"o",xpp::model().formulas[i]);
  for(int i=xpp::model().node;i<xpp::model().neq;i++)
    w.print("o {}\n",xpp::model().formulas[i]);
  for(int i=0;i<xpp::model().node;i++)w.print("b {} \n",xpp::model().bcs[i].string.data());
  w.print("done\n");
  return w.commit()?1:0;
}
