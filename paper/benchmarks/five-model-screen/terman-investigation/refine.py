#!/usr/bin/env -S uv run --locked python
"""Extend W239 controls with tolerance refinement using unmodified binaries."""
import argparse,json,shutil
from pathlib import Path
import investigate as study

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-root',type=Path,required=True)
    args=ap.parse_args();root=args.build_root.resolve()
    if root.parent!=Path('/tmp') or not root.name.startswith('xpp-paper-build-'):
        raise ValueError('requires isolated build.py tree in /tmp')
    out=study.HERE/'refinement';out.mkdir(exist_ok=False)
    work=root/'refinement-runs';work.mkdir()
    models=work/'models';shutil.copytree(study.BUNDLE/'models',models)
    setting=models/'episodic.ode.set';lines=setting.read_text().splitlines()
    assert lines[14].split()[1:]==['Tolerance']
    lines[14]='1e-9  Tolerance';setting.write_text('\n'.join(lines)+'\n')
    study.save(out/'settings.json',{'only_change':'SET line 15, QualRK tolerance','value':1e-9,'set_sha256':study.sha(setting)})
    outputs={};runs={};comparisons={}
    for engine,binary in [('legacy',root/'legacy/xppaut'),('modern',root/'diagnostic-binaries/modern-baseline')]:
        dest=out/engine;dest.mkdir()
        runs[engine]=study.screen.run_one(study.CASE,engine,1,binary,work,models,dest,2,study.screen.TIMEOUT)
        runs[engine]['binary_sha256']=study.sha(binary)
        study.save(out/'runs.json',runs)
        if not runs[engine]['valid']:raise RuntimeError(engine+' failed')
        outputs[engine]=dest/f'episodic-{engine}-1.dat.gz'
    comparisons['cross-program-1e-9']=study.screen.compare(outputs['legacy'],outputs['modern'])
    for engine in ['legacy','modern']:
        filename=f'episodic-{engine}-1.dat.gz'
        previous=study.HERE/'controls'/f'tolerance-1e-7-{engine}'/filename
        comparisons[engine+'-1e-7-vs-1e-9']=study.screen.compare(previous,outputs[engine])
        baseline=study.HERE/'controls'/f'{engine}-baseline'/filename
        comparisons[engine+'-1e-3-vs-1e-9']=study.screen.compare(baseline,outputs[engine])
        mid=study.HERE/'controls'/f'tolerance-1e-5-{engine}'/filename
        comparisons[engine+'-1e-5-vs-1e-9']=study.screen.compare(mid,outputs[engine])
    study.save(out/'comparisons.json',comparisons)
    study.save(out/'provenance.json',{'build_root':str(root),'script_sha256':study.sha(Path(__file__)),'baseline_provenance':'../controls/provenance.json'})
    for name,result in comparisons.items():print(name,json.dumps(result),flush=True)
if __name__=='__main__':main()
