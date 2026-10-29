#!/usr/bin/env -S uv run --locked python
"""Causal controls for the Terman mismatch; modify ONLY the disposable build tree.
Usage: uv run --locked python investigate.py --build-root /tmp/xpp-paper-build-XXXX
"""
import argparse,difflib,gzip,hashlib,importlib.util,json,os,shutil,subprocess,tempfile
from pathlib import Path

HERE=Path(__file__).resolve().parent
BUNDLE=HERE.parent
spec=importlib.util.spec_from_file_location('screen',BUNDLE/'screen.py')
screen=importlib.util.module_from_spec(spec);spec.loader.exec_module(screen)
CASE=next(c for c in screen.CASES if c['name']=='episodic')

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2,allow_nan=False)+'\n')
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--build-root',type=Path,required=True);a=ap.parse_args()
    root=a.build_root.resolve()
    if root.parent!=Path('/tmp') or not root.name.startswith('xpp-paper-build-'):raise ValueError('requires isolated build.py tree in /tmp')
    modern=root/'modern';legacy=root/'legacy/xppaut';out=HERE/'controls'
    out.mkdir(exist_ok=False)
    bins=root/'diagnostic-binaries';bins.mkdir();baseline=bins/'modern-baseline';shutil.copy2(modern/'xppautX',baseline)
    work=root/'diagnostic-runs';work.mkdir()
    sources={name:(modern/'core'/name).read_text() for name in ['expr_functions.cpp','stiff.cpp']}
    meta=json.loads((BUNDLE/'provenance.json').read_text());records={};comparisons={}
    def run(label,binary,engine,models=BUNDLE/'models'):
        w=work/label;w.mkdir();o=out/label;o.mkdir()
        record=screen.run_one(CASE,engine,1,binary,w,models,o,2,screen.TIMEOUT)
        record['binary_sha256']=sha(binary)
        if not record['valid']:raise RuntimeError(f'{label}: run failed, see {o}')
        records[label]=record;save(out/'runs.json',records)
        return o/f'episodic-{engine}-1.dat.gz'
    def cmp(label,left,right):
        result=screen.compare(left,right);comparisons[label]=result;save(out/'comparisons.json',comparisons)
        print(label,json.dumps(result),flush=True)
    legacy_output=run('legacy-baseline',legacy,'legacy')
    modern_output=run('modern-baseline',baseline,'modern')
    cmp('baseline',legacy_output,modern_output)
    # Compare model/settings as loaded by each executable, without integration.
    queries={}
    for engine,binary in [('legacy',legacy),('modern',baseline)]:
        q=work/(engine+'-query');q.mkdir()
        for f in (BUNDLE/'models').glob('episodic.*'):shutil.copy2(f,q/f.name)
        ext='ode' if engine=='legacy' else 'odex';dash='-' if engine=='legacy' else '--'
        cmd=[str(binary),'episodic.'+ext,dash+'silent',dash+'setfile','episodic.ode.set',dash+'qpars',dash+'qics',dash+'outfile','values.txt']
        env=dict(os.environ,HOME=str(q),LC_ALL='C');env.pop('DISPLAY',None)
        p=subprocess.run(cmd,cwd=q,env=env,capture_output=True,text=True,check=True)
        text=(q/'values.txt').read_text();(out/(engine+'-loaded-values.txt')).write_text(text)
        queries[engine]={line.split()[0].lower():float(line.split()[1]) for line in text.splitlines() if line and not line.startswith('#')}
    save(out/'loaded-values-comparison.json',{'equal_at_query_precision':queries['legacy']==queries['modern'],'legacy_count':len(queries['legacy']),'modern_count':len(queries['modern']),'differences':{key:[value,queries['modern'].get(key)] for key,value in queries['legacy'].items() if queries['modern'].get(key)!=value}})
    # Only the function bindings change; solver, expressions and input values remain.
    variants=[('expression-system',True,True,False),('solver-system',False,False,True),('all-system',True,True,True),('exp-only-system',True,False,False),('expression-pow-only-system',False,True,False)]
    for label,exp,power,solver in variants:
        changes={name:text for name,text in sources.items()}
        if exp:changes['expr_functions.cpp']=changes['expr_functions.cpp'].replace('xpp::math::exp,','::exp,')
        if power:changes['expr_functions.cpp']=changes['expr_functions.cpp'].replace('xpp::math::pow,','::pow,')
        if solver:changes['stiff.cpp']=changes['stiff.cpp'].replace('xpp::math::pow(', '::pow(')
        diff=''
        for name,text in changes.items():
            (modern/'core'/name).write_text(text)
            diff+=''.join(difflib.unified_diff(sources[name].splitlines(True),text.splitlines(True),fromfile='a/core/'+name,tofile='b/core/'+name))
        (out/(label+'.patch')).write_text(diff)
        print('Building control',label,flush=True)
        with (out/(label+'-build.log')).open('w') as log:subprocess.run(meta['build_commands']['modern'],cwd=modern,stdout=log,stderr=subprocess.STDOUT,check=True)
        output=run(label,modern/'xppautX','modern');cmp(label,legacy_output,output)
    # Tolerance refinement on the two UNMODIFIED programs, never the controls.
    for tolerance in ['1e-5','1e-7']:
        label='tolerance-'+tolerance;models=work/(label+'-models');shutil.copytree(BUNDLE/'models',models)
        sp=models/'episodic.ode.set';lines=sp.read_text().splitlines()
        assert lines[14].split()[1:]==['Tolerance']
        lines[14]=tolerance+'  Tolerance';sp.write_text('\n'.join(lines)+'\n')
        save(out/(label+'-settings.json'),{'only_change':'SET line 15, QualRK tolerance','value':tolerance,'set_sha256':sha(sp)})
        left=run(label+'-legacy',legacy,'legacy',models);right=run(label+'-modern',baseline,'modern',models)
        cmp(label,left,right)
    # Restore the disposable source files; production checkout was never edited.
    for name,text in sources.items():(modern/'core'/name).write_text(text)
    save(out/'provenance.json',{'legacy_commit':meta['legacy_commit'],'modern_commit':meta['modern_commit'],'build_root':str(root),'script_sha256':sha(Path(__file__)),'input_set_sha256':sha(BUNDLE/'models/episodic.ode.set'),'baseline_hashes_match_screen':{engine:records[engine+'-baseline']['output']['sha256']==next(r for r in json.loads((BUNDLE/'results/runs.json').read_text())['runs'] if r['engine']==engine and r['model']=='episodic' and r['repetition']==1)['output']['sha256'] for engine in ['legacy','modern']}})
    print('CONTROL_STUDY_COMPLETE',flush=True)
if __name__=='__main__':main()