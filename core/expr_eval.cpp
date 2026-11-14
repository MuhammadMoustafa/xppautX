/* The expression engine's evaluator (expr.h): runs a program
   (expr_program.h) on the Session's stacks (ParserState::stack). Every
   right-hand side goes through here at every step: the hottest code
   there is, so it is one loop over the program's decoded instructions
   (Program::ops, made once from the RPN by expr_decode.h's decoder:
   W717), its built-ins called through expr_functions.cpp's tables. The
   RPN stays the program's source: what the compiler writes, what is
   saved and translated. */
#include "expr_decode.h"
#include "expr_derivative.h"
#include "model.h"
#include "model_files.h"
#include "integrate.h"
#include "xpp_ui.h"
#include "simplenet.h"
#include "tabular.h"
#include "volterra2.h"
#include "session.h"
#include "expr.h"
#include "expr_program.h"
#include "xpp_error.h"
#include "xpp_io.h"
#include "xpp_mir.h"

#include <algorithm>
#include <utility>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xpp {

namespace {

/* a program's instructions, decoded at its first evaluation (below) */
const expr::Op *instructions(Session &s, const Program &program, int arguments, std::vector<int> &decoding);

/* an instruction's effect on the stack's depth */
int effect(const expr::Op &o)
{
  switch(o.kind){
  case expr::OpKind::Number: case expr::OpKind::Constant: case expr::OpKind::Variable: case expr::OpKind::Argument: case expr::OpKind::ArgumentAt:
  case expr::OpKind::Kernel:
    return 1;
  case expr::OpKind::Add: case expr::OpKind::Subtract: case expr::OpKind::Multiply: case expr::OpKind::Divide: case expr::OpKind::IeeeDivide:
  case expr::OpKind::Fun2: case expr::OpKind::If: case expr::OpKind::Then: case expr::OpKind::Delay: case expr::OpKind::Shift: case expr::OpKind::IShift:
  case expr::OpKind::RandomNormal:
    return -1;
  case expr::OpKind::DelayShift: case expr::OpKind::Set: case expr::OpKind::Sum:
    return -2;
  case expr::OpKind::Call: return 1-o.count;
  case expr::OpKind::Drop: return -o.a;
  default: return 0; /* the fused operations, Fun1, a table, network, vectorizer, the other random draws, Return */
  }
}

/* The RPN p, decoded, as the interpreter's instructions: one Op for each
   instruction the decoder kept (ENDFUN, MYELSE and the joins go: the
   caller drops a function's arguments, and a jump lands on whatever
   instruction follows its target), the jumps resolved to indices, an
   operand and the + - * / that takes it as its right operand fused into
   one (never where a jump lands on the operation), and a call of a user
   function inlined: its arguments stay on the stack, read by their depth
   (ArgumentAt), and Drop ends it. Each program's instructions are made
   once, so a function inlined into its callers is decoded once and
   copied; `decoding` holds the user functions whose instructions are
   being made up the chain of calls, and a call of one of them (a
   recursion) stays a call. */
std::expected<std::vector<expr::Op>, std::string> operations(Session &s, std::span<const int> p, int arguments, std::vector<int> &decoding)
{
  const auto decoded=expr::decode(s,p,arguments);
  if(!decoded)return std::unexpected(decoded.error());
  std::vector<expr::Op> ops;
  ops.reserve(decoded->size()+1);
  /* the instruction each program position's instruction became, for the jumps: a position without one (a
     MYELSE, a JOIN) is the instruction after it */
  std::vector<int> at(p.size()+1,-1);
  std::vector<char> targeted(p.size()+1,0);
  const auto target=[&](const expr::Instr &i){ return i.pc+2+static_cast<std::size_t>(p[i.pc+1]); };
  for(const expr::Instr &i:*decoded)
    if(i.op==MYIF||i.op==MYTHEN||i.op==SUMSYM)targeted[target(i)]=1;
  struct Jump { std::size_t op; std::size_t target; };
  std::vector<Jump> jumps;
  int depth=0;
  std::size_t last_pc=0; /* the program position of the last instruction made */
  for(const expr::Instr &i:*decoded){
    if(i.op==expr::JOIN)continue;
    at[i.pc]=static_cast<int>(ops.size());
    /* a jump lands on this instruction when its target is at or after the last one */
    bool landed=false;
    for(std::size_t pc=ops.empty()?0:last_pc+1;pc<=i.pc;pc++)landed=landed||targeted[pc];
    last_pc=i.pc;
    expr::Op o;
    const int op_code=i.op,type=i.type,in=i.in;
    if(op_code==NUMSYM){ o.kind=expr::OpKind::Number; o.number=i.number; }
    else if(op_code==INDXCOM){ o.kind=expr::OpKind::Number; o.number=expr::CURRENT_INDEX; }
    else if(op_code==MYIF||op_code==MYTHEN){
      o.kind=op_code==MYIF?expr::OpKind::If:expr::OpKind::Then;
      jumps.push_back({ops.size(),target(i)});
    }
    else if(op_code==SUMSYM){ o.kind=expr::OpKind::Sum; jumps.push_back({ops.size(),target(i)}); }
    else if(op_code==ENDSUM){ o.kind=expr::OpKind::Return; }
    else if(op_code==ENDDELAY){ o.kind=expr::OpKind::Delay; }
    else if(op_code==ENDDELSHFT){ o.kind=expr::OpKind::DelayShift; }
    else if(op_code==ENDSHIFT){ o.kind=expr::OpKind::Shift; }
    else if(op_code==ENDISHIFT){ o.kind=expr::OpKind::IShift; }
    else if(op_code==ENDSET){ o.kind=expr::OpKind::Set; }
    else if(op_code==RANDUNI){ o.kind=expr::OpKind::RandomUniform; }
    else if(op_code==RANDPOI){ o.kind=expr::OpKind::RandomPoisson; }
    else if(op_code==RANDNORM){ o.kind=expr::OpKind::RandomNormal; }
    else if(type==SVARTYPE||type==SCONTYPE){
      o.kind=expr::OpKind::Number;
      o.number=static_cast<double>(type==SCONTYPE?COM(CONTYPE,in):COM(VARTYPE,in));
    }
    else if(type==CONTYPE){ o.kind=expr::OpKind::Constant; o.a=in; }
    else if(type==VARTYPE){ o.kind=expr::OpKind::Variable; o.a=in; }
    else if(type==USTACKTYPE){ o.kind=expr::OpKind::Argument; o.a=in; o.count=depth; }
    else if(type==FUN1TYPE){ o.kind=expr::OpKind::Fun1; o.fun1=expr::fun1[static_cast<std::size_t>(in)]; }
    else if(type==FUN2TYPE){
      switch(in){
      case expr::ADD: o.kind=expr::OpKind::Add; break;
      case expr::SUBTRACT: o.kind=expr::OpKind::Subtract; break;
      case expr::MULTIPLY: o.kind=expr::OpKind::Multiply; break;
      case expr::DIVIDE: o.kind=expr::OpKind::Divide; break;
      case expr::IEEE_DIVIDE: o.kind=expr::OpKind::IeeeDivide; break;
      default: o.kind=expr::OpKind::Fun2; o.fun2=expr::fun2[static_cast<std::size_t>(in)];
      }
      if(o.kind!=expr::OpKind::Fun2&&!landed&&!ops.empty()){
        /* fused with the operand before it: + - * / are expr::OpKind's in that order, and so are their fused forms */
        expr::Op &operand=ops.back();
        const int which=static_cast<int>(o.kind)-static_cast<int>(expr::OpKind::Add);
        const int fused=operand.kind==expr::OpKind::Number?static_cast<int>(expr::OpKind::AddNumber)
          :operand.kind==expr::OpKind::Constant?static_cast<int>(expr::OpKind::AddConstant)
          :operand.kind==expr::OpKind::Variable?static_cast<int>(expr::OpKind::AddVariable):-1;
        if(fused>=0){
          operand.kind=static_cast<expr::OpKind>(fused+which);
          depth--;
          continue;
        }
      }
    }
    else if(type==UFUNTYPE){
      o.kind=expr::OpKind::Call; o.a=in; o.count=i.count;
      /* inlined when the function runs interpreted and is not being decoded up the chain: the body's
         instructions, its arguments read from the stack, its jumps moved */
      const Program &function=s.model().ufun_programs[static_cast<std::size_t>(in)];
      const expr::Op *body=nullptr;
      if(!function.native&&std::find(decoding.begin(),decoding.end(),in)==decoding.end()){
        decoding.push_back(in);
        body=instructions(s,function,i.count,decoding);
        decoding.pop_back();
      }
      if(body){
        const int base=static_cast<int>(ops.size());
        const int n=i.count;
        for(const expr::Op *b=body;;b++){
          expr::Op c=*b;
          if(c.kind==expr::OpKind::Return){ c.kind=expr::OpKind::Drop; c.a=n; ops.push_back(c); break; }
          if(c.kind==expr::OpKind::Argument){ c.kind=expr::OpKind::ArgumentAt; c.a=c.count+n-c.a; }
          else if(c.kind==expr::OpKind::If||c.kind==expr::OpKind::Then||c.kind==expr::OpKind::Sum)c.a+=base;
          ops.push_back(c);
        }
        depth+=1-n;
        continue;
      }
    }
    else if(type==TABTYPE){ o.kind=expr::OpKind::Table; o.a=in; }
    else if(type==NETTYPE){ o.kind=expr::OpKind::Network; o.a=in; }
    else if(type==VECTYPE){ o.kind=expr::OpKind::Vector; o.a=in; }
    else { o.kind=expr::OpKind::Kernel; o.a=in; } /* KERTYPE: the decoder refused every other instruction */
    depth+=effect(o);
    ops.push_back(o);
  }
  ops.push_back(expr::Op{}); /* ENDEXP: Return */
  at[p.size()]=static_cast<int>(ops.size())-1;
  for(std::size_t k=at.size();k-->1;)if(at[k-1]<0)at[k-1]=at[k];
  for(const Jump &j:jumps)ops[j.op].a=at[j.target];
  return ops;
}

/* program's instructions, decoded at its first evaluation (decode_program);
   none when it cannot be decoded, which stops the integration with the
   reason (a program the compiler wrote decodes; one evaluated before what
   it names exists does not) */
[[gnu::noinline]] const expr::Op *decode_program(Session &s, const Program &program, int arguments, std::vector<int> &decoding)
{
  auto ops=operations(s,program.rpn,arguments,decoding);
  if(!ops){
    stop_integration(s,Error{"evaluation",xpp::format("the program cannot be run: {}",ops.error()),command_place()});
    return nullptr;
  }
  program.ops=std::move(*ops);
  return program.ops.data();
}

inline const expr::Op *instructions(Session &s, const Program &program, int arguments, std::vector<int> &decoding)
{
  return program.ops.empty()?decode_program(s,program,arguments,decoding):program.ops.data();
}

inline const expr::Op *instructions(Session &s, const Program &program, int arguments)
{
  if(!program.ops.empty())return program.ops.data();
  std::vector<int> decoding;
  return decode_program(s,program,arguments,decoding);
}

/* a user function called at MAX_UFUN_DEPTH: the integration stops (out of the evaluator's loop, which keeps no
   frame for the error's text) */
[[gnu::noinline]] void nested_too_deep(Session &s, int function)
{
  stop_integration(s,Error{"evaluation",
      xpp::format("user function '{}' nests deeper than {} calls",s.model().ufun_names[function],MAX_UFUN_DEPTH),
      model_place(s.model(),s.model().ufun_names[function])});
}

/* Runs the instructions from op of the program code on p's stacks and
   returns the value on top at their Return (the program's end, or the end
   of the sum body op starts; a jump's index counts from code).

   The stack's top is kept in a local (a register) and written back to
   the stack (st.top) around every call that may evaluate a program
   itself -- this one, for a sum and a user function, and the delays,
   shifts, tables, networks, vectorizers and kernels -- then read back
   after it: a nested evaluate() starts the stack again from 0, and what
   follows here goes on from where that left it, as it always has. The
   built-in functions (fun1, fun2) never evaluate a program, so their
   calls need no write-back. */
double run(const expr::Op *code, const expr::Op *op, Session &s)
{
  ParserState &p=s.parser;
  ExprStack &st=p.stack;
  /* read once: the constants, variables and stacks */
  double *const constants=p.constants.data();
  double *const variables=p.variables.data();
  double *const values=st.values.data();
  double *sp=values+st.top;
  for(;;){
    const expr::Op &o=*op++;
    switch(o.kind){
    case expr::OpKind::Number: *sp++=o.number; break;
    case expr::OpKind::Constant: *sp++=constants[o.a]; break;
    case expr::OpKind::Variable: *sp++=variables[o.a]; break;
    case expr::OpKind::Argument: *sp++=st.args[st.nargs-1-o.a]; break;
    case expr::OpKind::ArgumentAt: *sp=sp[-o.a]; sp++; break;
    case expr::OpKind::Add: --sp; sp[-1]=sp[0]+sp[-1]; break;
    case expr::OpKind::Subtract: --sp; sp[-1]=sp[-1]-sp[0]; break;
    case expr::OpKind::Multiply: --sp; sp[-1]=sp[0]*sp[-1]; break;
    case expr::OpKind::Divide: {
      --sp;
      double divisor=sp[0];
      if(divisor==0.0)divisor=expr::ZERO_DIVISOR;
      sp[-1]=sp[-1]/divisor;
      break;
    }
    case expr::OpKind::IeeeDivide: --sp; sp[-1]=sp[-1]/sp[0]; break;
    case expr::OpKind::AddNumber: sp[-1]=o.number+sp[-1]; break;
    case expr::OpKind::SubtractNumber: sp[-1]=sp[-1]-o.number; break;
    case expr::OpKind::MultiplyNumber: sp[-1]=o.number*sp[-1]; break;
    case expr::OpKind::DivideNumber: {
      double divisor=o.number;
      if(divisor==0.0)divisor=expr::ZERO_DIVISOR;
      sp[-1]=sp[-1]/divisor;
      break;
    }
    case expr::OpKind::IeeeDivideNumber: sp[-1]=sp[-1]/o.number; break;
    case expr::OpKind::AddConstant: sp[-1]=constants[o.a]+sp[-1]; break;
    case expr::OpKind::SubtractConstant: sp[-1]=sp[-1]-constants[o.a]; break;
    case expr::OpKind::MultiplyConstant: sp[-1]=constants[o.a]*sp[-1]; break;
    case expr::OpKind::DivideConstant: {
      double divisor=constants[o.a];
      if(divisor==0.0)divisor=expr::ZERO_DIVISOR;
      sp[-1]=sp[-1]/divisor;
      break;
    }
    case expr::OpKind::IeeeDivideConstant: sp[-1]=sp[-1]/constants[o.a]; break;
    case expr::OpKind::AddVariable: sp[-1]=variables[o.a]+sp[-1]; break;
    case expr::OpKind::SubtractVariable: sp[-1]=sp[-1]-variables[o.a]; break;
    case expr::OpKind::MultiplyVariable: sp[-1]=variables[o.a]*sp[-1]; break;
    case expr::OpKind::DivideVariable: {
      double divisor=variables[o.a];
      if(divisor==0.0)divisor=expr::ZERO_DIVISOR;
      sp[-1]=sp[-1]/divisor;
      break;
    }
    case expr::OpKind::IeeeDivideVariable: sp[-1]=sp[-1]/variables[o.a]; break;
    case expr::OpKind::Fun1: sp[-1]=o.fun1(sp[-1]); break;
    case expr::OpKind::Fun2: --sp; sp[-1]=o.fun2(sp[-1],sp[0]); break;
    case expr::OpKind::Call: {
      const int n=o.count;
      for(int j=0;j<n;j++)st.args[st.nargs++]=*--sp;
      st.top=static_cast<int>(sp-values);
      double value=0.0;
      if(s.integrator->step_error){
        st.nargs-=n;
      }else if(st.ufun_depth>=MAX_UFUN_DEPTH){
        nested_too_deep(s,o.a);
        st.nargs-=n;
      }else{
        st.ufun_depth++;
        const Program &function=s.model().ufun_programs[o.a];
        if(function.native){
          /* the bridge drops the arguments itself (expr_native.cpp h_end) */
          value=mir::call(function.native,constants,variables,&s,function.rpn.data());
        }else if(const expr::Op *body=instructions(s,function,n)){
          p.interpreted_ops+=function.ops.size();
          value=run(body,body,s);
          st.nargs-=n; /* ENDFUN */
        }else st.nargs-=n;
        st.ufun_depth--;
      }
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::Drop: sp[-1-o.a]=sp[-1]; sp-=o.a; break;
    case expr::OpKind::If: --sp; if(*sp==0.0)op=code+o.a; break;
    case expr::OpKind::Then: op=code+o.a; break;
    case expr::OpKind::Sum: {
      const int high=static_cast<int>(*--sp);
      const int low=static_cast<int>(*--sp);
      double sum=0.0;
      if(low<=high)p.interpreted_ops+=static_cast<std::uint64_t>(high-low+1)*static_cast<std::uint64_t>(code+o.a-op);
      for(int is=low;is<=high;is++){
        constants[expr::SUM_INDEX]=static_cast<double>(is);
        st.top=static_cast<int>(sp-values);
        sum+=run(code,op,s);
        sp=values+st.top;
      }
      op=code+o.a;
      *sp++=sum;
      break;
    }
    case expr::OpKind::Delay: {
      const double tau=*--sp;
      const double variable=*--sp;
      st.top=static_cast<int>(sp-values);
      const double value=expr::do_delay(s,tau,variable);
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::DelayShift: {
      const double tau=*--sp;
      const double shift=*--sp;
      const double variable=*--sp;
      st.top=static_cast<int>(sp-values);
      const double value=expr::do_delay_shift(s,tau,shift,variable);
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::Shift: {
      const double shift=*--sp;
      const double variable=*--sp;
      st.top=static_cast<int>(sp-values);
      const double value=expr::do_shift(s,shift,variable);
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::IShift: --sp; sp[-1]=expr::do_ishift(sp[0],sp[-1]); break;
    case expr::OpKind::Set: {
      /* indirectly set a variable + shift to a value: SET(name,shift,value) */
      const double value=*--sp;
      const double shift=*--sp;
      const double variable=*--sp;
      *sp++=expr::do_set(s,shift,variable,value);
      break;
    }
    case expr::OpKind::Table: case expr::OpKind::Network: case expr::OpKind::Vector: {
      const double x=*--sp;
      st.top=static_cast<int>(sp-values);
      const double value=o.kind==expr::OpKind::Table?lookup(s,x,o.a):o.kind==expr::OpKind::Network?network_value(s,x,o.a):vector_value(s,x,o.a);
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::Kernel: {
      st.top=static_cast<int>(sp-values);
      const double value=ker_val(s,o.a);
      sp=values+st.top;
      *sp++=value;
      break;
    }
    case expr::OpKind::RandomUniform: sp[-1]=expr::do_random_uniform(s,sp[-1]); break;
    case expr::OpKind::RandomPoisson: sp[-1]=expr::do_random_poisson(s,sp[-1]); break;
    case expr::OpKind::RandomNormal: --sp; sp[-1]=expr::do_random_normal(s,sp[-1],sp[0]); break;
    case expr::OpKind::Return:
      --sp;
      st.top=static_cast<int>(sp-values);
      return *sp;
    default: std::unreachable(); /* every instruction is one of the above: no range check on the dispatch */
    }
  }
}

}

double evaluate(Session &s, const Program &code)
{
  if (code.native)
    return mir::call(code.native,s.parser.constants.data(),s.parser.variables.data(),&s,code.rpn.data());
  const expr::Op *ops=instructions(s,code,-1);
  if(!ops)return 0.0;
  ParserState &p=s.parser;
  p.interpreted_ops+=code.ops.size();
  p.stack.nargs=0;
  p.stack.top=0;
  p.stack.ufun_depth=0;
  return run(ops,ops,s);
}

double evaluate(Session &s, std::span<const int> program)
{
  Program code;
  code.rpn.assign(program.begin(),program.end());
  return evaluate(s,code);
}

double evaluate_pure(const Session &s, const Program &code, double *constants, double *variables)
{
  /* The native function's signature takes the Session for the general
     programs; an array_pure one never writes through it. */
  return mir::call(code.native,constants,variables,const_cast<Session *>(&s),code.rpn.data());
}

void evaluate_derivatives(const Session &s, const expr::Symbolic &graph, const JacobianProgram &p, const double *c, const double *v,
                          std::vector<double> &values, std::span<double> out)
{
  /* each node the entries need, in the graph's order, as the Generator's derivatives writes it (expr_native.cpp):
     the same operations in the same order, a Select's both values computed */
  if(values.size()<graph.size())values.resize(graph.size());
  for(const int id:p.nodes){
    const expr::Symbolic::Node &n=graph.node({id});
    double &value=values[static_cast<std::size_t>(id)];
    const auto arg=[&](int k){ return values[static_cast<std::size_t>(n.args[static_cast<std::size_t>(k)])]; };
    switch(n.op){
    case expr::Symbolic::Op::Number: value=n.number; break;
    case expr::Symbolic::Op::Constant: value=c[n.index]; break;
    case expr::Symbolic::Op::Variable: value=v[n.index]; break;
    case expr::Symbolic::Op::Fun1: value=expr::fun1[static_cast<std::size_t>(n.index)](arg(0)); break;
    case expr::Symbolic::Op::Fun2: {
      const double x=arg(0),y=arg(1);
      switch(n.index){
      case expr::ADD: value=x+y; break;
      case expr::SUBTRACT: value=x-y; break;
      case expr::MULTIPLY: value=x*y; break;
      case expr::DIVIDE: value=x/(y==0.0?expr::ZERO_DIVISOR:y); break;
      case expr::IEEE_DIVIDE: value=x/y; break;
      default: value=expr::fun2[static_cast<std::size_t>(n.index)](x,y); /* INTEGER_POWER's squaring included */
      }
      break;
    }
    case expr::Symbolic::Op::Select: value=arg(0)==0.0?arg(2):arg(1); break;
    case expr::Symbolic::Op::Table: value=lookup(s,arg(0),n.index); break;
    case expr::Symbolic::Op::TableSlope: value=lookup_slope(s,arg(0),n.index); break;
    }
  }
  for(std::size_t k=0;k<out.size();k++)out[k]=values[static_cast<std::size_t>(p.terms[k].id)];
}

} // namespace xpp
