/* The expression engine's evaluator (expr.h): runs a compiled program
   (expr_program.h) on the Session's stacks (ParserState::stack). Every
   right-hand side goes through here at every step: the hottest code
   there is, so it stays one loop over the program, its built-ins called
   through expr_functions.cpp's tables. */
#include "expr_internal.h"
#include "xpp_globals.h"
#include "model.h"
#include "simplenet.h"
#include "tabular.h"
#include "volterra2.h"

namespace xpp {


namespace {

/* Runs program equat on p's stacks and returns the value on top at its
   end (ENDEXP, or ENDSUM for the part SUMSYM runs).

   The stack's top is kept in a local (a register) and written back to
   the stack (st.top) around every call that may evaluate a program
   itself -- this one, for SUM and a user function, and the delays,
   shifts, tables, networks, vectorizers and kernels -- then read back
   after it: a nested evaluate() starts the stack again from 0, and what
   follows here goes on from where that left it, as it always has. The
   built-in function tables (fun1, fun2) never evaluate a program, so
   their calls need no write-back. */
double eval_rpn(const int *equat, xpp::Session &s)
{
  ParserState &p=s.parser;
   int i,it,in,j;
   const int *tmpeq;
  int is;

  int low,high,ijmp;
  double temx,temy,temz;
  double sum;
  /* read on every token: the constants, variables and stacks once */
  double *const constants=p.constants.data();
  double *const variables=p.variables.data();
  ExprStack &st=p.stack;
  double *const values=st.values.data();
  int top=st.top;
  auto push=[&](double a){ values[top++]=a; };
  auto pop=[&]{ return values[--top]; };
  /* around a call that may evaluate: the top written back, then read */
  auto save=[&]{ st.top=top; };
  auto restore=[&]{ top=st.top; };

  while((i=*equat++)!=ENDEXP)
  {

   switch(i)
   {
   case NUMSYM:
     /* the second half first (NUMSYM) */
     push(xpp::expr::number_from_halves(equat[1],equat[0]));
     equat+=2;
     break;
   case ENDFUN:
   		 i=*equat++;

    		 st.nargs-=i;

   		 break;

   case MYIF:
		temx=pop();
		ijmp=*equat++;
		if(temx==0.0)equat+=ijmp;
                break;
   case MYTHEN:
	       ijmp=*equat++;
	       equat+=ijmp;
		break;
   case MYELSE:
		break;

   case ENDDELSHFT:
     temx=pop();
     temy=pop();
     temz=pop();
     save();
     temx=xpp::expr::do_delay_shift(s,temx,temy,temz);
     restore();
     push(temx);
     break;
   case ENDSET:  /* indirectly set a variable + shift to a value
                    SET(name,shift,value)  */
     temx=pop();
     temy=pop();
     temz=pop();
     push(xpp::expr::do_set(s,temy,temz,temx));
     break;
   case ENDDELAY:
		    temx=pop();
		    temy=pop();
                   save();
                   temx=xpp::expr::do_delay(s,temx,temy);
                   restore();
                   push(temx);
		   break;

   case ENDSHIFT:
                 temx=pop();
                 temy=pop();
                 save();
                 temx=xpp::expr::do_shift(s,temx,temy);
                 restore();
                 push(temx);
                 break;
   case ENDISHIFT:
                 temx=pop();
                 temy=pop();
                 push(xpp::expr::do_ishift(temx,temy));
                 break;
   case SUMSYM:
              temx=pop();
              high=static_cast<int>(temx);
              temx=pop();
              low=static_cast<int>(temx);
              ijmp=*equat++;
              sum=0.0;
              if(low<=high){
		for(is=low;is<=high;is++){
		  tmpeq=equat;
		  constants[xpp::expr::SUM_INDEX]=static_cast<double>(is);
		  save();
		  sum+=eval_rpn(tmpeq,s);
		  restore();
		}
	      }
             equat+=ijmp;
             push(sum);
             break;

   case ENDSUM:
            temx=pop();
            save();
            return(temx);
   case INDXCOM:
     push(xpp::expr::CURRENT_INDEX);
     break;
   /* + - * /, the commonest instructions, dispatched here at once */
   case COM(FUN2TYPE,0):
     temx=pop();temy=pop();push(temx+temy);
     break;
   case COM(FUN2TYPE,1):
     temx=pop();temy=pop();push(temy-temx);
     break;
   case COM(FUN2TYPE,2):
     temx=pop();temy=pop();push(temx*temy);
     break;
   case COM(FUN2TYPE,3):
     temx=pop();if(temx==0.0)temx=xpp::expr::ZERO_DIVISOR;
     temy=pop();push(temy/temx);
     break;
   case COM(FUN2TYPE,xpp::expr::IEEE_DIVIDE):
     temx=pop();temy=pop();push(temy/temx);
     break;
   case RANDUNI:
     push(xpp::expr::do_random_uniform(s,pop()));
     break;
   case RANDPOI:
     push(xpp::expr::do_random_poisson(s,pop()));
     break;
   case RANDNORM:
     temx=pop();temy=pop();push(xpp::expr::do_random_normal(s,temy,temx));
     break;
   default:
   {
   it=i/MAXTYPE;
   in=i%MAXTYPE;
   switch(it)
    {
     case FUN1TYPE: push(xpp::expr::fun1[in](pop()));
            break;
     case FUN2TYPE:
             temx=pop();
             temy=pop();
	     push(xpp::expr::fun2[in](temy,temx));break;
     case CONTYPE:
              push(constants[in]);break;
    case VECTYPE:
             temx=pop();
             save();
             temx=vector_value(s,temx,in);
             restore();
             push(temx);
             break;
     case NETTYPE:
             temx=pop();
             save();
             temx=network_value(s,temx,in);
             restore();
             push(temx);
             break;
     case TABTYPE:
             temx=pop();
             save();
             temx=lookup(s,temx,in);
             restore();
             push(temx);
             break;

     case USTACKTYPE:
        /* ram: so this means ustacks really do need to be of USTACKTYPE */
            push(st.args[st.nargs-1-in]); break;
     case KERTYPE:
             save();
             temx=xpp::ker_val(s,in);
             restore();
             push(temx);
             break;
    case VARTYPE:
             push(variables[in]); break;

     /* indexes for shift and delay operators... */
     case SCONTYPE:

         push(static_cast<double>(COM(CONTYPE, in))); break;
     case SVARTYPE:

             push(static_cast<double>(COM(VARTYPE, in))); break;

     case UFUNTYPE: i=*equat++;

            for(j=0;j<i;j++)
            {
            st.args[st.nargs]=pop();

	    st.nargs++;
            }
            save();
            const Program &function=s.model().ufun_programs[in];
            temx=program.compile && function.native
                ? function.native(p.constants.data(),p.variables.data(),&s,function.rpn.data())
                : eval_rpn(function.rpn.data(),s);
            restore();
            push(temx);
break;
    }
   }
  }
  }
   temx=pop();
   save();
   return(temx);

}

}

double evaluate(xpp::Session &s, const int *program)
{
  ParserState &p=s.parser;
  p.stack.nargs=0;
  p.stack.top=0;
  return(eval_rpn(program,s));
}

double evaluate(Session &s, const Program &code)
{
  if (program.compile && code.native)
    return code.native(s.parser.constants.data(),s.parser.variables.data(),&s,code.rpn.data());
  return evaluate(s,code.rpn.data());
}

} // namespace xpp
