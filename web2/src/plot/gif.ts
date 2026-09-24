/* A minimal animated GIF (GIF89a) encoder, entirely in the client
   (docs/ui-v2.md T15: "GIF/PNG from the client"; the project keeps its
   dependencies to a handful of small, deliberately chosen ones - Preact,
   uPlot - rather than pull one in for this, so this is a small module of
   our own instead). A fixed 6x6x6 "web safe" palette (the same 51-step
   quantizing core/ui_json.cpp's web_safe_colors uses for the files it
   writes) needs no per-frame palette search: a pixel's index is a plain
   formula. LZW is the standard GIF compressor: a trie of codes already
   seen, variable code width (up to 12 bits), packed LSB-first into
   length-prefixed sub-blocks. Frames loop (NETSCAPE2.0) since a kinescope
   is a movie. Pure: an RGB buffer in, a Uint8Array out. */

/** base64 of any byte array (RGB pixels, a whole GIF, ...), chunked so a
    large one never blows the call stack on `String.fromCharCode(...)` */
export function bytesToBase64(bytes: Uint8Array | Uint8ClampedArray): string {
  let s = '';
  const CHUNK = 0x2000;
  for (let i = 0; i < bytes.length; i += CHUNK) s += String.fromCharCode(...bytes.subarray(i, i + CHUNK));
  return btoa(s);
}

export interface GifFrame {
  w: number;
  h: number;
  /** RGB, w*h*3 bytes, no alpha (chart.ts Chart.pixels()) */
  rgb: Uint8ClampedArray;
}

const LEVELS = [0, 51, 102, 153, 204, 255];
const PALETTE_COLORS = 216; /* 6 levels x 6 levels x 6 levels */
const MIN_CODE_SIZE = 8; /* ceil(log2(216)): also the global colour table's own bit depth, 2^8 = 256 */

function buildPalette(): Uint8Array {
  const pal = new Uint8Array(256 * 3);
  for (let idx = 0; idx < PALETTE_COLORS; idx++) {
    const ri = Math.floor(idx / 36), gi = Math.floor((idx % 36) / 6), bi = idx % 6;
    pal[idx * 3] = LEVELS[ri];
    pal[idx * 3 + 1] = LEVELS[gi];
    pal[idx * 3 + 2] = LEVELS[bi];
  }
  return pal;
}

function paletteIndices(rgb: Uint8ClampedArray): Uint8Array {
  const n = rgb.length / 3;
  const out = new Uint8Array(n);
  for (let i = 0, j = 0; i < n; i++, j += 3) {
    const ri = Math.min(5, Math.round(rgb[j] / 51));
    const gi = Math.min(5, Math.round(rgb[j + 1] / 51));
    const bi = Math.min(5, Math.round(rgb[j + 2] / 51));
    out[i] = ri * 36 + gi * 6 + bi;
  }
  return out;
}

/** codes packed LSB-first into bytes, as GIF's LZW stream requires */
class BitWriter {
  private bytes: number[] = [];
  private buf = 0;
  private nbits = 0;

  writeCode(code: number, size: number): void {
    this.buf |= code << this.nbits;
    this.nbits += size;
    while (this.nbits >= 8) {
      this.bytes.push(this.buf & 0xff);
      this.buf >>= 8;
      this.nbits -= 8;
    }
  }

  finish(): number[] {
    if (this.nbits > 0) this.bytes.push(this.buf & 0xff);
    this.buf = 0;
    this.nbits = 0;
    return this.bytes;
  }
}

/** bytes, as GIF sub-blocks: each at most 255 bytes, length-prefixed, a 0 terminator */
function subBlocks(bytes: number[]): number[] {
  const out: number[] = [];
  for (let i = 0; i < bytes.length; i += 255) {
    const chunk = bytes.length - i < 255 ? bytes.length - i : 255;
    out.push(chunk);
    for (let j = 0; j < chunk; j++) out.push(bytes[i + j]);
  }
  out.push(0);
  return out;
}

/** GIF's variable-width LZW: a trie of codes already assigned (dict[code] maps
    the next symbol to the code for that longer sequence), clearing when the
    12-bit code space (4096 codes) is full */
function lzwEncode(indices: Uint8Array, minCodeSize: number): number[] {
  const clearCode = 1 << minCodeSize, eoiCode = clearCode + 1;
  const bw = new BitWriter();
  let dict: Map<number, number>[] = [];
  let codeSize = 0, maxCode = 0, nextCode = 0;
  const reset = () => {
    dict = new Array(4096);
    for (let i = 0; i < clearCode; i++) dict[i] = new Map();
    nextCode = eoiCode + 1;
    codeSize = minCodeSize + 1;
    maxCode = (1 << codeSize) - 1;
    bw.writeCode(clearCode, codeSize);
  };
  reset();

  let prefix = -1;
  for (let i = 0; i < indices.length; i++) {
    const k = indices[i];
    if (prefix === -1) {
      prefix = k;
      continue;
    }
    const next = dict[prefix].get(k);
    if (next !== undefined) {
      prefix = next;
      continue;
    }
    bw.writeCode(prefix, codeSize);
    if (nextCode === 4096) {
      reset();
    } else {
      dict[prefix].set(k, nextCode);
      dict[nextCode] = new Map();
      nextCode++;
      if (nextCode > maxCode && codeSize < 12) {
        codeSize++;
        maxCode = (1 << codeSize) - 1;
      }
    }
    prefix = k;
  }
  if (prefix !== -1) bw.writeCode(prefix, codeSize);
  bw.writeCode(eoiCode, codeSize);
  return subBlocks(bw.finish());
}

function u16(n: number): [number, number] {
  return [n & 0xff, (n >> 8) & 0xff];
}

function append(dst: number[], src: ArrayLike<number>): void {
  for (let i = 0; i < src.length; i++) dst.push(src[i]);
}

/** an animated GIF of `frames` (all the same size), `delayMs` apart, looping
    by default (a kinescope replays) */
export function encodeGif(frames: GifFrame[], delayMs = 100, loop = true): Uint8Array {
  if (!frames.length) throw new Error('encodeGif: no frames');
  const {w, h} = frames[0];
  if (frames.some(f => f.w !== w || f.h !== h)) throw new Error('encodeGif: all clips must be the same size');
  const bytes: number[] = [];
  append(bytes, [0x47, 0x49, 0x46, 0x38, 0x39, 0x61]); /* "GIF89a" */
  append(bytes, [...u16(w), ...u16(h), 0xf7, 0, 0]); /* global colour table, 256 entries, 8 bit */
  append(bytes, buildPalette());
  if (loop) {
    append(bytes, [0x21, 0xff, 0x0b]);
    append(bytes, [0x4e, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30]); /* "NETSCAPE2.0" */
    append(bytes, [0x03, 0x01, 0, 0, 0x00]); /* loop forever */
  }
  const delayCs = Math.max(1, Math.round(delayMs / 10));
  for (const f of frames) {
    append(bytes, [0x21, 0xf9, 0x04, 0x04, ...u16(delayCs), 0, 0x00]); /* graphic control: do not dispose */
    append(bytes, [0x2c, 0, 0, 0, 0, ...u16(f.w), ...u16(f.h), 0x00]); /* image descriptor */
    bytes.push(MIN_CODE_SIZE);
    append(bytes, lzwEncode(paletteIndices(f.rgb), MIN_CODE_SIZE));
  }
  bytes.push(0x3b); /* trailer */
  return new Uint8Array(bytes);
}
