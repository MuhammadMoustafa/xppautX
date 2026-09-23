/* Form fields that pick from a list (docs/protocol.md "Asks", `form`): a
   field named `*nLabel` picks from `hello.lists[n]` (0 T and the variables,
   1 the ODE variables, 2 parameters, 3 both, 4 colours, 5 markers,
   6 methods). Items that start with a number (`2 Box`) answer with the
   number, as the X11 scroll list and the classic page do; the others (names)
   with themselves. Pure. */

export interface FieldSpec {
  /** the name shown, without the `*n` */
  label: string;
  /** the list it picks from, or null for a plain text field */
  list: number | null;
}

export function fieldSpec(name: string): FieldSpec {
  const m = /^\*(\d)(.*)$/s.exec(name);
  return m ? {label: m[2], list: Number(m[1])} : {label: name, list: null};
}

export interface ListOption {
  /** what the answer carries */
  value: string;
  /** what the select shows */
  label: string;
}

/** `2 Box` answers 2; a name answers itself */
export function listOption(item: string): ListOption {
  const m = /^(-?\d+)\s+\S/.exec(item);
  return {value: m ? m[1] : item, label: item};
}

const numeric = (s: string) => s.trim() !== '' && Number.isFinite(Number(s));

/** the select's options for a field whose value is `current`, and the one to select:
    the list item that is the value (by value, then without regard to case, then as a
    number); a value the list does not have (or none) is kept as a first option of its
    own, so opening the form and pressing OK never changes a field */
export function selectOptions(items: string[], current: string): {options: ListOption[]; selected: string} {
  const options = items.map(listOption);
  const cur = current.trim();
  const found = options.find(o => o.value === cur)
    ?? options.find(o => o.value.toLowerCase() === cur.toLowerCase())
    ?? (numeric(cur) ? options.find(o => numeric(o.value) && Number(o.value) === Number(cur)) : undefined);
  if (found) return {options, selected: found.value};
  return {options: [{value: current, label: cur === '' ? '(none)' : current}, ...options], selected: current};
}
