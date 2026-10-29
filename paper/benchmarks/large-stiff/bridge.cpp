#include "bridge.h"
#include "factor_timer.h"
#include "session.h"
#include "model.h"
#include "xpp_batch.h"
#include "my_rhs.h"
#include "expr_native.h"
#include "cvdense.h"
#include "cvband.h"
#include <valgrind/callgrind.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int cache_factorizations=3; // enough calls to expose reuse, not a full trajectory
struct CacheComplete {};
struct Study {
    xpp::Session *session=nullptr; // owned by load_model's session list
    std::string error;
    std::vector<double> point, workspace, values;
    int band=0;
    int cache_factors=0;
    std::chrono::steady_clock::time_point cache_start;
    double cache_seconds=0;
    decltype(CVodeMemRec{}.cv_lsetup) cache_setup=nullptr;
};
int cache_setup(CVodeMem memory,int failure,N_Vector y,N_Vector f,bool *current,
                N_Vector tmp1,N_Vector tmp2,N_Vector tmp3) {
    auto &s=*static_cast<Study *>(memory->cv_f_data);
    if(s.cache_factors==0) {
        s.cache_start=std::chrono::steady_clock::now();
        CALLGRIND_START_INSTRUMENTATION;
    }
    const int flag=s.cache_setup(memory,failure,y,f,current,tmp1,tmp2,tmp3);
    ++s.cache_factors;
    if(flag!=0) {
        CALLGRIND_STOP_INSTRUMENTATION;
        throw std::runtime_error("cache LU setup failed before the three-call cap");
    }
    if(s.cache_factors==cache_factorizations) {
        CALLGRIND_STOP_INSTRUMENTATION;
        s.cache_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-s.cache_start).count();
        // The legacy CVODE is compiled C++; unwind its owned run normally.
        throw CacheComplete{};
    }
    return flag;
}
template<class F> int guard(Study &s, F f) noexcept {
    try { f(); return 0; }
    catch(const std::exception &e) { s.error=e.what(); }
    catch(...) { s.error="study callback failed"; }
    return -1;
}
void rhs(int n, double t, N_Vector y, N_Vector f, void *data) {
    if(study_rhs(data,t,y->data,f->data)) throw std::runtime_error(study_error(data));
    (void)n;
}
}
extern "C" void *study_load(const char *path) {
    auto p=std::make_unique<Study>();
    guard(*p,[&] {
        std::string name="w378", file=path;
        char *argv[]={name.data(),file.data(),nullptr};
        auto loaded=xpp::load_model(2,argv,1);
        if(!loaded) throw std::runtime_error(loaded.error().text());
        p->session=*loaded;
        if(!xpp::ensure_exact_jacobian(**loaded)) throw std::runtime_error((*loaded)->model().jacobian.unavailable);
        const auto &m=(*loaded)->model();
        const auto &j=m.jacobian.states;
        for(std::size_t k=0;k<j.rows.size();++k)
            p->band=std::max(p->band,std::abs(j.rows[k]-j.columns[k]));
        p->point.resize(static_cast<std::size_t>(m.node)+1);
        p->workspace.resize(m.jacobian.workspace);
        p->values.resize(j.rows.size());
    });
    return p.release();
}
extern "C" const char *study_error(void *p) { return static_cast<Study *>(p)->error.c_str(); }
extern "C" int study_size(void *p) { return static_cast<Study *>(p)->session->model().node; }
extern "C" int study_nnz(void *p) { return static_cast<int>(static_cast<Study *>(p)->values.size()); }
extern "C" int study_band(void *p) { return static_cast<Study *>(p)->band; }
extern "C" int study_initial(void *p,double *y) {
    auto &s=*static_cast<Study *>(p);
    return guard(s,[&] {std::copy_n(s.session->last_ic.data(),study_size(p),y);});
}
extern "C" int study_rhs(void *p,double t,double *y,double *f) {
    auto &s=*static_cast<Study *>(p);
    return guard(s,[&] { my_rhs(*s.session,t,y,f,study_size(p)); });
}
extern "C" double study_rhs_parity(void *p) {
    auto &s=*static_cast<Study *>(p);
    double worst=-1;
    guard(s,[&] {
        const int n=study_size(p);
        std::vector<double> y(n), native(n), interpreted(n);
        study_initial(p,y.data());
        my_rhs(*s.session,0,y.data(),native.data(),n);
        const auto programs=xpp::model_programs(s.session->model());
        struct Restore {
            const std::vector<xpp::ModelProgram> &programs;
            std::vector<decltype(xpp::Program{}.native)> saved;
            ~Restore() { for(std::size_t k=0;k<programs.size();k++) programs[k].code->native=saved[k]; }
        } restore{programs,{}};
        for(const auto &entry:programs) restore.saved.push_back(entry.code->native);
        for(const auto &entry:programs) entry.code->native=nullptr;
        my_rhs(*s.session,0,y.data(),interpreted.data(),n);
        worst=0;
        for(int i=0;i<n;i++) {
            const double delta=std::abs(native[i]-interpreted[i]);
            // Strict relative check, with an absolute floor only at exact zero.
            const double scale=std::max(std::abs(native[i]),std::abs(interpreted[i]));
            worst=std::max(worst,scale?delta/scale:delta);
        }
    });
    return worst;
}
extern "C" int study_sparse(void *p,double t,double *y,int *row,int *col,double *value) {
    auto &s=*static_cast<Study *>(p);
    return guard(s,[&] {
        s.point[0]=t;
        std::copy_n(y,study_size(p),s.point.begin()+1);
        const auto &j=s.session->model().jacobian.states;
        // Evaluate the compiled sparse derivative program, no dense allocation.
        j.function(s.session->parser.constants.data(),s.point.data(),s.session,s.workspace.data(),s.values.data());
        for(std::size_t k=0;k<s.values.size();++k) {
            if(!std::isfinite(s.values[k])) throw std::runtime_error("nonfinite exact Jacobian entry");
            row[k]=j.rows[k]; col[k]=j.columns[k]; value[k]=s.values[k];
        }
    });
}
extern "C" int study_legacy(void *p,const char *solver,double tol,double total,int rows,const char *output,const char *stats) {
    auto &s=*static_cast<Study *>(p);
    return guard(s,[&] {
        const int n=study_size(p);
        std::unique_ptr<N_VectorContent,decltype(&N_VFree)> y(N_VNew(n,nullptr),N_VFree);
        if(study_initial(p,y->data)) throw std::runtime_error(s.error);
        std::array<int,OPT_SIZE> iopt{};
        std::array<double,OPT_SIZE> ropt{};
        // Same BDF/Newton/scalar settings as W34's adapter, 2000 steps/call.
        void *memory=CVodeMalloc(n,rhs,0,y.get(),BDF,NEWTON,SS,&tol,&tol,p,false,iopt.data(),ropt.data(),nullptr);
        if(!memory) throw std::runtime_error("CVodeMalloc failed");
        std::unique_ptr<void,decltype(&CVodeFree)> cv(memory,CVodeFree);
        const bool band=std::string(solver)=="legacy-band";
        if(band) CVBand(memory,s.band,s.band,nullptr,nullptr);
        else CVDense(memory,nullptr,nullptr);
        const bool cache=std::getenv("W378_CACHE_PROFILE")!=nullptr;
        if(cache) {
            if(band) throw std::runtime_error("cache profiling requires legacy dense");
            auto *m=static_cast<CVodeMem>(memory);
            s.cache_factors=0;
            s.cache_setup=m->cv_lsetup;
            m->cv_lsetup=cache_setup;
        }
        std::ofstream out(output);
        if(!out) throw std::runtime_error("cannot open trajectory");
        out<<std::setprecision(17);
        try { for(int k=0;k<rows;k++) {
            double t=total*k/(rows-1);
            if(k && CVode(memory,t,y.get(),&t,NORMAL)!=SUCCESS) throw std::runtime_error("legacy CVode failed");
            out<<t; for(int j=0;j<n;j++) out<<' '<<y->data[j]; out<<'\n';
        } } catch(const CacheComplete &) {
            std::ofstream report(stats);
            report<<std::setprecision(17)<<"{\"cache_complete\":true,\"factorizations\":"<<s.cache_factors
                  <<",\"profile_wall_seconds\":"<<s.cache_seconds<<"}\n";
            if(!report) throw std::runtime_error("cache stats write failed");
            return;
        }
        if(cache) throw std::runtime_error("run ended before the cache factorization cap");
        std::ofstream report(stats);
        report<<"{\"steps\":"<<iopt[NST]<<",\"rhs\":"<<iopt[NFE]<<",\"newton\":"<<iopt[NNI]
              <<",\"setups\":"<<iopt[NSETUPS]<<",\"jacobians\":"<<iopt[DENSE_NJE]
              <<",\"linear_iterations\":null,\"linear_iterations_note\":\"direct solve; not exposed\"}\n";
        if(!report || !out) throw std::runtime_error("trajectory/stats write failed");
    });
}
extern "C" void study_free(void *p) { delete static_cast<Study *>(p); }
extern "C" int __real_DenseFactor(DenseMat,integer *);
extern "C" int __wrap_DenseFactor(DenseMat a,integer *p) {
    return factor_event("legacy-dense",[&]{return __real_DenseFactor(a,p);});
}
extern "C" int __real_BandFactor(BandMat,integer *);
extern "C" int __wrap_BandFactor(BandMat a,integer *p) {
    return factor_event("legacy-band",[&]{return __real_BandFactor(a,p);});
}
