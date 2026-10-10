#include "xpptest.h"
#include "expr_native.h"
#include "expr_program.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include <bit>
#include <cstdint>
#include <limits>

namespace {
std::vector<int> number(double x)
{
    const auto halves=xpp::expr::number_halves(x);
    return {NUMSYM,halves[1],halves[0],ENDEXP};
}

/* depth sums of 0 to 0, one inside the other, of 0: the program, ended by end */
std::vector<int> nested_sums(int depth,int end)
{
    const auto zero=number(0.0);
    const std::vector<int> bound(zero.begin(),zero.end()-1);
    std::vector<int> p=bound;
    p.push_back(end);
    for (int i=0;i<depth;++i) {
        std::vector<int> body=p;
        body.back()=ENDSUM;
        p=bound;
        p.insert(p.end(),bound.begin(),bound.end());
        p.push_back(SUMSYM);
        p.push_back(static_cast<int>(body.size()));
        p.insert(p.end(),body.begin(),body.end());
        p.push_back(end);
    }
    return p;
}
}

std::uint64_t bits(double x) { return std::bit_cast<std::uint64_t>(x); }
template <class A> bool same(const A &a,const A &b)
{
    for (size_t i=0;i<a.size();++i) if (bits(a[i])!=bits(b[i])) return false;
    return true;
}

/* Program i compiled and interpreted, each from the same state: the same value, and
   the same left behind (the variables SET writes, the sum's I', the generator's next
   draws). */
void same_both_ways(xpp::Session &s,int i)
{
    const auto variables=s.parser.variables;
    const auto constants=s.parser.constants;
    s.random.seed(7+i);
    const std::string random=s.random.save();
    const double native=xpp::eval_program(s,i);
    const auto native_variables=s.parser.variables;
    const auto native_constants=s.parser.constants;
    const std::string native_random=s.random.save();
    s.parser.variables=variables;
    s.parser.constants=constants;
    CHECK(s.random.load(random));
    const double interpreted=xpp::evaluate(s,s.model().programs[i].data());
    CHECK(bits(native)==bits(interpreted));
    CHECK(same(native_variables,s.parser.variables));
    CHECK(same(native_constants,s.parser.constants));
    CHECK(native_random==s.random.save());
    s.parser.variables=variables;
    s.parser.constants=constants;
}

int main()
{
    auto model=std::make_unique<xpp::Model>();
    auto session=std::make_unique<xpp::Session>(*model);
    auto &s=*session;
    auto &m=*model;
    xpp::init_rpn(s);
    CHECK(xpp::add_var(s,"t",0.0)==0);
    CHECK(xpp::add_var(s,"x",2.0)==0);
    for (const auto *name:{"z0","z1","z2","z3"}) CHECK(xpp::add_var(s,name,0.0)==0);
    CHECK(xpp::add_con(s,"par",3.0)==0);
    m.this_file="compile.odex";
    m.statement_files={m.this_file};
    xpp::odex::Statement statement;
    statement.kind=xpp::odex::Statement::Kind::Ode;
    statement.name="x";
    statement.pos.line=7;
    m.statements.push_back(statement);
    m.node=1;
    m.uvar_names[0]="x";
    int count=0;
    auto append=[&](std::vector<int> p) { m.programs[count++]=std::move(p); };
    auto expression=[&](const std::string &text) {
        std::vector<int> p(MAXEXPLEN);
        int length=0;
        CHECK(xpp::add_expr(s,text,p.data(),&length)==0);
        p.resize(length);
        append(std::move(p));
    };
    for (double x:{0.1,1e-300,std::numeric_limits<double>::denorm_min(),-0.0}) append(number(x));
    for (const auto *text:{"par+x", "par-x", "par*x", "par/x", "1/0", "0/0",
         "if(x)then(if(par)then(0.1)else(1e-300))else(-0.0)",
         "2*if(x)then(3)else(4)+1", "if(x)then(3)else(if(par)then(4)else(5))"}) expression(text);
    m.ieee_division=true;
    expression("1/0"); expression("0/0"); expression("1/(-0.0)");
    /* Every pure built-in table entry, including noncommutative functions. */
    for (size_t i=0;i<xpp::expr::fun1.size();++i) {
        if (!xpp::expr::fun1[i]) continue;
        append({COM(CONTYPE,0),COM(FUN1TYPE,static_cast<int>(i)),ENDEXP});
    }
    for (size_t i=0;i<xpp::expr::fun2.size();++i) {
        if (!xpp::expr::fun2[i]) continue;
        append({COM(VARTYPE,1),COM(CONTYPE,0),COM(FUN2TYPE,static_cast<int>(i)),ENDEXP});
    }
    CHECK(xpp::add_ufun_name(s,"f",0,2)==0);
    CHECK(xpp::add_ufun_name(s,"g",1,2)==0);
    /* r draws and q sums: callers must call them in order, as statements */
    CHECK(xpp::add_ufun_name(s,"r",2,2)==0);
    CHECK(xpp::add_ufun_name(s,"q",3,1)==0);
    m.nfun=4;
    const std::array<std::string,2> args{"A","B"};
    CHECK(xpp::add_ufun_new(s,0,"a-b",args)==0);
    CHECK(xpp::add_ufun_new(s,1,"f(a,f(b,1))*2",args)==0);
    CHECK(xpp::add_ufun_new(s,2,"a+ran(b)",args)==0);
    CHECK(xpp::add_ufun_new(s,3,"sum(0,2)of(a*i')",{args.data(),1})==0);
    expression("g(x,par)+f(par,x)");
    /* sums (nested ones share I'), shifts, set, the index, random draws and delays:
       state-changing operations keep the interpreter's order, even in a branch,
       a function's call or a sum's body */
    for (const auto *text:{"sum(0,3)of(i'*par)", "sum(1,2)of(sum(1,3)of(i'+x))+i'", "sum(3,1)of(1)",
         "sum(0,2)of(shift(z0,i'))", "x+sum(0,1)of(set(z0,1,x*2))+z1",
         "shift(z0,1)+ishift(z0,1)+shift(par,1)", "z1+set(z0,1,7)+z1", "set(z0,1,z1+1)*set(z0,2,z1)",
         "if(x)then(set(z0,2,3)+ran(1))else(z2)+z2", "z2+if(par)then(set(z0,2,x+1))else(z1)",
         "if(x)then(sum(0,2)of(z1+i'))else(z1)*i'", "if(par)then(if(x)then(set(z0,1,1))else(ran(1)))else(z1)+z1",
         "ran(1)-ran(1)", "normal(par,2)*poisson(3)", "normal(ran(1),ran(2))", "ran(par)+if(x)then(ran(2))else(normal(1,2))",
         "@+x", "delay(z0,2)+delay(x,1)", "del_shft(z0,1,2)-x", "q(x)+r(x,par)-r(par,x)", "x*q(par)+z0*r(x,x)+r(1,z1)",
         "r(ran(1),ran(2))", "r(set(z0,1,1),z1)"}) expression(text);
    auto compiled=xpp::compile_model(s);
    if (!compiled) printf("%s\n",compiled.error().text().c_str());
    CHECK(compiled.has_value());
    CHECK(m.native_program!=nullptr);
    for (double x:{-0.0,0.0,1.0,-2.5}) {
        for (size_t i=0;i<s.parser.variables.size();++i) s.parser.variables[i]=0.5*static_cast<double>(i%7)-1.0;
        s.parser.variables[1]=x;
        s.parser.constants[2]=x+4.0;
        for (int i=0;i<count;++i) same_both_ways(s,i);
    }
    /* Unsupported instructions remove all compiled equations. The returned
       warning is rendered by the loader once, with this equation's place. */
    m.programs[0]=nested_sums(16,ENDEXP);
    CHECK(xpp::compile_model(s).has_value());
    m.programs[0]=nested_sums(17,ENDEXP);
    auto unsupported=xpp::compile_model(s);
    CHECK(!unsupported);
    CHECK(!m.native_program);
    CHECK(m.native_functions[1]==nullptr);
    if (!unsupported) {
        CHECK(unsupported.error().place.file==m.this_file);
        CHECK(unsupported.error().place.line==7);
        xpp::LogCapture messages;
        xpp::log(XPP_LOG_WARN,"{}\n",unsupported.error().text());
        CHECK(messages.text().find("compile.odex:7: not compiled: sums nested too deep; running the interpreter")!=std::string::npos);
        messages.clear();
    }
    CHECK(xpp::eval_program(s,0)==0.0);
    m.programs[0]={COM(CONTYPE,MAXPAR),ENDEXP};
    CHECK(!xpp::compile_model(s));
    m.programs[0]={COM(VARTYPE,MAXODE1),ENDEXP};
    CHECK(!xpp::compile_model(s));
    m.programs[0]={COM(FUN1TYPE,26),ENDEXP};
    CHECK(!xpp::compile_model(s));
    m.programs[0]={MYIF,999999,ENDEXP};
    CHECK(!xpp::compile_model(s));
    m.programs[0]={NUMSYM,0};
    CHECK(!xpp::compile_model(s));
    program.compile=false;
    m.programs[0]=number(0.1);
    CHECK(xpp::compile_model(s).has_value());
    CHECK(!m.native_program);
    CHECK(xpp::eval_program(s,0)==0.1);
    program.compile=true;
    /* Models with lookup tables, networks, vectorizers, kernels, delays, sums and shifts,
       Markov chains, integral equations and draws: every equation of each, as loaded, from
       its own state and a changed one, and the loaded model compiled whole. */
    for (const auto *path:{"examples/canonical/swindale.odex", "examples/canonical/gb_wnet.odex", "examples/ode/koho.odex",
         "examples/ode/angela.odex", "examples/ode/gausvolt.odex", "examples/ode/kuramot100.odex",
         "examples/ode/waterwheel.odex", "examples/canonical/mackey.odex", "examples/ode/kohox.odex",
         "examples/ode/kepler.odex", "examples/ode/lamvolt.odex", "examples/ode/junk2.odex"}) {
        std::string file=path;
        char name[]="test_compile";
        char *argv[]={name,file.data(),nullptr};
        auto loaded=xpp::load_model(2,argv,1);
        CHECK(loaded.has_value());
        if (!loaded) continue;
        xpp::Session &t=xpp::client_session();
        CHECK(t.model().native_program!=nullptr);
        for (int round=0;round<2;++round) {
            for (size_t i=0;i<t.parser.variables.size();++i) t.parser.variables[i]+=0.01*static_cast<double>(round*(i%5));
            for (size_t i=0;i<t.model().programs.size();++i)
                if (!t.model().programs[i].empty()) same_both_ways(t,static_cast<int>(i));
        }
    }
    TEST_REPORT("compiled equations");
}
