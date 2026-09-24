#include "userbut.h"

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include "kbs.h"
#include "xpp_ui.h"
#include "xpp_io.h"


int nuserbut=0;

USERBUT userbut[USERBUTMAX];


int get_button_info(char *s,char *bname,char *sc)
{
  int i=0,j=0,f=0,n=strlen(s);
  char c;
  if(n==0)return(-1);
  bname[0]=0;
  sc[0]=0;
  while(1){
    if(i==n)break;
    c=s[i];
    if(c==':'){
      f=1;
      bname[j]=0;
      j=0;
      i++;
    }
    else {
      if(f==0){
	bname[j]=c;
	j++;
      }
      else {
	sc[j]=c;
	j++;
      }
      i++;
    }
  }
  sc[j]=0;

 return(1); 
}

int find_kbs(char *sc)
{
  int i=0;
  while(1){
    if(strcmp(sc,kbs[i].seq)==0)
      return kbs[i].com;
    i++;
    if(kbs[i].com==0)return (-1);
  }
}

void add_user_button(char *s)
{
  char bname[10],sc[10];
  int z;
  if(nuserbut>=USERBUTMAX)return;
  if(strlen(s)==0)return;
  get_button_info(s,bname,sc);
  if(strlen(bname)==0||strlen(sc)==0)return;
  z=find_kbs(sc);
  if(z==-1){
    plintf("%s - not implemented\n",sc);
    return;
  }
  /*Don't add buttons with same functionality twice*/
  int i;
  for (i=0;i<nuserbut;i++)
  {
  	if (userbut[i].com == z)
	{
	  /*		plintf("But=%s:%s already implemented as button '%s'\n",bname,sc,userbut[i].bname); */
		return;	
	}
  }
  userbut[nuserbut].com=z;
  XPP_STRCPY(userbut[nuserbut].bname,bname);
  plintf(" added button(%d)  -- %s %d\n",
	 nuserbut,userbut[nuserbut].bname,userbut[nuserbut].com); 
  nuserbut++;
}
