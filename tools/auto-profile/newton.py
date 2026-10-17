# W412 (#474): gdb breakpoints, no measuring code in the program.
# profile.py --tool newton uses the same ModelDB workload and 20-point cut.
# solvbv includes fold-direction solves as in tables.py; only calls whose
# caller is stepbv are Newton corrections. setubv counts actual assemblies.
# The special-point tests' returns are events of their own (TESTS), with
# whether the test may start a search (*chng): the last one before a search's
# first solve is the test that started it.
import gdb, json, os
from gdb_memory import doubles

out = open(os.environ['NEWTON_OUT'], 'w', buffering=1)
PERIOD_INDEX = 10  # AUTO's PAR(11), zero-based in the C++ arrays.
TESTS = {'fnuzbv': 'uz', 'fnlpbv': 'fold', 'fnbpbv': 'bp', 'fnspbv': 'sp'}


def value(expression):
    return gdb.parse_and_eval(expression)


class Trace(gdb.Breakpoint):
    def __init__(self, function):
        super().__init__(function, internal=True)
        self.function = function

    def stop(self):
        # Fail closed: an unreadable counter must never look like zero work.
        row = {'event': self.function}
        if self.function == 'setubv':
            row['ntot'] = int(value('iap->ntot'))
        else:
            for field in ('ntot', 'nit', 'ntst', 'ncol', 'ndim'):
                row[field] = int(value('iap->' + field))
            row['dsold'] = float(value('rap->dsold'))
            if self.function == 'solvbv':
                row['ds'] = float(value('*rds'))
                parent = gdb.newest_frame().older()
                row.update(ifst=int(value('*ifst')), nllv=int(value('*nllv')),
                           caller=parent.name(), parent=parent.older().name(),
                           parameter=float(value('rlcur[0]')))
            if self.function in TESTS:
                TestReturn({**row, 'event': TESTS[self.function]}, int(value('chng')))
                return False
            if self.function == 'stplbv':
                row['parameter'] = float(value('par[icp[0]]'))
                row['period'] = float(value('par[%d]' % PERIOD_INDEX))
                row['tm'] = doubles(int(value('tm')), row['ntst'] + 1)
                row['ld'] = int(value('*ndxloc'))
                row['ups'] = doubles(int(value('ups')), row['ld'] * row['ndim'] * row['ncol'])
        out.write(json.dumps(row) + '\n')
        return False


class TestReturn(gdb.FinishBreakpoint):
    def __init__(self, row, chng):
        super().__init__(internal=True)
        self.row, self.chng = row, chng

    def stop(self):
        enabled = bool(int(value(f'*(logical *){self.chng}')))
        out.write(json.dumps({**self.row, 'value': float(self.return_value), 'enabled': enabled}) + '\n')
        return False


for function in ('solvbv', 'setubv', 'stplbv', *TESTS):
    Trace(function)
gdb.execute('run')
