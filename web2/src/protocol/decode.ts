/* A series column's values as they come (docs/protocol.md "The plot as
   data"): JSON numbers with null for NaN, or a base64 string of
   little-endian float32 (enc "f32"). They are the core's float32 storage
   either way, so they are kept as Float32Array: half the memory of doubles,
   and a million f32 rows need no copy at all. Pure; no DOM. */
import type {SeriesData} from './types';

const LITTLE = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

/** how many values `data` holds */
export function valueCount(data: SeriesData): number {
  if (typeof data !== 'string') return data.length;
  const n = data.length;
  const pad = n > 0 && data[n - 1] === '=' ? (n > 1 && data[n - 2] === '=' ? 2 : 1) : 0;
  return Math.floor(((n / 4) * 3 - pad) / 4);
}

function base64Bytes(s: string): Uint8Array {
  const native = (Uint8Array as unknown as {fromBase64?: (s: string) => Uint8Array}).fromBase64;
  if (native) return native(s);
  const bin = atob(s), out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

/** writes the values of `data` into out[at..], returns how many */
export function decodeInto(data: SeriesData, out: Float32Array, at: number): number {
  if (typeof data !== 'string') {
    for (let i = 0; i < data.length; i++) out[at + i] = data[i] ?? NaN;
    return data.length;
  }
  const f = floats(base64Bytes(data));
  out.set(f, at);
  return f.length;
}

/** the float32s of little-endian bytes: a view of them where the host is little-endian */
function floats(bytes: Uint8Array): Float32Array {
  const n = bytes.length >> 2;
  if (LITTLE && bytes.byteOffset % 4 === 0) return new Float32Array(bytes.buffer, bytes.byteOffset, n);
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = v.getFloat32(4 * i, true);
  return out;
}

/** the values of `data` as a new array (f32: the decoded bytes themselves, no copy) */
export function decode(data: SeriesData): Float32Array {
  if (typeof data === 'string') return floats(base64Bytes(data));
  const out = new Float32Array(data.length);
  decodeInto(data, out, 0);
  return out;
}
