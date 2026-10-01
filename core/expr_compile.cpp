/* The expression engine's compiler (expr.h): a formula's text becomes
   tokens, the symbol table's indexes (make_toks, checking each against the
   one before: check_syntax), then a program in reverse Polish order
   (alg_to_rpn), whose format expr_program.h documents. */
#include "expr_internal.h"
#include "model.h"
#include "xpp_log.h"
#include "xpp_io.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

using xpp::expr::is_ucon;
using xpp::expr::is_uvar;
using xpp::expr::is_ufun;

namespace xpp {

namespace {

/* the symbol table (ParserState::symbols) the compiler reads */
using Symbols=std::array<ExprSymbol,MAX_SYMBS>;

int unary_sym(int token);
int binary_sym(int token);

bool isvar(int y)
{
 return (y == VARTYPE);
}

bool iscnst(int y)
{
 return (y == CONTYPE);
}

bool isker(int y)
{
  return (y == KERTYPE);
}

void show_where(const char *string, int index)
{
  /* a caret under string's character index */
  std::string junk(index>0?index:0,' ');
  junk+='^';
  xpp::log(XPP_LOG_WARN, "{}\n{}\n",string,junk);
}

int function_sym(const Symbols &my_symb, int token) /* functions should have ( after them  */
{
  int com=my_symb[token].com;
  int i1=com/MAXTYPE;

    if(i1==FUN1TYPE&&!unary_sym(token))return(1); /* single variable functions */
  if(i1==FUN2TYPE&&!binary_sym(token))return(1); /* two-variable function */
  /* ram this was: if(i1==UFUN||i1==7||i1==6||i1==5)return(1); recall: 5 was bad */
  if (i1 == UFUNTYPE || i1 == TABTYPE || i1==VECTYPE||i1 == NETTYPE) return(1);
  if(token==DELSHFTSYM||token==SETSYM||token==DELSYM||token==SHIFTSYM||token==ISHIFTSYM||com==MYIF||com==MYTHEN||com==MYELSE
     ||com==SUMSYM||com==ENDSUM)return(1);
  return(0);
}

int unary_sym(int token)
{
  /* ram: these are tokens not byte code, so no change here? */
  if(token==9||token==55)return(1);
  return(0);
}

int binary_sym(int token)
{
  /* ram: these are tokens not byte code, so no change here? */
  if(token>2&&token<9)return(1);
  if(token>43&&token<51)return(1);
  if(token==54)return(1);
  return(0);
}

int pure_number(const Symbols &my_symb, int token)
{
  int com=my_symb[token].com;
  int i1=com/MAXTYPE;
/* !! */  if(token==NUMTOK||isvar(i1)||iscnst(i1)||isker(i1)||i1==USTACKTYPE||token==INDX)
    return(1);
  return(0);
}

int gives_number(const Symbols &my_symb, int token)
{
  int com=my_symb[token].com;
  int i1=com/MAXTYPE;
  if(token==INDX)return(1);
  if(token==NUMTOK)return(1);
  if(i1==FUN1TYPE&&!unary_sym(token))return(1); /* single variable functions */
  if(i1==FUN2TYPE&&!binary_sym(token))return(1); /* two-variable function */
  /* !! */ 
  /* ram: 5 issue; was if(i1==8||isvar(i1)||iscnst(i1)||i1==7||i1==6||i1==5||isker(i1)||i1==UFUN)return(1); */
  if (i1 == USTACKTYPE || isvar(i1) || iscnst(i1) || i1 == TABTYPE || i1==VECTYPE||i1 == NETTYPE || isker(i1) || i1 == UFUNTYPE) return(1);
  if(com==MYIF||token==DELSHFTSYM||token==SETSYM||token==DELSYM||token==SHIFTSYM||token==ISHIFTSYM||com==SUMSYM)return(1);
  return(0);
}

int check_syntax(const Symbols &my_symb, int oldtoken, int newtoken)  /* 1 is BAD!   */
{
  int com2=my_symb[newtoken].com;

/* if the first symbol or (  or binary symbol then must be unary symbol or 
   something that returns a number or another (   
*/

  if(unary_sym(oldtoken)||oldtoken==COMMA||oldtoken==STARTTOK
     ||oldtoken==LPAREN||binary_sym(oldtoken))
   {
     if(unary_sym(newtoken)||gives_number(my_symb,newtoken)||newtoken==LPAREN)return(0);
     return(1);
   }

/* if this is a regular function, then better have ( 
*/
 
 if(function_sym(my_symb,oldtoken)){
   if(newtoken==LPAREN)return(0);
   return(1);
 }

/* if we have a constant or variable or ) or kernel then better
   have binary symbol or "then" or "else" as next symbol
*/
   
 if(pure_number(my_symb,oldtoken)){
   if(binary_sym(newtoken)||newtoken==RPAREN
      ||newtoken==COMMA||newtoken==ENDTOK)
     return(0);

   return(1);
 }

 if(oldtoken==RPAREN){
   if(binary_sym(newtoken)||newtoken==RPAREN
      ||newtoken==COMMA||newtoken==ENDTOK)return(0);
   if(com2==MYELSE||com2==MYTHEN||com2==ENDSUM)return(0);

   return(1);
 }

  xpp::log_printf(XPP_LOG_WARN, "Bad token %d \n",oldtoken);
  return(1);
    
}

/******************************
*    PARSER                   *
******************************/

void tokeninfo(const Symbols &my_symb, int tok)
{
 xpp::log(XPP_LOG_DEBUG, " {} {} {} {} {} \n",
	my_symb[tok].name,my_symb[tok].len,my_symb[tok].com,
        my_symb[tok].arg,my_symb[tok].pri);
}

void find_tok(const ParserState &p, const char *source, int *index, int *tok)
{
  const Symbols &my_symb=p.symbols;
 int i=*index,maxlen=0,symlen;
 int k,j,my_tok,match;
 my_tok=p.nsym;
 for(k=0;k<p.nsym;k++)
 {
  symlen=my_symb[k].len;
  if(symlen<=maxlen)continue;

   match=1;
   for(j=0;j<symlen;j++)
   {
    if(source[i+j]!=my_symb[k].name[j])
     {
      match=0;
      break;
     }
   }
   if(match!=0)
    {
     my_tok=k;
     maxlen=symlen;
    }
 }
   *index=*index+maxlen;
   *tok=my_tok;
}

int make_toks(const ParserState &p, const char *dest, int *my_token)
{
 std::array<char,40> num{}; /* do_num writes the number's start */
 double value;
  int old_tok=STARTTOK,tok_in=0;
 int index=0,token,nparen=0,lastindex=0;

 while(dest[index]!='\0')
  {
   lastindex=index;
   find_tok(p,dest,&index,&token);
   if((token==MINUS)&&
   ((old_tok==STARTTOK)||(old_tok==COMMA)||(old_tok==LPAREN)))
  token=NEGATE;
  if(token==LPAREN)++nparen;
  if(token==RPAREN)--nparen;
  
  if(token==p.nsym)
    {
      if(do_num(dest,num.data(),&value,&index,p.errout)){
	show_where(dest,index);
	return(1);
      }
/*    new code        3/95      */
      const std::array<int,2> halves=xpp::expr::number_halves(value);
      my_token[tok_in++]=NUMTOK;
      my_token[tok_in++]=halves[0];
      my_token[tok_in++]=halves[1];
      if(check_syntax(p.symbols,old_tok,NUMTOK)==1){
	 xpp::log_printf(XPP_LOG_WARN, "Illegal syntax \n");
	 show_where(dest,lastindex);
	 return(1);
       }
      old_tok=NUMTOK;
 
    }
   
   else
     {
       my_token[tok_in++]=token;
       if(check_syntax(p.symbols,old_tok,token)==1){
	 xpp::log_printf(XPP_LOG_WARN, "Illegal syntax (Ref:%d %d) \n",old_tok,token);
	 show_where(dest,lastindex);
         tokeninfo(p.symbols,old_tok);
         tokeninfo(p.symbols,token);
	 return(1);
       }

       old_tok=token;
     }
 }

my_token[tok_in++]=ENDTOK;
if(check_syntax(p.symbols,old_tok,ENDTOK)==1){
  xpp::log_printf(XPP_LOG_WARN, "Premature end of expression \n");
  show_where(dest,lastindex);
  return(1);
}
if(nparen!=0)
{
 if(p.errout)xpp::log_printf(XPP_LOG_WARN, " parentheses don't match\n");
 return(1);
}
return(0);

}

int alg_to_rpn(xpp::Session &s, int *toklist, int *command)
{
  Symbols &my_symb=s.parser.symbols;
  xpp::Model &m=s.model();
  int tokstak[500],comptr=0,tokptr=0,lstptr=0,temp;
  int ncomma=0;
  int loopstk[100];
  int lptr=0;
  int nif=0,nthen=0,nelse=0;
  int newtok,oldtok;
  int my_com,my_arg,jmp;

  tokstak[0]=STARTTOK;
  tokptr=1;
  oldtok=STARTTOK;
  while(1)
         {
 getnew:
          newtok=toklist[lstptr++];
/*        check for delay symbol             */
          if(newtok==DELSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
   /* !! */   if(is_uvar(temp))
	   {
	    /* ram -- is this right? not sure I understand what was happening here */
	    my_symb[LASTTOK].com=COM(SVARTYPE,temp%MAXTYPE); /* create a temporary sybol */
            m.ndelays++;
           toklist[lstptr+1]=LASTTOK;
	  	
	    my_symb[LASTTOK].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log_printf(XPP_LOG_WARN, "Illegal use of DELAY \n");
		return(1);
           }

	 }

/*        check for delshft symbol             */
          if(newtok==DELSHFTSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
   /* !! */   if(is_uvar(temp))
	   {
	    /* ram -- same issue */
	    my_symb[LASTTOK].com=COM(SVARTYPE, temp%MAXTYPE); /* create a temporary sybol */
            m.ndelays++;
           toklist[lstptr+1]=LASTTOK;
	  	
	    my_symb[LASTTOK].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log_printf(XPP_LOG_WARN, "Illegal use of DELAY Shift \n");
		return(1);
           }

	 }

	  if(newtok==SETSYM){
	     temp=my_symb[toklist[lstptr+1]].com;
             if(is_uvar(temp))
	   {
	    /* ram -- same issue */
	    my_symb[LASTTOK].com=COM(SVARTYPE, temp%MAXTYPE); /* create a temporary sybol */
           toklist[lstptr+1]=LASTTOK;
	  	
	    my_symb[LASTTOK].pri=10;
	   }
	     else
	       {
		 xpp::log_printf(XPP_LOG_WARN, "Illegal use of set - variables only\n");
		   return(1);
	       }
	  }
	 
/* check for shift  */
	  if(newtok==SHIFTSYM||newtok==ISHIFTSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
/* !! */	   if(is_uvar(temp) || is_ucon(temp))
	   {
	    /* ram -- same issue */
             if(is_uvar(temp))my_symb[LASTTOK].com=COM(SVARTYPE, temp%MAXTYPE);
	        if(is_ucon(temp))my_symb[LASTTOK].com=COM(SCONTYPE, temp%MAXTYPE);
/* create a temporary sybol */
         
           toklist[lstptr+1]=LASTTOK;
	  	
	    my_symb[LASTTOK].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log_printf(XPP_LOG_WARN, "Illegal use of SHIFT \n");
		return(1);
           }

       	  }

 next:
          if((newtok==ENDTOK)&&(oldtok==STARTTOK))break;
         
          if(newtok==LPAREN)
           {
             tokstak[tokptr]=LPAREN;
             tokptr++;
             oldtok=LPAREN;
             goto getnew;
            }
           if(newtok==RPAREN)
           {
            switch(oldtok)
                  {
                     case LPAREN:
                                 tokptr--;
                                 oldtok=tokstak[tokptr-1];
                                 goto getnew;
                     case COMMA:
                                 tokptr--;
                                 ncomma++;
                                 oldtok=tokstak[tokptr-1];
                                 goto next;
                  }
           }
           if((newtok==COMMA)&&(oldtok==COMMA))
           {
            tokstak[tokptr]=COMMA;
            tokptr++;
            goto getnew;
           }
           /* ram -- the THOUS problem */

     if(my_symb[oldtok].pri>=my_symb[newtok].pri)
           {
            command[comptr]=my_symb[oldtok].com;
	    /* an .odex model divides as IEEE does (docs/odex.md) */
	    if(command[comptr]==COM(FUN2TYPE,3)&&m.ieee_division)
	      command[comptr]=COM(FUN2TYPE,xpp::expr::IEEE_DIVIDE);
	    if((my_symb[oldtok].arg==2)&&
	       (my_symb[oldtok].com/MAXTYPE==FUN2TYPE))
	      ncomma--;
            my_com=command[comptr];
	                comptr++;
 /*   New code   3/95      */
	   if(my_com==NUMSYM){
	     tokptr--;
	     command[comptr]=tokstak[tokptr-1];
	     comptr++;
	     tokptr--;
	     command[comptr]=tokstak[tokptr-1];
	     comptr++;
	   }
 /*   end new code    3/95    */
           if(my_com==SUMSYM){
	     loopstk[lptr]=comptr;
             comptr++;
             lptr++;
             ncomma-=1;
	   }
           if(my_com==ENDSUM){
	     lptr--;
             jmp=comptr-loopstk[lptr]-1;
             command[loopstk[lptr]]=jmp;
	   }
	   if(my_com==MYIF){
         	     loopstk[lptr]=comptr; /* add some space for jump */
                     comptr++;
		     lptr++;
		     nif++;
                }    
	   if(my_com==MYTHEN){ 
		              /* First resolve the if jump */
			lptr--;
			jmp=comptr-loopstk[lptr];  /* -1 is old */
			command[loopstk[lptr]]=jmp;
			   /* Then set up for the then jump */
			loopstk[lptr]=comptr;
			lptr++;
			comptr++;
			nthen++;
			}
	   if(my_com==MYELSE){
			     lptr--;
			     jmp=comptr-loopstk[lptr]-1;
			     command[loopstk[lptr]]=jmp;
			     nelse++;
			     }

             if(my_com==ENDDELAY||my_com==ENDSHIFT||my_com==ENDISHIFT){
        
	     ncomma-=1;
                }
             if(my_com==ENDDELSHFT||my_com==ENDSET)
	       ncomma-=2;  

            /*    CHECK FOR USER FUNCTION       */
            if(is_ufun(my_com))
            {
             my_arg=my_symb[oldtok].arg;
                         command[comptr]=my_arg;
             comptr++;
             ncomma=ncomma+1-my_arg;
            }
           /*      USER FUNCTION OKAY          */
            tokptr--;
            oldtok=tokstak[tokptr-1];
            goto next;
          }
  /*    NEW code       3/95     */
	  if(newtok==NUMTOK){
	    tokstak[tokptr++]=toklist[lstptr++];
	    tokstak[tokptr++]=toklist[lstptr++];
	  }
 /*  end  3/95     */
          tokstak[tokptr]=newtok;
          oldtok=newtok;
          tokptr++;
          goto getnew;
       }
        if(ncomma!=0){
        xpp::log_printf(XPP_LOG_WARN, "Illegal number of arguments\n");
	return(1);
        }
	if((nif!=nelse)||(nif!=nthen)){
	  xpp::log_printf(XPP_LOG_WARN, "If statement missing ELSE or THEN \n");
	  return(1);
	    }
        command[comptr]=my_symb[ENDTOK].com;

        return(0);
    }

}

/* ADD_EXPR   */

int add_expr(xpp::Session &s, std::string_view expr, int *command, int *length)
{
 int err,i;
 std::string dest=converted(expr);
 /* make_toks writes a token per character at most, three for a number
    (its token and the double's two ints), the end, and slack */
 std::vector<int> my_token(3*dest.size()+8);
 err=make_toks(s.parser,dest.c_str(),my_token.data());
 if(err!=0)return(1);
 err = alg_to_rpn(s,my_token.data(),command);
 if(err!=0)return(1);
  i=0;
   while(command[i]!=ENDEXP)i++;
   *length=i+1;
   return(0);
}


int do_num(std::string_view source, char *num, double *value, int *ind, int report)
{
 int i=*ind,error=0;
 int ndec=0,nexp=0,ndig=0;
 std::string text;
 char ch,oldch;
 oldch='\0';
 *value=0.0;
 while(1)
 {
  ch=static_cast<size_t>(i)<source.size()?source[i]:'\0';
  if(((ch=='+')||(ch=='-'))&&(oldch!='E'))break;
  if((ch=='*')||(ch=='^')||(ch=='/')||(ch==',')||(ch==')')||(ch=='\0')
              || (ch=='|') || (ch=='>') || (ch=='<') || (ch=='&')
               || (ch=='='))break;
  if((ch=='E')||(ch=='.')||(ch=='+')||(ch=='-')||isdigit(ch))
  {
   if(isdigit(ch))ndig++;
   switch(ch)
             {
              case 'E':
                       nexp++;
                       if((nexp==2)||(ndig==0))goto err;
                       break;
              case '.':
                       ndec++;
                       if((ndec==2)||(nexp==1))goto err;
                       break;

             }
   text+=ch;
   i++;
   oldch=ch;
  }
  else
  {
err:
    text+=ch;
    error=1;
    break;
  }
  }
  size_t n=text.copy(num,39);
  num[n]='\0';
  if(error==0)*value=atof(text.c_str());
  else
  if(report)xpp::log(XPP_LOG_WARN, " illegal expression: {}\n",text);
  *ind=i;
  return(error);
}

} // namespace xpp
