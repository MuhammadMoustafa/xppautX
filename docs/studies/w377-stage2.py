"""W377 production-path counts and 17-digit trajectory comparisons.
Run fixtures/prepare, heavy.sh bash build/w377-stage2/build.sh,
heavy.sh python docs/studies/w377-stage2.py run, then summarize.
Instrumentation exists only in the generated translation unit.
"""
from pathlib import Path
import importlib.util
import subprocess
import hashlib
import json
import sys
import csv

spec=importlib.util.spec_from_file_location('stage1',Path(__file__).with_name('w377-measure.py'))
study=importlib.util.module_from_spec(spec)
spec.loader.exec_module(study)
ROOT=study.ROOT
OUT=ROOT/'build/w377-stage2'
OUT.mkdir(parents=True,exist_ok=True)
study.OUT=OUT

DRIVER=r'''
    // Disposable measurement driver: recursive calls exercise this production solver.
    static bool measuring=false;
    if(getenv("W377_FILE")&&!measuring){
      measuring=true;
      auto &session=session_;
      session.numerics.tolerance=session.numerics.abs_tolerance=atof(getenv("W377_TOL"));
      const double dt=atof(getenv("W377_DT")),end=atof(getenv("W377_END"));
      const long points=static_cast<long>(floor(end/dt+1e-8));
      FILE *out=fopen(getenv("W377_FILE"),"w");
      if(!out)exit(91);
      std::vector<double> y(step.y,step.y+step.neq);
      double t=0;int start=1,status=1;long written=0;
      const auto row=[&]{fprintf(out,"%.17g",t);for(double value:y)fprintf(out,",%.17g",value);fprintf(out,"\n");};
      row();
      for(long j=1;j<=points;++j){
        const auto result=step_by({.y=y.data(),.t=&t,.neq=step.neq,.start=&start,.tout=j*dt});
        if(!result){fprintf(stderr,"%s\n",result.error().text().c_str());status=-2;break;}
        row();++written;
      }
      fclose(out);
      fprintf(stderr,"W377MAX %.17g\n",session.numerics.max_step);
      fprintf(stderr,"W377 %lld %ld %ld %d %ld\n",study_rhs,run_.naccpt,run_.nstep-run_.naccpt,status,written);
      exit(status==1?0:92);
    }
'''

def prepare():
    source=(ROOT/'core/dormpri.cpp').read_text()
    source=source.replace('static void dprhs(', 'static long long study_rhs=0;\nstatic void dprhs(',1)
    source=source.replace(' my_rhs(s,t,y,f,n);',' ++study_rhs;\n my_rhs(s,t,y,f,n);',1)
    needle='  Result<> step_by(const SolverStep &step) override\n  {'
    assert source.count(needle)==1
    source=source.replace(needle,needle+DRIVER,1)
    (OUT/'dormpri.cpp').write_text(source)
    (OUT/'build-info.json').write_text(json.dumps(dict(compiler=subprocess.check_output(['g++','--version'],text=True).splitlines()[0],flags='-std=gnu++23 -g -O2 -ffp-contract=off',core_archive_sha256=hashlib.sha256((ROOT/'build/ucrt/libxppcore.a').read_bytes()).hexdigest()),indent=2))
    result=subprocess.run(['mingw32-make','-n','-W','core/dormpri.cpp','xppautx','BUILDDIR=build/ucrt','WERROR=1'],capture_output=True,text=True,check=True)
    link=next(line for line in result.stdout.splitlines() if ' -o xppautX.exe ' in line)
    link=link.replace(' -o xppautX.exe ',' -o build/w377-stage2/study.exe build/w377-stage2/dormpri.o ')
    (OUT/'build.sh').write_text('#!/bin/bash\nset -e\ng++ -std=gnu++23 -g -O2 -ffp-contract=off -Icore -c build/w377-stage2/dormpri.cpp -o build/w377-stage2/dormpri.o\n'+link+'\n')

def summarize():
    before=json.loads((ROOT/'docs/studies/w377-counts.json').read_text())
    runs=json.loads((OUT/'counts.json').read_text())
    inputs=json.loads((OUT/'manifest.json').read_text())
    previous={r['name']:r for r in before['inputs']}
    for case in inputs:
        for key in ('sha256','fixture_sha256','set_sha256'):
            assert case.get(key)==previous[case['name']].get(key),(case['name'],key)
    assert len(runs)==len(inputs)*6 and all('status' in r for r in runs)
    for r in runs:
        if r['status']!=1:continue
        a,b=r['accepted'],r['rejected']
        expected=2+6*(a+b) if r['method']=='dopri5' else 2+15*a+11*b
        assert r['rhs']==expected,r
    bykey={(r['model'],r['method'],r['tolerance']):r for r in runs}
    rows=[]
    for r in runs:
        if r['tolerance']!=1e-6:continue
        r=dict(r)
        old=next(x for x in before['comparison'] if x['model']==r['model'] and x['method']==r['method'] and x['mode']=='now')
        r.update(before_rhs=old['rhs'],before_error=old['max_error'])
        ref=bykey[r['model'],'dopri8',1e-12]
        coarse=bykey[r['model'],'dopri8',1e-11]
        if r.get('status')==1 and ref.get('status')==1:
            analytic={'slow':lambda t:study.math.exp(-.01*t),'stiffish':study.math.cos}.get(r['model'])
            r['max_error']=study.difference(OUT/r['output'],OUT/ref['output'],analytic)
            r['reference_refinement']=study.difference(OUT/coarse['output'],OUT/ref['output'])
            assert r['max_error'] is not None
        rows.append(r)
    data=dict(head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
              production_solver_sha256=hashlib.sha256((ROOT/'core/dormpri.cpp').read_bytes()).hexdigest(),
              instrumented_solver_sha256=hashlib.sha256((OUT/'dormpri.cpp').read_bytes()).hexdigest(),
              production_source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'core').glob('*') if p.suffix in ('.cpp','.h')},
              build=json.loads((OUT/'build-info.json').read_text()),
              inputs=json.loads((OUT/'manifest.json').read_text()),runs=runs,comparison=rows)
    (ROOT/'docs/studies/w377-stage2.json').write_text(json.dumps(data,indent=2)+'\n')
    with (ROOT/'docs/studies/w377-stage2.csv').open('w',newline='') as f:
        fields=['model','method','max_step','before_rhs','rhs','before_error','max_error','reference_refinement','status']
        writer=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore',lineterminator='\n');writer.writeheader();writer.writerows(rows)
    for r in rows:print({k:r.get(k) for k in ('model','method','before_rhs','rhs','before_error','max_error','status')})

if __name__=='__main__':
    action=sys.argv[1]
    if action=='fixtures':study.fixtures()
    elif action=='prepare':prepare()
    elif action=='run':study.run(resume=True,modes=[('production',t) for t in (1e-6,1e-11,1e-12)])
    elif action=='summarize':summarize()
    else:raise ValueError(action)
