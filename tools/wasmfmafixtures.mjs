/* W185: one comparison owner for Node and browser FMA checks. */
export function checkFma(fma, vectors) {
  const view = new DataView(new ArrayBuffer(8));
  function value(hex) {view.setBigUint64(0,BigInt('0x'+hex)); return view.getFloat64(0);}
  function bits(number) {view.setFloat64(0,number); return view.getBigUint64(0).toString(16).padStart(16,'0');}
  let failures = 0;
  let firstMismatch = null;
  for (const [a,b,c,expected] of vectors) {
    const actual = fma(value(a),value(b),value(c));
    const equal = expected === 'nan' ? Number.isNaN(actual) : bits(actual) === expected;
    if (!equal) {
      firstMismatch ??= {a,b,c,expected,actual:bits(actual)};
      failures++;
    }
  }
  return {cases:vectors.length, failures, firstMismatch};
}