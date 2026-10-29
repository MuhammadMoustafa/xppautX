"""W439 (#501): Linux process clocks used by the comparison's drivers.

These clocks are observations, never acceptance criteria. Callers opt in; the
normal numerical checks do not read them.
"""
import os
from pathlib import Path
import signal
import subprocess
import threading
import time


def run_foreground(argv, *, cwd, env, timeout, stdout=subprocess.PIPE, stderr=subprocess.PIPE):
    """Wait for our Linux child; a timeout also stops its own descendants.

    AUTO's Python wrapper starts its executable and compiler. A private
    process group lets a failed wrapper leave none of those running.
    The wait itself has no timeout: with one, CPython polls waitpid with
    sleeps of up to 50 ms, and a measured wall time would be rounded up
    to that step. A timer enforces the limit instead.
    """
    with subprocess.Popen(argv, cwd=cwd, env=env, stdout=stdout, stderr=stderr,
                          text=True, start_new_session=True) as child:
        expired = threading.Event()

        def stop():
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass

        def expire():
            expired.set()
            stop()
        timer = threading.Timer(timeout, expire)
        timer.start()
        try:
            output, errors = child.communicate()
        except BaseException:
            stop()
            child.communicate()
            raise
        finally:
            timer.cancel()
        if expired.is_set():
            raise subprocess.TimeoutExpired(argv, timeout, output, errors)
        return subprocess.CompletedProcess(argv, child.returncode, output, errors)


def process_cpu(pid):
    # /proc/PID/stat comm may contain spaces and parentheses. Fields after its
    # final ')' start at field 3; utime/stime are fields 14 and 15.
    fields = (Path('/proc') / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')


def clock(pid):
    return time.perf_counter(), process_cpu(pid)


def elapsed(pid, start):
    wall, cpu = clock(pid)
    return dict(wall_seconds=wall - start[0], cpu_seconds=cpu - start[1])


class RunClock:
    """Observe the shared command adapter without another implementation of it.

    A run starts when auto.run is sent and ends with its done. Setup, grab,
    disk exports and client preparation are outside the window. The event
    subscription is the caller's.
    """
    def __init__(self, server):
        self.server = server
        self.send = server.send
        self.collect = server.collect
        self.start = None
        self.pending = False
        self.rows = []
        server.send = self.observed_send
        server.collect = self.observed_collect

    def observed_send(self, **cmd):
        if cmd.get('cmd') == 'auto.run':
            self.pending = True
            self.start = clock(self.server.proc.pid)
        return self.send(**cmd)

    def observed_collect(self, until, timeout=10):
        events, end = self.collect(until, timeout)
        if self.pending and end and end.get('ev') == 'done':
            self.rows.append(elapsed(self.server.proc.pid, self.start))
            self.pending = False
        return events, end
