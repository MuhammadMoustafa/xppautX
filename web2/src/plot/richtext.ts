/* XPP's text labels (Text,etc/Text) as Unicode runs. The core draws a
   label with its own little markup (core/graphics_x11.c
   special_put_text_x11): a backslash and a letter switch the rest of the
   text to the symbol font (\1, where the Latin letters are Greek ones:
   \1a is alpha) or back to roman (\0), down to a subscript (\s) or up to a
   superscript (\S) one size smaller, or back to the baseline and size
   (\n); any other character after a backslash is dropped. Shifts add up
   (\s\s is lower still), as XPP's do. Here the symbol font's letters
   become the Unicode Greek letters they show, so the page draws real text
   in its own font. Pure: no DOM. */

/** Latin letters in the symbol font: the Greek letter each one shows (Adobe Symbol encoding) */
const GREEK: Record<string, string> = {
  a: 'α', b: 'β', c: 'χ', d: 'δ', e: 'ε', f: 'φ', g: 'γ', h: 'η', i: 'ι', j: 'ϕ', k: 'κ', l: 'λ', m: 'μ',
  n: 'ν', o: 'ο', p: 'π', q: 'θ', r: 'ρ', s: 'σ', t: 'τ', u: 'υ', v: 'ϖ', w: 'ω', x: 'ξ', y: 'ψ', z: 'ζ',
  A: 'Α', B: 'Β', C: 'Χ', D: 'Δ', E: 'Ε', F: 'Φ', G: 'Γ', H: 'Η', I: 'Ι', J: 'ϑ', K: 'Κ', L: 'Λ', M: 'Μ',
  N: 'Ν', O: 'Ο', P: 'Π', Q: 'Θ', R: 'Ρ', S: 'Σ', T: 'Τ', U: 'Υ', V: 'ς', W: 'Ω', X: 'Ξ', Y: 'Ψ', Z: 'Ζ',
};

/** the symbol font's text as Unicode: its Latin letters as Greek, anything else as it is */
export function symbolToUnicode(s: string): string {
  let out = '';
  for (const c of s) out += GREEK[c] ?? c;
  return out;
}

export interface TextRun {
  /** Unicode: symbol-font letters already Greek */
  text: string;
  /** the baseline's shift in XPP's units: +1 a superscript (the font's ascent), -0.5 a subscript */
  rise: number;
  /** one size smaller (a sub- or superscript) */
  small: boolean;
}

/** a label's text as runs of one style each (empty runs left out); symbol:
    the whole label in the symbol font (a label's font 1) */
export function parseRichText(s: string, symbol = false): TextRun[] {
  const runs: TextRun[] = [];
  let greek = symbol, rise = 0, small = false, cur = '';
  const flush = () => {
    if (cur) runs.push({text: greek ? symbolToUnicode(cur) : cur, rise, small});
    cur = '';
  };
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (c !== '\\') {
      cur += c;
      continue;
    }
    flush();
    const e = s[++i];
    if (e === '0') greek = false;
    else if (e === '1') greek = true;
    else if (e === 'n') {
      rise = 0;
      small = false;
    } else if (e === 's') {
      rise -= 0.5;
      small = true;
    } else if (e === 'S') {
      rise += 1;
      small = true;
    }
  }
  flush();
  return runs;
}

/** what a label says, as plain text (for names and the plot's description) */
export function plainText(s: string, symbol = false): string {
  return parseRichText(s, symbol).map(r => r.text).join('');
}
