#!/usr/bin/env python3
"""W75: each quirk's source line and expected --check diagnostic, no writes."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

# Safety bound for a broken child process, never a correctness timing assertion.
CHECK_TIMEOUT = 60

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin', default='./xppautX')
    args = parser.parse_args()
    binary = str(Path(args.bin).resolve())
    cases = [
        ('aux z=2^3^2', 'power-associativity', 'info'),
        ('aux z=2**3**2', 'power-associativity', 'info'),
        ('aux z=-2^2', 'unary-minus-power', 'info'),
        ('aux z=2*3<4', 'comparison-precedence', 'warning'),
        ('aux z=3-1<2', 'comparison-precedence', 'warning'),
        ('aux z=1+2<3+4', 'comparison-precedence', 'warning'),
        ('aux z=1/2<1', 'comparison-precedence', 'warning'),
        ('aux z=-1<0', 'comparison-precedence', 'warning'),
        ('aux z=-1>=0', 'comparison-precedence', 'warning'),
        ('aux z=3<2<1', 'chained-comparison', 'warning'),
        ('aux z=1+1&1', 'logical-precedence', 'warning'),
        ('aux z=1&1+1', 'logical-precedence', 'warning'),
        ('aux z=1|1+1', 'logical-precedence', 'warning'),
        ('aux z=1&1*2', 'logical-precedence', 'warning'),
        ('aux z=1/0', 'division-by-zero', 'warning'),
        ('aux z=0/0', 'division-by-zero', 'warning'),
        ('aux z=1/(0)', 'division-by-zero', 'warning'),
        ('aux z=1/0*2', 'division-by-zero', 'warning'),
        ('aux z=if(1>0)then(10)else(20)+5', 'if-trailing-operator', 'info'),
        ('@ total = 0.03', 'option', 'warning'),
        ('@ total = 0.03,dt=0.01', 'option', 'warning'),
        ('init x=2*3', 'init-value', 'warning'),
        ('x(0)=2*3', 'initcond-formula', 'warning'),
        ('par a=2*3', 'declaration-value', 'warning'),
        ('set test {x=2*3}', 'set-value', 'warning'),
        ('par=1', 'keyword-name', 'warning'),
        ('par and=1', 'reserved-word', 'warning'),
        ('aux z=X', 'name-case', 'warning'),
        ('!d=x', 'derived-frozen', 'warning'),
    ]
    count = 0
    with tempfile.TemporaryDirectory(prefix='W75-') as folder:
        root = Path(folder)
        file = root / 'model.ode'

        def run(text, extension='.ode'):
            nonlocal count
            model = file.with_suffix(extension)
            model.write_text(text, encoding='utf-8')
            before = {p.name: p.read_bytes() for p in root.iterdir() if p.is_file()}
            result = subprocess.run([binary, '--check', str(model)], cwd=root,
                                    capture_output=True, text=True, timeout=CHECK_TIMEOUT)
            report = json.loads(result.stdout)
            assert report['file'] == str(model), report
            assert set(report) == {'file', 'diagnostics'}, report
            assert before == {p.name: p.read_bytes() for p in root.iterdir() if p.is_file()}, 'check wrote a file'
            for d in report['diagnostics']:
                assert set(d) == {'file', 'line', 'col', 'source', 'id', 'severity', 'message'}, d
                assert d['file'], d
                assert d['message'], d
            count += 1
            return result.returncode, report['diagnostics']

        for line, quirk, severity in cases:
            code, diagnostics = run("x'=0\n" + line + '\ndone\n')
            found = [d for d in diagnostics if d['id'] == quirk]
            assert len(found) == 1, (line, diagnostics)
            assert found[0]['line'] == 2 and found[0]['source'] == line, (line, found)
            assert found[0]['severity'] == severity, found
            columns = {
                'power-associativity': line.rfind('**' if '**' in line else '^') + 1,
                'unary-minus-power': line.index('^') + 1 if '^' in line else 0,
                'comparison-precedence': line.index('<') + 1 if '<' in line else line.index('>') + 1 if '>' in line else 0,
                'chained-comparison': line.rfind('<') + 1,
                'logical-precedence': line.index('&') + 1 if '&' in line else line.index('|') + 1 if '|' in line else 0,
                'division-by-zero': line.index('/') + 1 if '/' in line else 0,
                'if-trailing-operator': line.rfind('+') + 1,
            }
            assert found[0]['col'] == columns.get(quirk, 0), found
            wording = {
                'power-associativity': 'XPP powers group left;',
                'unary-minus-power': 'Power binds before unary minus,',
                'comparison-precedence': 'XPP comparisons bind before',
                'chained-comparison': "XPP compares the preceding comparison's result;",
                'logical-precedence': 'XPP logical operators share arithmetic precedence;',
                'division-by-zero': 'XPP replaces a zero divisor with 2.23e-15;',
                'if-trailing-operator': 'The trailing operator applies to the whole',
                'option': 'XPP ignores an option with spaces around its =',
                'option-value': 'XPP reads the number at its front,',
                'init-value': 'XPP reads the number at its front,',
                'initcond-formula': 'XPP starts x at 2',
                'declaration-value': 'XPP reads the number at its front,',
                'set-value': 'XPP reads the number at its front,',
                'keyword-name': 'renamed: par is par_ here',
                'reserved-word': 'renamed: and is and_ here',
                'name-case': 'names written as their declarations spell them',
                'derived-frozen': 'XPP freezes this derived quantity',
            }
            assert wording[quirk] in found[0]['message'], found
            assert code == (2 if quirk == 'derived-frozen' else 1 if severity == 'warning' else 0), (line, code, diagnostics)
        clean = [
            'aux z=(2^3)^2', 'aux z=2*(3<4)', 'aux z=(3-1)<2',
            'aux z=(3<2)&(2<1)', 'aux z=1+(1&1)', 'aux z=1*2&3',
            'aux z=1|0&0', 'aux z=1/(0+2)', 'aux z=1/0^0',
            'aux z=1/(0)^0', 'aux z=1/((0))**0',
            'aux z=1+2|3<4', 'aux z=-1|3<4',
            'aux z=1e-3^2', 'aux z=-2+3^2', 'aux z=1|2*3',
            '@ total=0,dt=0.1', '@ logfile=check.log',
        ]
        for line in clean:
            code, diagnostics = run("x'=0\n" + line + '\ndone\n')
            assert code == 0 and not diagnostics, (line, code, diagnostics)
        code, diagnostics = run("x(t+1)=x\ndone\n")
        assert code == 0 and any(d['id'] == 'discrete-map' for d in diagnostics), diagnostics
        # A whole-name Symplectic method is suitable only with an even state count.
        code, diagnostics = run("x'=-x\ny'=x\n@ meth=symplectic\ndone\n")
        assert code == 0 and diagnostics == [], diagnostics
        for method, selected in [('symplectic', 'Symplectic'), ('s', 'Stiff'), ('rk4', 'Runge-Kutta')]:
            file.write_text("x'=y\ny'=-x\ninit x=1,y=0\n@ meth=" + method + '\ndone\n', encoding='utf-8')
            converted = subprocess.run([binary, '--convert', '--auto', str(file)], cwd=root,
                                       capture_output=True, text=True, timeout=CHECK_TIMEOUT)
            assert converted.returncode == 0, converted.stderr + converted.stdout
            assert '@ method=' + selected in file.with_suffix('.odex').read_text(encoding='utf-8')
            file.with_suffix('.odex').unlink()
            count += 1
        # Includes retain the diagnostic's own file, line and source in both languages.
        bad = root / 'bad.inc'
        bad.write_text('aux z=unknown\n', encoding='utf-8')
        for extension, include in [('.ode', '#include bad.inc'), ('.odex', 'include "bad.inc"')]:
            code, diagnostics = run("x'=0\n" + include + ('\ndone\n' if extension == '.ode' else '\n'), extension)
            assert code == 2, diagnostics
            error = diagnostics[-1]
            assert Path(error['file']) == bad and error['line'] == 1 and error['source'] == 'aux z=unknown', error
        nested = root / 'nested'
        nested.mkdir()
        (nested / 'good.inc').write_text('aux z=0\n#done\n', encoding='utf-8')
        for extension, include in [('.ode', '#include nested/good.inc'), ('.odex', 'include "nested/good.inc"')]:
            code, diagnostics = run("x'=0\n" + include + ('\ndone\n' if extension == '.ode' else '\n'), extension)
            assert code == 0 and diagnostics == [], diagnostics
        # A received file cannot read its neighbour, absolute includes, or a linked subtree.
        received = root / 'received'
        received.mkdir()
        private = root / 'private.inc'
        private.write_text('aux private_secret=unknown\n', encoding='utf-8')
        def refused_include(name, extension):
            nonlocal count
            model = received / ('main' + extension)
            include = '#include ' + name if extension == '.ode' else 'include ' + json.dumps(name)
            model.write_text("x'=0\n" + include + '\ndone\n', encoding='utf-8')
            result = subprocess.run([binary, '--check', str(model)], cwd=received,
                                    capture_output=True, text=True, timeout=CHECK_TIMEOUT)
            report = json.loads(result.stdout)
            assert result.returncode == 2 and report['diagnostics'][-1]['id'] == 'load-error', report
            assert 'private_secret' not in result.stdout, report
            assert Path(report['diagnostics'][-1]['file']) == model, report
            assert report['diagnostics'][-1]['line'] == 2, report
            count += 1
        for extension in ['.ode', '.odex']:
            for name in ['../private.inc', str(private), '..\\private.inc', '.. /private.inc', 'linked/../private.inc']:
                refused_include(name, extension)
        linked = received / 'linked'
        if os.name == 'nt':
            junction = subprocess.run(['cmd', '/c', 'mklink', '/J', str(linked), str(root)], capture_output=True, text=True)
            assert junction.returncode == 0, junction.stdout + junction.stderr
        else:
            linked.symlink_to(root, target_is_directory=True)
        try:
            for extension in ['.ode', '.odex']:
                refused_include('linked/private.inc', extension)
        finally:
            if os.name == 'nt':
                linked.rmdir()
            else:
                linked.unlink()
        # Counts are checked before allocation or formula evaluation, including aggregate tables.
        for extension in ['.ode', '.odex']:
            # Fail safely just above the budget before exercising the billion-point attack.
            for points in [1000001, 1000000000]:
                first = f'table f % {points} 0 1 t' if extension == '.ode' else f'table f t, n={points}, lo=0, hi=1'
                code, diagnostics = run("x'=0\n" + first + '\n' + ('done\n' if extension == '.ode' else ''), extension)
                assert code == 2 and 'budget' in diagnostics[-1]['message'], diagnostics
                assert diagnostics[-1]['line'] == 2, diagnostics
            tables = 'table f % 600000 0 1 t\ntable g % 600000 0 1 t\ndone\n' if extension == '.ode' else 'table f t, n=600000, lo=0, hi=1\ntable g t, n=600000, lo=0, hi=1\n'
            code, diagnostics = run("x'=0\n" + tables, extension)
            assert code == 2 and 'budget' in diagnostics[-1]['message'], diagnostics
            assert diagnostics[-1]['line'] == 3, diagnostics
        values = root / 'values.dat'
        for header in ['1000000000', '999999999999999999999999']:
            values.write_text(header + '\n0\n1\n', encoding='utf-8')
            code, diagnostics = run("x'=0\ntable f values.dat\ndone\n")
            assert code == 2 and (root / diagnostics[-1]['file']).resolve() == values, diagnostics
            assert diagnostics[-1]['line'] == 1 and diagnostics[-1]['source'] == header, diagnostics
        code, diagnostics = run("x'=0\n", '.odex')
        assert code == 0 and diagnostics == [], diagnostics
        for extension, text in [('.ode', "x'=unknown\ndone\n"), ('.odex', "x'=unknown\n")]:
            code, diagnostics = run(text, extension)
            assert code == 2 and diagnostics[-1]['id'] == 'load-error', diagnostics
            assert diagnostics[-1]['line'] == 1 and diagnostics[-1]['source'] == "x'=unknown", diagnostics
        # The checked option reader now rejects these before conversion (W125).
        for line in ['@ total=2*3', '@ total=(4)', 'aux z=2*-3',
                     '@ meth=symplectic', '@ meth=stiffjunk',
                     'aux z=1&&1', 'aux z=1||1', 'aux z=!1',
                     'aux z=(1!=2)', 'aux z=if 1>0 then 10 else 20']:
            code, diagnostics = run("x'=0\n" + line + '\ndone\n')
            assert code == 2 and diagnostics[-1]['id'] == 'load-error', (line, diagnostics)
            assert diagnostics[-1]['line'] == 2 and diagnostics[-1]['source'] == line, diagnostics
        missing = root / 'missing.ode'
        result = subprocess.run([binary, '--check', str(missing)], capture_output=True, text=True, timeout=CHECK_TIMEOUT)
        assert result.returncode == 2 and json.loads(result.stdout)['diagnostics'][-1]['id'] == 'load-error'
        count += 1
    print(f'checkcheck: {count} passed, 0 failed')


if __name__ == '__main__':
    main()
