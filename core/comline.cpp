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

namespace xpp {

enum CommandId { SILENT, CONVERT, CHECK, NEWSEED, SETFILE, RUNNOW, PARFILE, OUTFILE, ICFILE, ITRNSETS, USET, RSET, INCLUDE, QSETS, QPARS, QICS, QUIET, LOGFILE, ANIFILE, VERSION, MKPLOT, PLOTFMT, NOOUT, DFDRAW, NCDRAW, READSET, WITH, EQUIL, VERBOSEOPT, DEBUGOPT };

namespace {

/* the files and text the options name, whatever their length */
std::string setfilename;
std::string parfilename;
std::string icfilename;
std::string readsetfile;
std::string externaloptionsstring;
/* --uset and --rset: the internal sets to run and not to run */
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
struct CommandOption {
  std::string_view name;
  int id;
  bool argument;
};

constexpr CommandOption my_cmd[] = {
  {"--silent", SILENT, false},
  {"--convert", CONVERT, false},
  {"--check", CHECK, false},
  {"--newseed", NEWSEED, false},
  {"--setfile", SETFILE, true},
  {"--runnow", RUNNOW, false},
  {"--parfile", PARFILE, true},
  {"--outfile", OUTFILE, true},
  {"--icfile", ICFILE, true},
  {"--internset", ITRNSETS, true},
  {"--uset", USET, true},
  {"--rset", RSET, true},
  {"--include", INCLUDE, true},
  {"--qsets", QSETS, false},
  {"--qpars", QPARS, false},
  {"--qics", QICS, false},
  {"--quiet", QUIET, true},
  {"--logfile", LOGFILE, true},
  {"--anifile", ANIFILE, true},
  {"--version", VERSION, false},
  {"--mkplot", MKPLOT, false},
  {"--plotfmt", PLOTFMT, true},
  {"--noout", NOOUT, false},
  {"--dfdraw", DFDRAW, true},
  {"--ncdraw", NCDRAW, true},
  {"--readset", READSET, true},
  {"--with", WITH, true},
  {"--equil", EQUIL, true},
  {"--verbose", VERBOSEOPT, false},
  {"--debug", DEBUGOPT, false},
};

static const CommandOption *command_option(std::string_view word)
{
  for (const CommandOption &option : my_cmd)
    if (option.name == word) return &option;
  return nullptr;
}

Result<> check_command_line(int argc, char **argv)
{
  bool model = false;
  for (int i = 1; i < argc; ++i) {
    const std::string_view word(argv[i]);
    const Place place{"command line", i, 0, std::string(word)};
    if (!word.starts_with('-')) {
      if (model) return fail("options", xpp::format("unexpected model argument {}", word), place);
      model = true;
      continue;
    }
    const CommandOption *option = command_option(word);
    if (!option) {
      if (!word.starts_with("--")) {
        const std::string spelling = "-" + std::string(word);
        if (command_option(spelling))
          return fail("options", xpp::format("{} is {}", word, spelling), place);
        // These switches are consumed by main before model loading.
        for (std::string_view name : {"--server", "--browser", "--help", "--port", "--no-open", "--web", "--auto"})
          if (name == spelling) return fail("options", xpp::format("{} is {}", word, spelling), place);
      }
      return fail("options", xpp::format("no such option {}", word), place);
    }
    if (option->argument && ++i >= argc)
      return fail("options", xpp::format("{} needs an argument", word), place);
  }
  return {};
}

int do_comline(xpp::Session &s, int argc, char **argv)
{ 
 if (const Result<> r = check_command_line(argc, argv); !r) throw LoadFailed{r.error()};
 int i,k;
 int model_argument=-1;

 s.got_file=0;
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
   const int had_file=s.got_file;
   k=parse_it(s,argv[i]);
   if(!had_file&&s.got_file)model_argument=i;
   if(k==1){
     setfilename=argv[i+1];
     i++;
     loadsetfile=1;
     
   }
   if(k==4){
     parfilename+=argv[i+1];
     i++;
     loadparfile=1;
   }
   if(k==5){
    xpp::log(XPP_LOG_INFO, "{}",argv[i+1]);
     batch_options.out_file=argv[i+1];
     batch_options.user_out_file=argv[i+1];
     i++;
   }
   if(k==6){
     icfilename+=argv[i+1];
     i++;
     loadicfile=1;
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
     set_option(s,"QUIET",argv[i+1],1,NULL);
     i++;
   }
   if(k==21){
     set_option(s,"LOGFILE",argv[i+1],1,NULL);
     i++;
   }
   if(k==22){
     s.animation.options.file=argv[i+1];
     s.animation.options.use_file=1;
     i++;
   }
   if(k==23){
     printf("XPPAUT Version %g.%g\nCopyright 2015 Bard Ermentrout\n",static_cast<float>(MYSTR1),static_cast<float>(MYSTR2));
     exit(0);
   }
   if(k==24){
     set_option(s,"PLOTFMT",argv[i+1],1,NULL);
     i++;
   }
   if(k==25){
     s.integrator.suppress_out=1;
     
   }
   if(k==26){
     set_option(s,"DFDRAW",argv[i+1],1,NULL);
     i++;
   } 
   if(k==27){
    
     set_option(s,"NCDRAW",argv[i+1],1,NULL);
     i++;
   }
   if(k==28){ /* --readset */
     readsetfile=argv[i+1];
     i++;
     externaloptionsflag=1;

   }
   if(k==29){  /* --with */
     externaloptionsstring=argv[i+1];
     i++;
     externaloptionsflag=2;
   }
   if(k==30){ /* --equil */
     batch_options.equilibria=atoi(argv[i+1]);
     i++;
     xpp::log(XPP_LOG_INFO, " Batch equilibria {:d} \n",batch_options.equilibria);
   }

 }
 return model_argument;
}

static int if_needed_load_ext_options(xpp::Session &s)
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
    if(const xpp::Result<> r=extract_action(s,("$ "+myopts),xpp::Place{readsetfile,1,0,myopts});!r){
      xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
      return 0;
    }
    return 1;
  }

  if(externaloptionsflag==2){
    if(const xpp::Result<> r=extract_action(s,("$ "+externaloptionsstring),xpp::Place{});!r){
      xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
      return 0;
    }
    return 1;
  }
  return 0;
}
int if_needed_select_sets(const xpp::Model &m)
{
	if(!select_intern_sets){return 1;}
	const std::vector<xpp::Model::InternalSet> &sets=m.intern_sets;
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
		xpp::log(XPP_LOG_INFO, "Internal set {} was included\n",name);
			if (use[j]==0){used++;}
			use[j]=1;
			
		}
		
		if (is_set_name(setsNOTuse,name))
		{
		xpp::log(XPP_LOG_INFO, "Internal set {} was excluded\n",name);
			if (use[j]==1){used--;}
			use[j]=0;
		}
	}
	
	xpp::log(XPP_LOG_INFO, "A total of {:d} internal sets will be used\n",used);
	
	return 1;
}

static int if_needed_load_set(xpp::Session &s)
{
  if(!loadsetfile)
  {
    return 1;
  }
  if(const xpp::Result<> r=import_xppaut_set(s,setfilename,true);!r)
  {
    xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
    return 0;
  }
  return 1;
}

static int if_needed_load_par(xpp::Session &s)
{

  if(!loadparfile)
  {
    return 1;
  }
  xpp::log(XPP_LOG_INFO, "Loading external parameter file: {}\n",parfilename);
  load_parameter_file_named(s,parfilename);
  return 1;
}

static int if_needed_load_ic(xpp::Session &s)
{
  
  if(!loadicfile)
  {
  	return 1;
  }
  xpp::log(XPP_LOG_INFO, "Loading external initial condition file: {}\n",icfilename);
  load_ic_file_named(s,icfilename);
  return(1);
}

void load_command_line_values(xpp::Session &s)
{
  for(int (*load)(xpp::Session &) : {if_needed_load_set,if_needed_load_par,if_needed_load_ic,if_needed_load_ext_options})
    load(s);
}

int parse_it(xpp::Session &s, std::string_view com)
{
  const CommandOption *option = command_option(com);
  if (option) {
    switch(option->id){
    case MKPLOT:
      s.integrator.make_plot_flag=1;
      break;
    case SILENT:
      batch_options.enabled=1;
      break;
    case CHECK:
      break;
    case CONVERT:
      ConvertStyle=1;
      break;
    case NEWSEED:
     xpp::log(XPP_LOG_INFO, "Random number seed changed\n");
      newseed=1;
      break;
    case RUNNOW:
      s.run_immediately=1;
      break;
    case SETFILE:
      return 1;
    case PARFILE:
      return 4;
    case OUTFILE:
      return 5; 
    case ICFILE:
      return 6;
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
      xpp::log_set_threshold(XPP_LOG_INFO);
      break;
    case DEBUGOPT:
      xpp::log_set_threshold(XPP_LOG_DEBUG);
      break;
    }
  }
  else {
    s.model().this_file=com;
    s.got_file=1;
  }
  return 0;
}

} // namespace xpp
