/* What an input box accepts (T31): one validator per kind, the one place
   the page decides whether a box's text may be committed. Every box goes
   through ui/Field.tsx with one of these specs; the core's `string` and
   `form` asks name each field's kind (docs/protocol.md "Asks", `kinds`),
   read by specOfKind. Pure.

   integer     a whole number, digits only (what the core's atoi reads
               whole), optionally within min..max
   number      a decimal number (1e-3, -0.5, .5), optionally above 0, other
               than 0 or within min..max; with `formula`, also XPP's %formula (%2*pi),
               which the core evaluates (new_float, the values panel)
   expression  a formula of the model (a boundary condition, a delay's
               initial data, a right-hand side): not empty, its brackets
               matched
   name        one of a given set (the model's variables or parameters),
               case not mattering; the box suggests them
   file        a file's base name, the core's own rule (store/files.ts safeName)
   text        anything */
import {safeName} from './files';

export type FieldSpec =
  | {kind: 'integer'; min?: number; max?: number}
  | {kind: 'number'; formula?: boolean; positive?: boolean; nonzero?: boolean; min?: number; max?: number}
  | {kind: 'expression'}
  | {kind: 'name'; names: readonly string[]; what?: string}
  | {kind: 'file'}
  | {kind: 'text'};

export type FieldKind = FieldSpec['kind'];

/** the specs most boxes use */
export const TEXT: FieldSpec = {kind: 'text'};
export const NUMBER: FieldSpec = {kind: 'number'};
/** a number, or XPP's %formula (a parameter, an initial condition) */
export const FORMULA: FieldSpec = {kind: 'number', formula: true};
export const EXPRESSION: FieldSpec = {kind: 'expression'};
export const FILE: FieldSpec = {kind: 'file'};

type SpecOf<K extends FieldKind> = Extract<FieldSpec, {kind: K}>;

interface KindEntry<K extends FieldKind> {
  /** the message for `text`, or null when the kind takes it */
  check: (text: string, spec: SpecOf<K>) => string | null;
  /** the on-screen keyboard it wants */
  inputMode: (spec: SpecOf<K>) => 'numeric' | 'decimal' | 'text';
}

const INTEGER = /^[+-]?\d+$/;
const DECIMAL = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;

export const FORMULA_HINT = 'a number, or %formula such as %2*pi';

function range(v: number, what: string, min?: number, max?: number): string | null {
  if (min !== undefined && max !== undefined && (v < min || v > max)) return `${what} from ${min} to ${max}`;
  if (min !== undefined && v < min) return `${what} of at least ${min}`;
  if (max !== undefined && v > max) return `${what} of at most ${max}`;
  return null;
}

/** brackets in order and closed: the one check an expression gets before the core parses it */
function bracketsMatch(text: string): boolean {
  const open: string[] = [];
  const pair: Record<string, string> = {')': '(', ']': '['};
  for (const c of text) {
    if (c === '(' || c === '[') open.push(c);
    else if (c in pair && open.pop() !== pair[c]) return false;
  }
  return open.length === 0;
}

/** the registry: one entry per kind */
const KINDS: {[K in FieldKind]: KindEntry<K>} = {
  integer: {
    check: (text, spec) => {
      const t = text.trim();
      if (!INTEGER.test(t)) return 'a whole number';
      return range(Number(t), 'a whole number', spec.min, spec.max);
    },
    inputMode: () => 'numeric',
  },
  number: {
    check: (text, spec) => {
      const t = text.trim();
      if (spec.formula && t.startsWith('%')) return t.length > 1 && bracketsMatch(t) ? null : FORMULA_HINT;
      if (!DECIMAL.test(t) || !Number.isFinite(Number(t))) return spec.formula ? FORMULA_HINT : 'a number';
      const v = Number(t);
      if (spec.positive && !(v > 0)) return 'a number above 0';
      if (spec.nonzero && v === 0) return 'a number other than 0';
      return range(v, 'a number', spec.min, spec.max);
    },
    inputMode: () => 'decimal',
  },
  expression: {
    check: text => {
      const t = text.trim();
      if (!t) return 'an expression, such as 2*x+1';
      return bracketsMatch(t) ? null : 'an expression whose brackets match';
    },
    inputMode: () => 'text',
  },
  name: {
    check: (text, spec) => {
      const t = text.trim().toLowerCase();
      if (t && spec.names.some(n => n.toLowerCase() === t)) return null;
      const some = spec.names.slice(0, 3).join(', ');
      return `${spec.what ?? 'a name of the model'}${some ? `, such as ${some}` : ''}`;
    },
    inputMode: () => 'text',
  },
  file: {
    check: text => (safeName(text) ? null : 'a file name only: no folders, no leading dot, none of \\ / : * ? " < > |'),
    inputMode: () => 'text',
  },
  text: {
    check: () => null,
    inputMode: () => 'text',
  },
};

function entry<K extends FieldKind>(spec: SpecOf<K>): KindEntry<K> {
  return KINDS[spec.kind as K];
}

/** why `text` is not what the box takes (a short phrase, "a whole number"), or null when it is */
export function fieldError(spec: FieldSpec, text: string): string | null {
  return entry(spec).check(text, spec as never);
}

/* the start of a number still being typed: a sign, a point, an exponent's e and sign */
const INTEGER_START = /^[+-]?$/;
const DECIMAL_START = /^[+-]?\d*\.?\d*([eE][+-]?)?$/;

/** whether `text` is the start of what the box takes rather than a wrong entry ("-" on the way
    to "-0.5", "1e-" to "1e-3", "%" to a formula): a box does not flag it while it is typed,
    only when it is committed as it is */
export function fieldIncomplete(spec: FieldSpec, text: string): boolean {
  const t = text.trim();
  if (spec.kind === 'integer') return INTEGER_START.test(t);
  if (spec.kind === 'number') return (!!spec.formula && t === '%') || (DECIMAL_START.test(t) && !DECIMAL.test(t));
  return false;
}

/** whether every text is what its box takes */
export function fieldsValid(specs: readonly FieldSpec[], texts: readonly string[]): boolean {
  return specs.every((s, i) => fieldError(s, texts[i] ?? '') === null);
}

/* ---- what a box lets through while it is typed (T35d): a keystroke, a paste or a drop is
   taken only when the resulting text is what the kind takes, or an incomplete prefix of it
   (fieldError/fieldIncomplete above); a number with a formula lets anything through once it
   starts with '%', since the core judges it (WF-001). expression/name/file/text are never
   filtered here: their check runs only on commit. Nothing is ever stripped or silently
   changed; a refused edit leaves the box exactly as it was, with a message (below). */
export function fieldAcceptsEdit(spec: FieldSpec, text: string): boolean {
  const t = text.trim();
  if (spec.kind === 'number' && spec.formula && t.startsWith('%')) return true;
  if (spec.kind === 'integer') return INTEGER_START.test(t) || INTEGER.test(t);
  if (spec.kind === 'number') return DECIMAL_START.test(t) || DECIMAL.test(t);
  return true;
}

/** "a number", "a whole number", ... the noun a kind's messages name it by */
function kindNoun(spec: FieldSpec): string {
  if (spec.kind === 'integer') return 'a whole number';
  if (spec.kind === 'number') return spec.formula ? 'a number or formula' : 'a number';
  return 'that';
}

/** a number/integer-only reason for refusing `ch` right after `before`, more specific than
    "can't go in": a second decimal point, one after the exponent, or a second e */
function numberCharReason(before: string, ch: string): string | null {
  const b = before.trim();
  if (ch === '.') {
    if (/e/i.test(b)) return "A number's exponent takes no decimal point";
    if (b.includes('.')) return 'A number has one decimal point';
  }
  if ((ch === 'e' || ch === 'E') && /e/i.test(b)) return 'A number has one e';
  return null;
}

/** the message for one refused character typed after `before` (a keystroke fieldAcceptsEdit
    turned away): names the character, or a more specific reason when there is one */
export function fieldCharMessage(spec: FieldSpec, before: string, ch: string): string {
  if (spec.kind === 'number') {
    const specific = numberCharReason(before, ch);
    if (specific) return specific;
  }
  return `"${ch}" can't go in ${kindNoun(spec)}`;
}

function shorten(text: string): string {
  return text.length > 24 ? `${text.slice(0, 21)}...` : text;
}

/** the first character of `added` (inserted between `before` and `after`, a paste or a drop)
    that a keystroke-by-keystroke typing of it would have refused, or null when all of it is
    taken */
export function fieldPasteOffender(spec: FieldSpec, before: string, added: string, after: string):
  {ch: string; index: number} | null {
  let acc = before;
  for (let i = 0; i < added.length; i++) {
    const next = acc + added[i];
    if (!fieldAcceptsEdit(spec, next + after)) return {ch: added[i], index: i + 1};
    acc = next;
  }
  return null;
}

/** the message for a refused paste or drop: the text pasted (shortened), what kind it is not,
    and where in it the first bad character is */
export function fieldPasteMessage(spec: FieldSpec, pasted: string, offender: {ch: string; index: number}): string {
  return `Pasted "${shorten(pasted)}" is not ${kindNoun(spec)} ("${offender.ch}" at character ${offender.index})`;
}

/** why `text` is left marked and uncommitted when the box it is in loses focus half-typed
    (fieldIncomplete: "-", "1e-", a lone "%", ...): what finishing it needs, or null to fall
    back to the kind's ordinary message (fieldError) */
export function fieldIncompleteReason(spec: FieldSpec, text: string): string | null {
  const t = text.trim();
  if (!fieldIncomplete(spec, t)) return null;
  if (spec.kind === 'number' && spec.formula && t === '%') return '"%" needs a formula';
  if (/e[+-]?$/i.test(t)) return `"${t}" needs an exponent's digits`;
  if (t === '-' || t === '+') return `"${t}" needs ${kindNoun(spec)}`;
  if (t === '.' || t === '-.' || t === '+.') return `"${t}" needs digits`;
  return null;
}

/** the input mode the box asks the on-screen keyboard for */
export function fieldInputMode(spec: FieldSpec): 'numeric' | 'decimal' | 'text' {
  return entry(spec).inputMode(spec as never);
}

/** a message to show: the phrase with a capital, "A whole number" */
export function fieldMessage(phrase: string): string {
  return phrase.charAt(0).toUpperCase() + phrase.slice(1);
}

/** a protocol kind as a spec: `integer`, `number`, `formula` (a number or
    %formula), `expression`, `file`, `text`, `name:N` (a name from
    `hello.lists[N]`; typed when that list is empty: a model with no
    parameters). Core and page ship together: a kind or list the page does
    not know is a bug, thrown */
export function specOfKind(kind: string, lists: readonly (readonly string[])[] = []): FieldSpec {
  switch (kind) {
    case 'text': return {kind: 'text'};
    case 'integer': return {kind: 'integer'};
    case 'number': return {kind: 'number'};
    case 'formula': return {kind: 'number', formula: true};
    case 'expression': return {kind: 'expression'};
    case 'file': return {kind: 'file'};
  }
  const m = /^name:(\d+)$/.exec(kind);
  const names = m ? lists[Number(m[1])] : undefined;
  if (!m || !names) throw new Error(`a field kind this page does not know: ${kind}`);
  return names.length ? {kind: 'name', names, what: NAME_LIST_WHAT[Number(m[1])]} : {kind: 'text'};
}

/** what a name from each of hello.lists is (docs/protocol.md "Asks", `form`) */
const NAME_LIST_WHAT: Record<number, string> = {
  0: 'T or a variable of the model',
  1: 'a variable of the model',
  2: 'a parameter of the model',
  3: 'a variable or parameter of the model',
};
