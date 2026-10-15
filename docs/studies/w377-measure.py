"""W377 disposable instrumentation; run from Git Bash through tools/heavy.sh.

Generates build/w377/dormpri.cpp, never edits core/. Build/link commands are
printed into build/w377/build.sh. The resulting executable is study-only.
"""
from pathlib import Path
import subprocess
import re
import json
import os
import hashlib
import csv
import sys
import math
import itertools

ROOT = Path(__file__).resolve().parents[2]
os.chdir(ROOT)
OUT = ROOT / 'build/w377'
OUT.mkdir(parents=True, exist_ok=True)

INSTRUMENT = r'''
// Disposable W377 instrumentation, absent from the production executable.
static long long study_rhs=0, study_accepted=0, study_rejected=0;
static FILE *study_file=nullptr;
static double study_dt=0, study_end=0;
static long study_index=1, study_points=0;
static std::vector<double> study_sample;
static void study_row(double t, const double *y, unsigned n) {
  fprintf(study_file,"%.17g",t);
  for(unsigned i=0;i<n;++i) fprintf(study_file,",%.17g",y[i]);
  fprintf(study_file,"\n");
}
static void study_dense(const DpRun &r, double x, unsigned n, bool eighth) {
  if(!study_file) return;
  while(study_index<=study_points && study_index*study_dt<=x) {
    const double t=study_index*study_dt, u=(t-r.xold)/(x-r.xold), v=1-u;
    std::vector<double> &y=study_sample;
    for(unsigned i=0;i<n;++i) {
      if(eighth)
        y[i]=r.rcont1[i]+u*(r.rcont2[i]+v*(r.rcont3[i]+u*(r.rcont4[i]+v*(r.rcont5[i]+u*(r.rcont6[i]+v*(r.rcont7[i]+u*r.rcont8[i]))))));
      else
        y[i]=r.rcont1[i]+u*(r.rcont2[i]+v*(r.rcont3[i]+u*(r.rcont4[i]+v*r.rcont5[i])));
    }
    study_row(t,y.data(),n);
    ++study_index;
  }
}
static void study_solout(long, double, double, double*, unsigned, int*) {}
static void study_run(xpp::Session &s, double *y, int n, int flag) {
  study_dt=atof(getenv("W377_DT"));
  study_end=atof(getenv("W377_END"));
  study_points=static_cast<long>(floor(study_end/study_dt+1e-8));
  study_end=study_points*study_dt;
  double tol=atof(getenv("W377_TOL"));
  const bool dense=std::string(getenv("W377_MODE"))=="continuous";
  study_file=fopen(getenv("W377_FILE"),"w");
  if(!study_file) exit(91);
  std::vector<double> work(30*n);
  study_sample.resize(n);
  study_row(0,y,n);
  int status=1;
  double t=0,h=0;
  const long calls=dense?1:study_points;
  for(long j=1;j<=calls;++j) {
    const double end=dense?study_end:j*study_dt;
    if(flag==0)
      status=dopri5(s,n,dprhs,t,y,end,&tol,&tol,0,study_solout,dense?2:0,0,0,0,0,0,0,h,0,0,1,dense?n:0,nullptr,0,work.data());
    else
      status=dop853(s,n,dprhs,t,y,end,&tol,&tol,0,study_solout,dense?2:0,0,0,0,0,0,0,h,0,0,1,dense?n:0,nullptr,0,work.data());
    if(status!=1) break;
    h=s.integrator.dp_hout;
    t=end;
    if(!dense) study_row(t,y,n);
  }
  fclose(study_file);
  fprintf(stderr,"W377 %lld %lld %lld %d %ld\n",study_rhs,study_accepted,study_rejected,status,dense?study_index-1:study_points);
  exit(status==1?0:92);
}
'''

def prepare():
    source = (ROOT / 'core/dormpri.cpp').read_text()
    source = source.replace('void dprhs(', INSTRUMENT + '\nvoid dprhs(', 1)
    source = source.replace(' my_rhs(s,t,y,f,n);', ' ++study_rhs;\n my_rhs(s,t,y,f,n);', 1)
    signature = 'int dormprin(xpp::Session &s, int *istart, double *y, double *t, int n, double tout, double *tol, double *atol, int flag, int *kflag, double *work)\n{'
    assert signature in source
    source = source.replace(signature, signature + '\n  if(getenv("W377_FILE")) study_run(s,y,n,flag);', 1)
    needle = '    return idid;'
    assert source.count(needle) == 2
    source = source.replace(needle, '    study_accepted+=r.naccpt; study_rejected+=r.nstep-r.naccpt;\n'+needle)
    needle = '      x = xph;'
    assert source.count(needle) == 2
    source = source.replace(needle, needle+'\n      if(iout==2) study_dense(r,x,n,true);',1)
    pos=source.index('} /* dop853 */')
    source=source[:pos]+source[pos:].replace(needle,needle+'\n      if(iout==2) study_dense(r,x,n,false);',1)
    (OUT / 'dormpri.cpp').write_text(source)
    # Ask make for its platform's exact production link command. The injected
    # object precedes the archive, so the original dormpri.o is never extracted.
    result=subprocess.run(['mingw32-make','-n','-W','core/dormpri.cpp','xppautx','BUILDDIR=build/ucrt','WERROR=1'],capture_output=True,text=True,check=True)
    link=next(line for line in result.stdout.splitlines() if ' -o xppautX.exe ' in line)
    link=link.replace(' -o xppautX.exe ',' -o build/w377/study.exe build/w377/dormpri.o ')
    (OUT/'build.sh').write_text('#!/bin/bash\nset -e\ng++ -std=gnu++23 -g -O2 -ffp-contract=off -Icore -c build/w377/dormpri.cpp -o build/w377/dormpri.o\n'+link+'\n')

def fixtures():
    # The paper directory is intentionally ignored by git. Copy inputs only;
    # the study never runs or writes in the main checkout.
    archive=Path('C:/gitRepos/xppautX/paper/benchmarks/five-model-screen/models')
    cases=[('lecar',ROOT/'examples/ode/lecar.ode',30,.05)]
    for name,total,dt in [('episodic',1999,.4),('lecdiff200',20,.05),('kuramot100',100,.05),('amari',100,.05),('clustor',500,1)]:
        cases.append((name,archive/(name+'.ode'),total,dt))
    (OUT/'slow.ode').write_text("x'=-.01*x\ninit x=1\ndone\n")
    (OUT/'stiffish.ode').write_text("x'=-20*(x-cos(t))-sin(t)\ninit x=1\ndone\n")
    cases += [('slow',OUT/'slow.ode',100,.05),('stiffish',OUT/'stiffish.ode',30,.05)]
    py=Path('C:/gitRepos/xppautX/paper/benchmarks/auto/models/PY_S1Bf_bench.ode')
    cases.append(('PY_S1Bf',py,2000,.01))
    manifest=[]
    for name,path,total,dt in cases:
        raw=path.read_text()
        # Replace final done, preserving formulas and initial values. Both
        # methods and all tolerances use exactly the same model settings.
        text=re.sub(r'(?im)^(?:done|d)[ \t]*\r?$', '',raw)
        if name=='PY_S1Bf':
            assert 'par Iapp=20000' in text
            text=text.replace('par Iapp=20000','par Iapp=2000')
        target=OUT/(name+'.ode')
        text+='\n@ meth=dopri5,total=1,dt=.05,trans=0,bound=1e20\ndone\n'
        target.write_text(text)
        record=dict(name=name,path=str(target.relative_to(ROOT)),total=total,dt=dt,source=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),fixture_sha256=hashlib.sha256(target.read_bytes()).hexdigest())
        if name=='episodic':
            record['set_sha256']=hashlib.sha256(path.with_suffix('.ode.set').read_bytes()).hexdigest()
        manifest.append(record)
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))

def run(resume=False):
    records=[]
    if resume and (OUT/'counts.json').exists():
        records=[r for r in json.loads((OUT/'counts.json').read_text()) if 'status' in r]
    completed={(r['model'],r['method'],r['mode'],r['tolerance']) for r in records}
    for case in json.loads((OUT/'manifest.json').read_text()):
        name=case['name']
        original=(ROOT/case['path']).read_text()
        for method in ('dopri5','dopri8'):
            path=OUT/(name+'-'+method+'.ode')
            path.write_text(original.replace('meth=dopri5','meth='+method))
            path.with_suffix('.odex').unlink(missing_ok=True)
            extra=[]
            if name=='episodic':
                setpath=OUT/(name+'-'+method+'.set')
                setsource=Path(case['source']).with_suffix('.ode.set')
                text=setsource.read_text()
                assert '8   Qual RK' in text
                text=text.replace('8   Qual RK','11   DorPrin5' if method=='dopri5' else '12   DorPri8(3)')
                setpath.write_text(text)
                extra=['--setfile',str(setpath)]
            for mode,tol in [('now',1e-6),('continuous',1e-6),('continuous',1e-11),('continuous',1e-12)]:
                if (name,method,mode,tol) in completed:
                    continue
                output=OUT/f'{name}-{method}-{mode}-{tol}.csv'
                env=dict(os.environ,W377_DT=str(case['dt']),W377_END=str(case['total']),W377_TOL=str(tol),W377_MODE=mode,W377_FILE=str(output))
                result=subprocess.run([str(OUT/'study.exe'),str(path),'--silent','--internset','0',*extra],capture_output=True,text=True,env=env,timeout=900)
                match=re.search(r'W377 (\d+) (\d+) (\d+) (-?\d+) (\d+)',result.stderr)
                record=dict(model=name,method=method,mode=mode,tolerance=tol,returncode=result.returncode,output=output.name)
                if match:
                    record.update(zip(('rhs','accepted','rejected','status','points'),map(int,match.groups())))
                else:
                    record['error']=(result.stdout+result.stderr)[-2000:]
                records.append(record)
                (OUT/'counts.json').write_text(json.dumps(records,indent=2))
                print(record,flush=True)

def difference(left,right,analytic=None):
    maximum=0.0
    rows=0
    with left.open() as a, right.open() as b:
        for la,lb in itertools.zip_longest(a,b):
            if la is None or lb is None:
                return None
            va=list(map(float,la.split(',')))
            vb=list(map(float,lb.split(',')))
            if va[0]!=vb[0] or len(va)!=len(vb):
                return None
            for x,y in zip(va[1:],vb[1:]):
                if not math.isfinite(x) or not math.isfinite(y):
                    return None
                if analytic:
                    y=analytic(va[0])
                maximum=max(maximum,abs(x-y))
            rows+=1
    return maximum if rows>1 else None

def summarize():
    counts=json.loads((OUT/'counts.json').read_text())
    manifest={case['name']:case for case in json.loads((OUT/'manifest.json').read_text())}
    for r in counts:
        if r.get('status')!=1:
            continue
        case=manifest[r['model']]
        intervals=math.floor(case['total']/case['dt']+1e-8)
        accepted,rejected=r['accepted'],r['rejected']
        if r['mode']=='now':
            expected=intervals+6*(accepted+rejected)+1 if r['method']=='dopri5' else intervals+12*accepted+11*rejected+1
        else:
            expected=2+6*(accepted+rejected) if r['method']=='dopri5' else 2+15*accepted+11*rejected
        assert r['rhs']==expected, r
    bykey={(r['model'],r['method'],r['mode'],r['tolerance']):r for r in counts}
    assert len(counts)==len(manifest)*8 and len(bykey)==len(counts)
    assert all('status' in r for r in counts), 'Uninstrumented run in study'
    rows=[]
    for r in counts:
        if r['tolerance'] not in (1e-6,1e-11):
            continue
        r=dict(r)
        ref=bykey[(r['model'],'dopri8','continuous',1e-12)]
        coarse=bykey[(r['model'],'dopri8','continuous',1e-11)]
        if r.get('status')==1 and ref.get('status')==1:
            analytic={'slow':lambda t:math.exp(-.01*t),'stiffish':math.cos}.get(r['model'])
            r['max_error']=difference(OUT/r['output'],OUT/ref['output'],analytic)
            r['reference_refinement']=difference(OUT/coarse['output'],OUT/ref['output'])
            if r['max_error'] is None or r['reference_refinement'] is None:
                raise ValueError('Invalid output grid or nonfinite states: '+r['output'])
        rows.append(r)
    destination=ROOT/'docs/studies/w377-counts.json'
    destination.write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),production_solver_sha256=hashlib.sha256((ROOT/'core/dormpri.cpp').read_bytes()).hexdigest(),instrumented_solver_sha256=hashlib.sha256((OUT/'dormpri.cpp').read_bytes()).hexdigest(),inputs=json.loads((OUT/'manifest.json').read_text()),runs=counts,comparison=rows),indent=2)+'\n')
    fields=['model','method','mode','tolerance','rhs','accepted','rejected','max_error','reference_refinement','status','points']
    with (ROOT/'docs/studies/w377-counts.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)
    tables=[]
    for case in json.loads((OUT/'manifest.json').read_text()):
        selected=[r for r in rows if r['model']==case['name']]
        refinement=next(r['reference_refinement'] for r in selected if 'reference_refinement' in r)
        tables += [f"### {case['name']}", '',f"T={math.floor(case['total']/case['dt']+1e-8)*case['dt']:g}, dt={case['dt']:g}; reference refinement delta={refinement:.3g}.", '',
                   '| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |',
                   '|---|---|---:|---:|---:|---:|---:|---|']
        for r in selected:
            error=f"{r['max_error']:.3g}" if 'max_error' in r else 'not complete'
            status='complete' if r['status']==1 else f"step cap at {r['points']} outputs"
            tables.append(f"| {r['method']} | {r['mode']} | {r['tolerance']:g} | {r['rhs']:,} | {r['accepted']:,} | {r['rejected']:,} | {error} | {status} |")
        tables.append('')
    study=ROOT/'docs/studies/w377-dense-output.md'
    text=study.read_text()
    start='<!-- W377 BEGIN DATA -->'
    end='<!-- W377 END DATA -->'
    replacement=start+'\n\n'+'\n'.join(tables)+'\n'+end
    text=re.sub(re.escape(start)+r'.*?'+re.escape(end),lambda _:replacement,text,flags=re.S)
    study.write_text(text)
    print(f'{len(counts)} instrumented runs; {len(rows)} comparison rows; tables generated.')

if __name__=='__main__':
    {'prepare':prepare,'fixtures':fixtures,'run':run,'resume':lambda:run(True),'summarize':summarize}[sys.argv[1]]()
