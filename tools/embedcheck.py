#!/usr/bin/env python3
"""W178: exercise both build generators' failure cleanup and diagnostic places."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

# Bound a broken generator process; correctness never depends on its speed.
GENERATOR_TIMEOUT = 60


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build/ucrt')
    args = parser.parse_args()
    build = Path(args.build_dir).resolve()
    checks = 0
    with tempfile.TemporaryDirectory(prefix='W178-') as folder:
        root = Path(folder)
        source = root / 'input.js'
        source.write_bytes(bytes(range(256)))
        missing = root / 'missing.js'
        target = root / 'asset.cpp'
        blocked = root / 'blocked.cpp'
        blocked.mkdir()
        for name in ('embed', 'embed_bytes'):
            binary = build / (name + ('.exe' if os.name == 'nt' else ''))

            def run(output, input_file, **kwargs):
                command = [str(binary), str(output)]
                if name == 'embed_bytes':
                    command.append('test_asset')
                return subprocess.run(command + [str(input_file)], capture_output=True,
                                      text=True, timeout=GENERATOR_TIMEOUT, **kwargs)

            def failed(output, input_file, place, **kwargs):
                nonlocal checks
                before = output.read_bytes() if output.is_file() else None
                result = run(output, input_file, **kwargs)
                assert result.returncode == 1, result
                assert result.stderr.startswith(str(place) + ':1:'), result.stderr
                assert not list(root.glob('*.tmp-*')), list(root.iterdir())
                if before is None:
                    assert not output.is_file(), output
                else:
                    assert output.read_bytes() == before, output
                checks += 1

            failed(target, missing, missing)
            target.write_bytes(b'previous complete target')
            failed(target, missing, missing)
            failed(target, root, root)
            failed(blocked, source, blocked)
            failed(root / 'absent' / 'asset.cpp', source, root / 'absent' / 'asset.cpp')
            for _ in range(2):
                result = run(target, source)
                assert result.returncode == 0, result.stderr
                assert '0,1,2,3' in target.read_text(), target.read_text()
                assert not list(root.glob('*.tmp-*'))
                checks += 1
            if os.name == 'posix':
                import resource
                import signal

                def no_output_bytes():
                    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                    resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))

                # Force a write error inside write_bytes, after opening the input.
                source.write_bytes(bytes(range(256)) * 256)
                failed(target, source, target, preexec_fn=no_output_bytes)
                target.unlink()
                failed(target, source, target, preexec_fn=no_output_bytes)
                source.chmod(0)
                try:
                    if os.geteuid() != 0:
                        failed(target, source, source)
                finally:
                    source.chmod(0o600)
                if Path('/dev/full').exists():
                    result = run(Path('/dev/full'), source)
                    assert result.returncode == 1 and result.stderr.startswith('/dev/full:1:'), result
                    checks += 1
                link = root / 'link.cpp'
                link.symlink_to(source)
                failed(link, source, link)
                assert link.is_symlink(), link
                link.unlink()
            target.unlink(missing_ok=True)
    print(f'embedcheck: {checks} checks, 0 failed')


if __name__ == '__main__':
    main()
