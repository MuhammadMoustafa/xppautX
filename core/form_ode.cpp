#include <new>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include "form_ode.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "aniparse.h"

#include "parserslow.h"
#include "markov.h"
#include "read_dir.h"
#include <unistd.h>
#include "flags.h"



#include "main.h"
#include "ggets.h"
#include "load_eqn.h"
#include "dae_fun.h"
#include "derived.h"
#include "extra.h"
#include "browse.h"
#include "simplenet.h"
#include "integrate.h"
#include "newpars.h"
#include "xpp_ui.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifndef WCTYPE
#include <ctype.h>
#else
#include <wctype.h>
#endif

#include "xpplim.h"

#include "my_pars.h"
#include "shoot.h"
#include "newpars.h"
#include "xpp_batch.h"
#include "xpp_globals.h"
#include "comline.h"

#define MAXONLY 1000

#define MAXCOMMENTS 500

int IN_INCLUDED_FILE=0;
char uvar_names[MAXODE][XPP_NAME_MAX+1];
char *ode_names[MAXODE];
char upar_names[MAXPAR][XPP_NAME_MAX+1];
char *save_eqn[MAXLINES];
double default_val[MAXPAR];
extern int NODE;
extern int NUPAR;
extern int NLINES;
extern int IN_VARS;
extern int leng[MAXODE];

namespace {
/* the lines of the model being read, in order: do_new_parser() adds them,
   compile_em() compiles them, free_varinfo() lets them go */
std::vector<VAR_INFO> model_lines;
}

int *my_ode[MAXODE];

int leng[MAXODE];

typedef struct {
  char *text,*action;
  int aflag;
} ACTION;

extern int loadincludefile;

char *onlylist[MAXONLY];
int *plotlist;
int N_only=0,N_plist;

ACTION comments[MAXCOMMENTS];
int is_a_map=0;
int n_comments=0;
 extern char delay_string[MAXODE][80];
BC_STRUCT my_bc[MAXODE];


double default_ic[MAXODE];
extern double last_ic[];
int NODE,NUPAR,NLINES;
int PrimeStart;
int NCON_START,NSYM_START;
 int BVP_NL,BVP_NR,BVP_N;
 extern int BVP_FLAG;

#define cstringmaj MYSTR1
#define cstringmin MYSTR2

int ConvertStyle=0;
FILE *convertf;
extern int ERROUT;
 extern int NTable;
int OldStyle=1;
int NCON_ORIG,NSYM_ORIG;
int IN_VARS;
int NMarkov;

int FIX_VAR;

extern int NEQ,NVAR,NKernel;
extern int NFUN;
int NEQ_MIN;
extern int NCON,NSYM;
extern int NWiener;
/*extern char this_file[100];
*/
extern char this_file[XPP_MAX_NAME];
int EqType[MAXODE];
int Naux=0;
char aux_names[MAXODE][XPP_NAME_MAX+1];

int NUMODES=0,NUMFIX=0,NUMPARAM=0,NUMMARK=0,NUMVOLT=0,NUMSOL=0;


FIXINFO fixinfo[MAXODE];
extern char cur_dir[];



int make_eqn()
  {
   
   int okay;
   NEQ=2;
   FIX_VAR=0;
   NMarkov=0;
   
   /* initscr(); */
   /*
   pos_prn("*(r)ead or (c)reate:",0,0);
   ch=getuch();
   pos_prn("",0,0); 
   okay=0;
   switch(ch)
   {
    case 'r':okay=read_eqn(); break;
    case 'c': okay=create_eqn();break;
      default : read_eqn();break;
   }
   */
   okay=read_eqn();

   return(okay);
  }

void strip_saveqn()
{
  int i;
  int j,n;
  for(i=0;i<NLINES;i++){
    n=strlen(save_eqn[i]);
    for(j=0;j<n;j++)
      if(save_eqn[i][j]<32)
	save_eqn[i][j]=32;
  }
}

static void save_line(const std::string &line);

namespace {

/* fprintf's type-checked counterpart: std::format (xpp::format) into fp */
template <class... Args>
void put(FILE *fp, std::format_string<Args...> fmt, Args &&...args)
{
  std::string text = xpp::format(fmt, std::forward<Args>(args)...);
  std::fwrite(text.data(), 1, text.size(), fp);
}

} // namespace

int disc(const char *string)
{
  if(is_a_map==1)return(1);
  /* what follows the first '.' */
  std::string_view s(string);
  size_t dot=s.find('.');
  std::string_view end=dot==std::string_view::npos?std::string_view():s.substr(dot+1);
  return end=="dis"||end=="dif";
}

void format_list(const char *const *s,int n)
{
 int i,ip;
 int ncol;
 int k,j;
 int lmax=0,l=0;
 for(i=0;i<n;i++){
   l=strlen(s[i]);
   if(lmax<l)lmax=l;
 }
 ncol=80/(lmax+2);
 if(ncol<1)ncol=1;
 if(ncol>8)ncol=8;
 k=n/ncol;
 j=n-ncol*k;
 /* each name right-aligned in lmax+2 columns */
 for(ip=0;ip<k;ip++){
   for(i=0;i<ncol;i++)
     xpp::log(XPP_LOG_INFO, "{:>{}}",s[ip*ncol+i],lmax+2);
   xpp_log(XPP_LOG_INFO, "\n");
 }
  for(i=0;i<j;i++)
     xpp::log(XPP_LOG_INFO, "{:>{}}",s[k*ncol+i],lmax+2);
  xpp_log(XPP_LOG_INFO, "\n");
}

/* the next whitespace-separated word typed on stdin (what scanf("%s")
   read, of any length); false at the end of input */
static bool read_word(std::string &word)
{
  int c;
  word.clear();
  while((c=getchar())!=EOF&&isspace(c)){}
  if(c==EOF)return false;
  do word+=static_cast<char>(c);
  while((c=getchar())!=EOF&&!isspace(c));
  return true;
}

/* The ODE file to read: asked on the console in batch mode ((r)un,
   (c)d, (l)ist wild), else with the file selector. 0 on cancel. */
static int get_a_filename(std::string &filename,std::string &wild)
{
 if(batch_options.enabled)
 {
  std::string string;
  list_em(wild.c_str());
  while(1){
  xpp_log(XPP_LOG_INFO, "(r)un (c)d (l)ist ");
  if(!read_word(string))return 0;
  if(string[0]=='r'){
    xpp_log(XPP_LOG_INFO, "Run file: ");
    if(!read_word(filename))return 0;
    xpp::log(XPP_LOG_INFO, "Loading {}\n ",filename);
    return 1;
  }
  else
    {
      if(string[0]=='l'){
        xpp_log(XPP_LOG_INFO, "List files of type: ");
        if(!read_word(wild))return 0;
        list_em(wild.c_str());
      }
      else
        {
 	 if(string[0]=='c'){
	   xpp_log(XPP_LOG_INFO, "Change to directory: ");
	   if(!read_word(string))return 0;
	   change_directory(string.c_str());
	   list_em(wild.c_str());
	 }
        }
    }
  }
  }
  else
  {
    /* get_directory fills XPP_MAX_NAME bytes, file_selector up to 256 */
    std::array<char, XPP_MAX_NAME+10> buf{};
    get_directory(buf.data());
    std::string dir(buf.data());
    if (dir.empty() || dir.back() != '/')
      dir += '/';
    dir.copy(buf.data(), std::min(dir.size(), buf.size()-1));
    if (file_selector ("Select an ODE file", buf.data(), wild.c_str()) == 0) {
      bye_bye ();
      return 0;
    }
    filename = buf.data();
    return 1;
  }
  return(0);
}


void list_em(const char *wild)
{
  get_directory(cur_dir);
  xpp_log(XPP_LOG_INFO, "%s: \n",cur_dir);
  FILEINFO ff{};
  if(!get_fileinfo(wild,cur_dir,&ff))return;
  xpp_log(XPP_LOG_INFO, "DIRECTORIES:\n");
  format_list(ff.dirnames,ff.ndirs);
  xpp_log(XPP_LOG_INFO, "FILES OF TYPE %s:\n",wild);
  format_list(ff.filenames,ff.nfiles);
  free_finfo(&ff);
}
int read_eqn()
{
  std::string wild="*.ode",string;
  get_a_filename(string,wild);
  FILE *fptr=fopen(string.c_str(),"r");
  if(fptr==NULL)
   {
    xpp::log(XPP_LOG_WARN, "\n Cannot open {} \n",string);
    return(0);
   }
   XPP_FORMAT_TO_BUF(this_file,"{}",string);
   clrscr();
   int okay=get_eqn(fptr);
   fclose(fptr);
   return(okay);
 }



int get_eqn(FILE *fptr)
{
  char bob[MAXEXPLEN];
  int done=1,i;
  int flag;
  char prim[XPP_NAME_MAX+2];
  char t_name[]="t",t_prime[]="t'"; /* add_var takes a char * */
  init_rpn();
  NLINES=0;
  IN_VARS=0;
  NODE=0;
  BVP_N=0;
  BVP_NL=0;
  BVP_NR=0;
  NUPAR=0;
  NWiener=0;
  /*check_for_xpprc();  This is now done just once and in do_vis_env()
  */
  options_file="default.opt";
  add_var(t_name,0.0);
  /* plintf(" NEQ: "); */
  if(fgets(bob,MAXEXPLEN,fptr)==NULL)bob[0]=0;
  save_line(bob);
  /* plintf("incr NLINE in geteqn  %s \n",bob); */
  i=atoi(bob);
  if(i<=0) { /* New parser ---   */
    
    OldStyle=0;
    ConvertStyle=0;
    flag=do_new_parser(fptr,bob,0);
    if(flag<0) exit(0);
  }
  else{
    OldStyle=1;
    NEQ=i;
    xpp_log(XPP_LOG_INFO, "NEQ=%d\n",NEQ);
    if(ConvertStyle){
      std::string filename=this_file[0]==0?std::string("convert.ode"):std::string(this_file)+".new";
      if((convertf=fopen(filename.c_str(),"w"))==NULL){
	xpp::log(XPP_LOG_WARN, " Cannot open {} - no conversion done \n",filename);
	ConvertStyle=0;
      }
      put(convertf,"# converted {} \n",this_file);
    }
    while(done)
      {
	if(fgets(bob,MAXEXPLEN,fptr)==NULL)break;
	save_line(bob);
	/* plintf("inc NLINES in geteqn2 %s \n",bob); */
	done=compiler(bob,fptr);
      }
    if(ConvertStyle){
      put(convertf,"done\n");
      fclose(convertf);
    }
  }
 if((NODE+NMarkov)==0){
   xpp_log(XPP_LOG_ERROR, " Must have at least one equation! \n Probably not an ODE file.\n");
   exit(0);
 }
  if(BVP_N>IN_VARS ){
    xpp_log(XPP_LOG_ERROR, "Too many boundary conditions\n");
    exit(0);
  }
  /* plintf("BVP_N=%d NODE=%d NVAR=%d IN_VARS=%d\n",BVP_N,NODE,NVAR,IN_VARS); */
  if(BVP_N<IN_VARS ){
    if(BVP_N>0)xpp_log(XPP_LOG_WARN, "Warning: Too few boundary conditions\n");
    for(i=BVP_N;i<IN_VARS ;i++){
      my_bc[i].com=static_cast<int *>(xpp_malloc(200*sizeof(int)));
      my_bc[i].string=static_cast<char *>(xpp_malloc(256));
      my_bc[i].name=static_cast<char *>(xpp_malloc(10));
      my_bc[i].side=0;
      xpp_strlcpy(my_bc[i].string,"0",256);
      xpp_strlcpy(my_bc[i].name,"0=",10);
    }
  }
  BVP_FLAG=1;
  
  if(NODE!=NEQ+FIX_VAR-NMarkov)
    {
      xpp_log(XPP_LOG_ERROR, " Too many/few equations\n");
      exit(0);
    }
  if(IN_VARS>NEQ)
    {
      xpp_log(XPP_LOG_ERROR, " Too many variables\n");
	exit(0);
    }
  NODE=IN_VARS;
  
  for(i=0; i<Naux; i++)
    XPP_FORMAT_TO_BUF(uvar_names[i+NODE+NMarkov],"{}",aux_names[i]);
  
  for(i=NODE+NMarkov+Naux;i<NEQ;i++)
    {
      XPP_SPRINTF(uvar_names[i],"AUX%d",i-NODE-NMarkov+1);
    }
  
  
  for(i=0;i<NEQ;i++)
      {
	strupr(uvar_names[i]);
	strupr(ode_names[i]);
        de_space(ode_names[i]);
      }
  /*
     add primed variables                              */
  PrimeStart=NVAR;
  if(NVAR<MAXPRIMEVAR){
  add_var(t_prime,0.0);
  for(i=0;i<NODE ;i++){
    snprintf(prim,sizeof(prim),"%.*s'",XPP_NAME_MAX,uvar_names[i]);
    add_var(prim,0.0);
  }
}
  else {
    xpp_log(XPP_LOG_WARN, " Warning: primed variables not added must have < %d variables\n",
     MAXPRIMEVAR);
    xpp_log(XPP_LOG_WARN, " Averaging and boundary value problems cannot be done\n");
  }
  if(NMarkov>0)
    compile_all_markov();
  if(compile_flags()==1){
    xpp_log(XPP_LOG_ERROR, " Error in compiling a flag \n");
    exit(0);
  }
  /*  add auxiliary variables   */
  for(i=NODE+NMarkov;i<NEQ;i++)add_var(uvar_names[i],0.0); 
  NCON_START=NCON;
  NSYM_START=NSYM;
  NCON_ORIG=NCON;
  NSYM_ORIG=NSYM;
  NEQ_MIN=NEQ;
  program.version_major=static_cast<float>(cstringmaj);
  program.version_minor=static_cast<float>(cstringmin);
  xpp_log(XPP_LOG_INFO, "Used %d constants and %d symbols \n",NCON,NSYM);
  xpp_log(XPP_LOG_INFO, "XPPAUT %g.%g Copyright (C) 2002-now  Bard Ermentrout \n",program.version_major,program.version_minor);
    return(1);
}
int compiler(char *bob, FILE *fptr)
{
  double value,xlo,xhi;
  int narg,done,nn,iflg=0,VFlag=0,nstates,alt,index,sign;
  char *ptr,*my_string,*command;
  char name[MAXEXPLEN],formula[MAXEXPLEN];
  char condition[MAXEXPLEN];
  static char fixname[MAXODE1][MAXVNAM];
  int nlin,i;
  ptr=bob;
  done=1;
  if(bob[0]=='@'){
    /* printf("Storing opts from formode \n"); */
    stor_internopts(bob);
    if(ConvertStyle)
      put(convertf,"{}\n",bob);
    return(done);
  }
  command=get_first(ptr," ,");
  strlwr(command);
  switch(*command)
    {
    case 'd': done=0;
      break;
    case 's': show_syms();
      break;
    case 'h': welcome();
      break;
    case 'x':
      my_string=get_next("{ ");
      XPP_FORMAT_TO_BUF(condition,"{}",my_string);

      my_string=get_next("}\n");
           XPP_FORMAT_TO_BUF(formula,"{}",my_string);
      add_intern_set(condition,formula);
      break;
    case 'w':  /*  Make a Wiener (heh heh) constants  */
      xpp_log(XPP_LOG_INFO, "Wiener constants\n");
      if(ConvertStyle)
	put(convertf,"wiener ");
      advance_past_first_word(&ptr);
      for(std::optional<std::string> tok;(tok=get_next2(&ptr));)
	{
	  take_apart(tok->c_str(),&value,name);
	  xpp_log(XPP_LOG_DEBUG, "|%s|=%f ",name,value);
	  if(ConvertStyle)
	    put(convertf,"{}  ",name);
	  if(add_con(name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    exit(0);
	  }
	  add_wiener(NCON-1);
	 
	}
      if(ConvertStyle)
	put(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
           break;
    case 'n':    
      xpp_log(XPP_LOG_INFO, " Hidden params:\n");
      if(ConvertStyle)
	put(convertf,"number ");
	
      advance_past_first_word(&ptr);
      for(std::optional<std::string> tok;(tok=get_next2(&ptr));)
	{
	  take_apart(tok->c_str(),&value,name);
	  if(ConvertStyle)
	    put(convertf,"{}={:g}  ",name,value);
          
	  xpp_log(XPP_LOG_DEBUG, "|%s|=%f ",name,value);
	  if(add_con(name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    exit(0);
	  }

	}
       if(ConvertStyle)
	put(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break; 
    case 'g': /* global */
      my_string=get_next("{ ");
      sign=atoi(my_string);
      xpp_log(XPP_LOG_DEBUG, " GLOBAL: sign =%d \n",sign);
      my_string=get_next("{}");
      XPP_FORMAT_TO_BUF(condition,"{}",my_string);
      xpp_log(XPP_LOG_DEBUG, " condition = %s \n",condition);
      my_string=get_next("\n");
      XPP_FORMAT_TO_BUF(formula,"{}",my_string);
      xpp_log(XPP_LOG_DEBUG, " events=%s \n",formula);
      if(add_global(condition,sign,formula)){
	xpp_log(XPP_LOG_WARN, "Bad global !! \n");
	exit(0);
      }
      if(ConvertStyle){
	put(convertf,"global {} {{{}}} {}\n",sign,condition,formula);
      }
      break;
    case 'p':
      xpp_log(XPP_LOG_INFO, "Parameters:\n");
      if(ConvertStyle)
	put(convertf,"par ");

      advance_past_first_word(&ptr);

      for(std::optional<std::string> tok;(tok=get_next2(&ptr));)
	{

	  take_apart(tok->c_str(),&value,name);
	  if(add_con(name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    exit(0);
	  }
	  default_val[NUPAR]=value;
	  XPP_FORMAT_TO_BUF(upar_names[NUPAR++],"{}",name);
	  if(ConvertStyle)
	    put(convertf,"{}={:g}  ",name,value);
	  xpp_log(XPP_LOG_DEBUG, "|%s|=%f ",name,value);

	}
      if(ConvertStyle)
	put(convertf,"\n");
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
    case 'c': my_string=get_next(" \n");
      options_file=my_string;
      xpp_log(XPP_LOG_INFO, " Loading new options file:<%s>\n",my_string);
      if(ConvertStyle)
	put(convertf,"option {}\n",options_file.c_str());
      break;
    case 'f':iflg=0;
      xpp_log(XPP_LOG_INFO, "\nFixed variables:\n");
      goto vrs;
    case 'm': /* Markov variable  */
      my_string=get_next(" ");
      XPP_FORMAT_TO_BUF(name,"{}",my_string);
      my_string=get_next(" ");
      value=atof(my_string);
      my_string=get_next(" \n");
      nstates=atoi(my_string);
      if(name_too_long(name)||add_var(name,value)){
	xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	exit(0);
      }
      XPP_FORMAT_TO_BUF(uvar_names[IN_VARS+NMarkov],"{}",name);
      last_ic[IN_VARS+NMarkov]=value;
      default_ic[IN_VARS+NMarkov]=value;
      xpp_log(XPP_LOG_INFO, " Markov variable %s=%f has %d states \n",name,value,nstates);
      if(OldStyle)add_markov(nstates,name);
      if(ConvertStyle)
	put(convertf,"{}(0)={:g}\n",name,value);
      break;
    case 'r': /* state table for Markov variables  */
      my_string=get_next("\n");
      XPP_FORMAT_TO_BUF(name,"{}",my_string);
      nlin=NLINES;
      index=old_build_markov(fptr,name);
      XPP_FORMAT_TO_BUF(formula,"{}",save_eqn[nlin]);
      ode_names[IN_VARS+index]=xpp_strdup(xpp::format("{{ {} ... }}",formula).c_str());
      break;
    case 'v':      
      iflg=1;
      xpp_log(XPP_LOG_INFO, "\nVariables:\n");
      if(ConvertStyle)
	put(convertf,"init ");
    vrs:
      if(NMarkov>0&&OldStyle) {
	xpp_log(XPP_LOG_WARN, " Error at line %d \n Must declare Markov variables after fixed and regular variables\n",NLINES);
	exit(0);
      }
      advance_past_first_word(&ptr);
      for(std::optional<std::string> tok;(tok=get_next2(&ptr));)
	{
	  if((IN_VARS>NEQ)||(IN_VARS==MAXODE))
	    {
	      xpp_log(XPP_LOG_ERROR, " too many variables at line %d\n",NLINES);
	      exit(0);
	    }
	  take_apart(tok->c_str(),&value,name);
	  if(name_too_long(name)||add_var(name,value)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    exit(0);
	  }
	  if(iflg)
	    {
	      XPP_FORMAT_TO_BUF(uvar_names[IN_VARS],"{}",name);
	      last_ic[IN_VARS]=value;
              default_ic[IN_VARS]=value;   
	      IN_VARS++;
	      if(ConvertStyle)
		put(convertf,"{}={:g}  ",name,value);
	    }
	  else {
	    if(ConvertStyle)
	      XPP_FORMAT_TO_BUF(fixname[FIX_VAR],"{}",name);
	    FIX_VAR++;

	  }
	  xpp_log(XPP_LOG_DEBUG, "|%s| ",name);
	  
	}
      xpp_log(XPP_LOG_DEBUG, " \n");
      if(iflg&&ConvertStyle)
	put(convertf,"\n");
      break;
    case 'b':
            my_string=get_next("\n");
      my_bc[BVP_N].com=static_cast<int *>(xpp_malloc(200*sizeof(int)));
      /*         plintf(" adding boundary condition %s \n",my_string);
       */
      my_bc[BVP_N].string=static_cast<char *>(xpp_malloc(256));
      my_bc[BVP_N].name=static_cast<char *>(xpp_malloc(10));
      xpp_strlcpy(my_bc[BVP_N].string,my_string,256);
      xpp_strlcpy(my_bc[BVP_N].name,"0=",10);
      if(ConvertStyle)
	put(convertf,"bndry {}\n",my_bc[BVP_N].string);
      
      
      
      
      xpp_log(XPP_LOG_DEBUG, "|%s| |%s| \n",my_bc[BVP_N].name,my_bc[BVP_N].string);
      BVP_N++;
      break;
    case 'k':
      if(ConvertStyle)
	xpp_log(XPP_LOG_WARN, " Warning  kernel declaration cannot be converted \n");
      my_string=get_next(" ");
      XPP_FORMAT_TO_BUF(name,"{}",my_string);
      my_string=get_next(" ");
      value=atof(my_string);
      my_string=get_next("$");
      XPP_FORMAT_TO_BUF(formula,"{}",my_string);
      xpp_log(XPP_LOG_DEBUG, "Kernel mu=%f %s = %s \n",value,name,formula);
      if(add_kernel(name,value,formula)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	exit(0);
      }
      break;
    case 't': 
      if(NTable>=MAX_TAB)
	{
	  if(ERROUT)xpp_log(XPP_LOG_WARN, "too many tables !!\n");
	  exit(0);
	}
      my_string=get_next(" ");
      XPP_FORMAT_TO_BUF(name,"{}",my_string);
      my_string=get_next(" \n");
      if(my_string[0]=='%') {
	xpp_log(XPP_LOG_INFO, " Function form of table....\n");
	my_string=get_next(" ");
	nn=atoi(my_string);
	my_string=get_next(" ");
	xlo=atof(my_string);
	my_string=get_next(" ");
	xhi=atof(my_string);
	my_string=get_next("\n");
	XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	xpp_log(XPP_LOG_INFO, " %s has %d pts from %f to %f = %s\n",
	       name,nn,xlo,xhi,formula);
	add_table_name(NTable,name);

	if(add_form_table(NTable,nn,xlo,xhi,formula)){
	  xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	  exit(0);
	}

	if(ConvertStyle)
	  put(convertf,"table {} % {} {:g} {:g} {}\n",
		  name,nn,xlo,xhi,formula);
	NTable++;
	xpp_log(XPP_LOG_INFO, " NTable = %d \n",NTable);

	
	
      }
      else 
	if(my_string[0]=='@'){
	  xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	  my_string=get_next(" ");
	  XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	  xpp_log(XPP_LOG_INFO, " %s = %s \n",name,formula);
	  if(add_2d_table(name,formula)){
	    xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	    exit(0);
	  }
	}
	else
	  {
	    XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	    xpp_log(XPP_LOG_INFO, "Lookup table %s = %s \n",name,formula);
            add_table_name(NTable,name);
	    if(add_file_table(NTable,formula)){
	      xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	      exit(0);
	    }
	    if(ConvertStyle)
	      put(convertf,"table {} {}\n",
		      name,formula);
	    NTable++;
	  }
      break;
      
    case 'u':
      my_string=get_next(" ");
      XPP_FORMAT_TO_BUF(name,"{}",my_string);
      my_string=get_next(" ");
      narg=atoi(my_string);
      my_string=get_next("$");
      XPP_FORMAT_TO_BUF(formula,"{}",my_string);
      xpp_log(XPP_LOG_INFO, "%s %d :\n",name,narg);
      if(ConvertStyle){
	put(convertf,"{}(",name);
	for(i=0;i<narg;i++){
	  put(convertf,"arg{}",i+1);
	  if(i<(narg-1))
	    put(convertf,",");
	}
	put(convertf,")={}",formula);
      }
      if(add_ufun(name,formula,narg)){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	exit(0);
      }

      xpp_log(XPP_LOG_INFO, "user %s = %s\n",name,formula);
      break;
    case 'i': VFlag=1;
    case 'o':
      if(NODE>=(NEQ+FIX_VAR-NMarkov))
	{
	  done=0;
	  break;
	}
      my_string=get_next("\n");
      XPP_FORMAT_TO_BUF(formula,"{}",my_string);
      my_ode[NODE]=static_cast<int *>(xpp_malloc(MAXEXPLEN*sizeof(int)));
      
      if(NODE<IN_VARS)
	{
	  ode_names[NODE]=xpp_strdup(formula);
	  if(ConvertStyle){
	    if(VFlag)
	      put(convertf,"volt {}={}\n",uvar_names[NODE],formula);
	    else
	      put(convertf,"{}'={}\n",uvar_names[NODE],formula);
	  }
	  find_ker(formula,&alt);
	  
	
	  EqType[NODE]=VFlag;
	
	  VFlag=0;
	}
      if(NODE>=IN_VARS&&NODE<(IN_VARS+FIX_VAR))
	{
	  if(ConvertStyle)
	    put(convertf,"{}={}\n",fixname[NODE-IN_VARS],formula);
	  find_ker(formula,&alt);
	  
	}
      
      
      if(NODE>=(IN_VARS+FIX_VAR))
	{
	  i=NODE-(IN_VARS+FIX_VAR);
	  ode_names[NODE-FIX_VAR+NMarkov]=xpp_strdup(formula);
	  if(ConvertStyle){
	    if(i<Naux)
	      put(convertf,"aux {}={}\n",aux_names[i],formula);
	    else
	      put(convertf,"aux aux{}={}\n",i+1,formula);
	  }
	}
      xpp_log(XPP_LOG_INFO, "RHS(%d)=%s\n",NODE,formula);
      if(add_expr(formula,my_ode[NODE],&leng[NODE])){
	xpp_log(XPP_LOG_WARN, "ERROR at line %d\n",NLINES);
	exit(0);
      }
      /* fpr_command(my_ode[NODE]); */
      NODE++;
      break;
      
    case 'a':   /* name auxiliary variables */
      xpp_log(XPP_LOG_INFO, "Auxiliary variables:\n");
      while((my_string=get_next(" ,\n"))!=NULL)
	{
	  if(name_too_long(my_string))exit(0);
	  XPP_FORMAT_TO_BUF(aux_names[Naux],"{}",my_string);   
	  xpp_log(XPP_LOG_DEBUG, "|%s| ",aux_names[Naux]);
	  Naux++;
	};
      xpp_log(XPP_LOG_DEBUG, "\n");
      break;
     
    default:
      if(ConvertStyle) {
	my_string=get_next("\n");
	put(convertf,"{} {}\n",command,my_string?my_string:"");
      }
      break;
    }
  
  return(done);
}

void welcome()
{
 xpp_log(XPP_LOG_INFO, "\n The commands are: \n");
 xpp_log(XPP_LOG_INFO, " P(arameter) -- declare parameters <name1>=<value1>,<name2>=<value2>,...\n");
 xpp_log(XPP_LOG_INFO, " F(ixed)     -- declare fixed variables\n");
 xpp_log(XPP_LOG_INFO, " V(ariables) -- declare ode variables \n");
 xpp_log(XPP_LOG_INFO, " U(ser)      -- declare user functions <name> <nargs> <formula>\n");
 xpp_log(XPP_LOG_INFO, " C(hange)    -- change option file   <filename>\n");
 xpp_log(XPP_LOG_INFO, " O(de)       -- declare RHS for equations\n");
 xpp_log(XPP_LOG_INFO, " D(one)      -- finished compiling formula\n");
 xpp_log(XPP_LOG_INFO, " H(elp)      -- this menu                 \n");
 xpp_log(XPP_LOG_INFO, " S(ymbols)   -- Valid functions and symbols\n");
 xpp_log(XPP_LOG_INFO, " I(ntegral)  -- rhs for integral eqn\n");
 xpp_log(XPP_LOG_INFO, " K(ernel)    -- declare kernel for integral eqns\n");
 xpp_log(XPP_LOG_INFO, " T(able)     -- lookup table\n");
 xpp_log(XPP_LOG_INFO, " A(ux)       -- name auxiliary variable\n");
 xpp_log(XPP_LOG_INFO, " N(umbers)   --  hidden parameters\n");
 xpp_log(XPP_LOG_INFO, " M(arkov)    --  Markov variables \n");
 xpp_log(XPP_LOG_INFO, " W(iener)    -- Wiener parameter \n");
 xpp_log(XPP_LOG_INFO, "_________________________________________________________________________\n");

}

void show_syms()
{
 xpp_log(XPP_LOG_INFO, "(    ,    )    +    -      *    ^    **    / \n");
 xpp_log(XPP_LOG_INFO, "sin  cos  tan  atan  atan2 acos asin\n");
 xpp_log(XPP_LOG_INFO, "exp  ln   log  log10 tanh  cosh sinh \n");
 xpp_log(XPP_LOG_INFO, "max  min  heav flr   mod   sign sqrt \n");
 xpp_log(XPP_LOG_INFO, "t    pi   ran  \n");
}

/* ram: do I need to strip the name of any whitespace? */
void take_apart(const char *bob, double *value, char *name)
{
 int k,l;
 l=strlen(bob);
 k=strcspn(bob,"=");
 if(k==l)
 {
  *value=0.0;
  strcpy_trim(name,bob);
  }
  else
  {
  strncpy_trim(name,bob,k);
  name[k]='\0';
  /* the number after the '=', whatever its length */
  *value=atof(bob+k+1);
  }
}

char *get_first(char *string, const char *src)
{
 char *ptr;
 ptr=strtok(string,src);
 return(ptr);
}
char *get_next(const char *src)
{
 char *ptr;
 ptr=strtok(NULL,src);
 return(ptr);
}

void find_ker(char *string, int *alt)   /* this extracts the integral operators from the string */ 
{
  /* int[mu]{form} (or int{form}) becomes the kernel's name, K##n */
  std::string newstr,form;
  double mu=0.0;
  bool fflag=false;
  int i=0;
  int n=strlen(string);
  char ch;
  *alt=0;
  while(i<n){
    ch=string[i];
    if(ch=='['){
      newstr.resize(newstr.size()>=3?newstr.size()-3:0); /* the "int" */
      std::string num;
      i++;
      while((ch=string[i])!=']'&&ch!=0){
	num+=ch;
	i++;
      }
      mu=atof(num.c_str());
      fflag=true;
      *alt=1;
      form.clear();
      i+=2;
      continue;
    }
    if(ch=='{'){
      newstr.resize(newstr.size()>=3?newstr.size()-3:0); /* the "int" */
      fflag=true;
      form.clear();
      *alt=1;
      i++;
      continue;
    }
    if(ch=='}'){
      std::string name=xpp::format("K##{}",NKernel);
      xpp::log(XPP_LOG_DEBUG, "Kernel mu={:f} {} = {} \n",mu,name,form);
      if(add_kernel(name.c_str(),mu,form.c_str()))exit(0);
      newstr+=name;
      mu=0.0;
      form.clear();
      fflag=false;
      i++;
      continue;
    }
    if(fflag)
      form+=ch;
    else
      newstr+=ch;
    i++;
  }
  /* string is rewritten in place: newstr is never longer (every branch
     that appends to it consumes at least as much of string) */
  size_t len=newstr.copy(string,n);
  string[len]=0;

}

void clrscr()
{
 if(system("clear")){}
 }




/***   remove this for full PP   ***/




/*   This is the new improved parser for input files.
     It is much more natural.  The format is as follows:

# comments    
par  name=val, ....
init name=val,...
number name=value, ...
wiener name,..
table name ...
markov name #states (replaces m r)
{ }  ..... { }
.
.
{ }  ..... { } 
options filename
aux name = expression
bndry ....
global ...
special name=conv(....)
special name=sparse(...)

u' = expression    \
                    ----   Differential equations (replaces o v)
du/dt = expression /     

u(t+1) = expression >--- Difference equation   (replace o v)  

u(t) = expression with int{} or int[]  <--- volterra equation (replaces i v)

f(x,y,...) = expression >----   function (replaces u)

u = expression>---  fixed  (replaces f o)

u(0) = value >---  initial data (replaces v, init is also OK ) 

*/







/*
   XPP INTERNALS DEMAND THE FOLLOWING ORDER CONVENTION:

   external names :  ODES MARKOV AUXILLIARY (uvar_names)
   internal names :  ODES FIXED MARKOV  (variables)
   internal formula: ODES FIXED AUXILLIARY (my_ode)
   external formula: odes markov auxilliary (ode_names)

   NODE = #ode variables
   NMarkov = # Markov variables
   NAux = # named auxiliary variables
   NEQ = ode+naux   --> plotted quantities
  
   my_ode[] <---  formulas
   ode_names[] <---- "rhs"
   uvar_names[] <----\  
   aux_names[]  <----/ external names

   New parser reads in each line storing it in model_lines
   if it is a markov (the only truly multiline command) then it
   ** immediately ** reads in the markov stuff
 
   It makes free use of "compiler"  in the old parser by
   sending it new strings
 
   On the first pass it does nothing except markov stuff
   On the second pass it imitates an ode file doing things in the
   "correct" order

    Only functions have changed syntax ...

*/

/* "#include file": the file's name, blanks removed, into nf */
static bool if_include_file(const char *old, std::string &nf)
{
  std::string_view s(old);
  if(!s.starts_with("#include"))return false;
  size_t blank=s.find(' ');
  if(blank==std::string_view::npos)return false;
  nf=s.substr(blank+1);
  de_space(nf.data());
  nf.resize(strlen(nf.c_str()));
  return true;
}

int if_end_include(const char *old)
{
  if (IN_INCLUDED_FILE>0)
  {
  	if(strncmp(old,"#done",5)==0)return 1;
  	if(strncmp(old,"done",4)==0)return 1;
	/*Note that feof termination of an included file
	 is also possible but that condition is checked 
	elsewhere (currently near the bottom of do_new_parser)
	*/
  }
  return 0;
}

void count_object(int type)
{
  switch(type){
  case ODE:
  case MAP:
    NUMODES++;
    break;
  case FIXED:
    NUMFIX++;
    break;
  case VEQ:
    NUMVOLT++;
    break;
  case MARKOV_VAR:
    NUMMARK++;
    break;
  case DERIVE_PAR:
  case PAR_AM:
    NUMPARAM++;
    break;
  case SOL_VAR:
    NUMSOL++;
    break;
  
  }



}

static int parse_model(FILE *fp, const char *first, int nnn);

/* no exception crosses into C: the only one parse_model() can throw is
   std::bad_alloc, and running out of memory ends the program, as
   xpp_malloc() does */
int do_new_parser(FILE *fp, const char *first, int nnn)
{
  try {
    return parse_model(fp, first, nnn);
  } catch (const std::bad_alloc &) {
    xpp_log(XPP_LOG_ERROR, "out of memory reading %s\n", first);
    exit(1);
  }
}

static int parse_model(FILE *fp, const char *first, int nnn)
{
 VAR_INFO v;
 std::vector<std::string> strings; /* this line, or a for loop's lines */
 int ns;
 int done=0,start=0,i0,i1,i2,istates;
 int jj1=0,jj2=0,jj,notdone=1,jjsgn=1;
 std::string name;
 int nstates=0;
 std::string newfile;
 FILE *fnew;
 /* the line read, with its array range worked out, and one of its
    strings with its subscripts worked out (parse_a_string edits it in
    place, never longer) */
 std::string old,newstr,big;
 /* a Markov line's states as read and with their subscripts worked out */
 std::vector<std::string> markov_states,markov_states2;
 char *my_string;
 int is_array=0;
 if(nnn==0){init_varinfo();}
 while(notdone){
   strings.clear();
   if(start||nnn==1){
     read_a_line(fp,old);
/* plintf(" read line BVP_N=%d  \n",BVP_N); */

   }
   else {
        if(loadincludefile)
	{
		loadincludefile=0;/*Only do this once*/
		for (const std::string &inc : include_files)
		{
			fnew=fopen(inc.c_str(),"r");
      			if(fnew==NULL){
         		  xpp::log(XPP_LOG_ERROR, "Can't open include file <{}>\n",inc);
			  exit(-1);
			  /*continue;*/
       			} 
      			xpp::log(XPP_LOG_INFO, "Including {} \n",inc);
			IN_INCLUDED_FILE++;
       			do_new_parser(fnew,inc.c_str(),1);
       			fclose(fnew);
		}
       		/*continue;*/
	}
    	
     old=first; /* pass the first line ....  */
     start=1;
   }
   if (IN_INCLUDED_FILE > 0) 
    {
	    if (if_end_include(old.c_str()) || feof(fp))
	    {
	    	xpp_log(XPP_LOG_INFO, "Completed include of file %s\n",first);
	    	IN_INCLUDED_FILE--;
	    	return 1; 
	    }
    }
    if(if_include_file(old.c_str(),newfile)){
      fnew=fopen(newfile.c_str(),"r");
      if(fnew==NULL){
         xpp::log(XPP_LOG_WARN, "Cant open include file <{}>\n",newfile);
         continue;
       }
       xpp::log(XPP_LOG_INFO, "Including {}...\n",newfile);
       IN_INCLUDED_FILE++;
       do_new_parser(fnew,newfile.c_str(),1);
       fclose(fnew);
       if (IN_INCLUDED_FILE > 0) 
       {
	       if (feof(fp))
	       {
       			/*plintf("We are at end of file now %d\n",IN_INCLUDED_FILE);*/
	       }
       }
       else
       {
             continue;
       }
    }
     
    /*    printf("calling search %s \n",old); */
    search_array(old.data(),newstr,&jj1,&jj2,&is_array);
   jj=jj1;
   jjsgn=1;
   if(jj2<jj1)jjsgn=-1;
  
   switch(is_array){
     case 0:  /*  not a for loop so */ 
     case 1:
           strings.assign(1,newstr);
           break;
      case 2: /*  a for loop, so we will ignore the first line */
	/* is_array=1; */
            while(1){ 
             read_a_line(fp,old);
             if(old[0]=='%')
               break;
             strings.push_back(old);
             if(strings.size()>255)break;
             }
             
            break;
       }  
         
              
            
  
   while(1){
      for(ns=0;ns<static_cast<int>(strings.size());ns++){
      subsk(strings[ns].c_str(),big,jj,is_array);
     
 
   done=parse_a_string(big.data(),&v);

   if(done==-1){
     xpp::log(XPP_LOG_ERROR, " Error in parsing {} \n",big.c_str());
     return -1;
   }
   if(done==1){
     if(v.type==COMMAND)strupr(v.lhs);
     if(v.type==COMMAND && v.lhs[0]=='G' && v.lhs[1]=='R') {
        my_string=get_first(v.rhs," ");
       name=my_string?my_string:"";
       my_string=get_next(" \n");
       if(my_string==NULL)
	 nstates=0;
       else
	 nstates=atoi(my_string);
       if(nstates<1){
	 xpp::log(XPP_LOG_ERROR, "Group {}  must have at least 1 part \n",name);
	 return -1;
       }
       xpp::log(XPP_LOG_INFO, "Group {} has {} parts\n",name,nstates);
       for(istates=0;istates<nstates;istates++){
	 read_a_line(fp,old);
	 xpp::log(XPP_LOG_DEBUG, "part {} is {} \n",istates,old);
       }
 
       v.type=GROUP;
     }
   /* check for Markov to get rid of extra lines */

     if(v.type==COMMAND && v.lhs[0]=='M' && v.lhs[1]=='A'){
       my_string=get_first(v.rhs," ");
       name=my_string?my_string:"";
       my_string=get_next(" \n");
       if(my_string==NULL)
	 nstates=0;
       else
	 nstates=atoi(my_string);
       if(nstates<2){
	 xpp::log(XPP_LOG_ERROR, "Markov variable {}  must have at least 2 states \n",name);
	 return -1;
       }
       if(name_too_long(name.c_str()))return -1;
       add_markov(nstates,name.c_str());
       if(jj==jj1) {  /* test to see if this is the first one */
	 markov_states.assign(nstates,std::string());
	 for(istates=0;istates<nstates;istates++){
           if(is_array==2)
	     markov_states[istates]=strings[ns+1+istates];
	   else
	     read_a_line(fp,markov_states[istates]);
	 }
       }

       /*  now we clean up these arrays */
       markov_states2.assign(nstates,std::string());
       std::vector<const char *> states(nstates);
       for(istates=0;istates<nstates;istates++){
	 subsk(markov_states[istates].c_str(),markov_states2[istates],jj,is_array);
	 states[istates]=markov_states2[istates].c_str();
       }

       build_markov(states.data(),name.c_str());
       v.type=MARKOV_VAR;
       XPP_FORMAT_TO_BUF(v.lhs,"{}",name);
       /* strcpy(v.rhs,save_eqn[nlin]); */
       XPP_FORMAT_TO_BUF(v.rhs,"{}","...many states.."); 
     }

   

        /* take care of special form for SOLVE-VARIABLE */      
          if(v.type==COMMAND && v.lhs[0]=='S' && v.lhs[1]=='O'){
           if(find_char(v.rhs,"=",0,&i1)<0){
             XPP_FORMAT_TO_BUF(v.lhs,"{}",v.rhs);
             XPP_FORMAT_TO_BUF(v.rhs,"{}","0");
            }
          else{
	  	
          strpiece(v.lhs,v.rhs,0,i1-1);
          big=v.rhs;
          strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
          }
          v.type=SOL_VAR;
          /*    plintf(" Its a sol-var! \n"); */

     }

   /* take care of special form for auxiliary */       
     if(v.type==COMMAND && v.lhs[0]=='A' && v.lhs[1]=='U'){
       if(find_char(v.rhs,"=",0,&i1)>=0){
       strpiece(v.lhs,v.rhs,0,i1-1);
       big=v.rhs;
       strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
       }
       v.type=AUX_VAR;
     }
   
     /* take care of special form for vector */      
     if(v.type==COMMAND && v.lhs[0]=='V' && v.lhs[1]=='E' && v.lhs[5]=='R')
     {
      if(find_char(v.rhs,"=",0,&i1)>=0){
       strpiece(v.lhs,v.rhs,0,i1-1);
       big=v.rhs;
       strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
       }
       v.type=VECTOR;


     }
        /* take care of special form for special */      
     if(v.type==COMMAND && v.lhs[0]=='S'&&v.lhs[1]=='P'&&v.lhs[5]=='A'){
       if(find_char(v.rhs,"=",0,&i1)>=0){
       strpiece(v.lhs,v.rhs,0,i1-1);
       big=v.rhs;
       strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
       }
       v.type=SPEC_FUN;
     }

/*   import-export to external C program   */
     if(v.type==COMMAND && v.lhs[0]=='E' && v.lhs[1]=='X'){
       v.type=EXPORT;
       if(find_char(v.rhs,"}",0,&i1)>=0){
       strpiece(v.lhs,v.rhs,0,i1);
       big=v.rhs;
       strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
       }

    }
   
/*  ONLY save options  */
    
    if(v.type==COMMAND && v.lhs[0]=='O' && v.lhs[1]=='N')
    {
     
      break_up_list(v.rhs);
      v.type=ONLY;
     }

 /*  forced integral equation form */
     if(v.type==COMMAND && v.lhs[0]=='V'){
       if(find_char(v.rhs,"=",0,&i1)>=0){
       strpiece(v.lhs,v.rhs,0,i1-1);
       big=v.rhs;
       strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
       }
       v.type=VEQ;
     }
    /* take care of tables   */

     if(v.type==COMMAND && v.lhs[0]=='T' && v.lhs[1]=='A'){
      i0=0;
      next_nonspace(v.rhs,i0,&i1);
      i0=i1;
      i2=find_char(v.rhs," ",i0,&i1);
      if(i2!=0){
	xpp_log(XPP_LOG_WARN, " Illegal definition of table %s \n",v.rhs);
	exit(0);
      }
      strpiece(v.lhs,v.rhs,i0,i1-1);
      big=v.rhs;
      strpiece(v.rhs,big.c_str(),i1+1,static_cast<int>(big.size()));
      v.type=TABLE;
    }
  
     
    /* printf("v.lhs=%s v.rhs=%s v.type=%d v.args=%s\n",v.lhs,v.rhs,v.type,v.args);
   */
    add_varinfo(v.type,v.lhs,v.rhs,v.nargs,v.args);
    count_object(v.type);
      }
   } /* end loop for the strings */
   if(done==2)notdone=0;
   if(feof(fp))
   {
        /*if (IN_INCLUDED_FILE>0)
	{
		plintf("End of include file reached NOW \n");
		IN_INCLUDED_FILE--;
		return(1);
	}*/
   	notdone=0;
   }
   
   if(jj==jj2)break;

     jj+=jjsgn;

   }

   /* a Markov line's states (build_markov copied them) */
   markov_states.clear();
   markov_states2.clear();
 
     
 }
 compile_em();
 
 free_varinfo();
 /*  print_count_of_object(); */
 return 1;

}

void create_plot_list()
{
  int i,j=0,k;
  if(N_only==0)return;
  plotlist=static_cast<int *>(xpp_malloc(sizeof(int)*(N_only+1)));
  for(i=0;i<N_only;i++){
    find_variable(onlylist[i],&k);
    if(k>=0){
      plotlist[j]=k;
      j++;
    }
    N_plist=j;
  }
    
}

void add_only(const char *s)
{
  if(strlen(s)<1)return;
  if(N_only>=MAXONLY)return;
  onlylist[N_only]=xpp_strdup(s);

  N_only++;
}

void break_up_list(const char *rhs)
{
  /* the names between blanks and commas */
  std::string s;
  for(const char *c=rhs;*c;c++){
    if(*c==' '||*c==','){
      add_only(s.c_str());
      s.clear();
    }
    else
      s+=*c;
  }
  add_only(s.c_str());
}


int find_the_name(char list[][MAXVNAM], int n, const char *name)
{
  int i;

  for(i=0;i<n;i++){

    if(strcmp(list[i],name)==0)
      return(i);
  }
  return(-1);
}
 
void compile_em() /* Now we try to keep track of markov, fixed, etc as
		well as their names  */
{
 VAR_INFO *v;
 /* static: together they are too big for a thread's stack */
 static char vnames[MAXODE1][MAXVNAM],fnames[MAXODE1][MAXVNAM],anames[MAXODE1][MAXVNAM];
 static char mnames[MAXODE1][MAXVNAM];
 double z,xlo,xhi;
 char tmp[MAXEXPLEN],big[MAXEXPLEN],formula[MAXEXPLEN],*my_string,*junk,*ptr,name[MAXEXPLEN];
 int nmark=0,nfix=0,naux=0,nvar=0,nn,alt,in,i,ntab=0,nufun=0;
 int in1,in2,iflag;
 int fon;
 FILE *fp=NULL;

 /* On this first pass through, all the variable names
    are kept as well as fixed declarations, boundary conds,
    and parameters, functions and tables.  Once this pass is
    completed all the names will be known to the compiler.
 */
 for(VAR_INFO &line : model_lines)
   {
     v=&line;
      

    if(v->type==COMMAND && v->lhs[0]=='P'){
      snprintf(big,sizeof(big),"par %.1017s \n",v->rhs);
      compiler(big,fp);
    }
    if(v->type==COMMAND && v->lhs[0]=='W'){
      snprintf(big,sizeof(big),"wie %.1017s \n",v->rhs);
      compiler(big,fp);
    }
    if(v->type==COMMAND && v->lhs[0]=='N'){
      snprintf(big,sizeof(big),"num %.1017s \n",v->rhs);
      compiler(big,fp);
    }
    if(v->type==COMMAND && v->lhs[0]=='O'){
     snprintf(big,sizeof(big),"c %.1019s \n",v->rhs);
     compiler(big,fp);

    }
    if(v->type==COMMAND && v->lhs[0]=='S' && v->lhs[1]=='E'){
      snprintf(big,sizeof(big),"x %.1020s\n",v->rhs);
      compiler(big,fp);
    }

    if(v->type==COMMAND && v->lhs[0]=='B'){
      snprintf(big,sizeof(big),"b %.1019s \n",v->rhs);
      compiler(big,fp);
    }
    if(v->type==COMMAND && v->lhs[0]=='G'){
      snprintf(big,sizeof(big),"g %.1019s \n",v->rhs);
      compiler(big,fp);
    }
    if(v->type==MAP||v->type==ODE||v->type==VEQ){
      convert(v->lhs,tmp);
      if(name_too_long(tmp))exit(0);
      if(find_the_name(vnames,nvar,tmp)<0){
	XPP_FORMAT_TO_BUF(vnames[nvar],"{}",tmp);
	nvar++;
      }
      else
	{
	  xpp_log(XPP_LOG_ERROR, " %s is a duplicate name \n",tmp);
	  exit(0);
	}
      
      /*  plintf("%d:%s = %s \n",nvar-1,vnames[nvar-1],v->rhs);   */
    }

    if(v->type==MARKOV_VAR){
      convert(v->lhs,tmp);
      if(name_too_long(tmp))exit(0);
      if(find_the_name(mnames,nmark,tmp)<0){
	XPP_FORMAT_TO_BUF(mnames[nmark],"{}",tmp);
	nmark++;
      }
      
/*      plintf("%s = %s \n",mnames[nmark-1],v->rhs); */
    }
    if(v->type==EXPORT){
      add_export_list(v->lhs,v->rhs);
    }
    if(v->type==VECTOR){
      add_vectorizer_name(v->lhs,v->rhs);

    }
    if(v->type==SPEC_FUN){
      add_special_name(v->lhs,v->rhs);
      
	}
    if(v->type==SOL_VAR){
       if(add_svar(v->lhs,v->rhs)==1)
	 exit(0);
    }
       
    if(v->type==AUX_VAR){
      convert(v->lhs,tmp);
      if(name_too_long(tmp))exit(0);
      XPP_FORMAT_TO_BUF(anames[naux],"{}",tmp);
      naux++;
      xpp_log(XPP_LOG_INFO, "%s = %s \n",anames[naux-1],v->rhs); 
    }
    if(v->type==DERIVE_PAR){
      if(add_derived(v->lhs,v->rhs)==1)
	exit(0);
    }
    if(v->type==FIXED){
      fixinfo[nfix].name=xpp_strdup(v->lhs);
      fixinfo[nfix].value=xpp_strdup(v->rhs);
      convert(v->lhs,tmp);
      if(name_too_long(tmp))exit(0);
      XPP_FORMAT_TO_BUF(fnames[nfix],"{}",tmp);
      nfix++;
     xpp_log(XPP_LOG_INFO, "%s = %s \n",fnames[nfix-1],v->rhs); 
    }

    if(v->type==TABLE){
      convert(v->lhs,tmp);
      if(add_table_name(ntab,tmp)==1){
	xpp_log(XPP_LOG_ERROR, " %s is duplicate name \n", tmp);
	exit(0);
      }
      xpp_log(XPP_LOG_DEBUG, "added name %d\n",ntab);
      ntab++;
    }
    
    if(v->type==FUNCTION){
      convert(v->lhs,tmp);
      if(add_ufun_name(tmp,nufun,v->nargs)==1){
	xpp_log(XPP_LOG_ERROR, "Duplicate name or too many functions for %s \n",tmp);
	exit(0);
      }
    
      nufun++;
    }
   }
 
 /*  plintf(" Found\n %d variables\n %d markov\n %d fixed\n %d aux\n %d fun \n %d tab\n ",
     nvar,nmark,nfix,naux,nufun,ntab); */


 /* now we add all the names of the variables and the 
    fixed stuff 
 */
 for(i=0;i<nvar;i++){
      if(add_var(vnames[i],0.0)){
	xpp_log(XPP_LOG_ERROR, " Duplicate name %s \n",vnames[i]);
	exit(0);
      }
      XPP_FORMAT_TO_BUF(uvar_names[i],"{}",vnames[i]);
      last_ic[i]=0.0;
      default_ic[i]=0.0;
    }
 for(i=0;i<nfix;i++){
   if(add_var(fnames[i],0.0)){
	xpp_log(XPP_LOG_ERROR, " Duplicate name %s \n",fnames[i]);
	exit(0);
      }
 }
 for(i=0;i<nmark;i++){
   if(add_var(mnames[i],0.0)){
	xpp_log(XPP_LOG_ERROR, " Duplicate name %s \n",mnames[i]);
	exit(0);
      }
   XPP_FORMAT_TO_BUF(uvar_names[i+nvar],"{}",mnames[i]);
   last_ic[i+nvar]=0.0;
   default_ic[i+nvar]=0.0;
 }
 for(i=0;i<naux;i++)
   XPP_FORMAT_TO_BUF(aux_names[i],"{}",anames[i]);
 add_svar_names();
 
 
/* NODE = nvars ; Naux = naux ; NEQ = NODE+NMarkov+Naux ; FIX_VAR = nfix; */

 IN_VARS=nvar;
 Naux=naux;
 NEQ=nvar+NMarkov+Naux;
 FIX_VAR=nfix;
 NTable=ntab;
 NFUN=nufun;
 /* plintf(" IN_VARS=%d\n",IN_VARS); */

/* Reset all this stuff so we align the indices correctly */

 nvar=0;
 naux=0;
 ntab=0;
 nufun=0;
 nfix=0; 
 nmark=0;


 for(VAR_INFO &line : model_lines)
   {
     v=&line;
     
     if(v->type==COMMAND && v->lhs[0]=='I'){
       snprintf(big,sizeof(big),"i %.1019s \n",v->rhs);
       ptr=big;
       junk=get_first(ptr," ,");
       if (junk == NULL)
       {
       	/*No more tokens.  Should this throw an error?*/
       }
      advance_past_first_word(&ptr);
      for(std::optional<std::string> tok;(tok=get_next2(&ptr));)
	{
	  take_apart(tok->c_str(),&z,name);
	   convert(name,tmp);
	   in=find_the_name(vnames,IN_VARS,tmp);
	   if(in>=0){
	     last_ic[in]=z;
	     default_ic[in]=z;
	     set_val(tmp,z);
	     xpp_log(XPP_LOG_INFO, " Initial %s(0)=%g\n",tmp,z);
	   }
	   else {
	     in=find_the_name(mnames,NMarkov,tmp);
	     if(in>=0){
	       last_ic[in+IN_VARS]=z;
               default_ic[in+IN_VARS]=z;
	       set_val(tmp,z);
	       xpp_log(XPP_LOG_INFO, " Markov %s(0)=%g\n",tmp,z);
	     }
	     else
	       {
		 xpp_log(XPP_LOG_ERROR, "In initial value statement no variable %s \n",
			tmp);
		 exit(0);
	       }
	   }
	 } /* end take apart */
     }  /* end  init  command    */
     if(v->type==IC){
       convert(v->lhs,tmp);
       fon=formula_or_number(v->rhs,&z);
       	
	  if(fon==1){

	 if(v->rhs[0]=='-'&&(isdigit(v->rhs[1])||(v->rhs[1]=='.')))
	   {
        
	     z=atof(v->rhs);
	     
	   }
       }
	 
       in=find_the_name(vnames,IN_VARS,tmp);
       if(in>=0){
	 last_ic[in]=z;
         default_ic[in]=z;
	 set_val(tmp,z);
	 /* if(fon==1) */
	   XPP_FORMAT_TO_BUF(delay_string[in],"{}",v->rhs);
	   
	 xpp_log(XPP_LOG_INFO, " Initial %s(0)=%s\n",tmp,v->rhs);
       }
       else {
	 in=find_the_name(mnames,NMarkov,tmp);
	 if(in>=0){
	   last_ic[in+IN_VARS]=z;
           default_ic[in+IN_VARS]=z;
	   set_val(tmp,z);
	   xpp_log(XPP_LOG_INFO, " Markov %s(0)=%g\n",tmp,z);
	 }
	 else
	   {
	     xpp_log(XPP_LOG_ERROR, "In initial value statement no variable %s \n",
		    tmp);
	     exit(0);
	   }
       }
     } /* end IC stuff  */

 /*   all that is left is the right-hand sides !!   */
     iflag=0;
     switch(v->type){
     case VEQ:
       iflag=1;
     case ODE:
     case MAP:
       EqType[nvar]=iflag;
       ode_names[nvar]=xpp_strdup(v->rhs);
       my_ode[nvar]=static_cast<int *>(xpp_malloc(MAXEXPLEN*sizeof(int)));
       find_ker(v->rhs,&alt);
       if(add_expr(v->rhs,my_ode[nvar],&leng[nvar])){
	 xpp_log(XPP_LOG_ERROR, "ERROR compiling %s' \n",v->lhs);
	 exit(0);
       }
       /* fpr_command(my_ode[nvar]); */
       if(v->type==MAP){
	 xpp_log(XPP_LOG_INFO, "%s(t+1)=%s\n",v->lhs,v->rhs);
	 is_a_map=1;
       }
       if(v->type==VEQ)
	 xpp_log(XPP_LOG_INFO, "%s(t)=%s\n",v->lhs,v->rhs);
       if(v->type==ODE)
	 xpp_log(XPP_LOG_INFO, "%d:d%s/dt=%s\n",nvar,v->lhs,v->rhs);
       nvar++;
       break;
      case FIXED:
       find_ker(v->rhs,&alt);
       my_ode[nfix+IN_VARS]=static_cast<int *>(xpp_malloc(MAXEXPLEN*sizeof(int)));
       if(add_expr(v->rhs,my_ode[nfix+IN_VARS],&leng[IN_VARS+nfix])!=0){
	 xpp_log(XPP_LOG_ERROR, " Error allocating or compiling %s\n",v->lhs);
	 exit(0);
       }
       nfix++;
       xpp_log(XPP_LOG_INFO, "%s=%s\n",v->lhs,v->rhs);
       break;
     case DAE:
       if(add_aeqn(v->rhs)==1)
	 exit(0);
       xpp_log(XPP_LOG_INFO, " DAE eqn: %s=0 \n",v->rhs);
       break;
	 
     case  AUX_VAR:
       in1=IN_VARS+NMarkov+naux;
       in2=IN_VARS+FIX_VAR+naux;
       ode_names[in1]=xpp_strdup(v->rhs);
       my_ode[in2]=static_cast<int *>(xpp_malloc(MAXEXPLEN*sizeof(int)));
       if(add_expr(v->rhs,my_ode[in2],&leng[in2])){
	 xpp_log(XPP_LOG_ERROR, "ERROR compiling %s \n",v->lhs);
	 exit(0);
       }
       naux++;
       xpp_log(XPP_LOG_INFO, "%s=%s\n",v->lhs,v->rhs);
       break;
     case VECTOR:
       if(add_vectorizer(v->lhs,v->rhs)==0){
	 xpp_log(XPP_LOG_ERROR, " Illegal vector  %s \n",v->rhs);
	 exit(0);
       }

       break;
     case SPEC_FUN:
       if(add_spec_fun(v->lhs,v->rhs)==0){
	 xpp_log(XPP_LOG_ERROR, " Illegal special function %s \n",v->rhs);
	 exit(0);
       }
       break;
     case MARKOV_VAR:
       ode_names[IN_VARS+nmark]=xpp_strdup(v->rhs);
       nmark++;
       xpp_log(XPP_LOG_INFO, "%s: %s",v->lhs,v->rhs);
       break;
     case  FUNCTION:
       if(add_ufun_new(nufun,v->nargs,v->rhs,v->args)!=0){
	 xpp_log(XPP_LOG_ERROR, " Function %s messed up \n",v->lhs);
	 exit(0);
       }
       nufun++;
       xpp_log(XPP_LOG_INFO, "%s(%s",v->lhs,v->args[0]);
       for(in=1;in<v->nargs;in++)
	 xpp_log(XPP_LOG_INFO, ",%s",v->args[in]);
       xpp_log(XPP_LOG_INFO, ")=%s\n",v->rhs);
       break;
     
     case TABLE:
       snprintf(big,sizeof(big),"t %.509s %.509s ",v->lhs,v->rhs);
       ptr=big;
       junk=get_first(ptr," ,");
       my_string=get_next(" ");
       my_string=get_next(" \n");
       if(my_string[0]=='%') {
	 xpp_log(XPP_LOG_INFO, " Function form of table....\n");
	 my_string=get_next(" ");
	 nn=atoi(my_string);
	 my_string=get_next(" ");
	 xlo=atof(my_string);
	 my_string=get_next(" ");
	 xhi=atof(my_string);
	 my_string=get_next("\n");
	 XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	 xpp_log(XPP_LOG_INFO, " %s has %d pts from %f to %f = %s\n",
		v->lhs,nn,xlo,xhi,formula);
	 /* plintf(" ntab = %d\n",ntab); */
	 if(add_form_table(ntab,nn,xlo,xhi,formula)){
	   xpp_log(XPP_LOG_ERROR, "ERROR computing %s\n",v->lhs);
	   exit(0);
	 }
	 ntab++;
       }
       else 
	 if(my_string[0]=='@'){
	   xpp_log(XPP_LOG_INFO, " Two-dimensional array: \n ");
	   my_string=get_next(" ");
	   XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	   xpp_log(XPP_LOG_INFO, " %s = %s \n",name,formula);
	   if(add_2d_table(name,formula)){
	     xpp_log(XPP_LOG_ERROR, "ERROR at line %d\n",NLINES);
	     exit(0);
	   }
	 }
	 else
	   {
	     XPP_FORMAT_TO_BUF(formula,"{}",my_string);
	     xpp_log(XPP_LOG_INFO, "Lookup table %s = %s \n",v->lhs,formula);
	     
	     if(add_file_table(ntab,formula)){
	       xpp_log(XPP_LOG_ERROR, "ERROR computing %s",v->lhs);
	       exit(0);
	     }
	     ntab++;
	   }
       break;
     }
   }
 if(compile_derived()==1)
   exit(0);
 if(compile_svars()==1)
   exit(0);
 evaluate_derived();
 do_export_list();  
 xpp_log(XPP_LOG_INFO, " All formulas are valid!!\n");
 NODE=nvar+naux+nfix;
 xpp_log(XPP_LOG_INFO, " nvar=%d naux=%d nfix=%d nmark=%d NEQ=%d NODE=%d \n",
	nvar,naux,nfix,nmark,NEQ,NODE);
 
}

/* this code checks if the right-hand side for an initial
   condition is a formula (for delays) or a number
*/
int formula_or_number(const char *expr,double *z)
{
  std::array<char,40> num{}; /* do_num's 40 bytes */
  int flag,i=0;
  int olderr=ERROUT;
  ERROUT=0;
  *z=0.0; /* initial it to 0 */
  /* convert only drops blanks: never longer than expr */
  std::string form(expr);
  convert(expr,form.data());
  form.resize(strlen(form.c_str()));
  flag=do_num(form.c_str(),num.data(),z,&i);
  if(i<static_cast<int>(form.size()))flag=1;
  ERROUT=olderr;
  if(flag==0)
    return 0; /* 0 is a number */
  return 1; /* 1 is a formula */
}
void strpiece(char *dest, const char *src, int i0, int ie)
{
  int i;
  for(i=i0;i<=ie;i++)
    dest[i-i0]=src[i];
  dest[ie-i0+1]=0;
}

int parse_a_string(char *s1, VAR_INFO *v)
{
  int i0=0,i1,i2,i3;
  char lhs[MAXEXPLEN],rhs[MAXEXPLEN],args[MAXARG][NAMLEN+1];
  int i,type,type2;
  int narg=0;
  int n1=strlen(s1)-1;
  char s1old[MAXEXPLEN];
  char ch;
  if(s1[0]=='"'){
    add_comment(s1);
    return 0;
  }
  if(s1[0]=='@') {
    /*    printf("internopts from parse string\n"); */
    stor_internopts(s1);
    return 0;
  }
    remove_blanks(s1); 

  XPP_FORMAT_TO_BUF(s1old,"{}",s1);
  strupr(s1);
  /*   plintf(" <%s> \n",s1);   */
  if(strlen(s1)<1){
 /*   plintf(" Empty line \n"); */
    return 0;
  }
  if(s1[0]=='0'&&s1[1]=='='){ /* ||(s1[1]==' '&&s1[2]=='='))) */
    /* plintf("DAE --- \n");  */
   type2=DAE;
   XPP_SPRINTF(lhs,"0=");
   strpiece(rhs,s1,2,n1);
    v->type=type2;
  XPP_FORMAT_TO_BUF(v->lhs,"{}",lhs);
  XPP_FORMAT_TO_BUF(v->rhs,"{}",rhs);
  goto good_type;
  }
  if(s1[0]=='#'){
  /*  plintf("Comment! \n"); */
    return 0;
  }

  type=find_char(s1," =/'(",i0,&i1);
  switch(type){
  case 0:
    i0=i1;
    ch=static_cast<char>(next_nonspace(s1,i0,&i2));
    switch(ch){
    case '=' :
      if(s1[0]=='!'){
	strpiece(lhs,s1,1,i1-1);
	strpiece(rhs,s1,i2+1,n1);
	type2=DERIVE_PAR;
	break;
      }
      strpiece(lhs,s1,0,i1-1);
      strpiece(rhs,s1,i2+1,n1);
      type2=FIXED;
      break;
    default:
      type2=COMMAND;
      strpiece(lhs,s1,0,i1-1);
      strpiece(rhs,s1old,i2,n1);
      break;
    }
    break;
  case 1:
    if(s1[0]=='!'){
      strpiece(lhs,s1,1,i1-1);
      strpiece(rhs,s1,i1+1,n1);
      type2=DERIVE_PAR;
      break;
    }
    
    type2=FIXED;
    strpiece(lhs,s1,0,i1-1);
    strpiece(rhs,s1,i1+1,n1);
    break;
  case 2:
    if(s1[0]!='D')return -1;
    if(extract_ode(s1,&i2,i1)){
      strpiece(lhs,s1,1,i1-1);
      strpiece(rhs,s1,i2,n1);
      type2=ODE;
    }
    else
      return -1;
    break;
  case 3:
    if(extract_ode(s1,&i2,i1)){
      strpiece(lhs,s1,0,i1-1);
      strpiece(rhs,s1,i2,n1);
      type2=ODE;
    }
    else
      return -1;
    break;
    
  case 4:
    i0=i1;
    if(strparse(s1,"T+1)=",i0,&i2)){
      type2=MAP;
      is_a_map=1;
      strpiece(lhs,s1,0,i1-1);
      strpiece(rhs,s1,i2,n1);
      break;
    }
    if(strparse(s1,"(0)=",i0-1,&i2)){

      type2=IC;
      strpiece(lhs,s1,0,i1-1);
      strpiece(rhs,s1,i2,n1);
      break;
     }
    if(strparse(s1,"T)=",i0,&i2)){
  
      if(strparse(s1,"INT{",0,&i3)==1||
	 strparse(s1,"INT[",0,&i3)==1){
	type2=VEQ;
	strpiece(lhs,s1,0,i1-1);
	strpiece(rhs,s1,i2,n1);
	break;
      }
      else {
	type2=FUNCTION;
        if(extract_args(s1,i0+1,&i2,&narg,args)==0)return -1;
	strpiece(lhs,s1,0,i0-1);
	strpiece(rhs,s1,i2,n1);
	break;
      }
    }
    i0++;
    if(extract_args(s1,i0,&i2,&narg,args)==0)return -1;
    type2=FUNCTION;
    strpiece(lhs,s1,0,i0-2);
    strpiece(rhs,s1,i2,n1);
    break;
  default: 
    return -1;
  }

good_type:
  v->type=type2;
  XPP_FORMAT_TO_BUF(v->lhs,"{}",lhs);
  XPP_FORMAT_TO_BUF(v->rhs,"{}",rhs);
  v->nargs=narg;
  for(i=0;i<narg;i++)
    XPP_FORMAT_TO_BUF(v->args[i],"{}",args[i]);

  /* plintf("type=%d type2 = %d : %s = %s \n",type,v->type,v->lhs,v->rhs); 
   if(type2==FUNCTION){
    plintf(" %d args \n",v->nargs); 
     for(i=0;i<narg;i++)
       plintf("(%s) ",v->args[i]);
    plintf("\n");
    
  }
  */
  
  if(lhs[0]=='D'&&type2==COMMAND)
    return 2;
  return 1;
}

/* a new model: no lines yet */
void init_varinfo()
{
  model_lines.clear();
}

void add_varinfo(int type, const char *lhs, const char *rhs, int nargs, char args[MAXARG][NAMLEN+1])
{
  VAR_INFO v{};
  int i;
  v.type=type;
  v.nargs=nargs;
  XPP_FORMAT_TO_BUF(v.lhs,"{}",lhs);
  XPP_FORMAT_TO_BUF(v.rhs,"{}",rhs);
  for(i=0;i<nargs;i++)
    XPP_FORMAT_TO_BUF(v.args[i],"{}",args[i]);
  try {
    model_lines.push_back(v);
  } catch (const std::bad_alloc &) { /* no exception crosses into C */
    xpp_log(XPP_LOG_ERROR, "out of memory: the model's line %d\n", static_cast<int>(model_lines.size()) + 1);
    exit(1);
  }
}

/* compiled: the lines and their memory go */
void free_varinfo()
{
  std::vector<VAR_INFO>().swap(model_lines);
}


int extract_ode(const char *s1, int *ie, int i1)  /* name is char 1-i1  ie is start of rhs */
{
  int i=0,n=strlen(s1);
  
  i=i1;
  while(i<n){
    if(s1[i]=='='){
      *ie=i+1;
      return 1;
    }
    i++;
  }
  return 0;
}

int strparse(const char *s1, const char *s2, int i0, int *i1)
{
  int i=i0;
  int n=strlen(s1);
  int m=strlen(s2);
  int j=0;
  char ch;
  int start=0;

  while(i<n){
    ch=s1[i];
    if(start==1){

      if(ch==s2[j]|| ch==' '){
        if(ch==s2[j])j++;
        i++;
	if(j==m){
	  *i1=i;
	  return(1);
	}
      }
      else
	{
	  start=0;
	  j=0;
	}
    } 
    else /* just starting */
      {
         
	if(ch==s2[0]){
	  j++;
	  i++;
	  start=1;
	  if(j==m){  /* only one char */
	    *i1=i;
	    return(1);
	  }
	}
      else
	i++;
      }
	
  }
  return(0);
}

int extract_args(const char *s1, int i0, int *ie, int *narg, char args[MAXARG][NAMLEN+1])
{
  int k,i=i0,n=strlen(s1);
  int type,na=0,i1;
  while(i<n){
    type=find_char(s1,",)",i,&i1);
    if(type<0)break;
    if(na>=MAXARG){
      xpp_log(XPP_LOG_ERROR, "More than %d arguments\n",MAXARG);
      return 0;
    }
    if(i1-i>NAMLEN){
      xpp_log(XPP_LOG_ERROR, "Argument name longer than %d characters\n",NAMLEN);
      return 0;
    }
    if(type==0){
      for(k=i;k<i1;k++)
	args[na][k-i]=s1[k];
      args[na][i1-i]=0;
      na++;
      i=i1+1;
    }
    if(type==1){
      for(k=i;k<i1;k++)
	args[na][k-i]=s1[k];
      args[na][i1-i]=0;
      na++;
      i=i1+1;
      find_char(s1,"=",i,&i1);
      *ie=i1+1;
      *narg=na;
      return 1;
    }
  }
  return(0);
}
      
    
    
int find_char(const char *s1, const char *s2, int i0, int *i1)
{
  int m=strlen(s2),n=strlen(s1);
  int i=i0;
  char ch;
  int j;
  while(i<n){
    ch=s1[i];
    for(j=0;j<m;j++){
      if(ch==s2[j]){
	*i1=i;
	return(j);
      }
    }
    i++;
  }
  return(-1);
}

int next_nonspace(const char *s1, int i0, int *i1)
{
  int i=i0;
  int n=strlen(s1);
  char ch;
  *i1=n-1;
  while(i<n){
    ch=s1[i];
    if(ch!=' '){
      *i1=i;
      return(static_cast<int>(ch));
    }
    i++;
  }
  return(-1);
}

/* removes starting blanks from s  */
void remove_blanks(char *s1)
{
  int i=0,n=strlen(s1),l;
  int j;
  char ch;
  while(i<n){
    ch=s1[i];
    if(isspace(ch))
      i++;
    else
      break;
  }
  if(i==n) s1[0]=0;
  else {
    l=n-i;
    for(j=0;j<l;j++)
      s1[j]=s1[j+i];
    s1[l]=0;
  }
 
}
      

/* fgets without its size: the next line of fp with its '\n', whatever
   its length ("" at the end of the file) */
static std::string read_raw_line(FILE *fp)
{
  std::string line;
  int c;
  while((c=getc(fp))!=EOF){
    line+=static_cast<char>(c);
    if(c=='\n')break;
  }
  return line;
}

/* keeps one line of the model's source in save_eqn (C text: strip_saveqn
   and the front ends read it) */
static void save_line(const std::string &line)
{
  if (NLINES>=MAXLINES) {
    xpp_log(XPP_LOG_ERROR, "The model has more than %d lines\n", MAXLINES);
    exit(1);
  }
  save_eqn[NLINES++]=xpp_strdup(line.c_str());
}

/* The next logical line: a line ending in a backslash goes on in the
   next one (the text from the first backslash on is dropped); the
   line's end becomes a blank and one more blank follows it. */
void read_a_line(FILE *fp, std::string &s)
{
  bool more=true;
  s.clear();
  while(more){
    std::string temp=read_raw_line(fp);
    save_line(temp);
    size_t hat=temp.find('\\');
    more=hat!=std::string::npos;
    if(more)temp.resize(hat);
    s+=temp;
  }
  if(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.back()=' '; /* empty at the end of the file */
  s+=' ';
}


/* old with its array range x[i..j] made x[j] (i1, i2 the range; flag 1,
   or 2 for a %[i..j] for loop): newstr. 0 (newstr old) when the range
   is malformed. A line of initial data x[..](0)=... goes to
   extract_ic_data, which may rewrite old. */
int search_array(char *old, std::string &newstr, int *i1, int *i2, int *flag)
{
  int i,j;
  int ileft,iright;
  int n=strlen(old);
  std::string num1="0",num2="0";
  char ch,chp;
  ileft=n-1;
  iright=-1;
  *i1=0;
  *i2=0;
  *flag=0;
  if(old[0]=='#'||(n>0&&old[1]=='#')) {  /* check for comments */
    newstr=old;
    return 1;
  }
  if(check_if_ic(old)==1){
    extract_ic_data(old);
    newstr=old;
    return 1;
  }
  for(i=0;i<n;i++){
    ch=old[i];
    chp=old[i+1];
    if(ch=='.'&&chp=='.'){
      j=0;
      *flag=1;
      if(old[0]=='%')
	*flag=2;   /*   FOR LOOP CONSTRUCTION  */
      while(1){
	ch=old[i+j];
	if(ch=='['){
	  ileft=i+j;
	  num1.assign(old+i+j+1,old+i);
	  break;
	}
	j--;
	if((i+j)<=0){
	  *i1=0;
          *i2=0;
	  newstr=old;
          xpp_log(XPP_LOG_WARN, " Possible error in array %s -- ignoring it \n",old);
	  return(0); /* error in array  */
	}
      }
      j=2;
      while(1){
	ch=old[i+j];
	if(ch==']'){
	  iright=i+j;
	  num2.assign(old+i+2,old+i+j);
	  break;
	}
	j++;
	if((i+j)>=n) {
	  *i1=0;
          *i2=0;
	  newstr=old;
          xpp_log(XPP_LOG_WARN, " Possible error in array  %s -- ignoring it \n",old);
	  return(0); /* error again   */
	}
      }
    }
  }
  *i1=atoi(num1.c_str());
  *i2=atoi(num2.c_str());
  /* now we have the numbers and will get rid of the junk inbetween */
  newstr.assign(old,old+ileft+1);
  if(iright>0){
    newstr+='j';
    newstr.append(old+iright,old+n);
  }
  return 1;
}


int check_if_ic(const char *big)
{
  char c;
  int n=strlen(big);
  int j;
  j=0;
  while(1){
    c=big[j];
    if(c==']'){
      /*  plintf(" %c %c %c \n",big[j+1],big[j+2],big[j+3]); */
      if((big[j+1]=='(') && (big[j+2]=='0') && (big[j+3]==')')){
	return 1;
	
      }
    }
    j++;
    if(j>=n)break;
  }
  return 0;
}

int not_ker(const char *s, int i) /* returns 1 if string is not 'int[' */
{
  if(i<3)return 1;
  if(s[i-3]=='i'&&s[i-2]=='n'&&s[i-1]=='t')return 0;
  return 1;
}

int is_comment(const char *s)
{
  int n=strlen(s);
  int i=0;
  char c;
  while(1) {
    c=s[i];
    if(c=='#')return 1;
    if(isspace(c)){
      i++;
     
      if(i>=n)return 0;
    }
    else
      return 0;
  }
}
 
  
/* big with its subscripts worked out for index k: [n] becomes n, [j+n]
   k+n, [j-n] k-n, [j*n] k*n ([j] only in an array line, flag nonzero) */
void subsk(const char *big, std::string &newstr, int k, int flag)
{
  int n=strlen(big),i=0,add,isign,multflag=0;
  bool ok;
  char ch,chp;
  std::string num;
  newstr.clear();
  if(is_comment(big)){
    newstr=big;
    return;
  }
  /* the subscript's text runs to its ']' */
  auto unterminated=[big](){
    xpp_log(XPP_LOG_ERROR, "Error in %s The expression does not terminate. Perhaps a ] is missing.\n",big);
    exit(0);
  };
  while(i<n){
    ch=big[i];
    chp=big[i+1];
    if(ch=='['&&chp != 'j'&&not_ker(big,i)){
      ok=true;
      num.clear();
      i++;
      while(ok){
	if(i>=n)unterminated();
	ch=big[i];
	i++;
	if(ch==']'){
	  add=atoi(num.c_str());
	  newstr+=std::to_string(add);
	  ok=false;
	}
	else
	  num+=ch;
      }
    }
    else if(ch=='['&&chp=='j'){
      if(flag==0){
	xpp_log(XPP_LOG_WARN, " Illegal use of [j] at %s \n",big);
	exit(0);
      }
      num.clear();
      isign=1;
      i+=2;
      ok=true;
      while(ok){
	if(i>=n)unterminated();
	ch=big[i];
	switch(ch){
	case '+':
	  isign=1;
	  i++;
	  break;
	case '-':
	  isign=-1;
	  i++;
	  break;
	case '*':
	  i++;
	  isign=1;
	  multflag=1;
	  break;
	case ']':
	  i++;
	  if(multflag==0){
	    add=atoi(num.c_str())*isign+k;
	  }
	  else {
	    add=atoi(num.c_str())*k;
	    multflag=0;
	  }
	  newstr+=std::to_string(add);
	  ok=false;
	  break;
	default:
	  i++;
	  num+=ch;
	  break;
	}
      }
    }
    else {
      newstr+=ch;
      i++;
    }
  }
}



/* A " line: its text, or with {name=value,...} an action ("$ name=value
   ...") and the text after the braces ("* text") */
void add_comment(const char *s)
{
  if(n_comments>=MAXCOMMENTS)return;
  std::string_view line(s);
  size_t open=line.find('{');
  if(open==std::string_view::npos){
    comments[n_comments].text=xpp_strdup(line.empty()?"":s+1);
    comments[n_comments].aflag=0;
  }
  else {
    std::string action="$ ";
    size_t j1=open+1;
    for(size_t i=open+1;i<line.size();i++){
      char ch=line[i];
      if(ch==','){
        action+=' ';
        continue;
      }
      if(ch=='}'){
        action+=' ';
        j1=i+1;
        break;
      }
      action+=ch;
    }
    std::string text="* ";
    text+=line.substr(j1);
    /* COMMENT is C API: xpp_strdup'd text */
    comments[n_comments].text=xpp_strdup(text.c_str());
    comments[n_comments].action=xpp_strdup(action.c_str());
    comments[n_comments].aflag=1;
  }
 xpp_log(XPP_LOG_DEBUG, "text=%s \n",comments[n_comments].text);
 if(comments[n_comments].aflag==1)
   xpp_log(XPP_LOG_DEBUG, "action=%s \n",comments[n_comments].action);
 n_comments++;
}



void advance_past_first_word(char** sptr) {
    /* changes the string pointed to by sptr to start after the end of the string...
       this may seem odd, but it has to do with avoiding \0's added by strtok */
    int len = strlen(*sptr);
    (*sptr) += len + 1;
}

/* old's first length characters, a final comma dropped */
std::string new_string2(const char * old, int length) {
    std::string s(old, length);
    if (!s.empty() && s.back() == ',')
        s.pop_back();
    return s;
}


std::optional<std::string> get_next2(char** tokens_ptr) {
    /* grabs (a copy of) the next block of the form var = val, ending with a \n, space, or comma */
    /* importantly, this supports white space around the equal sign */
    /* returns nullopt if no more text */
    /* advances tokens_ptr */
    /* modified 2012-10-12 to also work if no = */
    int success = 0;
    int i=0;
    char* tokens = *tokens_ptr;
    for (; *tokens; tokens++) {
        if (!isspace(tokens[i])) break;
    }
    if (!(*tokens)) {
        (*tokens_ptr) = tokens;
        return std::nullopt;
    }
    int len = strlen(tokens);
    /* advance past space/the equal sign/comma */
    success = 0;
    for (i = 1; i < len; i++) {
        if (tokens[i] == '=' || isspace(tokens[i]) || tokens[i] == ',') {
            success = 1;
            break;
        }
    }

    if (!success) {
        /* this is either a variable alone or a syntax error */    
        *tokens_ptr = &tokens[len];
        return new_string2(tokens, len);
    }

    /* advance past any spaces */
    success = 0;
    for (; i < len; i++) {
        if (!isspace(tokens[i])) {
            success = 1;
            break;
        }
    }
    
    if (!success) {
        /* this is either a variable alone or a syntax error */    
        *tokens_ptr = &tokens[len];
        return new_string2(tokens, len);
    }

    if (tokens[i] != '=') {
        if (tokens[i] == ',') {
            *tokens_ptr = &tokens[i + 1];
        } else {
            *tokens_ptr = &tokens[i];
        }
        return new_string2(tokens, i);
    }
    

    
    
    /* advance until the first non-space */
    success = 0;
    for (i = i + 1; i < len; i++) {
        if (!isspace(tokens[i])) {
            success = 1;
            break;
        }
    }
    if (!success) {
        /* also a syntax error */
        *tokens_ptr = &tokens[len];
        return new_string2(tokens, len);
    }
    
    /* advance past the nonspaces and non-commas */
    for (; i < len; i++) {
        if (isspace(tokens[i]) || tokens[i] == ',') break;
    }
    
    /* advance past any spaces */
    for (; i < len; i++) {
        if (!isspace(tokens[i])) {
            break;
        }
    }
    
    /* advance past a comma, if any */
    if (i < len) {
        if (tokens[i] == ',') i++;
    }
    
    /* advance the pointer to point to the next character, or the null character if no more */
    *tokens_ptr = &tokens[i];
    return new_string2(tokens, i);
}

void strcpy_trim(char * dest, const char * source) {
    /* like strcpy, except removes leading and trailing whitespace */
    while (*source && isspace(*source)) {
        source++;
    }
    int len = strlen(source), i;
    for (i = len - 1; i >= 0; i--) {
        if (!isspace(source[i])) break;
    }
    strncpy(dest, source, i + 1);
    dest[i + 1] = '\0';
}
void strncpy_trim(char * dest, const char * source, int n) {
    /* like strncpy, except removes leading and trailing whitespace (and always ends with a \0) */
    while (*source && isspace(*source)) {
        source++;
        n--;
    }
    int i;
    for (i = n - 1; i >= 0; i--) {
        if (!isspace(source[i])) break;
    }
    if (i + 1 > n) i = n - 1;
    strncpy(dest, source, i + 1);
    dest[i + 1] = '\0';
}

