#include "xpptest.h"
#include "expr_native.h"
#include "expr_program.h"
#include "session.h"
#include "xpp_log.h"
#include <bit>
#include <cstdint>
#include <limits>

namespace {
std::vector<int> number(double x)
{
    const auto halves=xpp::expr::number_halves(x);
    return {NUMSYM,halves[1],halves[0],ENDEXP};
}
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
    m.nfun=2;
    const std::array<std::string,2> args{"A","B"};
    CHECK(xpp::add_ufun_new(s,0,"a-b",args)==0);
    CHECK(xpp::add_ufun_new(s,1,"f(a,f(b,1))*2",args)==0);
    expression("g(x,par)+f(par,x)");
    auto compiled=xpp::compile_model(s);
    if (!compiled) printf("%s\n",compiled.error().text().c_str());
    CHECK(compiled.has_value());
    CHECK(m.native_program!=nullptr);
    for (double x:{-0.0,0.0,1.0,-2.5}) {
        s.parser.variables[1]=x;
        s.parser.constants[2]=x+4.0;
        for (int i=0;i<count;++i) {
            const double interpreted=xpp::evaluate(s,m.programs[i].data());
            const double native=xpp::eval_program(s,i);
            CHECK(std::bit_cast<std::uint64_t>(native)==std::bit_cast<std::uint64_t>(interpreted));
        }
    }
    /* Unsupported instructions remove all compiled equations. The returned
       warning is rendered by the loader once, with this equation's place. */
    m.programs[0]={INDXCOM,ENDEXP};
    auto unsupported=xpp::compile_model(s);
    CHECK(!unsupported);
    CHECK(!m.native_program);
    CHECK(m.native_functions[1]==nullptr);
    if (!unsupported) {
        CHECK(unsupported.error().place.file==m.this_file);
        CHECK(unsupported.error().place.line==7);
        xpp::LogCapture messages;
        xpp::log(XPP_LOG_WARN,"{}\n",unsupported.error().text());
        CHECK(messages.text().find("compile.odex:7: not compiled: uses the vector index; running the interpreter")!=std::string::npos);
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
    m.no_compile=true;
    m.programs[0]=number(0.1);
    CHECK(xpp::compile_model(s).has_value());
    CHECK(!m.native_program);
    CHECK(xpp::eval_program(s,0)==0.1);
    TEST_REPORT("compiled equations");
}
