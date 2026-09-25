#include "xpplim.h"
#include "xpp_mem.h"
#include "comline.h"
#include "ggets.h"
#include "load_eqn.h"
#include "lunch-new.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include <stdlib.h>
#include <string.h>
/* command-line stuff for xpp */
#include <stdio.h>
#include "xpp_batch.h"
#include "aniparse.h"
#define NCMD 47 /* add new commands as needed  */

#define MAKEC 0
#define XORFX 1
#define SILENT 2 
#define CONVERT 3
#define NOICON 4
#define NEWSEED 5
#define ALLWIN 6
#define SETFILE 7
#define MSSTYLE 8
#define PWHITE 9
#define RUNNOW 10
#define BIGF 11
#define SMALLF 12
#define PARFILE 13
#define OUTFILE 14
#define ICFILE 15
#define FCOLOR 16
#define BCOLOR 17
#define BBITMAP 18
#define GRADS 19
#define MINWIDTH 20
#define MINHEIGHT 21
#define MWCOLOR 22
#define DWCOLOR 23
#define BELL 24
#define ITRNSETS 25
#define USET 26
#define RSET 27
#define INCLUDE 28
#define QSETS 29
#define QPARS 30
#define QICS 31
#define QUIET 32
#define LOGFILE 33
#define ANIFILE 34
#define VERSION 35
#define MKPLOT 36
#define PLOTFMT 37
#define NOOUT 38
#define DFDRAW 39
#define NCDRAW 40
#define DEFINE 41
#define READSET 42
#define WITH 43
#define EQUIL 44
#define VERBOSEOPT 45
#define DEBUGOPT 46


extern int SuppressOut;
extern int RunImmediately;
extern int got_file;

/*
char setfilename[100];
char parfilename[100];
char icfilename[100];
char includefilename[MaxIncludeFiles][100];
*/
char setfilename[XPP_MAX_NAME];
char parfilename[XPP_MAX_NAME];
char icfilename[XPP_MAX_NAME];
char includefilename[MaxIncludeFiles][XPP_MAX_NAME];

char readsetfile[XPP_MAX_NAME];
int externaloptionsflag=0;
char externaloptionsstring[1024];
int NincludedFiles=0;
/*extern char UserOUTFILE[256];
*/
/*extern char anifile[256];
*/
int select_intern_sets=0;



extern int Nintern_set;
int Nintern_2_use=0;


SET_NAME *sets2use,*setsNOTuse;

extern INTERN_SET intern_set[MAX_INTERN_SET];


/*extern char batchout[256];
*/

int loadsetfile=0;
int loadparfile=0;
int loadicfile=0;
int loadincludefile=0;
int querysets=0;
int querypars=0;
int queryics=0;
int dryrun=0;
/*extern char this_file[100];
*/
extern char this_file[XPP_MAX_NAME];
extern int MakePlotFlag;
extern int xorfix;
extern int newseeed;
extern int silent;
extern int ConvertStyle;
int noicon=1;
int newseed=0;
typedef struct {
  char name[11];
  int len;

} VOCAB;

VOCAB my_cmd[NCMD]=
{
  {"-m",3},         
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


int is_set_name(SET_NAME *set, const char *nam)
{
	if (set==NULL){return(0);}
	SET_NAME *curr;
	
	curr=set;
	
	while(curr)
	{
		if (strcmp(curr->name,nam)==0)
		{
			return(1);
		}
		curr=(SET_NAME*)curr->next;
	}
	
	return(0);
}

SET_NAME * add_set(SET_NAME *set, const char *nam)
{
	if (!is_set_name(set,nam))
	{
		SET_NAME *curr;	
		curr = (SET_NAME *)xpp_malloc(sizeof(SET_NAME));
        	curr->name = nam;
		curr->next  = (struct SET_NAME *)set;
		set=curr;
	}
	
	return(set);
}


void do_comline(int argc, char **argv)
{ 
 int i,k;

 silent = 0;
 got_file=0;
 xorfix=1;
 /*PaperWhite=0;
 */
 setfilename[0]=0;
 parfilename[0]=0;
 icfilename[0]=0;
 /*includefilename[0]=0;
 */
 for(i=1;i<argc;i++){
   k=parse_it(argv[i]);
   if(k==1){
     XPP_STRCPY(setfilename,argv[i+1]);
     i++;
     loadsetfile=1;
     
   }
   if(k==2){
     /* -smallfont: the X11 font, accepted and not kept */
     if (notAlreadySet.SMALL_FONT_NAME){notAlreadySet.SMALL_FONT_NAME=0;};
     i++;
   }
   if(k==3){
     /* -bigfont: the X11 font, accepted and not kept */
     if (notAlreadySet.BIG_FONT_NAME){notAlreadySet.BIG_FONT_NAME=0;};
     i++;
   } 
   if(k==4){
     strcat(parfilename,"!load ");
     strcat(parfilename,argv[i+1]);
     i++;
     loadparfile=1;
   }
   if(k==5){
    xpp_log(XPP_LOG_INFO, "%s",argv[i+1]);
     snprintf(batch_options.out_file,sizeof batch_options.out_file,"%s",argv[i+1]);
     snprintf(batch_options.user_out_file,sizeof batch_options.user_out_file,"%s",argv[i+1]);
     i++;
   }
   if(k==6){
     strcat(icfilename,argv[i+1]);
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
     /*strcpy(UserBGBitmap,argv[i+1]);
     */
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
     sets2use=add_set(sets2use,argv[i+1]);
     i++;
     select_intern_sets=1;
   }
   if(k==18){
     setsNOTuse=add_set(setsNOTuse,argv[i+1]);
     i++;
     select_intern_sets=1;
   } 
   if(k==19){
     if (NincludedFiles>MaxIncludeFiles)
     {
         xpp_log(XPP_LOG_WARN, "Max number of include files exceeded.\n");
     }
     XPP_STRCPY(includefilename[NincludedFiles],argv[i+1]);
     NincludedFiles++;
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
     XPP_STRCPY(ani_options.file,argv[i+1]);
     ani_options.use_file=1;
     i++;
   }
   if(k==23){
     printf("XPPAUT Version %g.%g\nCopyright 2015 Bard Ermentrout\n",(float)MYSTR1,(float)MYSTR2);
     exit(0);
   }
   if(k==24){
     set_option("PLOTFMT",argv[i+1],1,NULL);
     i++;
   }
   if(k==25){
     SuppressOut=1;
     
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
     XPP_STRCPY(readsetfile,argv[i+1]);
     i++;
     externaloptionsflag=1;

   }
   if(k==29){  /* -with */
     XPP_STRCPY(externaloptionsstring,argv[i+1]);
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
  FILE *fp;
  char myopts[1024];
  char myoptsx[1026];
  /*   printf("flag=%d file=%s\n",externaloptionsflag,readsetfile); */
  if(externaloptionsflag==0)
    return 1;
  if(externaloptionsflag==1){
    fp=fopen(readsetfile,"r");
    if(fp==NULL){
      xpp_log(XPP_LOG_WARN, "%s external set not found\n",readsetfile);
      return 0;
    }
    if(fgets(myopts,1024,fp)==NULL)myopts[0]=0;
    XPP_SPRINTF(myoptsx,"$ %s",myopts);
    xpp_log(XPP_LOG_DEBUG, "Got this string: {%s}\n",myopts);
    extract_action(myoptsx);
    fclose(fp);
    return 1;
  }

  if(externaloptionsflag==2){
    XPP_SPRINTF(myoptsx,"$ %s",externaloptionsstring);
    extract_action(myoptsx);
    return 1;
  }  
  return 0;
}
int if_needed_select_sets()
{
	if(!select_intern_sets){return 1;}
	int j;
	for(j=0;j<Nintern_set;j++)
  	{
		intern_set[j].use=batch_options.use_intern_sets;
		Nintern_2_use+=batch_options.use_intern_sets;
		
		if (is_set_name(sets2use,intern_set[j].name))
		{
		xpp_log(XPP_LOG_INFO, "Internal set %s was included\n",intern_set[j].name);
			if (intern_set[j].use==0){Nintern_2_use++;}
			intern_set[j].use=1;
			
		}
		
		if (is_set_name(setsNOTuse,intern_set[j].name))
		{
		xpp_log(XPP_LOG_INFO, "Internal set %s was excluded\n",intern_set[j].name);
			if (intern_set[j].use==1){Nintern_2_use--;}
			intern_set[j].use=0;
		}
	}
	
	xpp_log(XPP_LOG_INFO, "A total of %d internal sets will be used\n",Nintern_2_use);
	
	return 1;
}


int if_needed_load_set()
{
  FILE *fp;
  if(!loadsetfile)
  {
    return 1;
  }
  fp=fopen(setfilename,"r");
  if(fp==NULL)
  {
    xpp_log(XPP_LOG_WARN, "Couldn't load %s\n",setfilename);
    return 0;
  }
  read_lunch(fp);
  fclose(fp);
  return 1;
}



int if_needed_load_par()
{

  if(!loadparfile)
  {
    return 1;
  }
  xpp_log(XPP_LOG_INFO, "Loading external parameter file: %s\n",parfilename);
  io_parameter_file(parfilename,1);
  return 1;
}


int if_needed_load_ic()
{
  
  if(!loadicfile)
  {
  	return 1;
  }
  xpp_log(XPP_LOG_INFO, "Loading external initial condition file: %s\n",icfilename);
  io_ic_file(icfilename,1);
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
    case MAKEC:
     xpp_log(XPP_LOG_WARN, " C files are no longer part of this version. \n Sorry \n");
      break;
    case MKPLOT:
      MakePlotFlag=1;
      break;
    case SILENT:
      batch_options.enabled=1;
      break;
    case XORFX:
      xorfix=0;
      break;
    case CONVERT:
      ConvertStyle=1;
      break;
    case NOICON:
      noicon=0;
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
      /*PaperWhite=1;
      notAlreadySet.PaperWhite=0;
      break;
      */
    case RUNNOW:
      RunImmediately=1;
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
    if(com[0]=='-'||got_file==1){ 
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
      XPP_STRCPY(this_file,com);
      got_file=1;
    }
  }
  return 0;
}







