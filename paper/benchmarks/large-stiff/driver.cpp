// W378 standalone worker: W34's BDF/scalar-tolerance adapter settings.
#include "bridge.h"
#include "factor_timer.h"
#include <cvode/cvode.h>
#include <cvode/cvode_ls.h>
#include <nvector/nvector_serial.h>
#include <sunlinsol/sunlinsol_band.h>
#include <sunlinsol/sunlinsol_spgmr.h>
#include <sunmatrix/sunmatrix_band.h>
#include <sunmatrix/sunmatrix_sparse.h>
#ifdef W378_KLU
#include <sunlinsol/sunlinsol_klu.h>
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr long max_steps_per_call=2000; // matches both legacy CVODE and W34
constexpr int krylov_dimension=30; // restarted GMRES with exact band LU on left
int measured_band_setup(SUNLinearSolver s,SUNMatrix a) {
    return factor_event("sundials-band",[&]{return SUNLinSolSetup_Band(s,a);});
}
#ifdef W378_KLU
int measured_klu_setup(SUNLinearSolver s,SUNMatrix a) {
    return factor_event("sundials-klu",[&]{return SUNLinSolSetup_KLU(s,a);});
}
#endif
void check(int flag) { if(flag<0) throw std::runtime_error("SUNDIALS flag "+std::to_string(flag)); }
struct Run {
    std::unique_ptr<void,decltype(&study_free)> model{nullptr,study_free};
    SUNContext context=nullptr;
    N_Vector y=nullptr;
    SUNMatrix matrix=nullptr, preconditioner=nullptr;
    SUNLinearSolver linear=nullptr, prelinear=nullptr;
    void *cv=nullptr;
    std::vector<int> row,col,order;
    std::vector<double> value;
    bool sparse=false;
    int band=0, n=0;
    explicit Run(const char *path):model(study_load(path),study_free) {
      try {
        if(*study_error(model.get())) throw std::runtime_error(study_error(model.get()));
        n=study_size(model.get()); band=study_band(model.get());
        const int nnz=study_nnz(model.get());
        row.resize(nnz); col.resize(nnz); value.resize(nnz); order.resize(nnz);
        check(SUNContext_Create(SUN_COMM_NULL,&context));
        y=N_VNew_Serial(n,context);
        if(!y) throw std::runtime_error("N_VNew_Serial failed");
        check(study_initial(model.get(),N_VGetArrayPointer(y)));
        entries(0,y);
        std::iota(order.begin(),order.end(),0);
        std::sort(order.begin(),order.end(),[&](int a,int b){return col[a]!=col[b]?col[a]<col[b]:row[a]<row[b];});
      } catch(...) { release(); throw; }
    }
    ~Run() { release(); }
    void release() noexcept {
        if(cv) CVodeFree(&cv);
        if(linear) SUNLinSolFree(linear);
        if(prelinear) SUNLinSolFree(prelinear);
        if(matrix) SUNMatDestroy(matrix);
        if(preconditioner) SUNMatDestroy(preconditioner);
        if(y) N_VDestroy(y);
        if(context) SUNContext_Free(&context);
    }
    void entries(double t,N_Vector state) {
        check(study_sparse(model.get(),t,N_VGetArrayPointer(state),row.data(),col.data(),value.data()));
    }
    static int rhs(double t,N_Vector y,N_Vector f,void *p) noexcept {
        auto &r=*static_cast<Run *>(p);
        return study_rhs(r.model.get(),t,N_VGetArrayPointer(y),N_VGetArrayPointer(f));
    }
    static int jac(double t,N_Vector y,N_Vector,SUNMatrix a,void *p,N_Vector,N_Vector,N_Vector) noexcept {
        try {
            auto &r=*static_cast<Run *>(p); r.entries(t,y);
            check(SUNMatZero(a));
            if(r.sparse) {
                auto *ptr=SUNSparseMatrix_IndexPointers(a), *idx=SUNSparseMatrix_IndexValues(a);
                auto *data=SUNSparseMatrix_Data(a);
                for(int c=0,k=0;c<=r.n;c++) {
                    ptr[c]=k;
                    while(k<static_cast<int>(r.order.size()) && r.col[r.order[k]]==c) {
                        const int e=r.order[k]; idx[k]=r.row[e]; data[k]=r.value[e]; ++k;
                    }
                }
            } else {
                for(std::size_t k=0;k<r.row.size();k++) SM_ELEMENT_B(a,r.row[k],r.col[k])=r.value[k];
            }
            return 0;
        } catch(...) {return -1;}
    }
    static int psetup(double t,N_Vector y,N_Vector,sunbooleantype, sunbooleantype *current,double gamma,void *p) noexcept {
        try {
            auto &r=*static_cast<Run *>(p); r.entries(t,y);
            check(SUNMatZero(r.preconditioner));
            for(std::size_t k=0;k<r.row.size();k++)
                SM_ELEMENT_B(r.preconditioner,r.row[k],r.col[k])=-gamma*r.value[k];
            for(int i=0;i<r.n;i++) SM_ELEMENT_B(r.preconditioner,i,i)+=1;
            *current=true;
            return SUNLinSolSetup(r.prelinear,r.preconditioner);
        } catch(...) {return -1;}
    }
    static int psolve(double,N_Vector,N_Vector,N_Vector b,N_Vector x,double,double,int,void *p) noexcept {
        auto &r=*static_cast<Run *>(p);
        return SUNLinSolSolve(r.prelinear,r.preconditioner,x,b,0);
    }
    void integrate(const std::string &solver,double tol,double total,int rows,const char *output,const char *stats) {
        cv=CVodeCreate(CV_BDF,context);
        if(!cv) throw std::runtime_error("CVodeCreate failed");
        check(CVodeInit(cv,rhs,0,y)); check(CVodeSetUserData(cv,this));
        check(CVodeSStolerances(cv,tol,tol)); check(CVodeSetMaxNumSteps(cv,max_steps_per_call));
        if(solver=="gmres") {
            preconditioner=SUNBandMatrix(n,band,band,context);
            prelinear=SUNLinSol_Band(y,preconditioner,context);
            if(prelinear) prelinear->ops->setup=measured_band_setup;
            linear=SUNLinSol_SPGMR(y,SUN_PREC_LEFT,krylov_dimension,context);
        } else if(solver=="reference") {
            matrix=SUNBandMatrix(n,band,band,context);
            linear=SUNLinSol_Band(y,matrix,context);
            if(linear) linear->ops->setup=measured_band_setup;
        } else if(solver=="klu") {
#ifdef W378_KLU
            sparse=true;
            matrix=SUNSparseMatrix(n,n,static_cast<sunindextype>(row.size()),CSC_MAT,context);
            linear=SUNLinSol_KLU(y,matrix,context);
            if(linear) linear->ops->setup=measured_klu_setup;
#else
            throw std::runtime_error("KLU unavailable: install libsuitesparse-dev and rebuild SUNDIALS with ENABLE_KLU=ON");
#endif
        } else throw std::runtime_error("unknown solver "+solver);
        if(!linear || (solver=="gmres" && (!prelinear || !preconditioner))) throw std::runtime_error("linear solver allocation failed");
        check(CVodeSetLinearSolver(cv,linear,matrix));
        if(solver=="gmres") check(CVodeSetPreconditioner(cv,psetup,psolve));
        else check(CVodeSetJacFn(cv,jac));
        std::ofstream out(output);
        if(!out) throw std::runtime_error("cannot open trajectory");
        out<<std::setprecision(17);
        for(int k=0;k<rows;k++) {
            double t=total*k/(rows-1);
            if(k) check(CVode(cv,t,y,&t,CV_NORMAL));
            out<<t;
            for(int j=0;j<n;j++) out<<' '<<N_VGetArrayPointer(y)[j];
            out<<'\n';
        }
        long steps=0,fe=0,ni=0,li=0,je=0,setups=0,pe=0,ps=0;
        check(CVodeGetNumSteps(cv,&steps)); check(CVodeGetNumRhsEvals(cv,&fe));
        check(CVodeGetNumNonlinSolvIters(cv,&ni)); check(CVodeGetNumLinIters(cv,&li));
        check(CVodeGetNumJacEvals(cv,&je)); check(CVodeGetNumLinSolvSetups(cv,&setups));
        if(solver=="gmres") {check(CVodeGetNumPrecEvals(cv,&pe)); check(CVodeGetNumPrecSolves(cv,&ps));}
        std::ofstream report(stats);
        report<<"{\"steps\":"<<steps<<",\"rhs\":"<<fe<<",\"newton\":"<<ni<<",\"linear_iterations\":"<<li
              <<",\"jacobians\":"<<(solver=="gmres"?pe:je)<<",\"setups\":"<<setups
              <<",\"preconditioner_solves\":"<<ps<<"}\n";
        if(!report || !out) throw std::runtime_error("trajectory/stats write failed");
    }
};
}

int main(int argc,char **argv) {
    try {
        if(argc!=8) throw std::runtime_error("usage: worker MODEL SOLVER TOL TOTAL ROWS OUTPUT STATS");
        const std::string solver=argv[2];
        const double tol=std::stod(argv[3]), total=std::stod(argv[4]);
        const int rows=std::stoi(argv[5]);
        if(!(tol>0 && total>0 && rows>=2)) throw std::runtime_error("invalid tolerance, total or rows");
        Run r(argv[1]);
        if(solver=="probe") {
            std::vector<double> f(r.n);
            check(study_rhs(r.model.get(),0,N_VGetArrayPointer(r.y),f.data()));
            std::ofstream out(argv[6]);
            out<<std::setprecision(17); for(double v:f) out<<v<<'\n';
            std::ofstream meta(argv[7]);
            const double parity=study_rhs_parity(r.model.get());
            if(parity<0 || parity>1e-12) throw std::runtime_error("compiled/interpreted RHS parity failed");
            meta<<std::setprecision(17)<<"{\"n\":"<<r.n<<",\"nnz\":"<<r.row.size()<<",\"band\":"<<r.band
                <<",\"rhs_relative_difference\":"<<parity<<"}\n";
        } else if(solver=="legacy-dense" || solver=="legacy-band") {
            if(study_legacy(r.model.get(),argv[2],tol,total,rows,argv[6],argv[7]))
                throw std::runtime_error(study_error(r.model.get()));
        }
        else r.integrate(solver,tol,total,rows,argv[6],argv[7]);
        return 0;
    } catch(const std::exception &e) {std::cerr<<argv[1]<<": "<<e.what()<<'\n'; return 1;}
}
