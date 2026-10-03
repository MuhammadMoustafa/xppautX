"""Platform-independent regression tests for the shared protocol checker."""
import unittest
from xppclient import macos_thread_states


class MacosThreadStates(unittest.TestCase):
    # Captured-format fixture from ps(1)'s -M table (ps.c's mfmt): one line
    # per thread, including state modifiers and spaces in the command.
    OUTPUT = '''USER   PID TT  %CPU STAT PRI     STIME     UTIME COMMAND
runner 123 ??   0.0 S    31T   0:00.01   0:00.03 ./xppautX --server
runner 123 ??   0.0 S+   31T   0:00.00   0:00.00 ./xppautX --server
'''

    def test_sleeping_threads(self):
        self.assertEqual(macos_thread_states(self.OUTPUT), ['S', 'S+'])

    def test_running_thread(self):
        self.assertEqual(macos_thread_states(self.OUTPUT.replace('S+ ', 'R+ ')), ['S', 'R+'])

    def test_no_threads(self):
        self.assertEqual(macos_thread_states(''), [])
        self.assertEqual(macos_thread_states(self.OUTPUT.splitlines()[0]), [])

    def test_bad_output_is_reported(self):
        with self.assertRaisesRegex(ValueError, 'output:1: missing STAT'):
            macos_thread_states('PID STATE\n123 S')
        with self.assertRaisesRegex(ValueError, 'output:2: missing thread state'):
            macos_thread_states(self.OUTPUT.splitlines()[0] + '\nrunner 123')


if __name__ == '__main__':
    unittest.main()
