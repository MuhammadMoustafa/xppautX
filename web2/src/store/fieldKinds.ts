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

/** whether every text is what its box takes */
export function fieldsValid(specs: readonly FieldSpec[], texts: readonly string[]): boolean {
  return specs.every((s, i) => fieldError(s, texts[i] ?? '') === null);
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
    `hello.lists[N]`); anything else (or none) is text, so an older core's
    asks behave as they did */
export function specOfKind(kind: string | undefined, lists?: readonly (readonly string[])[] | null): FieldSpec {
  switch (kind) {
    case 'integer': return {kind: 'integer'};
    case 'number': return {kind: 'number'};
    case 'formula': return {kind: 'number', formula: true};
    case 'expression': return {kind: 'expression'};
    case 'file': return {kind: 'file'};
  }
  const m = /^name:(\d+)$/.exec(kind ?? '');
  const names = m ? lists?.[Number(m[1])] : undefined;
  if (names?.length) return {kind: 'name', names, what: NAME_LIST_WHAT[Number(m![1])]};
  return {kind: 'text'};
}

/** what a name from each of hello.lists is (docs/protocol.md "Asks", `form`) */
const NAME_LIST_WHAT: Record<number, string> = {
  0: 'T or a variable of the model',
  1: 'a variable of the model',
  2: 'a parameter of the model',
  3: 'a variable or parameter of the model',
};
