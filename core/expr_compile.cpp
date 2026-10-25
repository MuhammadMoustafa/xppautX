/* The expression engine's compiler (expr.h): a formula's text becomes
   tokens, the symbol table's indexes (make_toks, checking each against the
   one before: check_syntax), then a program in reverse Polish order
   (alg_to_rpn), whose format expr_program.h documents. */
#include "expr_internal.h"
#include "model.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "xpp_math.h"
#include "expr_decode.h"

#include <array>
#include <algorithm>
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
using Symbols=std::vector<ExprSymbol>;

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
  if(token==NUMTOK||isvar(i1)||iscnst(i1)||isker(i1)||i1==USTACKTYPE||token==INDX)
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

  xpp::log(XPP_LOG_WARN, "Bad token {:d} \n",oldtoken);
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
  int length;
  *tok=p.symbol_prefix(source+*index,length);
  *index+=length;
}

int make_toks(const ParserState &p, const char *dest, std::vector<int> &my_token)
{
 double value;
  int old_tok=STARTTOK;
 int index=0,token,nparen=0,lastindex=0;

 my_token.clear();

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
      if(do_num(dest,&value,&index,p.errout)){
	show_where(dest,index);
	return(1);
      }
/*    new code        3/95      */
      const std::array<int,2> halves=xpp::expr::number_halves(value);
      my_token.push_back(NUMTOK);
      my_token.push_back(halves[0]);
      my_token.push_back(halves[1]);
      if(check_syntax(p.symbols,old_tok,NUMTOK)==1){
	 xpp::log(XPP_LOG_WARN, "Illegal syntax \n");
	 show_where(dest,lastindex);
	 return(1);
       }
      old_tok=NUMTOK;
 
    }
   
   else
     {
       my_token.push_back(token);
       if(check_syntax(p.symbols,old_tok,token)==1){
	 xpp::log(XPP_LOG_WARN, "Illegal syntax (Ref:{:d} {:d}) \n",old_tok,token);
	 show_where(dest,lastindex);
         tokeninfo(p.symbols,old_tok);
         tokeninfo(p.symbols,token);
	 return(1);
       }

       old_tok=token;
     }
 }

my_token.push_back(ENDTOK);
if(check_syntax(p.symbols,old_tok,ENDTOK)==1){
  xpp::log(XPP_LOG_WARN, "Premature end of expression \n");
  show_where(dest,lastindex);
  return(1);
}
if(nparen!=0)
{
 if(p.errout)xpp::log(XPP_LOG_WARN, " parentheses don't match\n");
 return(1);
}
return(0);

}

int alg_to_rpn(xpp::Session &s, int *toklist, std::vector<int> &command)
{
  Symbols &my_symb=s.parser.symbols;
  /* the compiler's temporary symbol: ParserState keeps it past nsym */
  const int temporary_token=static_cast<int>(my_symb.size())-2;
  xpp::Model &m=s.model();
  std::vector<int> tokstak;
  int comptr=0,tokptr=0,lstptr=0,temp;
  int ncomma=0;
  std::vector<int> loopstk;
  int lptr=0;
  int nif=0,nthen=0,nelse=0;
  int newtok,oldtok;
  int my_com,my_arg,jmp;
  /* where the last literal's NUMSYM and halves end: a ^ written there
     raises to that literal */
  int number_end=-1;

  command.clear();

  /* writes val at the top of stack, growing it, and moves top on */
  auto push = [](std::vector<int> &stack, int &top, int val) {
    if (top == static_cast<int>(stack.size())) stack.push_back(val);
    else stack[top] = val;
    top++;
  };

  push(tokstak,tokptr,STARTTOK);
  oldtok=STARTTOK;
  while(1)
         {
 getnew:
          newtok=toklist[lstptr++];
/*        check for delay symbol             */
          if(newtok==DELSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
   if(is_uvar(temp))
	   {
	    /* the argument names a variable: the temporary symbol keeps its place for the delay (SVARTYPE) */
	    my_symb[temporary_token].com=COM(SVARTYPE,temp%MAXTYPE); /* create a temporary sybol */
            m.ndelays++;
           toklist[lstptr+1]=temporary_token;
	  	
	    my_symb[temporary_token].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log(XPP_LOG_WARN, "Illegal use of DELAY \n");
		return(1);
           }

	 }

/*        check for delshft symbol             */
          if(newtok==DELSHFTSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
   if(is_uvar(temp))
	   {
	    /* as for delay: the temporary symbol names the variable (SVARTYPE) by its place */
	    my_symb[temporary_token].com=COM(SVARTYPE, temp%MAXTYPE); /* create a temporary sybol */
            m.ndelays++;
           toklist[lstptr+1]=temporary_token;
	  	
	    my_symb[temporary_token].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log(XPP_LOG_WARN, "Illegal use of DELAY Shift \n");
		return(1);
           }

	 }

	  if(newtok==SETSYM){
	     temp=my_symb[toklist[lstptr+1]].com;
             if(is_uvar(temp))
	   {
	    /* as for delay: the temporary symbol names the variable (SVARTYPE) by its place */
	    my_symb[temporary_token].com=COM(SVARTYPE, temp%MAXTYPE); /* create a temporary sybol */
           toklist[lstptr+1]=temporary_token;
	  	
	    my_symb[temporary_token].pri=10;
	   }
	     else
	       {
		 xpp::log(XPP_LOG_WARN, "Illegal use of set - variables only\n");
		   return(1);
	       }
	  }
	 
/* check for shift  */
	  if(newtok==SHIFTSYM||newtok==ISHIFTSYM)
	  {
           temp=my_symb[toklist[lstptr+1]].com;
	   if(is_uvar(temp) || is_ucon(temp))
	   {
	    /* as for delay: the temporary symbol names the variable (SVARTYPE) or the constant (SCONTYPE) by its place */
             if(is_uvar(temp))my_symb[temporary_token].com=COM(SVARTYPE, temp%MAXTYPE);
	        if(is_ucon(temp))my_symb[temporary_token].com=COM(SCONTYPE, temp%MAXTYPE);
/* create a temporary sybol */
         
           toklist[lstptr+1]=temporary_token;
	  	
	    my_symb[temporary_token].pri=10;
	 
	   	    }
	   else 
	   {
		xpp::log(XPP_LOG_WARN, "Illegal use of SHIFT \n");
		return(1);
           }

       	  }

 next:
          if((newtok==ENDTOK)&&(oldtok==STARTTOK))break;
         
          if(newtok==LPAREN)
           {
             push(tokstak,tokptr,LPAREN);
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
            push(tokstak,tokptr,COMMA);
            goto getnew;
           }
           /* ram -- the THOUS problem */

     if(my_symb[oldtok].pri>=my_symb[newtok].pri)
           {
            int ins = my_symb[oldtok].com;
	    /* an .odex model divides as IEEE does (docs/odex.md) */
	    if(ins==COM(FUN2TYPE,3)&&m.ieee_division)
	      ins=COM(FUN2TYPE,xpp::expr::IEEE_DIVIDE);
	    /* ran, poisson and normal: instructions of their own */
	    ins=xpp::expr::random_instruction(ins);
	    /* x^n with a literal whole n from 2 to 16: multiplied out (W471) */
	    if(ins==COM(FUN2TYPE,xpp::expr::POW)&&number_end==comptr&&
	       xpp::math::is_integer_power(xpp::expr::number_from_halves(command[comptr-1],command[comptr-2])))
	      ins=COM(FUN2TYPE,xpp::expr::INTEGER_POWER);
	    push(command,comptr,ins);
	    if((my_symb[oldtok].arg==2)&&
	       (my_symb[oldtok].com/MAXTYPE==FUN2TYPE))
	      ncomma--;
            my_com=ins;
 /*   New code   3/95      */
	   if(my_com==NUMSYM){
	     tokptr--;
	     push(command,comptr,tokstak[tokptr-1]);
	     tokptr--;
	     push(command,comptr,tokstak[tokptr-1]);
	     number_end=comptr;
	   }
 /*   end new code    3/95    */
           if(my_com==SUMSYM){
	     push(loopstk,lptr,comptr);
             push(command,comptr,0);
             ncomma-=1;
	   }
           if(my_com==ENDSUM){
	     lptr--;
             jmp=comptr-loopstk[lptr]-1;
             command[loopstk[lptr]]=jmp;
	   }
	   if(my_com==MYIF){
          	     push(loopstk,lptr,comptr); /* add some space for jump */
                     push(command,comptr,0);
		     nif++;
                }    
	   if(my_com==MYTHEN){ 
		              /* First resolve the if jump */
			lptr--;
			jmp=comptr-loopstk[lptr];  /* -1 is old */
			command[loopstk[lptr]]=jmp;
			   /* Then set up for the then jump */
			push(loopstk,lptr,comptr);
			push(command,comptr,0);
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
             push(command,comptr,my_arg);
             ncomma=ncomma+1-my_arg;
            }
           /*      USER FUNCTION OKAY          */
            tokptr--;
            oldtok=tokstak[tokptr-1];
            goto next;
          }
  /*    NEW code       3/95     */
	  if(newtok==NUMTOK){
	    push(tokstak,tokptr,toklist[lstptr++]);
	    push(tokstak,tokptr,toklist[lstptr++]);
	  }
 /*  end  3/95     */
          push(tokstak,tokptr,newtok);
          oldtok=newtok;
          goto getnew;
       }
        if(ncomma!=0){
        xpp::log(XPP_LOG_WARN, "Illegal number of arguments\n");
	return(1);
        }
	if((nif!=nelse)||(nif!=nthen)){
	  xpp::log(XPP_LOG_WARN, "If statement missing ELSE or THEN \n");
	  return(1);
	    }
        push(command,comptr,my_symb[ENDTOK].com);

        return(0);
}

}

/* ADD_EXPR   */

void size_expr_stack(Session &s, std::span<const int> code, int arguments)
{
 std::size_t depth=0;
 const auto decoded=xpp::expr::decode(s,code,arguments,&depth);
 /* Before all function metadata is installed, the instruction count
    bounds even a program the decoder cannot yet validate. */
 std::size_t values=decoded?depth:code.size();
 /* At most MAX_UFUN_DEPTH user functions are active at once, plus the
    arguments of the call refused past it: cover that many of the largest
    frame (its value program or its argument frame, even unused args).
    This conservative bound also covers SUM bodies sharing the stack. */
 const Model &m=s.model();
 std::size_t frame=0;
 for(std::size_t i=0;i<m.ufun_programs.size();++i)
   frame=std::max({frame,m.ufun_programs[i].rpn.size(),xpp::size_product(m.narg_fun[i])});
 values=xpp::size_sum(values,xpp::size_product(MAX_UFUN_DEPTH+1,frame));
 auto &stack=s.parser.stack;
 if(stack.values.size()<values)stack.values.resize(values);
 if(stack.args.size()<values)stack.args.resize(values);
}

int add_expr(xpp::Session &s, std::string_view expr, std::vector<int> &command, int *length)
{
 int err;
 std::string dest=converted(expr);
 std::vector<int> my_token;
 my_token.reserve(3*dest.size()+8);
 err=make_toks(s.parser,dest.c_str(),my_token);
 if(err!=0)return(1);
 err = alg_to_rpn(s,my_token.data(),command);
 if(err!=0)return(1);
 /* Function bodies are compiled before ENDFUN and their argument count
    are installed. MAXARG admits the compiler's temporary argument names. */
 size_expr_stack(s,command,MAXARG);
 if(length)*length=static_cast<int>(command.size());
 return(0);
}


int do_num(std::string_view source, double *value, int *ind, int report)
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
  if(error==0)*value=atof(text.c_str());
  else
  if(report)xpp::log(XPP_LOG_WARN, " illegal expression: {}\n",text);
  *ind=i;
  return(error);
}

} // namespace xpp
