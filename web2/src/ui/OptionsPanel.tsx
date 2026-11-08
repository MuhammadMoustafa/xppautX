/* A group's settings panel (W616, docs/protocol.md "The settings panels"): built from `hello.options`, the option table's rows, whatever
   the group. Its title is the group's label, its rows are in the table's order, a dotted section is a titled fieldset, and a field's
   kind, label, choices, default, limits and help are the row's: nothing about one setting is written here. The values come from
   `options.get` and go out as one `options.configure` (or `output.configure`), all or nothing, of the rows changed; Apply closes the panel, as a command form's Run does. A field is the same
   one every command form has (Field, ColorField, a select), in the form's own markup; useOptionRows is the one place a row is turned into
   a field: a panel of its own, the settings part of a command's form and Values' Numerics all use it. */
import {useEffect, useRef, useState} from 'preact/hooks';
import type {OptionGroup, OptionRow} from '../protocol/types';
import {choiceLabel, fieldError, specOfOption} from '../store/fieldKinds';
import {ColorField} from './ColorField';
import {useSession, useStore} from './context';
import {cycleDialogFocus, useDialogFocus} from './dialogFocus';
import {Field} from './Field';
import {Modal} from './Modal';
import {useDark} from './theme';

/** the one registry command that holds a group's settings; the output group's has the file dialog's picker besides (core commands_options.cpp) */
const OUTPUT_GROUP = 'output';
const configureCommand = (group: string) => group === OUTPUT_GROUP ? 'output.configure' : 'options.configure';

/** the rows of a group in sections: the rows with no section first, then each section in the order the table has it */
export function sectionsOf(group: OptionGroup): {section: string; rows: OptionRow[]}[] {
  const sections: {section: string; rows: OptionRow[]}[] = [];
  for (const row of group.rows) {
    const last = sections[sections.length - 1];
    if (last && last.section === row.section) last.rows.push(row);
    else sections.push({section: row.section, rows: [row]});
  }
  return sections;
}

/** the rows of a group a panel (or a command form, or Values' Numerics) edits, with the drafts typed over what the core holds and the command that
    sends them. `live`: a change is sent at once (Values: a select and a checkbox on change, a box on Enter or leaving it), as the Values panel's
    fields always were; else the drafts wait for the panel's Apply. The rows the method or model do not use are `unused`, for the caller to separate. */
export function useOptionRows(group: OptionGroup, sections: string[] | null, live = false) {
  const session = useSession();
  const stored = useStore(s => s.optionValues[group.group]);
  const lists = useStore(s => s.hello?.lists ?? []);
  const dark = useDark(useStore(s => s.theme));
  const [drafts, setDrafts] = useState<Record<string, string>>({});
  const values = stored?.values ?? null;
  useEffect(() => setDrafts({}), [values]);
  const unavailable = stored?.unavailable ?? {};
  const names = [...new Set([...(lists[0] ?? []), ...(lists[3] ?? [])])];
  const rows = sections ? group.rows.filter(row => sections.includes(row.section)) : group.rows;
  const shown = (row: OptionRow) => drafts[row.key] ?? values?.[row.key] ?? '';
  const used = (row: OptionRow) => values !== null && row.key in values;
  const changed = rows.filter(row => row.key in drafts && drafts[row.key] !== values?.[row.key]);
  const problem = (row: OptionRow) => fieldError(specOfOption(row, names), shown(row));
  const invalid = changed.some(problem);
  const command = (keys: OptionRow[], texts: Record<string, string>) => ({cmd: configureCommand(group.group), ...(group.group === OUTPUT_GROUP ? {} : {group: group.group}),
    values: Object.fromEntries(keys.map(row => [row.key, texts[row.key]]))});
  /** the command that sets the rows changed, none when nothing was */
  const settings = (): {cmd: string; [k: string]: unknown} | null => changed.length ? command(changed, drafts) : null;
  const control = (row: OptionRow) => {
    const value = shown(row), disabled = values === null || !used(row);
    /* live: sent at once; else kept as a draft */
    const set = (text: string) => live ? session.send(command([row], {[row.key]: text})) : setDrafts(old => ({...old, [row.key]: text}));
    if (row.boolean) return <input type="checkbox" checked={value === 'true'} disabled={disabled} onChange={e => set(String(e.currentTarget.checked))} />;
    if (row.kind === 'color') return <ColorField value={value} dark={dark} label={row.label} onChange={set} />;
    if (row.kind === 'name' && names.length) {
      /* a row whose value is one of the model's names is a select of them (a value the model no longer has stays listed) */
      return <select value={value} disabled={disabled} aria-label={row.label} onChange={e => set(e.currentTarget.value)}>
        {(names.includes(value) || !value ? names : [value, ...names]).map(name => <option key={name} value={name}>{name}</option>)}</select>;
    }
    if (row.kind === 'choice') {
      const no = unavailable[row.key] ?? {};
      return <select value={value} disabled={disabled} onChange={e => set(e.currentTarget.value)}>
        {(row.choices ?? []).map(name => <option key={name} value={name} disabled={!!no[name]} title={no[name]}>{choiceLabel(name)}</option>)}</select>;
    }
    const field = live ? <Field spec={specOfOption(row, names)} value={value} disabled={disabled} aria-label={row.label} onCommit={set} />
      : <Field spec={specOfOption(row, names)} value={value} disabled={disabled} aria-label={row.label} onInput={set} />;
    return <span>{field}
      {row.kind === 'path' && group.group === OUTPUT_GROUP && <button type="button" onClick={() => session.send({cmd: 'output.configure', setting: row.key, file_picker: true})}>Choose file…</button>}</span>;
  };
  const rowBox = (row: OptionRow) => <div class="registry-argument" data-argument={row.key} data-num={row.key} key={row.key}>
    <label><span>{row.label}</span>{control(row)}</label>
    <small class="value-hint">{row.help} Default: {row.boolean || row.kind !== 'choice' ? row.default : choiceLabel(row.default)}.{used(row) || values === null ? '' : ' Not used by this method or model.'}</small>
    {Object.entries(unavailable[row.key] ?? {}).map(([name, why]) => <span key={name} class="value-hint" data-unavailable={name}>{choiceLabel(name)} is not available: {why}</span>)}
    {changed.includes(row) && problem(row) && <span class="field-error">{problem(row)}</span>}
  </div>;
  const fieldsOf = (shownRows: OptionRow[]) => sectionsOf({...group, rows: shownRows}).map(({section, rows: inSection}) => <fieldset class="options-section" key={section} data-section={section}>
    {section && <legend>{inSection[0].section_label}</legend>}
    {inSection.map(rowBox)}
  </fieldset>);
  const usedRows = rows.filter(used), unusedRows = values === null ? [] : rows.filter(row => !used(row));
  return {fields: fieldsOf(values === null ? rows : usedRows), unusedFields: fieldsOf(unusedRows), unused: unusedRows.length, invalid, settings, changed};
}

/** a place that edits a group's rows asks the core for their values when it opens, and again when `again` changes */
export function useOpenOptions(group: string | undefined, again: unknown = null): void {
  const session = useSession();
  useEffect(() => {
    if (group) session.fetchOptions(group);
  }, [group, again]);
}

export function OptionsPanel() {
  const panel = useStore(s => s.optionsPanel);
  const inForm = useStore(s => !!s.commandForm);
  const group = useStore(s => s.hello?.options.find(g => g.group === s.optionsPanel?.group));
  return panel && group && !inForm ? <Panel key={group.group} group={group} /> : null;
}

function Panel({group}: {group: OptionGroup}) {
  const session = useSession();
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
  const {fields, unusedFields, unused, invalid, settings, changed} = useOptionRows(group, null);
  const close = () => session.store.dispatch({type: 'optionsPanel', panel: null});
  const apply = () => {
    const command = settings();
    if (!invalid && command && session.send(command)) close();
  };
  return <Modal><div class="dialog registry-form options-panel" ref={box} role="dialog" aria-modal="true" aria-label={group.label} data-options-panel={group.group}
    onKeyDown={e => {cycleDialogFocus(e, box.current); if (e.key === 'Escape') {e.stopPropagation(); close();}}}>
    <h2>{group.label}</h2>
    <form onSubmit={e => {e.preventDefault(); apply();}}>
      {fields}
      {unused > 0 && <details class="unused-settings"><summary>Settings not used by this method or model</summary>{unusedFields}</details>}
      <div class="dialog-actions"><button type="button" onClick={close}>Cancel</button>
        <button type="submit" disabled={invalid || !changed.length}>Apply</button></div>
    </form>
  </div></Modal>;
}
