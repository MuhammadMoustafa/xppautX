# Shared raw-double access for the matrix and Newton gdb collectors.
import gdb, struct


def doubles(address, count):
    raw = gdb.selected_inferior().read_memory(address, 8 * count).tobytes()
    return list(struct.unpack('<%dd' % count, raw))
