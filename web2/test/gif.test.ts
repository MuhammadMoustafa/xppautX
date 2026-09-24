/* The GIF encoder (docs/ui-v2.md T15, src/plot/gif.ts), without a browser
   (npm test). A small standalone GIF89a/LZW decoder, independent of the
   encoder's own code, checks that what it writes is a real, decodable GIF:
   the frame count and each frame's size structurally, and its pixels (at
   the encoder's fixed 6x6x6 "web safe" quantizing) round-trip. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {encodeGif, type GifFrame} from '../src/plot/gif';

/* ---- a minimal GIF reader, independent of gif.ts's own LZW writer ---- */

function lzwDecode(data: number[], minCodeSize: number, expectedLen: number): number[] {
  const clearCode = 1 << minCodeSize, eoiCode = clearCode + 1;
  let dict: number[][] = [], codeSize = 0, nextCode = 0;
  const reset = () => {
    dict = [];
    for (let i = 0; i < clearCode; i++) dict[i] = [i];
    nextCode = eoiCode + 1;
    codeSize = minCodeSize + 1;
  };
  reset();
  let bitPos = 0;
  const totalBits = data.length * 8;
  const readCode = (size: number): number => {
    let code = 0;
    for (let i = 0; i < size; i++) {
      const byteIndex = bitPos >> 3, bitIndex = bitPos & 7;
      code |= ((data[byteIndex] >> bitIndex) & 1) << i;
      bitPos++;
    }
    return code;
  };
  const out: number[] = [];
  let prev: number[] | null = null;
  while (bitPos + codeSize <= totalBits) {
    const code = readCode(codeSize);
    if (code === clearCode) {
      reset();
      prev = null;
      continue;
    }
    if (code === eoiCode) break;
    let entry: number[];
    if (dict[code]) entry = dict[code];
    else if (code === nextCode && prev) entry = [...prev, prev[0]];
    else throw new Error(`gif test decoder: bad LZW code ${code}`);
    for (const v of entry) out.push(v);
    if (prev) {
      dict[nextCode] = [...prev, entry[0]];
      nextCode++;
      if (nextCode === (1 << codeSize) && codeSize < 12) codeSize++;
    }
    prev = entry;
  }
  return out.slice(0, expectedLen);
}

interface DecodedFrame {
  w: number;
  h: number;
  delayCs: number;
  rgb: number[]; /* w*h*3, palette-quantized */
}

function decodeGif(bytes: Uint8Array): {w: number; h: number; palette: number[][]; frames: DecodedFrame[]} {
  let p = 0;
  const u8 = () => bytes[p++];
  const u16 = () => { const v = bytes[p] | (bytes[p + 1] << 8); p += 2; return v; };
  assert.equal(String.fromCharCode(...bytes.slice(0, 6)), 'GIF89a');
  p = 6;
  const w = u16(), h = u16();
  const packed = u8();
  u8(); u8(); /* background colour index, pixel aspect ratio */
  const gctFlag = !!(packed & 0x80), gctSize = gctFlag ? 2 << (packed & 7) : 0;
  const palette: number[][] = [];
  for (let i = 0; i < gctSize; i++) palette.push([u8(), u8(), u8()]);
  const frames: DecodedFrame[] = [];
  let delayCs = 0;
  for (;;) {
    const block = u8();
    if (block === 0x3b) break; /* trailer */
    if (block === 0x21) { /* extension */
      const label = u8();
      if (label === 0xf9) {
        const size = u8();
        assert.equal(size, 4);
        u8(); /* packed */
        delayCs = u16();
        u8(); /* transparent colour index */
        assert.equal(u8(), 0); /* block terminator */
      } else {
        let len: number;
        while ((len = u8()) !== 0) p += len;
      }
      continue;
    }
    assert.equal(block, 0x2c, `unexpected GIF block 0x${block.toString(16)}`);
    u16(); u16(); /* left, top */
    const fw = u16(), fh = u16();
    const ipacked = u8();
    let framePalette = palette;
    if (ipacked & 0x80) {
      const lsize = 2 << (ipacked & 7);
      framePalette = [];
      for (let i = 0; i < lsize; i++) framePalette.push([u8(), u8(), u8()]);
    }
    const minCodeSize = u8();
    const data: number[] = [];
    let len: number;
    while ((len = u8()) !== 0) for (let i = 0; i < len; i++) data.push(u8());
    const indices = lzwDecode(data, minCodeSize, fw * fh);
    const rgb: number[] = [];
    for (const idx of indices) rgb.push(...framePalette[idx]);
    frames.push({w: fw, h: fh, delayCs, rgb});
  }
  return {w, h, palette, frames};
}

/* a solid-colour w x h frame, quantized the way the encoder will: */
function solid(w: number, h: number, [r, g, b]: [number, number, number]): GifFrame {
  const rgb = new Uint8ClampedArray(w * h * 3);
  for (let i = 0; i < w * h; i++) {
    rgb[i * 3] = r;
    rgb[i * 3 + 1] = g;
    rgb[i * 3 + 2] = b;
  }
  return {w, h, rgb};
}

test('encodeGif rejects no frames', () => {
  assert.throws(() => encodeGif([]));
});

test('encodeGif rejects frames of different sizes', () => {
  assert.throws(() => encodeGif([solid(2, 2, [0, 0, 0]), solid(3, 2, [0, 0, 0])]));
});

test('two frames: the header, frame count and each frame\'s size', () => {
  const bytes = encodeGif([solid(4, 3, [255, 0, 0]), solid(4, 3, [0, 0, 255])], 80);
  const gif = decodeGif(bytes);
  assert.equal(gif.w, 4);
  assert.equal(gif.h, 3);
  assert.equal(gif.frames.length, 2);
  for (const f of gif.frames) {
    assert.equal(f.w, 4);
    assert.equal(f.h, 3);
  }
});

test('the delay (centiseconds) round-trips', () => {
  const bytes = encodeGif([solid(2, 2, [0, 0, 0]), solid(2, 2, [255, 255, 255])], 250);
  const gif = decodeGif(bytes);
  assert.equal(gif.frames[0].delayCs, 25);
  assert.equal(gif.frames[1].delayCs, 25);
});

test('the pixels round-trip through the fixed 6x6x6 web-safe palette', () => {
  /* 51 is exact (a palette level); 130 and 4 round to the nearest level
     (153 and 0), the same quantizing core/ui_json.cpp's web_safe_colors does
     for the files it writes */
  const red = solid(3, 2, [255, 51, 4]), blue = solid(3, 2, [0, 130, 255]);
  const bytes = encodeGif([red, blue], 100);
  const gif = decodeGif(bytes);
  assert.equal(gif.frames.length, 2);
  const px = (f: DecodedFrame, i: number): number[] => [f.rgb[i * 3], f.rgb[i * 3 + 1], f.rgb[i * 3 + 2]];
  for (let i = 0; i < 6; i++) {
    assert.deepEqual(px(gif.frames[0], i), [255, 51, 0]);
    assert.deepEqual(px(gif.frames[1], i), [0, 153, 255]);
  }
});

test('a larger, varied frame (every pixel a different colour) still decodes to the right count', () => {
  const w = 16, h = 12, rgb = new Uint8ClampedArray(w * h * 3);
  for (let i = 0; i < w * h; i++) {
    rgb[i * 3] = (i * 7) % 256;
    rgb[i * 3 + 1] = (i * 13) % 256;
    rgb[i * 3 + 2] = (i * 29) % 256;
  }
  const bytes = encodeGif([{w, h, rgb}, {w, h, rgb}], 40);
  const gif = decodeGif(bytes);
  assert.equal(gif.frames.length, 2);
  assert.equal(gif.frames[0].rgb.length, w * h * 3);
  /* every decoded pixel is on the 6x6x6 palette */
  for (let i = 0; i < w * h; i++) {
    for (const c of [gif.frames[0].rgb[i * 3], gif.frames[0].rgb[i * 3 + 1], gif.frames[0].rgb[i * 3 + 2]]) {
      assert.ok([0, 51, 102, 153, 204, 255].includes(c), String(c));
    }
  }
});
