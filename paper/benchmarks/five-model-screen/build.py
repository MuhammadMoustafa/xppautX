#!/usr/bin/env -S uv run --locked python
"""Rebuild the two pinned sources in isolated Linux temporary storage; no installation.
Requires GCC/G++ 15 with C++23 support, make, X11 headers/libraries, Python 3.
The build directory is intentionally retained until the screen is complete.
"""
import argparse,json,pathlib,subprocess,tarfile,tempfile

def main():
    bundle=pathlib.Path(__file__).resolve().parent
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo',type=pathlib.Path,default=bundle.parents[2])
    # W269: a later study names its own pinned commits and commands; the default is W238's
    ap.add_argument('--provenance',type=pathlib.Path,default=bundle/'provenance.json')
    # W269: /tmp became a tmpfs that WSL empties whenever the distribution stops between calls
    ap.add_argument('--tmp',default='/tmp',help='the ext4 folder the build tree is made in')
    args=ap.parse_args();meta=json.loads(args.provenance.read_text())
    root=pathlib.Path(tempfile.mkdtemp(prefix='xpp-paper-build-',dir=args.tmp))
    for name,key in [('legacy','legacy_commit'),('modern','modern_commit')]:
        dest=root/name;dest.mkdir();archive=root/(name+'.tar')
        with archive.open('wb') as f:subprocess.run(['git','-C',str(args.repo),'archive',meta[key]],stdout=f,check=True)
        with tarfile.open(archive) as t:t.extractall(dest,filter='data')
        archive.unlink()
        with (root/(name+'-build.log')).open('w') as log:subprocess.run(meta['build_commands'][name],cwd=dest,stdout=log,stderr=subprocess.STDOUT,check=True)
    print(json.dumps({'build_directory':str(root),'legacy':str(root/'legacy/xppaut'),'modern':str(root/'modern/xppautX')},indent=2))
if __name__=='__main__':main()