"""The comparison of an exported model's rows with xppautX's own (W764, #843; W766, #846; W295, #353), shared by tools/exportcheck.py and
the comparison kit's server study (paper/benchmarks/comparison/server.py): one place for what "the same rows" means.

An exported script's rows against the output.dat of the model's `--silent` run (xppautX keeping doubles, `with_double_storage`): a model of
arithmetic alone is compared exactly, a model that calls a function (exp, tanh, ...) to RELATIVE and ABSOLUTE (xpp::math's correctly rounded
result against the C library's, an ulp each, which a run grows to a few 1e-11 on lecar: 1e-9 fails what is wrong and nothing else), and an
adaptive method's rows (scipy at a tolerance) to the bound its caller gives. "Relative" is to the value's column scale, the largest magnitude
xppautX's column reaches, when the value is smaller (W813, #894: a state crossing zero, lecdiff200's v20 at 2.1e-4 of a column reaching 0.49,
differed by 8.6e-9 of itself and 4e-12 of its column, an ulp's drift that no relative bound of the value alone can tell from an error).
"""
import math
import re

RELATIVE, ABSOLUTE = 1e-9, 1e-12


def with_double_storage(text):
    """the model's text with storage_precision: double in its options' integration group"""
    i = text.find('options {')
    if i < 0:
        return text + 'options { integration: { storage_precision: double } }\n'
    group = re.compile(r'integration:\s*\{').search(text, i)
    at = group.end() if group else i + len('options {')
    return text[:at] + (' storage_precision: double,' if group else ' integration: { storage_precision: double },') + text[at:]


def on_runge_kutta(total_time=None):
    """a text edit: the model on the method the script integrates with, run for total_time when given (a chaotic model's last rows
    would grow the ulp that the C library's functions differ by to the tolerance)"""
    def edit(text):
        if re.search(r'\bmethod:\s*\w+', text):
            text = re.sub(r'\bmethod:\s*\w+', 'method: runge_kutta', text, count=1)
        else:
            text = text.replace('integration: {', 'integration: { method: runge_kutta,', 1)
        return re.sub(r'\btotal_time:\s*[\d.]+', 'total_time: %g' % total_time, text, count=1) if total_time else text
    return edit


def first_difference(ours, theirs, exact=False, relative=RELATIVE, absolute=ABSOLUTE, names=None, other='script'):
    """Two tables of numbers (rows of values or of numeric text) compared value by value: (where, largest), where being '' when every
    value agrees (equal when exact, else within relative of the larger of the two and of the column's scale, and absolute of zero) or the
    first worst one as text, largest the largest relative difference seen (by the same scale); `names` are the columns' names for the
    text, `other` what `theirs` is called. The caller checks the shapes first."""
    ours = [[float(x) for x in row] for row in ours]
    scale = [max((abs(x) for x in column if not math.isnan(x)), default=0.0) for column in zip(*ours)]
    worst, where, largest = 0.0, '', 0.0
    for i, (a, b) in enumerate(zip(ours, theirs)):
        for j, (x, y) in enumerate(zip(a, b)):
            y = float(y)
            if math.isnan(x) and math.isnan(y):
                continue
            size = max(abs(x), abs(y), scale[j])
            largest = max(largest, abs(x - y) / max(size, 1e-300))
            excess = (x != y) if exact else abs(x - y) - (relative * size + absolute)
            if excess > worst or (excess > 0 and not where):
                worst, where = max(worst, excess), 'row %d column %s: xppautX %r, %s %r' % (i, names[j] if names else j, x, other, y)
    return where, largest
