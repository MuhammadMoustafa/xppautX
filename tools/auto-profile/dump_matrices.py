# W358 (#420): a gdb script (tools/auto-profile/gdb_wrap.sh runs it): writes to $MATRIX_OUT, one line
# per call, the matrices the program hands to EISPACK, so a reference can be computed outside
# the program (tools/auto-profile/eig_accuracy.py). The program carries no code for this.
#   rg NM N MATZ A...     eig / HomCont's eigenvectors: a general matrix (column-major, n columns of n)
#   qz NM N A... B...     flowkm's deflated Floquet pencil, at qzhes: A x = lambda B x
# Numbers are C99 hex floats (exact). A call that cannot be read is skipped.
import gdb, os, struct

out = open(os.environ['MATRIX_OUT'], 'a', buffering=1)


def doubles(address, count):
    raw = gdb.selected_inferior().read_memory(address, 8 * count).tobytes()
    return [float.hex(x) for x in struct.unpack('<%dd' % count, raw)]


def block(pointer, nm, n):
    """the n x n matrix at pointer with leading dimension nm, as n columns of n"""
    address = int(pointer)
    return [x for j in range(n) for x in doubles(address + 8 * j * nm, n)]


class Dump(gdb.Breakpoint):
    def __init__(self, function, kind):
        super().__init__(function, internal=True)
        self.kind = kind

    def stop(self):
        try:
            nm, n = int(gdb.parse_and_eval('nm')), int(gdb.parse_and_eval('n'))
            if self.kind == 'rg':
                line = ['rg', nm, n, int(gdb.parse_and_eval('matz'))] + block(gdb.parse_and_eval('a'), nm, n)
            else:
                line = ['qz', nm, n] + block(gdb.parse_and_eval('a'), nm, n) + block(gdb.parse_and_eval('b'), nm, n)
            out.write(' '.join(map(str, line)) + '\n')
        except gdb.error:
            pass
        return False


Dump('rg', 'rg')
Dump('qzhes', 'qz')
gdb.execute('run')
