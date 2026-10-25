#!/usr/bin/env python3
"""W358 (#420): third_party/lapack from Reference-LAPACK's source tarball, verbatim.

    uv run --locked python tools/lapack/extract.py lapack-3.12.1.tar.gz [--out third_party/lapack]

Checks the tarball's SHA-256 against VERSIONS, then copies, unchanged, the routines the closure of
DGEEV (eigenvalues and vectors of a general matrix, replacing EISPACK's RG) and DGGEV (generalized
eigenvalues, replacing EISPACK's QZ) calls, with the BLAS they use (reference BLAS), the machine
constants (INSTALL/dlamch.f) and LAPACK's licence. A routine is found by its name in a CALL or a
function reference of the sources already taken, so a new LAPACK version needs no list edited.
Nothing is downloaded: the maintainer fetched the tarball.
"""
import argparse, hashlib, re, sys, tarfile
from pathlib import Path

VERSIONS = {  # tarball name: GitHub tag v3.12.1's archive
    'lapack-3.12.1.tar.gz': '2ca6407a001a474d4d4d35f3a61550156050c48016d949f0da0529c0aa052422',
}
ROOTS = ['dgeev', 'dggev']
# where each routine may live; BLAS/SRC first, so the BLAS copy of xerbla and lsame is the one taken
FOLDERS = ('BLAS/SRC', 'SRC', 'INSTALL')
SOURCE = re.compile(r'(?:%s)/[^/]+\.(?:f|f90|F90)$' % '|'.join(FOLDERS))


def rank(name):
    """the position of name's folder in FOLDERS"""
    return next(i for i, f in enumerate(FOLDERS) if name.startswith(f + '/'))


def references(source):
    """lower-case names a source calls or uses (comments removed; over-approximate on purpose)"""
    code = re.sub(r'(?m)^[cC*].*$', '', source)
    code = re.sub(r'!.*', '', code)
    names = {n.lower() for n in re.findall(r'(?i)\bcall\s+([a-z0-9_]+)', code)}
    names |= {n.lower() for n in re.findall(r'(?i)\b([a-z][a-z0-9_]*)\s*\(', code)}
    names |= {n.lower() for n in re.findall(r'(?im)^\s*use\s+([a-z0-9_]+)', code)}
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('tarball')
    ap.add_argument('--out', default='third_party/lapack')
    args = ap.parse_args()
    path = Path(args.tarball)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if VERSIONS.get(path.name) != digest:
        sys.exit(f'{path.name}: sha256 {digest} is not a known release (extract.py VERSIONS)')
    tar = tarfile.open(path)
    sources, text, license_text = {}, {}, None
    for m in tar.getmembers():
        name = m.name.split('/', 1)[1] if '/' in m.name else m.name
        if name == 'LICENSE':
            license_text = tar.extractfile(m).read()
        elif SOURCE.match(name):
            routine = name.rsplit('/', 1)[1].rsplit('.', 1)[0].lower()
            if routine not in sources or rank(name) < rank(sources[routine]):
                sources[routine] = name
            text[name] = tar.extractfile(m).read()
    taken, todo = {}, list(ROOTS)
    while todo:
        r = todo.pop()
        if r in taken or r not in sources:
            continue
        taken[r] = sources[r]
        todo += sorted(references(text[sources[r]].decode('latin-1')) - taken.keys())
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for old in out.iterdir():
        if old.suffix in ('.f', '.f90', '.F90'):
            old.unlink()
    for r, name in sorted(taken.items()):
        (out / name.rsplit('/', 1)[1]).write_bytes(text[name])
    (out / 'LICENSE').write_bytes(license_text)
    print(f'{len(taken)} routines from {path.name} ({digest}) into {out}')
    print(' '.join(sorted(taken)))


if __name__ == '__main__':
    main()
