"""Build only study adapters against unchanged core and installed SUNDIALS."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent


def build(prefix, out):
    out.mkdir(parents=True, exist_ok=True)
    prefix, out = prefix.resolve(), out.resolve()
    core = out / 'core'
    # PIC lets the two incompatible CVODE C ABIs live in separate symbol scopes.
    commands = [['make', '-j4', 'lib', f'BUILDDIR={core}', 'OPT=-g -O2 -fPIC', 'WINDOW=0']]
    exports = out / 'bridge.map'
    exports.write_text('{ global: study_*; local: *; };\n')
    commands += [['g++', '-std=c++23', '-g', '-O2', '-fPIC', '-ffp-contract=off', '-Wall', '-pedantic', '-Werror',
                  '-Icore', '-I'+str(HERE), '-shared', str(HERE/'bridge.cpp'), str(core/'libxppcore.a'),
                  '-Wl,-Bsymbolic', '-Wl,--version-script='+str(exports), '-Wl,--no-undefined',
                  '-Wl,--wrap=DenseFactor', '-Wl,--wrap=BandFactor', '-lm', '-ldl', '-o', str(out/'libstudy.so')]]
    # W34's explicit static component list, extended with sparse and SPGMR.
    libs = ['cvode', 'sunlinsolband', 'sunmatrixband', 'sunlinsolspgmr', 'sunmatrixsparse', 'nvecserial', 'sunnonlinsolnewton', 'core']
    klu = (prefix/'lib/libsundials_sunlinsolklu.a').is_file()
    cmd = ['g++', '-std=c++23', '-g', '-O2', '-ffp-contract=off', '-Wall', '-Wextra', '-Werror',
           '-I'+str(prefix/'include'), '-I'+str(HERE), str(HERE/'driver.cpp'),
           '-L'+str(out), '-lstudy', '-Wl,-rpath,$ORIGIN']
    if klu:
        cmd += ['-DW378_KLU', '-I/usr/include/suitesparse']
        libs.insert(1, 'sunlinsolklu')
    cmd += ['-Wl,--start-group'] + [str(prefix/f'lib/libsundials_{name}.a') for name in libs] + ['-Wl,--end-group']
    if klu:
        cmd += ['-lklu', '-lamd', '-lcolamd', '-lbtf', '-lsuitesparseconfig']
    cmd += ['-lm', '-ldl', '-o', str(out/'worker')]
    commands.append(cmd)
    for command in commands:
        print('build:', command, flush=True)
        subprocess.run(command, check=True)
    metadata=dict(commands=commands, sundials_prefix=str(prefix), klu=klu,
                  compiler=subprocess.check_output(['g++', '--version'], text=True),
                  sundials_version=(prefix/'include/sundials/sundials_config.h').read_text(),
                  sources={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in HERE.iterdir() if p.suffix in ('.cpp', '.h')})
    (out/'build.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print('KLU:', 'enabled' if klu else 'UNAVAILABLE (explicitly omitted only for stage-1 checks)')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sundials', type=Path, required=True)
    ap.add_argument('--out', type=Path, default=Path('build/w378'))
    a = ap.parse_args()
    build(a.sundials, a.out)
