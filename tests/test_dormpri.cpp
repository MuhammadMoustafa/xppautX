#include "xpptest.h"
#include "session.h"
#include "xpp_batch.h"
#include "dormpri.h"
#include "flags.h"
#include "expr.h"
#include "delay_handle.h"
#include "xpp_math.h"
#include <array>
#include <cmath>

int main()
{
    char name[]="test_dormpri", file[]="tools/models/w377-dense.odex";
    char *argv[]={name,file,nullptr};
    const auto loaded=xpp::load_model(2,argv,1);
    CHECK(loaded.has_value());
    if(!loaded)TEST_REPORT("DoPri dense");
    auto &s=**loaded;
    s.numerics.max_step=20;
    s.numerics.tolerance=s.numerics.abs_tolerance=1e-9;
    s.numerics.bound=1e8;
    for(auto method:{xpp::method::DP5,xpp::method::DP83}){
        const auto fresh=[&]{
            CHECK(xpp::set_val(s,"rate",.01));
            s.model().flags.clear();s.numerics.poincare_map=0;s.integrator.stop_flag=0;
            return xpp::make_dormand_prince(xpp::solver_info(method),s,3);
        };
        auto solver=fresh();
        std::array<double,3> y{1,0,0};double t=0;int start=1;
        for(int i=1;i<=2000;++i){
            const double out=i*.05;
            CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=out,.sections=true}).has_value());
            CHECK(t==out);
            double current=0;
            CHECK(xpp::get_val(s,"x",&current)&&current==y[0]);
            CHECK(xpp::get_val(s,"t",&current)&&current==t);
            CHECK(fabs(y[0]-xpp::math::exp(-.01*t))<2e-8);
            CHECK(fabs(y[1]-t)<1e-10);
        }
        // Backwards samples use the same polynomial with signed step sizes.
        solver=fresh();y={xpp::math::exp(-1),100,xpp::math::sin(100)};t=100;start=1;
        for(int i=1;i<=100;++i){
            CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=100-i*1.0,.sections=true}).has_value());
            CHECK(t==100-i*1.0);CHECK(fabs(y[0]-xpp::math::exp(-.01*t))<2e-8);
        }
        // Section and every state at a root strictly between scheduled samples.
        solver=fresh();s.numerics.poincare_map=1;s.numerics.poincare_variable=2;
        s.numerics.poincare_plane=.371;s.numerics.poincare_sign=1;
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(solver->section_found());CHECK(fabs(t-.371)<1e-12);
        CHECK(fabs(y[0]-xpp::math::exp(-.01*t))<2e-8);CHECK(fabs(y[1]-.371)<1e-12);
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());CHECK(t==1);
        solver=fresh();s.numerics.poincare_map=1;s.numerics.poincare_variable=4;
        s.numerics.poincare_plane=.742;s.numerics.poincare_sign=1;
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(solver->section_found());CHECK(fabs(t-.371)<1e-12);
        solver=fresh();s.numerics.poincare_map=1;s.numerics.poincare_variable=2;
        s.numerics.poincare_plane=.371;s.numerics.poincare_sign=1;
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1}).has_value());
        CHECK(t==1);CHECK(!solver->section_found());
        // Reset discards the unconsumed step and its FSAL derivative.
        solver=fresh();
        CHECK(add_global(s,"t-.371",1,{{"x","2"}})==0);CHECK(compile_flags(s)==0);
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=.36,.sections=true}).has_value());
        CHECK(t==.36);CHECK(fabs(y[0]-xpp::math::exp(-.0036))<2e-8);
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(t==1);CHECK(fabs(y[0]-2*xpp::math::exp(-.01*(1-.371)))<2e-8);
        // A halt returns the root time and the complete root state.
        solver=fresh();
        CHECK(add_global(s,"t-.371",1,{{"arret","1"}})==0);CHECK(compile_flags(s)==0);
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(s.integrator.stop_flag);CHECK(fabs(t-.371)<1e-12);CHECK(fabs(y[1]-.371)<1e-12);
        solver=fresh();
        CHECK(add_global(s,"t-.371",1,{{"rate","2"}})==0);CHECK(compile_flags(s)==0);
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(fabs(y[0]-xpp::math::exp(-.01*.371-2*(1-.371)))<2e-8);
        solver=fresh();
        CHECK(add_global(s,"t-.371",1,{{"arret","1"}})==0);CHECK(compile_flags(s)==0);
        s.model().flags[0].nointerp=1;
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(t>=.371&&t<=1);CHECK(s.integrator.stop_flag);
        // Multiple crossings in one output interval, and simultaneous resets.
        solver=fresh();
        CHECK(add_global(s,"sin(20*t-.2)",1,{{"x","x+1"}})==0);CHECK(compile_flags(s)==0);
        y={0,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        double expected=0;
        for(double root=.01;root<1;root+=2*xpp::math::acos(-1.0)/20)expected+=xpp::math::exp(-.01*(1-root));
        CHECK(fabs(y[0]-expected)<2e-8);
        solver=fresh();
        CHECK(add_global(s,"t-.371",1,{{"x","2"}})==0);
        CHECK(add_global(s,"t-.371",1,{{"y","7"}})==0);CHECK(compile_flags(s)==0);
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());
        CHECK(fabs(y[0]-2*xpp::math::exp(-.01*(1-.371)))<2e-8);CHECK(fabs(y[1]-7.629)<1e-11);
        // Extrema are zeros of the actual RHS, with all components interpolated.
        solver=fresh();s.numerics.poincare_map=2;s.numerics.poincare_variable=3;s.numerics.poincare_sign=1;
        y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=4,.sections=true}).has_value());
        CHECK(solver->section_found());CHECK(fabs(t-xpp::math::acos(-1.0)/2)<1e-12);CHECK(fabs(y[2]-1)<2e-8);
        solver=fresh();s.numerics.poincare_map=2;s.numerics.poincare_variable=3;s.numerics.poincare_sign=1;
        y={xpp::math::exp(-.04),4,xpp::math::sin(4)};t=4;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=0,.sections=true}).has_value());
        CHECK(solver->section_found());CHECK(fabs(t-xpp::math::acos(-1.0)/2)<1e-12);CHECK(fabs(y[2]-1)<2e-8);
        solver=fresh();
        CHECK(add_global(s,"t-.371",-1,{{"arret","1"}})==0);CHECK(compile_flags(s)==0);
        y={xpp::math::exp(-.01),1,xpp::math::sin(1)};t=1;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=0,.sections=true}).has_value());
        CHECK(s.integrator.stop_flag);CHECK(fabs(t-.371)<1e-12);CHECK(fabs(y[1]-.371)<1e-12);
        // Torus-like caller edits invalidate the retained internal endpoint.
        solver=fresh();y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=.5,.sections=true}).has_value());
        y[1]-=.4;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());CHECK(fabs(y[1]-.6)<1e-12);
        // A sampled grazing zero is detected; a near miss must not fire.
        solver=fresh();
        CHECK(add_global(s,"(t-.5)^2",0,{{"arret","1"}})==0);CHECK(compile_flags(s)==0);
        std::array<double,3> scratch{},derivative{};
        const auto state=[](double time,std::span<double> values){values[0]=1;values[1]=time;values[2]=xpp::math::sin(time);};
        const auto grazing=xpp::dense_event(s,0,1,0,1,state,scratch,derivative);
        CHECK(grazing.found);CHECK(grazing.flag_count==1);CHECK(grazing.time==.5);
        s.model().flags.clear();
        CHECK(add_global(s,"(t-.5)^2+.0001",0,{{"arret","1"}})==0);CHECK(compile_flags(s)==0);
        CHECK(!xpp::dense_event(s,0,1,0,1,state,scratch,derivative).found);
        // A bound between output points stops before any later sample is valid.
        solver=fresh();s.numerics.bound=1.5;y={1,0,0};t=0;start=1;
        CHECK(solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=1,.sections=true}).has_value());CHECK(t==1);
        CHECK(!solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=2,.sections=true}));CHECK(fabs(t-1.5)<1e-12);
        s.numerics.bound=1e8;
        // A continuous run cannot renew the attempt budget at each output.
        solver=fresh();s.numerics.max_step=.0001;y={1,0,0};t=0;start=1;
        const auto capped=solver->advance({.y=y.data(),.t=&t,.neq=3,.start=&start,.tout=11,.sections=true});
        CHECK(!capped);
        if(!capped)CHECK(capped.error().what.find("100000")!=std::string::npos);
        CHECK(t<11);s.numerics.max_step=20;
        if(method==xpp::method::DP5){
          // Range/bounds suppression must not turn budget exhaustion into success.
          s.numerics.method=method;s.numerics.max_step=.0001;
          s.integrator.range_flag=1;xpp::start_solver(s);
          std::array<double,4> trajectory{1,0,0,0};t=0;start=1;
          const auto range=xpp::integrate(s,&t,trajectory.data(),11,11,0,1,&start);
          CHECK(!range);
          if(!range)CHECK(range.error().what.find("100000")!=std::string::npos);
          CHECK(t<11);s.integrator.range_flag=0;s.numerics.max_step=20;
        }
        s.numerics.bound=1e8;
    }
    // Delay queries retain their existing uniform-grid history contract.
    char delay_file[]="tools/models/w377-delay.odex";
    char *delay_argv[]={name,delay_file,nullptr};
    const auto delay_loaded=xpp::load_model(2,delay_argv,1);
    CHECK(delay_loaded.has_value());
    if(delay_loaded){
      auto &delay=**delay_loaded;
      for(auto method:{xpp::method::DP5,xpp::method::DP83}){
        CHECK(xpp::alloc_delay(delay,2).has_value());CHECK(xpp::do_init_delay(delay,2).has_value());
        auto solver=xpp::make_dormand_prince(xpp::solver_info(method),delay,1);
        double y=1,t=0;int start=1;
        xpp::stor_delay(delay,&y);
        for(int i=1;i<=10;++i){
          CHECK(solver->advance({.y=&y,.t=&t,.neq=1,.start=&start,.tout=i*.05,.sections=true}).has_value());
          CHECK(t==i*.05);CHECK(fabs(y-(1+t))<1e-12);
          xpp::stor_delay(delay,&y);
        }
      }
    }
    TEST_REPORT("DoPri dense");
}
