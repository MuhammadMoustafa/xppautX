#include "extra.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "init_conds.h"
#include "ggets.h"
#include "read_dir.h"
#include "parserslow.h"
#include <cstdlib>
#include <cstring>
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
#define MAXW 50

extern "C" {
/* set by load_eqn.c from the file's dll_lib= and dll_fun= */
char dll_lib[256];
char dll_fun[256];
int dll_flag=0;
extern double variables[], constants[];
extern char cur_dir[];
}

namespace {

struct InOut {
  char *lin = nullptr, *lout = nullptr;
  int *in = nullptr, *intype = nullptr;
  int *out = nullptr, *outtype = nullptr;
  int nin = 0, nout = 0;
  double *vin = nullptr, *vout = nullptr;
};
InOut in_out;

struct DlFun {
  char libname[1024];
  char libfile[256];
  char fun[256];
  int loaded;
};
DlFun dlf;

}  // namespace

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
    XPP_SPRINTF(dlf.libfile,"%s",dll_lib);
    XPP_SPRINTF(dlf.libname,"%s/%s",cur_dir,dlf.libfile);
    XPP_SPRINTF(dlf.fun,"%s",dll_fun);
    dlf.loaded=0;
  }
}

void load_new_dll(void)
{
  int status;
  if(dlf.loaded!=0&&dlhandle!=nullptr)
    dlclose(dlhandle);
  status=file_selector((char *)"Library:",dlf.libfile,(char *)"*.so");
  if(status==0)return;
  XPP_SPRINTF(dlf.libname,"%s/%s",cur_dir,dlf.libfile);
  new_string((char *)"Function name:",dlf.fun);
  dlf.loaded=0;
}

void get_import_values(int n, double *ydot, char *soname, char *sofun,
		       int ivar, double **wgt,
		       double *var, double *con)
{
  char sofullname[256];
  const char *error;
  if(dll_loaded==1){
    import_fun(n,ivar,con,var,wgt,ydot);
    return;
  }
  if(dll_loaded==-1)
    return;
  xpp::log(XPP_LOG_INFO, "soname = {}  sofun = {} \n",soname,sofun);
  get_directory(cur_dir);
  XPP_SPRINTF(sofullname,"%s/%s",cur_dir,soname);
  import_handle=dlopen(sofullname, RTLD_LAZY);
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
    dlhandle=dlopen(dlf.libname, RTLD_LAZY);
    if(!dlhandle){
      xpp::log(XPP_LOG_WARN, " Cant find the library \n");
      dlf.loaded=-1;
      return 0;
    }
    /* dlerror() clears any old error, dlsym(), then dlerror() again says
       whether dlsym failed (a symbol may be NULL) */
    dlerror();
    export_fun=symbol<ExportFun>(dlhandle,dlf.fun);
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

void get_import_values(int, double *, char *, char *, int, double **, double *, double *)
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
  if(!my_fun(in_out.vin,in_out.vout,in_out.nin,in_out.nout,variables,constants))
    return;
  for(i=0;i<in_out.nout;i++){
    if(in_out.outtype[i]==PAR)
      constants[in_out.out[i]]=in_out.vout[i];
    else
      variables[in_out.out[i]]=in_out.vout[i];
  }
}

void add_export_list(char *in,char *out)
{
  int i;
  /* a model loaded before this one had its own list */
  xpp_free(in_out.lin);
  xpp_free(in_out.lout);
  xpp_free(in_out.in);
  xpp_free(in_out.intype);
  xpp_free(in_out.vin);
  xpp_free(in_out.out);
  xpp_free(in_out.outtype);
  xpp_free(in_out.vout);
  in_out.lin=xpp_strdup(in); /* was malloc(strlen(in)): one byte short */
  in_out.lout=xpp_strdup(out);
  i=get_export_count(in);
  in_out.in=(int *)xpp_calloc(i+1,sizeof(int));
  in_out.intype=(int *)xpp_calloc(i+1,sizeof(int));
  in_out.vin=(double *)xpp_calloc(i+1,sizeof(double));
  in_out.nin=i;
  i=get_export_count(out);
  in_out.out=(int *)xpp_calloc(i+1,sizeof(int));
  in_out.outtype=(int *)xpp_calloc(i+1,sizeof(int));
  in_out.vout=(double *)xpp_calloc(i+1,sizeof(double));
  in_out.nout=i;
}

int get_export_count(char *s)
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
 parse_inout(in_out.lin,0);
 parse_inout(in_out.lout,1);
}

void parse_inout(char *l,int flag)
{
  size_t i=0;
  int j=0;
  int k=0,index;
  char name[XPP_NAME_MAX+1],c;
  int done=1;
  size_t len=strlen(l);
  while(done)
    {
      c=l[i];
      switch(c){
      case '{':
	i++;
	break;
      case ' ':
	i++;
	break;
      case ',':
      case '}':
	i++;
	name[j]=0;
	index=get_param_index(name);
	if(index<0) /* not a parameter */
	  {
	    index=get_var_index(name);
	    if(index<0)
	      {
		xpp::log(XPP_LOG_ERROR, "Cant export {} - non existent!\n",name);
		exit(0);
	      }
	    else /* it is a variable */
	      {
		if(flag==0){
		  in_out.in[k]=index;
		  in_out.intype[k]=VAR;
		}
		else {
		  in_out.out[k]=index;
		  in_out.outtype[k]=VAR;
		}
		k++;
	      }
	  } /* it is a parameter */
	else
	  {
	    if(flag==0)
	      {
		in_out.in[k]=index;
		in_out.intype[k]=PAR;
	      }
	  else
	    {
	      in_out.out[k]=index;
	      in_out.outtype[k]=PAR;
	    }
	    k++;
	  }
	if(c=='}')
	  done=0;
	j=0;
	break;

      default:
	if(j>=XPP_NAME_MAX){
	  xpp_log(XPP_LOG_WARN, "Cant export %.*s... - name too long!\n",XPP_NAME_MAX,name);
	  exit(0);
	}
	name[j]=c;
	j++;
	i++;
      }
      if(i>len)
	done=0;
    }
}
