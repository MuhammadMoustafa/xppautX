#!/usr/bin/env -S uv run --locked python
"""Matched Linux headless screen. Timings include process startup, load, solve and output.
Run on an ext4 filesystem; --output can point at the paper folder after timing.
"""
import argparse, csv, gzip, hashlib, itertools, json, math, os
from pathlib import Path
import shutil, statistics, subprocess, tempfile, time, threading

REPEATS = 5  # Requested independent process launches per executable/model.
TIMEOUT = 300  # Safety limit only, never a speed pass/fail threshold.
ATOL = 5e-6  # Screening threshold for six-significant-digit text exports; not scientific validation.
TIMING_FRACTION = 1e-3  # of an output step: the time shift that may explain every value outside the screen tolerance
CURVE_ROWS = 1000  # rows per point of a comparison's error curve
RTOL = 5e-5  # Relative screening threshold, declared before observing the outputs.
STORAGE_ROWS = 20000  # Exceeds every planned trajectory, avoiding truncation.
CASES = [
    dict(name='episodic', states=120, total=1999, dt=.4, method='qualrk', setfile='episodic.ode.set'),
    dict(name='lecdiff200', states=400, total=20, dt=.05, method='Runge-Kutta'),
    dict(name='kuramot100', states=100, total=100, dt=.05, method='Runge-Kutta'),
    dict(name='amari', states=200, total=100, dt=.05, method='euler'),
    dict(name='clustor', states=366, total=500, dt=1, method='qualrk'),
]
# W269 (#326): the maintainer's sandbox model as rhs-ceiling (W249) integrated it:
# ../auto/models/PY_S1Bf_bench.ode (copied verbatim into sources/) at Iapp = 2000 pA,
# where the cell spikes, with its own settings (stiff, tolerance 1e-8, 2000 ms, dt 0.01:
# 200001 rows); only the storage is raised so XPPAUT keeps every row. Not in the default
# selection, so the W238 command still runs W238's five cases.
EXTRA_CASES = [
    dict(name='PY_S1Bf', source='PY_S1Bf_bench.ode', states=8, aux=2, total=2000, dt=.01, method='stiff',
         replace={'par Iapp=20000': 'par Iapp=2000'}, controls='@ maxstor=200010'),
]
# W269: an engine is a program and its extra arguments; 'interp' is xppautX run with
# --no-compile (the interpreter), 'modern' xppautX as it starts by default (compiled).
ENGINE_ARGS = {'legacy': [], 'modern': [], 'interp': ['--no-compile']}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False)+'\n')

def convert_args():
    """The final-tip converter; historical --auto syntax is not accepted."""
    return ['--convert', '--yes']

def prepare(bundle, binary, legacy, cases=CASES):
    """Keep originals; make explicit screen settings before converting to ODEX."""
    models=bundle/'models';models.mkdir(exist_ok=True)
    for c in cases:
        name=c['name'];src=bundle/'sources'/c.get('source',f'{name}.ode')
        lines=src.read_text().splitlines()
        for old,new in c.get('replace',{}).items():
            hits=[i for i,s in enumerate(lines) if s==old]
            if len(hits)!=1:raise ValueError(f'{src}: expected one line {old!r}')
            lines[hits[0]]=new
        if name=='clustor':
            # XPPAUT 8.0 supplied source line 12 lacks its final ]; retain the original.
            broken=[i for i,s in enumerate(lines) if s.rstrip().endswith('*w1_[j')]
            if len(broken)!=1:raise ValueError('clustor source differs from documented repair')
            lines[broken[0]] += ']'

        endings=[i for i,s in enumerate(lines) if s.strip().lower() in ('d','done')]
        if len(endings)!=1:raise ValueError(f'{src}: expected exactly one end marker')
        bound=5000 if name=='episodic' else 100000
        controls=c.get('controls',f"@ total={c['total']},dt={c['dt']},meth={c['method']},t0=0,trans=0,nout=1,maxstor={STORAGE_ROWS},bound={bound},toler=.001,atoler=.001,dtmin=1e-12,dtmax=1")
        # The published SET deliberately supplies its own tolerances/ICs/parameters.
        lines.insert(endings[0], '# Paper timing screen: explicit settings; source preserved in ../sources')
        lines.insert(endings[0]+1, controls)
        target=models/f'{name}.ode'
        if src.name!=target.name:shutil.copy2(src,models/(name+'.source.ode'))  # W269: the model as published, beside its prepared copy
        target.write_text('\n'.join(lines)+'\n')
        if 'setfile' in c:
            # Ask XPPAUT itself for declaration order; never infer it from model text.
            with tempfile.TemporaryDirectory(prefix='xpp-query-',dir='/tmp') as query:
                q=Path(query);shutil.copy2(target,q/target.name)
                shutil.copy2(bundle/'sources'/c['setfile'],q/c['setfile'])
                env=dict(os.environ,HOME=str(q),LC_ALL='C');env.pop('DISPLAY',None)
                proc=subprocess.run([str(legacy),target.name,'-silent','-setfile',c['setfile'],'-qpars','-outfile','parameters.txt'],cwd=q,env=env,capture_output=True,text=True,check=True)
                query_text=(q/'parameters.txt').read_text()
                (models/'episodic.legacy-parameter-query.txt').write_text(query_text)
                names=[line.split()[0] for line in query_text.splitlines() if line and not line.startswith('#')]
            setlines=(bundle/'sources'/c['setfile']).read_text().splitlines()
            start=setlines.index('# Parameters')+1
            if int(setlines[2].split()[0])!=len(names):raise ValueError('parameter counts differ')
            repairs=[]
            for i,name in enumerate(names):
                value,old=setlines[start+i].split()
                if old.lower()!=name.lower():repairs.append(dict(line=start+i+1,old_name=old,new_name=name,value=value))
                setlines[start+i]=value+'  '+name
            (models/c['setfile']).write_text('\n'.join(setlines)+'\n')
            write_json(models/'episodic.set-label-repairs.json',repairs)
        p=subprocess.run([str(binary),*convert_args(),str(target)],capture_output=True,text=True)
        (models/f'{name}.conversion.log').write_text(p.stdout+p.stderr)
        if p.returncode:raise RuntimeError(f'{name}: conversion failed ({p.returncode}): {p.stdout} {p.stderr}')
    write_json(bundle/'cases.json',cases)

def inspect_output(path,c):
    if not path.exists():return dict(valid=False,reason='no output.dat')
    rows=0;first=last=None;columns=None;prev=None
    try:
        for line in path.open():
            if not line.strip():continue
            values=[float(x) for x in line.split()]
            if not all(math.isfinite(x) for x in values):raise ValueError('nonfinite output')
            if columns is None:columns=len(values);first=values[0]
            if len(values)!=columns:raise ValueError('ragged rows')
            if prev is not None and values[0]<=prev:raise ValueError('time not increasing')
            prev=last=values[0];rows+=1
        # Legacy fixed-grid endpoint may be one step below a nonintegral Total/DT.
        expected_last=math.floor(c['total']/c['dt']+1e-8)*c['dt']
        valid=(columns==c['states']+c.get('aux',0)+1 and rows>1 and first==0 and abs(last-expected_last)<c['dt']*.1)
        return dict(valid=valid,rows=rows,columns=columns,first_time=first,last_time=last,expected_last=expected_last,sha256=sha(path))
    except (ValueError,OverflowError) as exc:return dict(valid=False,reason=str(exc),rows=rows)

def equivalent_shifts(pending,before,at,after):
    """Each value outside tolerance as the time shift that would explain it: the error over the trajectory's own slope there
    (central difference, one-sided at the ends); infinite where the trajectory is flat, since no shift explains it."""
    lo,hi=before or at,after or at
    span=hi[0]-lo[0]
    return [(col,error/abs((hi[col]-lo[col])/span) if span and hi[col]!=lo[col] else math.inf) for col,error in pending]

def compare(a,b):
    result=dict(same_shape=True,time_grid_equal=True,exact_values=True,max_abs_error=0.,max_scaled_error=0.,outside_tolerance=0,values=0)
    window=[None,None];pending=[];shifts=[]  # the two rows before the current one, and the previous row's outside values
    curve=[];block=0.  # worst scaled error per CURVE_ROWS rows: where the outputs leave each other, for a later plot
    with gzip.open(a,'rt') as fa,gzip.open(b,'rt') as fb:
        for row,(la,lb) in enumerate(itertools.zip_longest(fa,fb),1):
            if la is None or lb is None:result['same_shape']=False;break
            va=[float(v) for v in la.split()];vb=[float(v) for v in lb.split()]
            if len(va)!=len(vb):result['same_shape']=False;break
            if va[0]!=vb[0]:result['time_grid_equal']=False
            if pending:shifts+=equivalent_shifts(pending,window[0],window[1],va);pending=[]
            outside=[]
            for col,(x,y) in enumerate(zip(va[1:],vb[1:]),1):
                error=abs(x-y);scaled=error/(ATOL+RTOL*abs(x))
                if not scaled<=block:block=scaled
                bad=not scaled<=1  # NaN is outside too: a comparison with NaN is false both ways
                if bad and 'first_outside_row' not in result:result.update(first_outside_row=row,first_outside_time=va[0],first_outside_column=col)
                if not error<=result['max_abs_error']:result.update(max_abs_error=error,worst_abs_row=row,worst_abs_column=col)
                if not scaled<=result['max_scaled_error']:result['max_scaled_error']=scaled
                result['outside_tolerance']+=int(bad)
                if bad:outside.append((col,error))
                result['exact_values'] &= x==y
                result['values']+=1
            if row%CURVE_ROWS==0:curve.append((va[0],block));block=0.
            window=[window[1],va];pending=outside
            if row==2:result['output_step']=va[0]-window[0][0]  # the grid's step, for the timing check
    if pending:shifts+=equivalent_shifts(pending,window[0],window[1],None)
    result['error_curve']=curve
    # JSON has no infinity or NaN: a diverged output (Euler on a stiff model) is recorded as None with `non_finite_error`.
    result['non_finite_error']=not all(math.isfinite(v) for v in (result['max_abs_error'],result['max_scaled_error'],*(b for _,b in curve)))
    result['error_curve']=[(t,b if math.isfinite(b) else None) for t,b in curve]
    for key in ('max_abs_error','max_scaled_error'):
        if not math.isfinite(result[key]):result[key]=None
    result['final_time']=va[0] if result['values'] else None
    result['within_screen_tolerance']=result['same_shape'] and result['time_grid_equal'] and result['outside_tolerance']==0
    # Declared before looking at the outputs: a difference that a time shift of under TIMING_FRACTION of an output step explains,
    # at every value outside tolerance, is the same trajectory sampled a hair apart (steep gating variables, rounding-level phase).
    worst=max((s for _,s in shifts),default=0.)
    result.update(max_equivalent_shift=worst if math.isfinite(worst) else None,
                  max_equivalent_shift_over_step=(worst/result['output_step'] if result.get('output_step') and math.isfinite(worst) else None),
                  within_timing_tolerance=bool(result['same_shape'] and result['time_grid_equal'] and result.get('output_step')
                                               and worst<=TIMING_FRACTION*result['output_step']))
    return result

def run_one(c,engine,repetition,binary,work,models,results,cpu,timeout):
    import resource  # Unix only; imported here so compare/inspect_output also load on Windows.
    run=work/f"{c['name']}-{engine}-{repetition}";run.mkdir()
    for p in models.iterdir():
        if p.name.startswith(c['name']+'.') or p.name in c.get('files',()):shutil.copy2(p,run/p.name)  # W269: files = a model's #include files
    ext='ode' if engine=='legacy' else 'odex'
    args=['taskset','-c',str(cpu),str(binary),f"{c['name']}.{ext}",'-silent','-internset','0']
    if engine!='legacy':args=[a.replace('-silent','--silent').replace('-internset','--internset') for a in args]+ENGINE_ARGS[engine]
    if 'setfile' in c:args += ['-setfile' if engine=='legacy' else '--setfile',c['setfile']]
    home=run/'home';home.mkdir()
    env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),LC_ALL='C',OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1')
    env.pop('DISPLAY',None)
    before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.perf_counter()
    try:
        with (run/'run.log').open('wb') as log:
            proc=subprocess.Popen(args,cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT)
            timed_out=[]
            def expire():
                if proc.poll() is None:
                    timed_out.append(True)
                    proc.kill()  # Only this runner's own child, by PID.
            timer=threading.Timer(timeout,expire);timer.start()
            try:code=proc.wait()  # Blocking wait avoids timeout polling quantizing short timings.
            finally:timer.cancel();timer.join()
            if timed_out:code='timeout'
    except subprocess.TimeoutExpired:code='timeout'
    wall=time.perf_counter()-start;after=resource.getrusage(resource.RUSAGE_CHILDREN)
    record=dict(model=c['name'],engine=engine,repetition=repetition,wall_seconds=wall,cpu_seconds=(after.ru_utime+after.ru_stime)-(before.ru_utime+before.ru_stime),exit_code=code,command=args)
    record['output']=inspect_output(run/'output.dat',c)
    record['valid']=code==0 and record['output']['valid']
    label=f"{c['name']}-{engine}-{repetition}"
    shutil.copy2(run/'run.log',results/(label+'.log'))
    if (run/'output.dat').exists():
        with (run/'output.dat').open('rb') as src,gzip.open(results/(label+'.dat.gz'),'wb') as dst:shutil.copyfileobj(src,dst)
    print(f"{label}: wall={wall:.6f}s CPU={record['cpu_seconds']:.6f}s exit={code} valid={record['valid']}",flush=True)
    return record

def measure(cases, engines, binary, work, models, cpu, timeout, pilot_only=False):
    """W269: the timed runs (a warm-up and REPEATS per case and engine), their comparisons and
    summaries, written into work/results, which is returned; screen.py's main and other studies
    (../modeldb-267710) share it"""
    results=work/'results';results.mkdir();records=[]
    metadata=dict(platform=os.uname()._asdict() if hasattr(os.uname(),'_asdict') else list(os.uname()),cpu_affinity=cpu,repetitions=REPEATS,atol=ATOL,rtol=RTOL,timing_scope='whole headless process: startup + model load + integration + output; conversion excluded',warmup='one excluded pilot per engine/model',binary_sha256={k:sha(v) for k,v in binary.items()},environment_policy='fresh HOME per process, no DISPLAY, LC_ALL=C, OMP/OPENBLAS threads=1',engines=engines,engine_args={e:ENGINE_ARGS[e] for e in engines},start_time_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()))
    for index,c in enumerate(cases):
        pilots=[]
        for engine in engines:pilots.append(run_one(c,engine,0,binary[engine],work,models,results,cpu,timeout))
        records.extend(pilots)
        if not pilot_only:
            for repetition in range(1,REPEATS+1):
                # W238's alternation for two engines; with three, the order rotates (W269)
                order=(engines if (index+repetition)%2 else engines[::-1]) if len(engines)==2 else engines[(index+repetition)%len(engines):]+engines[:(index+repetition)%len(engines)]
                for engine in order:records.append(run_one(c,engine,repetition,binary[engine],work,models,results,cpu,timeout))
        write_json(results/'runs.json',dict(metadata=metadata,runs=records))
    comparisons=[];summaries=[]
    for c in cases:
        for left,right in itertools.combinations(engines,2):
          for rep in range(0,1 if pilot_only else REPEATS+1):
            pair=[r for r in records if r['model']==c['name'] and r['repetition']==rep and r['engine'] in (left,right)]
            comparison=dict(model=c['name'],repetition=rep,both_runs_valid=all(r['valid'] for r in pair))
            if len(engines)>2:comparison['pair']=[left,right]  # W269; W238's two-engine file keeps its fields
            if comparison['both_runs_valid']:
                comparison.update(compare(results/f"{c['name']}-{left}-{rep}.dat.gz",results/f"{c['name']}-{right}-{rep}.dat.gz"))
            comparisons.append(comparison)
        for engine in engines:
            rr=[r for r in records if r['model']==c['name'] and r['engine']==engine and r['repetition']>0]
            valid=[r for r in rr if r['valid']]
            summary=dict(model=c['name'],engine=engine,attempted=len(rr),successful=len(valid),repeated_output_identical=len({r['output'].get('sha256') for r in valid})==1 if valid else False)
            # No misleading mean over a partial success subset.
            for metric in ['wall_seconds','cpu_seconds']:
                values=[r[metric] for r in valid]
                summary[metric+'_mean']=statistics.mean(values) if len(values)==REPEATS else None
                summary[metric+'_sample_sd']=statistics.stdev(values) if len(values)==REPEATS else None
            summaries.append(summary)
    write_json(results/'comparison.json',dict(atol=ATOL,rtol=RTOL,comparisons=comparisons))
    write_json(results/'summary.json',summaries)
    with (results/'summary.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(summaries[0]));w.writeheader();w.writerows(summaries)
    with (results/'timings.csv').open('w',newline='') as f:
        fields=['model','engine','repetition','wall_seconds','cpu_seconds','exit_code','valid'];w=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore');w.writeheader();w.writerows(records)
    return results

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--legacy',type=Path,required=True);ap.add_argument('--modern',type=Path,required=True)
    ap.add_argument('--bundle',type=Path,default=Path(__file__).resolve().parent)
    ap.add_argument('--cpu',type=int,default=2);ap.add_argument('--timeout',type=float,default=TIMEOUT)
    ap.add_argument('--prepare-only',action='store_true');ap.add_argument('--pilot-only',action='store_true')
    ap.add_argument('--result-name')
    ap.add_argument('--models',nargs='+',choices=[c['name'] for c in CASES+EXTRA_CASES])
    ap.add_argument('--engines',default='legacy,modern',help="W269: comma-separated from "+','.join(ENGINE_ARGS)+"; 'interp' runs --modern with --no-compile")
    a=ap.parse_args();cases=[c for c in CASES+EXTRA_CASES if c['name'] in a.models] if a.models else CASES;bundle=a.bundle.resolve()
    engines=a.engines.split(',')
    if not engines or any(e not in ENGINE_ARGS for e in engines) or len(set(engines))!=len(engines):raise ValueError('--engines: distinct names of '+','.join(ENGINE_ARGS))
    binary={e:(a.legacy if e=='legacy' else a.modern).resolve() for e in engines}
    if a.cpu not in os.sched_getaffinity(0):raise ValueError('CPU unavailable')
    result_name=a.result_name or ('pilot-results' if a.pilot_only else 'results')
    if Path(result_name).name!=result_name:raise ValueError('result-name must be a folder name')
    target=bundle/result_name
    if target.exists() and any(target.iterdir()):raise FileExistsError(f'{target}: refusing to overwrite; choose --result-name')
    # Conversion and all filesystem staging happen before timed process launches.
    with tempfile.TemporaryDirectory(prefix='xpp-screen-',dir='/tmp') as temp:
        work=Path(temp);local=work/'bundle';shutil.copytree(bundle/'sources',local/'sources')
        prepare(local,a.modern.resolve(),a.legacy.resolve(),cases)
        shutil.copytree(local/'models',bundle/'models',dirs_exist_ok=True);shutil.copy2(local/'cases.json',bundle/'cases.json')
        if a.prepare_only:return
        results=measure(cases,engines,binary,work,local/'models',a.cpu,a.timeout,a.pilot_only)
        if target.exists() and any(target.iterdir()):raise FileExistsError(f'{target}: use a new result folder; refusing to overwrite')
        shutil.copytree(results,target,dirs_exist_ok=True)
        print('Saved',target,flush=True)
if __name__=='__main__':main()