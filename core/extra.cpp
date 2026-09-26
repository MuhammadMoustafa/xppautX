#include "extra.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "init_conds.h"
#include "ggets.h"
#include "read_dir.h"
#include "parserslow.h"
#include "load_eqn.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/* this is a way to communicate XPP with other stuff

# complex right-hand sides
# let xpp know about the names
xp=0
yp=0
x'=xp
y'=yp
# tell xpp input info and output info
export {x,y} {xp,yp}

*/

#define PAR 0
#define VAR 1


extern "C" {
extern double variables[], constants[];
extern char cur_dir[XPP_MAX_NAME]; /* read_dir.cpp */
}

namespace {

/* the model's export {inputs} {outputs}: where each comes from (a
   parameter or a variable, by index) and its value */
struct InOut {
  std::string lin, lout;
  std::vector<int> in, intype;
  std::vector<int> out, outtype;
  int nin = 0, nout = 0;
  std::vector<double> vin, vout;
};

InOut in_out;

/* set by load_eqn.cpp from the file's dll_lib= and dll_fun= */
std::string dll_lib;
std::string dll_fun;
int dll_flag=0;

struct DlFun {
  std::string libname;
  std::string libfile;
  std::string fun;
  int loaded = 0;
};

DlFun dlf;

/* a dialog's buffer (new_string/file_selector write up to 256 bytes)
   with text as its default */
std::array<char, 256> dialog_buffer(const std::string &text)
{
  std::array<char, 256> buf{};
  text.copy(buf.data(), std::min(text.size(), buf.size() - 1));
  return buf;
}

}  // namespace

void set_dll_library(std::string_view lib)
{
  dll_lib = lib;
  dll_flag += 1;
}

void set_dll_function(std::string_view fun)
{
  dll_fun = fun;
  dll_flag += 2;
}

#ifdef HAVEDLL
/* this loads a dynamically linked library of the
   users choice
*/

#include "xpp_dlfcn.h"

namespace {

/* export's function (dll_fun=): the exported inputs in, the outputs out */
using ExportFun = void (*)(double *in, double *out, int nin, int nout, double *v, double *c);
/* a network's import(...) function (simplenet.c) */
using ImportFun = void (*)(int n, int ivar, double *con, double *var, double **wgt, double *ydot);

void *dlhandle = nullptr;
ExportFun export_fun = nullptr;
void *import_handle = nullptr;
ImportFun import_fun = nullptr;
int dll_loaded=0;

/* dlsym's object pointer as a function pointer, without the cast C++ only conditionally allows */
template <typename F> F symbol(void *handle, const char *name)
{
  void *sym=dlsym(handle,name);
  F f;
  std::memcpy(&f,&sym,sizeof f);
  return f;
}

}  // namespace

void auto_load_dll(void)
{
  if(dll_flag==3){
    get_directory(cur_dir);
    xpp::log(XPP_LOG_INFO, "DLL lib {}/{} with function {} \n",cur_dir,dll_lib,dll_fun);
    dlf.libfile=dll_lib;
    dlf.libname=xpp::format("{}/{}",cur_dir,dlf.libfile);
    dlf.fun=dll_fun;
    dlf.loaded=0;
  }
}

void load_new_dll(void)
{
  if(dlf.loaded!=0&&dlhandle!=nullptr)
    dlclose(dlhandle);
  std::array<char, 256> file=dialog_buffer(dlf.libfile);
  if(file_selector("Library:",file.data(),"*.so")==0)return;
  dlf.libfile=file.data();
  dlf.libname=xpp::format("{}/{}",cur_dir,dlf.libfile);
  std::array<char, 256> fun=dialog_buffer(dlf.fun);
  new_string("Function name:",fun.data());
  dlf.fun=fun.data();
  dlf.loaded=0;
}

void get_import_values(int n, double *ydot, const char *soname, const char *sofun,
		       int ivar, double **wgt,
		       double *var, double *con)
{
  const char *error;
  if(dll_loaded==1){
    import_fun(n,ivar,con,var,wgt,ydot);
    return;
  }
  if(dll_loaded==-1)
    return;
  xpp::log(XPP_LOG_INFO, "soname = {}  sofun = {} \n",soname,sofun);
  get_directory(cur_dir);
  std::string sofullname=xpp::format("{}/{}",cur_dir,soname);
  import_handle=dlopen(sofullname.c_str(), RTLD_LAZY);
  if(!import_handle){
    xpp::log(XPP_LOG_WARN, " Cant find the library {}\n",soname);
    dll_loaded=-1;
    return;
  }
  dlerror();
  import_fun=symbol<ImportFun>(import_handle,sofun);
  error=dlerror();
  if(error!=nullptr){
    xpp::log(XPP_LOG_WARN, "Problem with function.. {}\n",sofun);
    dll_loaded=-1;
    return;
  }
  dll_loaded=1;
  import_fun(n,ivar,con,var,wgt,ydot);
}

/* 1 when the library's function ran (and wrote `out`), 0 when there is none */
int my_fun(double *in, double *out, int nin,int nout,double *v,double *c)
{
  const char *error;
  if(dlf.loaded==-1)return(0);
  if(dlf.loaded==0){
    dlhandle=dlopen(dlf.libname.c_str(), RTLD_LAZY);
    if(!dlhandle){
      xpp::log(XPP_LOG_WARN, " Cant find the library \n");
      dlf.loaded=-1;
      return 0;
    }
    /* dlerror() clears any old error, dlsym(), then dlerror() again says
       whether dlsym failed (a symbol may be NULL) */
    dlerror();
    export_fun=symbol<ExportFun>(dlhandle,dlf.fun.c_str());
    error=dlerror();
    if(error!=nullptr){
      xpp::log(XPP_LOG_WARN, "Problem with function..\n");
      dlf.loaded=-1;
      return 0;
    }
    dlf.loaded=1;
  }  /* Ok we have a nice function */
  export_fun(in,out,nin,nout,v,c);
  return(1);
}

#else

void get_import_values(int, double *, const char *, const char *, int, double **, double *, double *)
{
}

void load_new_dll(void)
{
}

int my_fun(double *, double *, int, int, double *, double *)
{
  return 0;
}

void auto_load_dll(void)
{
}

#endif

void do_in_out(void)
{
  int i;
  if(in_out.nin==0||in_out.nout==0)return;
  for(i=0;i<in_out.nin;i++){
    if(in_out.intype[i]==PAR)
      in_out.vin[i]=constants[in_out.in[i]];
    else
      in_out.vin[i]=variables[in_out.in[i]];
  }
  /* no library (none named, or it did not load): the outputs keep their own
     values; they took whatever the never-written buffer held before, which
     differed between runs on Windows (W20) */
  if(!my_fun(in_out.vin.data(),in_out.vout.data(),in_out.nin,in_out.nout,variables,constants))
    return;
  for(i=0;i<in_out.nout;i++){
    if(in_out.outtype[i]==PAR)
      constants[in_out.out[i]]=in_out.vout[i];
    else
      variables[in_out.out[i]]=in_out.vout[i];
  }
}

void add_export_list(const char *in,const char *out)
{
  /* a model loaded before this one had its own list: assign replaces it */
  in_out.lin=in;
  in_out.lout=out;
  int i=get_export_count(in);
  in_out.in.assign(i+1,0);
  in_out.intype.assign(i+1,0);
  in_out.vin.assign(i+1,0.0);
  in_out.nin=i;
  i=get_export_count(out);
  in_out.out.assign(i+1,0);
  in_out.outtype.assign(i+1,0);
  in_out.vout.assign(i+1,0.0);
  in_out.nout=i;
}

int get_export_count(const char *s)
{
  int i=0;
  for(const char *p=s;*p;p++)
    if(*p==',')i++;
  i++;
  return(i);
}

void do_export_list(void)
{
 if(in_out.nin==0||in_out.nout==0)return;
 parse_inout(in_out.lin.c_str(),0);
 parse_inout(in_out.lout.c_str(),1);
}

/* l is "{name,name,...}": each name's index and kind into the inputs
   (flag 0) or the outputs (flag 1) */
void parse_inout(const char *l,int flag)
{
  std::vector<int> &where=flag==0?in_out.in:in_out.out;
  std::vector<int> &type=flag==0?in_out.intype:in_out.outtype;
  size_t k=0;
  std::string name;
  size_t len=strlen(l);
  for(size_t i=0;i<=len;i++){
    char c=l[i];
    switch(c){
    case '{':
    case ' ':
      break;
    case ',':
    case '}':{
      int index=get_param_index(name.c_str());
      int kind=PAR;
      if(index<0){ /* not a parameter */
        index=get_var_index(name.c_str());
        if(index<0){
          xpp::log(XPP_LOG_ERROR, "Cant export {} - non existent!\n",name);
          exit(0);
        }
        kind=VAR;
      }
      if(k<where.size()){
        where[k]=index;
        type[k]=kind;
      }
      k++;
      if(c=='}')
        return;
      name.clear();
      break;
    }
    default:
      if(name.size()>=XPP_NAME_MAX){
        xpp::log(XPP_LOG_WARN, "Cant export {}... - name too long!\n",name);
        exit(0);
      }
      name+=c;
    }
  }
}
