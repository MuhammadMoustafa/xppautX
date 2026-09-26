#include "edit_rhs.h"
#include "xpp_mem.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "extra.h"
#include "parserslow.h"
#include "browse.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "xpplim.h"
#include "struct.h"
#include "shoot.h"
#include "load_eqn.h"
#include "xpp_io.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

extern char uvar_names[MAXODE][XPP_NAME_MAX+1];
extern char *ode_names[MAXODE];
extern int METHOD,NEQ,NODE,NMarkov,FIX_VAR;

extern int *my_ode[];
extern int NUPAR;
extern double last_ic[MAXODE];

extern char upar_names[MAXPAR][XPP_NAME_MAX+1],this_file[XPP_MAX_NAME];
extern int EqType[MAXODE];

extern char *ufun_def[MAXUFUN];
extern char ufun_names[MAXUFUN][XPP_NAME_MAX+1];
extern int narg_fun[MAXUFUN], *ufun[MAXUFUN];

extern UFUN_ARG ufun_arg[MAXUFUN];
extern BC_STRUCT my_bc[MAXODE];

extern int NFUN;

namespace {

/* do_edit_box's fields: the names it shows and the values it edits in
   place (MAX_LEN_EBOX bytes each, what the front ends write at most) */
class EditBox {
public:
  void add(std::string name, const char *value)
  {
    names_.push_back(std::move(name));
    std::array<char, MAX_LEN_EBOX> v{};
    std::string_view text(value ? value : "");
    text.copy(v.data(), std::min(text.size(), v.size() - 1));
    values_.push_back(v);
  }
  /* 0 on cancel */
  int show(const char *title)
  {
    std::vector<const char *> names;
    std::vector<char *> values;
    for (size_t i = 0; i < names_.size(); i++) {
      names.push_back(names_[i].c_str());
      values.push_back(values_[i].data());
    }
    return do_edit_box(static_cast<int>(names_.size()), title, names.data(), values.data());
  }
  const std::string &name(int i) const { return names_[i]; }
  const char *value(int i) const { return values_[i].data(); }
private:
  std::vector<std::string> names_;
  std::vector<std::array<char, MAX_LEN_EBOX>> values_;
};

/* the command add_expr compiles an expression into */
using Command = std::array<int, 200>;

template <class... Args>
void put(FILE *fp, std::format_string<Args...> fmt, Args &&...args)
{
  std::string s = xpp::format(fmt, std::forward<Args>(args)...);
  std::fwrite(s.data(), 1, s.size(), fp);
}

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
       /* ode_names is the parser's table of xpp_malloc'd formulas */
       xpp_free(ode_names[i]);
       ode_names[i]=xpp_strdup(box.value(i));
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
   set_new_arg_names(narg_fun[i],ufun_arg[i].args);
   int err=add_expr(box.value(i),command.data(),&len);
   set_old_arg_names(narg_fun[i]);
   if(err==1)
     err_msg(xpp::format("Bad func.:{}={}",box.name(i),box.value(i)).c_str());
   else {
     /* ufun_def[i] is the parser's MAXEXPLEN (1024, newpars.h -- not
        included here) bytes, parserslow2.cpp's every allocation site */
     xpp_strlcpy(ufun_def[i],box.value(i),1024);
     for(int j=0;j<=len;j++)
       ufun[i][j]=command[j];
     fixup_endfun(ufun[i],len,narg_fun[i]);
   }
 }
}

int save_as()
{
  std::array<char, 256> filename{};
  std::string_view file(this_file);
  file.copy(filename.data(), std::min(file.size(), filename.size() - 1));
  ping();
  if(!file_selector("Save As",filename.data(),"*.ode"))return(-1);
  if(!may_write_file(filename.data()))return(-1);
  xpp::Writer w(filename.data());
  if(!w){
    err_msg("Cannot open file");
    return(-1);
  }
  FILE *fp=w.file();
  double z;
  put(fp,"{}",NEQ);
  for(int i=0;i<NODE;i++){
    if(i%5==0)put(fp,"\nvariable ");
    put(fp," {}={:.16g} ",uvar_names[i],last_ic[i]);
  }
  put(fp,"\n");
  for(int i=NODE;i<NEQ;i++){
    if((i-NODE)%5==0)put(fp,"\naux ");
    put(fp," {} ",uvar_names[i]);
  }
  put(fp,"\n");
  for(int i=0;i<NUPAR;i++){
    if(i%5==0)put(fp,"\nparam  ");
    get_val(upar_names[i],&z);
    put(fp," {}={:.16g}   ",upar_names[i],z);
  }
  put(fp,"\n");
  for(int i=0;i<NFUN;i++)
    put(fp,"user {} {} {}\n",ufun_names[i],narg_fun[i],ufun_def[i]);
  for(int i=0;i<NODE;i++)
    put(fp,"{} {}\n",EqType[i]==1?"i":"o",ode_names[i]);
  for(int i=NODE;i<NEQ;i++)
    put(fp,"o {}\n",ode_names[i]);
  for(int i=0;i<NODE;i++)put(fp,"b {} \n",my_bc[i].string);
  put(fp,"done\n");
  return w.commit()?1:0;
}
