/* The animation's frames (docs/protocol.md "The animation as data"): the
   primitives as sent, decoded; where the dimension box sits on a canvas of
   any size; and the mapping of a unit point (u, v: y up) to it. Pure: no
   DOM, unit-tested in Node (test/ani.test.ts). */
import type {AniColor, AniPrimWire} from '../protocol/types';

export type AniPrim =
  | {kind: 'line'; u1: number; v1: number; u2: number; v2: number; color: AniColor; width: number}
  | {kind: 'rect'; u1: number; v1: number; u2: number; v2: number; color: AniColor; width: number; fill: boolean}
  /** a circle's radius along u and along v (the .ani's r over the box's width and height) */
  | {kind: 'circle' | 'ellipse'; u: number; v: number; ru: number; rv: number; color: AniColor; width: number;
    fill: boolean}
  /** a comet's filled circle of r pixels */
  | {kind: 'dot'; u: number; v: number; r: number; color: AniColor}
  /** text from its baseline's left end; size 0..4, font 0 roman, 1 symbol (Greek) */
  | {kind: 'text'; u: number; v: number; text: string; color: AniColor; size: number; font: number};

/** the primitives of a frame event, in order; one with a coordinate the
    .ani's formula has no value for (null: NaN) is left out */
export function decodePrims(wire: readonly AniPrimWire[]): AniPrim[] {
  const out: AniPrim[] = [];
  for (const p of wire) {
    switch (p[0]) {
      case 'line': {
        const [, u1, v1, u2, v2, color, width] = p;
        if (u1 !== null && v1 !== null && u2 !== null && v2 !== null) out.push({kind: 'line', u1, v1, u2, v2, color, width});
        break;
      }
      case 'rect': {
        const [, u1, v1, u2, v2, color, width, fill] = p;
        if (u1 !== null && v1 !== null && u2 !== null && v2 !== null)
          out.push({kind: 'rect', u1, v1, u2, v2, color, width, fill: fill !== 0});
        break;
      }
      case 'circle':
      case 'ellipse': {
        const [kind, u, v, ru, rv, color, width, fill] = p;
        if (u !== null && v !== null && ru !== null && rv !== null)
          out.push({kind, u, v, ru: Math.abs(ru), rv: Math.abs(rv), color, width, fill: fill !== 0});
        break;
      }
      case 'dot': {
        const [, u, v, r, color] = p;
        if (u !== null && v !== null) out.push({kind: 'dot', u, v, r, color});
        break;
      }
      case 'text': {
        const [, u, v, text, color, size, font] = p;
        if (u !== null && v !== null) out.push({kind: 'text', u, v, text, color, size, font});
        break;
      }
    }
  }
  return out;
}

/** where the dimension box is drawn on a canvas, CSS pixels */
export interface Box {
  x: number;
  y: number;
  w: number;
  h: number;
}

/** the box's aspect (width over height): its units are equal along x and
    y, so a pendulum's rod keeps its length as it swings */
export function aspectOf(dim: readonly number[]): number {
  const w = dim[2] - dim[0], h = dim[3] - dim[1];
  return w > 0 && h > 0 && Number.isFinite(w / h) ? w / h : 1;
}

/** the largest box of that aspect centred in a cw x ch canvas, less a margin */
export function fitBox(cw: number, ch: number, aspect: number, margin = 0): Box {
  const aw = Math.max(0, cw - 2 * margin), ah = Math.max(0, ch - 2 * margin);
  let w = aw, h = aspect > 0 ? aw / aspect : ah;
  if (h > ah) {
    h = ah;
    w = ah * aspect;
  }
  return {x: (cw - w) / 2, y: (ch - h) / 2, w, h};
}

/** a unit point on the canvas (v is up, the canvas's y down) */
export function toCanvas(box: Box, u: number, v: number): [number, number] {
  return [box.x + u * box.w, box.y + (1 - v) * box.h];
}

/** the canvas point as a unit point: the inverse of toCanvas */
export function fromCanvas(box: Box, x: number, y: number): [number, number] {
  return [box.w ? (x - box.x) / box.w : 0, box.h ? 1 - (y - box.y) / box.h : 0];
}

/** pixel sizes (line widths, dots, text) are the classic window's: they grow
    and shrink with the picture's area, within a half and four times */
export function penScale(box: Box, w: number, h: number): number {
  if (!(w > 0 && h > 0) || !(box.w > 0 && box.h > 0)) return 1;
  return Math.min(4, Math.max(0.5, Math.sqrt((box.w * box.h) / (w * h))));
}

/** a line of width 0 is X11's thin line: one pixel */
export function lineWidth(width: number, scale: number): number {
  return Math.max(1, width) * scale;
}

/** XPP's five text sizes, px (web/xpp-client.js TEXT_SIZES) */
const TEXT_SIZES = [8, 10, 12, 14, 18];

export function scaledTextPx(size: number, scale: number): number {
  return TEXT_SIZES[Math.max(0, Math.min(4, Math.round(size)))] * scale;
}

/** a circle's radius on the canvas: the mean of its radius along x and along y, as XPP draws it */
export function circleRadius(box: Box, ru: number, rv: number): number {
  return (ru * box.w + rv * box.h) / 2;
}

/* the symbol font's letters as Greek (the X11 symbol font's mapping, web/xpp-client.js GREEK) */
const GREEK: Record<string, string> = {
  a: 'α', b: 'β', c: 'χ', d: 'δ', e: 'ε', f: 'φ', g: 'γ', h: 'η', i: 'ι', j: 'ϕ', k: 'κ', l: 'λ', m: 'μ',
  n: 'ν', o: 'ο', p: 'π', q: 'θ', r: 'ρ', s: 'σ', t: 'τ', u: 'υ', v: 'ϖ', w: 'ω', x: 'ξ', y: 'ψ', z: 'ζ',
  A: 'Α', B: 'Β', C: 'Χ', D: 'Δ', E: 'Ε', F: 'Φ', G: 'Γ', H: 'Η', I: 'Ι', K: 'Κ', L: 'Λ', M: 'Μ', N: 'Ν',
  O: 'Ο', P: 'Π', Q: 'Θ', R: 'Ρ', S: 'Σ', T: 'Τ', U: 'Υ', W: 'Ω', X: 'Ξ', Y: 'Ψ', Z: 'Ζ',
};

/** the text a primitive shows: the symbol font's as Greek, without the .ani's trailing blanks */
export function textOf(p: {text: string; font: number}): string {
  const t = p.text.replace(/\s+$/, '');
  return p.font === 1 ? Array.from(t, ch => GREEK[ch] ?? ch).join('') : t;
}
