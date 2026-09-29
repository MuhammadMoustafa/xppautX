#include "model.h"
#include "session.h"
#include "comline.h"
#include "lunch-new.h"
#include "xpp_log.h"
#include <stdlib.h>
#include <string.h>
/* command-line stuff for xpp */
#include <stdio.h>
#include "xpp_batch.h"
#include "aniparse.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#define NCMD 46 /* add new commands as needed  */

#define XORFX 0
#define SILENT 1 
#define CONVERT 2
#define NOICON 3
#define NEWSEED 4
#define ALLWIN 5
#define SETFILE 6
#define MSSTYLE 7
#define PWHITE 8
#define RUNNOW 9
#define BIGF 10
#define SMALLF 11
#define PARFILE 12
#define OUTFILE 13
#define ICFILE 14
#define FCOLOR 15
#define BCOLOR 16
#define BBITMAP 17
#define GRADS 18
#define MINWIDTH 19
#define MINHEIGHT 20
#define MWCOLOR 21
#define DWCOLOR 22
#define BELL 23
#define ITRNSETS 24
#define USET 25
#define RSET 26
#define INCLUDE 27
#define QSETS 28
#define QPARS 29
#define QICS 30
#define QUIET 31
#define LOGFILE 32
#define ANIFILE 33
#define VERSION 34
#define MKPLOT 35
#define PLOTFMT 36
#define NOOUT 37
#define DFDRAW 38
#define NCDRAW 39
#define READSET 41
#define WITH 42
#define EQUIL 43
#define VERBOSEOPT 44
#define DEBUGOPT 45


namespace {

/* the files and text the options name, whatever their length */
std::string setfilename;
std::string parfilename;
std::string icfilename;
std::string readsetfile;
std::string externaloptionsstring;
/* -uset and -rset: the internal sets to run and not to run */
std::vector<std::string> sets2use, setsNOTuse;

bool is_set_name(const std::vector<std::string> &sets, const char *nam)
{
  return std::find(sets.begin(), sets.end(), nam) != sets.end();
}

void add_set(std::vector<std::string> &sets, const char *nam)
{
  if(!is_set_name(sets,nam)) sets.emplace_back(nam);
}

} // namespace

static int externaloptionsflag=0;
std::vector<std::string> include_files;
static int select_intern_sets=0;


static int loadsetfile=0;
static int loadparfile=0;
static int loadicfile=0;
int loadincludefile=0;
int querysets=0;
int querypars=0;
int queryics=0;
int dryrun=0;
int newseed=0;
typedef struct {
  const char *name;
  int len;
} VOCAB;

constexpr VOCAB my_cmd[NCMD]=
{
  {"-xorfix",7},
  {"-silent",7},
  {"-convert",8},
  {"-iconify",7},
  {"-newseed",7},
  {"-allwin",6},
  {"-setfile",7},
  {"-ee",3},
  {"-white", 6},
  {"-runnow",7},
  {"-bigfont",8},
  {"-smallfont",10},
  {"-parfile",8},
  {"-outfile",8},
  {"-icfile",7},
  {"-forecolor",10},
  {"-backcolor",10},
  {"-backimage",10},
  {"-grads",6},
  {"-width",6},
  {"-height",7},
  {"-mwcolor",8},
  {"-dwcolor",8},
  {"-bell",4},
  {"-internset",10},
  {"-uset",5},
  {"-rset",5},
  {"-include",8},
  {"-qsets",6},
  {"-qpars",6},
  {"-qics",5},
  {"-quiet",6},
  {"-logfile",8},
  {"-anifile",8},
  {"-version",8},
  {"-mkplot",7},
  {"-plotfmt",8},
  {"-noout",6},
  {"-dfdraw",7},
  {"-ncdraw",7},
  {"-def",4},
  {"-readset",8},
  {"-with",5},
  {"-equil",6},
  {"-verbose",8},
  {"-debug",6}
 };

void do_comline(int argc, char **argv)
{ 
 int i,k;

 xpp::session().got_file=0;
 setfilename.clear();
 parfilename.clear();
 icfilename.clear();
 /* what a command line before asked for (a load before this one: File >
    Open model, Reload) goes; this one says it again if it does */
 loadsetfile=loadparfile=loadicfile=0;
 loadincludefile=0;
 include_files.clear();
 select_intern_sets=0;
 sets2use.clear();
 setsNOTuse.clear();
 externaloptionsflag=0;
 readsetfile.clear();
 externaloptionsstring.clear();
 for(i=1;i<argc;i++){
   k=parse_it(argv[i]);
   if(k==1){
     setfilename=argv[i+1];
     i++;
     loadsetfile=1;
     
   }
   if(k==2){
     /* -smallfont: the X11 font, accepted and not kept */
     if (xpp::session().not_already_set.SMALL_FONT_NAME){xpp::session().not_already_set.SMALL_FONT_NAME=0;};
     i++;
   }
   if(k==3){
     /* -bigfont: the X11 font, accepted and not kept */
     if (xpp::session().not_already_set.BIG_FONT_NAME){xpp::session().not_already_set.BIG_FONT_NAME=0;};
     i++;
   } 
   if(k==4){
     parfilename+=argv[i+1];
     i++;
     loadparfile=1;
   }
   if(k==5){
    xpp_log(XPP_LOG_INFO, "%s",argv[i+1]);
     batch_options.out_file=argv[i+1];
     batch_options.user_out_file=argv[i+1];
     i++;
   }
   if(k==6){
     icfilename+=argv[i+1];
     i++;
     loadicfile=1;
   }
   if(k==7){
     if (strlen(argv[i+1]) != 6)
     {
       xpp_log(XPP_LOG_WARN, "Color must be given as hexadecimal string.\n");
	exit(-1);
     }
     set_option("FORECOLOR",argv[i+1],1,NULL);
     i++;
     
   }
   if(k==8){
     if (strlen(argv[i+1]) != 6)
     {
       xpp_log(XPP_LOG_WARN, "Color must be given as hexadecimal string.\n");
	exit(-1);
     }
     set_option("BACKCOLOR",argv[i+1],1,NULL);
     i++;
   }
   if(k==9){
     set_option("BACKIMAGE",argv[i+1],1,NULL);
     i++;
   }
   if(k==10){
     set_option("GRADS",argv[i+1],1,NULL);
     i++;
   }
   if(k==11){
     set_option("WIDTH",argv[i+1],1,NULL);
     i++;
   }if(k==12){
     set_option("HEIGHT",argv[i+1],1,NULL);
     i++;
   }if(k==13){
     if (strlen(argv[i+1]) != 6)
     {
       xpp_log(XPP_LOG_WARN, "Color must be given as hexadecimal string.\n");
	exit(-1);
     }
     set_option("MWCOLOR",argv[i+1],1,NULL);
     i++;
   }if(k==14){
     if (strlen(argv[i+1]) != 6)
     {
       xpp_log(XPP_LOG_WARN, "Color must be given as hexadecimal string.\n");
	exit(-1);
     }
     set_option("DWCOLOR",argv[i+1],1,NULL);
     i++;
   }
   if(k==15){
     set_option("BELL",argv[i+1],1,NULL);
     i++;
   }
   if(k==16){
     batch_options.use_intern_sets=atoi(argv[i+1]);
     select_intern_sets=1;
     i++;
   }  
   if(k==17){
     add_set(sets2use,argv[i+1]);
     i++;
     select_intern_sets=1;
   }
   if(k==18){
     add_set(setsNOTuse,argv[i+1]);
     i++;
     select_intern_sets=1;
   } 
   if(k==19){
     include_files.emplace_back(argv[i+1]);
     i++;
     loadincludefile=1;
   } 
   if(k==20){
     set_option("QUIET",argv[i+1],1,NULL);
     i++;
   }
   if(k==21){
     set_option("LOGFILE",argv[i+1],1,NULL);
     i++;
   }
   if(k==22){
     xpp::session().animation.options.file=argv[i+1];
     xpp::session().animation.options.use_file=1;
     i++;
   }
   if(k==23){
     printf("XPPAUT Version %g.%g\nCopyright 2015 Bard Ermentrout\n",static_cast<float>(MYSTR1),static_cast<float>(MYSTR2));
     exit(0);
   }
   if(k==24){
     set_option("PLOTFMT",argv[i+1],1,NULL);
     i++;
   }
   if(k==25){
     xpp::session().integrator.suppress_out=1;
     
   }
   if(k==26){
     set_option("DFDRAW",argv[i+1],1,NULL);
     i++;
   } 
   if(k==27){
    
     set_option("NCDRAW",argv[i+1],1,NULL);
     i++;
   }
   if(k==28){ /* -readset */
     readsetfile=argv[i+1];
     i++;
     externaloptionsflag=1;

   }
   if(k==29){  /* -with */
     externaloptionsstring=argv[i+1];
     i++;
     externaloptionsflag=2;
   }
   if(k==30){ /* -equil */
     batch_options.equilibria=atoi(argv[i+1]);
     i++;
     xpp_log(XPP_LOG_INFO, " Batch equilibria %d \n",batch_options.equilibria);
   }

 }
}

int if_needed_load_ext_options()
{
  if(externaloptionsflag==0)
    return 1;
  if(externaloptionsflag==1){
    /* the file's first line, whatever its length */
    xpp::LineReader lr(readsetfile.c_str());
    if(!lr){
      xpp::log(XPP_LOG_WARN, "{} external set not found\n",readsetfile);
      return 0;
    }
    std::string myopts(lr.next().value_or(std::string_view()));
    xpp::log(XPP_LOG_DEBUG, "Got this string: {{{}}}\n",myopts);
    extract_action(("$ "+myopts).c_str());
    return 1;
  }

  if(externaloptionsflag==2){
    extract_action(("$ "+externaloptionsstring).c_str());
    return 1;
  }
  return 0;
}
int if_needed_select_sets()
{
	if(!select_intern_sets){return 1;}
	const std::vector<xpp::Model::InternalSet> &sets=xpp::model().intern_sets;
	std::vector<int> &use=batch_options.intern_set_use;
	int &used=batch_options.intern_sets_used;
	use.assign(sets.size(),1);
	for(std::size_t j=0;j<sets.size();j++)
  	{
		const char *name=sets[j].name.c_str();
		use[j]=batch_options.use_intern_sets;
		used+=batch_options.use_intern_sets;
		
		if (is_set_name(sets2use,name))
		{
		xpp_log(XPP_LOG_INFO, "Internal set %s was included\n",name);
			if (use[j]==0){used++;}
			use[j]=1;
			
		}
		
		if (is_set_name(setsNOTuse,name))
		{
		xpp_log(XPP_LOG_INFO, "Internal set %s was excluded\n",name);
			if (use[j]==1){used--;}
			use[j]=0;
		}
	}
	
	xpp_log(XPP_LOG_INFO, "A total of %d internal sets will be used\n",used);
	
	return 1;
}

int if_needed_load_set()
{
  if(!loadsetfile)
  {
    return 1;
  }
  xpp::UniqueFile fp=xpp::open_read(setfilename.c_str());
  if(!fp)
  {
    xpp::log(XPP_LOG_WARN, "Couldn't load {}\n",setfilename);
    return 0;
  }
  read_lunch(fp.get());
  return 1;
}

int if_needed_load_par()
{

  if(!loadparfile)
  {
    return 1;
  }
  xpp::log(XPP_LOG_INFO, "Loading external parameter file: {}\n",parfilename);
  io_parameter_file(parfilename.c_str(),1);
  return 1;
}

int if_needed_load_ic()
{
  
  if(!loadicfile)
  {
  	return 1;
  }
  xpp::log(XPP_LOG_INFO, "Loading external initial condition file: {}\n",icfilename);
  io_ic_file(icfilename.c_str(),1);
  return(1);
}

int parse_it(const char *com)
{
  int j;
  for(j=0;j<NCMD;j++)
  {
  	if(strncmp(com,my_cmd[j].name,my_cmd[j].len)==0)
    	{
    		break;
  	}
  } 

  if(j<NCMD){
    switch(j){
    case MKPLOT:
      xpp::session().integrator.make_plot_flag=1;
      break;
    case SILENT:
      batch_options.enabled=1;
      break;
    case XORFX: /* the X11 work-around: accepted and ignored */
      break;
    case CONVERT:
      ConvertStyle=1;
      break;
    case NOICON:
      /* -iconify: the X11 icon is gone; accepted and ignored */
      break;
    case NEWSEED:
     xpp_log(XPP_LOG_INFO, "Random number seed changed\n");
      newseed=1;
      break;  
    case ALLWIN:  /* X11 window options: accepted, nothing to do */
    case MSSTYLE:
      break;
    case PWHITE:
      xpp_log(XPP_LOG_WARN, "-white option is no longer part of this version. \n Sorry \n");
      break;
    case RUNNOW:
      xpp::session().run_immediately=1;
      break;
    case SETFILE:
      return 1;
    case SMALLF:
      return 2;
    case BIGF:
      return 3;
    case PARFILE:
      return 4;
    case OUTFILE:
      return 5; 
    case ICFILE:
      return 6;
    case FCOLOR:
      return 7;
    case BCOLOR:
      return 8;
    case BBITMAP:
      return 9;
    case GRADS:
      return 10;
    case MINWIDTH:
      return 11;
    case MINHEIGHT:
      return 12;
    case MWCOLOR:
      return 13;
    case DWCOLOR:
      return 14;
    case BELL:
      return 15;
    case ITRNSETS:
      return 16;
    case USET:
      return 17;
    case RSET:
      return 18;
    case INCLUDE:
      return 19; 
    case QUIET:
      return 20; 
    case LOGFILE:
      return 21; 
    case ANIFILE:
      return 22;
    case VERSION:
      return 23;
    case PLOTFMT:
      return 24;  
    case NOOUT:
      return 25;
    case DFDRAW:
      return 26;
    case NCDRAW:
      return 27;
    case READSET:
      return 28;
    case WITH:
      return 29;
    case EQUIL: 
      return 30;
    case QSETS:
      batch_options.enabled=1;
      querysets=1;
      dryrun=1;
      break;
    case QPARS: 
      batch_options.enabled=1;
      querypars=1;
      dryrun=1;
      break;
    case QICS:
      batch_options.enabled=1;
      queryics=1;
      dryrun=1;
      break;
    case VERBOSEOPT:
      xpp_log_set_threshold(XPP_LOG_INFO);
      break;
    case DEBUGOPT:
      xpp_log_set_threshold(XPP_LOG_DEBUG);
      break;
    }
  }
  else {
    if(com[0]=='-'||xpp::session().got_file==1){ 
     xpp_log(XPP_LOG_WARN, "Problem reading option %s\n",com);
     xpp_log(XPP_LOG_WARN, "\nUsage: xppaut filename [options ...]\n\n");
     xpp_log(XPP_LOG_WARN, "Options:\n");
     xpp_log(XPP_LOG_WARN, "  -silent                Batch run without the interface and dump solutions to a file\n");
     xpp_log(XPP_LOG_WARN, "  -xorfix                Work-around for exclusive Or with X on some monitors/graphics setups\n");
     xpp_log(XPP_LOG_WARN, "  -convert               Convert old style ODE files (e.g. phaseplane) to new ODE style\n");
     xpp_log(XPP_LOG_WARN, "  -newseed               Randomizes the random number generator which will often use the same seed\n");
     xpp_log(XPP_LOG_WARN, "  -ee                    Emulates shortcuts of Evil Empire style (MS)\n");
     xpp_log(XPP_LOG_WARN, "  -allwin                Brings XPP up with all the windows visible\n");
     xpp_log(XPP_LOG_WARN, "  -white                 Uses white screen instead of black\n");
     xpp_log(XPP_LOG_WARN, "  -setfile <filename>    Loads the set file before starting up\n");
     xpp_log(XPP_LOG_WARN, "  -runnow                Runs ode file immediately upon startup (implied by -silent)\n");
     xpp_log(XPP_LOG_WARN, "  -bigfont <font>        Use the big font whose filename is given\n");
     xpp_log(XPP_LOG_WARN, "  -smallfont <font>      Use the small font whose filename is given\n");
     xpp_log(XPP_LOG_WARN, "  -parfile <filename>    Load parameters from the named file\n");
     xpp_log(XPP_LOG_WARN, "  -outfile <filename>    Send output to this file (default is output.dat)\n");
     xpp_log(XPP_LOG_WARN, "  -icfile <filename>     Load initial conditions from the named file\n");
     xpp_log(XPP_LOG_WARN, "  -forecolor <######>    Hexadecimal color (e.g. 000000) for foreground\n");
     xpp_log(XPP_LOG_WARN, "  -backcolor <######>    Hexadecimal color (e.g. EDE9E3) for background\n");
     xpp_log(XPP_LOG_WARN, "  -backimage <filename>  Name of bitmap file (.xbm) to load in background\n");
     xpp_log(XPP_LOG_WARN, "  -mwcolor <######>      Hexadecimal color (e.g. 808080) for main window\n");
     xpp_log(XPP_LOG_WARN, "  -dwcolor <######>      Hexadecimal color (e.g. FFFFFF) for drawing window\n");
     xpp_log(XPP_LOG_WARN, "  -grads < 1 | 0 >       Color gradients will | won't be used\n"); 
     xpp_log(XPP_LOG_WARN, "  -width N               Minimum width in pixels of main window\n");
     xpp_log(XPP_LOG_WARN, "  -height N              Minimum height in pixels of main window\n");
     xpp_log(XPP_LOG_WARN, "  -bell < 1 | 0 >        Events will | won't trigger system bell\n");
     xpp_log(XPP_LOG_WARN, "  -internset < 1 | 0 >   Internal sets will | won't be run during batch run\n");
     xpp_log(XPP_LOG_WARN, "  -uset <setname>        Named internal set will be run during batch run\n");
     xpp_log(XPP_LOG_WARN, "  -rset <setname>        Named internal set will not be run during batch run\n");
     xpp_log(XPP_LOG_WARN, "  -include <filename>    Named file will be included (see #include directive)\n");
     xpp_log(XPP_LOG_WARN, "  -qsets                 Query internal sets (output saved to OUTFILE)\n");
     xpp_log(XPP_LOG_WARN, "  -qpars                 Query parameters (output saved to OUTFILE)\n");
     xpp_log(XPP_LOG_WARN, "  -qics                  Query initial conditions (output saved to OUTFILE)\n");
     xpp_log(XPP_LOG_WARN, "  -quiet <1 |0>          Do not print *anything* out to console\n");
     xpp_log(XPP_LOG_WARN, "  -logfile <filename>    Print console output to specified logfile \n");
     xpp_log(XPP_LOG_WARN, "  -anifile <filename>    Load an animation code file (.ani) \n");
     xpp_log(XPP_LOG_WARN, "  -plotfmt <svg|ps>       Set Batch plot format\n");
     xpp_log(XPP_LOG_WARN, "  -mkplot                Do a plot in batch mode \n");
     xpp_log(XPP_LOG_WARN, " -ncdraw 1|2               Draw nullclines in batch (1) to file (2) \n");
     xpp_log(XPP_LOG_WARN, " -dfdraw 1-5       Draw dfields in batch (1-3) to file (4-5)  \n");
     xpp_log(XPP_LOG_WARN, "  -version               Print XPPAUT version and exit \n");
     xpp_log(XPP_LOG_WARN, "  -readset <filename>   Read in a set file like the internal sets\n");
     xpp_log(XPP_LOG_WARN, "  -with string   String must be surrounded with quotes; anything that is in an internal set is valid\n");
     xpp_log(XPP_LOG_WARN, "  -equil <0|1>    Write equilibria to equil.dat and if <1> manifolds um1.dat,...,sm2.dat\n");
     xpp_log(XPP_LOG_WARN, "  -verbose               Show the startup banner, parser stats and other INFO logging\n");
     xpp_log(XPP_LOG_WARN, "  -debug                 Show DEBUG logging too (see core/xpp_log.h)\n");
     xpp_log(XPP_LOG_WARN, "\n");

     xpp_log(XPP_LOG_WARN, "Environment variables:\n");
     xpp_log(XPP_LOG_WARN, "  XPPEDITOR              Editor File > .Xpprc opens\n");
     xpp_log(XPP_LOG_WARN, "  XPPSTART               Path to start looking for ODE files\n");
     xpp_log(XPP_LOG_WARN, "\n");
     exit(0);
    }
    else {
      xpp::model().this_file=com;
      xpp::session().got_file=1;
    }
  }
  return 0;
}

