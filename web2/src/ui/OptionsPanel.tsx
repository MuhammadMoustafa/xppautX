/* A group's settings panel (W616, docs/protocol.md "The settings panels"): built from `hello.options`, the option table's rows, whatever
   the group. Its title is the group's label, its rows are in the table's order, a dotted section is a titled fieldset, and a field's
   kind, label, choices, default, limits and help are the row's: nothing about one setting is written here. The values come from
   `options.get` and go out as one `options.configure` (or `output.configure`), all or nothing, of the rows changed; Apply closes the panel, as a command form's Run does. A field is the same
   one every command form has (Field, ColorField, a select), in the form's own markup; useOptionRows is the one place a row is turned into
   a field: a panel of its own, the settings part of a command's form and Values' Numerics all use it. */
import {useEffect, useState} from 'preact/hooks';
import type {OptionGroup, OptionRow} from '../protocol/types';
import {choiceLabel, ruleMessage, specOfOption} from '../store/fieldKinds';
import {Control, FormRow, focusFirstError, useFeedback, type Feedback} from './FormFields';
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';
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

/** the rows of a group a panel (or a command form, or Values' Numerics) edits, drawn by the one field renderer, with the drafts typed over what the core
    holds and the command that sends them. `live`: a change is sent at once (Values: as its fields always were); else the drafts wait for the action.
    The rows the method or model do not use are `unused`, for the caller to separate. */
export function useOptionRows(group: OptionGroup, sections: string[] | null, live = false, own?: Feedback) {
  const session = useSession();
  const stored = useStore(s => s.optionValues[group.group]);
  const lists = useStore(s => s.hello?.lists ?? []);
  const dark = useDark(useStore(s => s.theme));
  const made = useFeedback();
  const feedback = own ?? made;
  const [drafts, setDrafts] = useState<Record<string, string>>({});
  const values = stored?.values ?? null;
  useEffect(() => setDrafts({}), [values]);
  const unavailable = stored?.unavailable ?? {};
  const names = [...new Set([...(lists[0] ?? []), ...(lists[3] ?? [])])];
  const rows = sections ? group.rows.filter(row => sections.includes(row.section)) : group.rows;
  const shown = (row: OptionRow) => drafts[row.key] ?? values?.[row.key] ?? '';
  const used = (row: OptionRow) => values !== null && row.key in values;
  const changed = rows.filter(row => row.key in drafts && drafts[row.key] !== values?.[row.key]);
  /* an empty box is wrong for every row but a text or a path, which may be empty (nothing: the default) */
  const message = (row: OptionRow) => shown(row).trim() === '' ? (row.kind === 'text' || row.kind === 'path' ? null : 'Required') : ruleMessage(specOfOption(row, names), shown(row));
  const invalid = changed.some(message);
  const shownProblems = changed.filter(row => feedback.shows(row.key) && message(row)).length;
  const command = (keys: OptionRow[], texts: Record<string, string>) => ({cmd: configureCommand(group.group), ...(group.group === OUTPUT_GROUP ? {} : {group: group.group}),
    values: Object.fromEntries(keys.map(row => [row.key, texts[row.key]]))});
  /** the command that sets the rows changed, none when nothing was */
  const settings = (): {cmd: string; [k: string]: unknown} | null => changed.length ? command(changed, drafts) : null;
  const rowBox = (row: OptionRow) => {
    const spec = specOfOption(row, names), value = shown(row), disabled = values === null || !used(row);
    /* live: the text stays shown as sent until the core's values come (a setting made during a run applies after it) */
    const set = (text: string) => {
      setDrafts(old => ({...old, [row.key]: text}));
      if (live) session.send(command([row], {[row.key]: text}));
    };
    const error = changed.includes(row) && feedback.shows(row.key) ? message(row) : null;
    /* the default as the control shows it (on/off for a switch, a choice's words), left out when it is empty */
    const shownDefault = row.boolean ? (row.default === 'true' ? 'on' : 'off') : row.kind === 'choice' ? choiceLabel(row.default) : row.default;
    const help = <>{row.help}{shownDefault ? ` Default: ${shownDefault}.` : ''}{used(row) || values === null ? '' : ' Not used by this method or model.'}
      {Object.entries(unavailable[row.key] ?? {}).map(([name, why]) => <> <span key={name} class="value-hint unavailable-note" data-unavailable={name}>{choiceLabel(name)} is not available: {why}</span></>)}</>;
    return <FormRow key={row.key} name={row.key} label={row.label} help={help} error={error}>
      <Control spec={spec} value={value} label={row.label} dark={dark} disabled={disabled} unavailable={unavailable[row.key]} verbatim={row.kind === 'name'} invalid={!!error}
        file={row.kind === 'path'} onDropFile={row.kind === 'path' ? async f => { const name = await session.uploadArgument(f); if (name) set(name); } : undefined}
        onChange={set} onBlur={() => feedback.touch(row.key)} />
    </FormRow>;
  };
  const fieldsOf = (shownRows: OptionRow[]) => sectionsOf({...group, rows: shownRows}).map(({section, rows: inSection}) => <fieldset class="form-group options-section" key={section} data-section={section}>
    {section && <legend>{inSection[0].section_label}</legend>}
    {inSection.map(rowBox)}
  </fieldset>);
  const usedRows = rows.filter(used), unusedRows = values === null ? [] : rows.filter(row => !used(row));
  return {fields: fieldsOf(values === null ? rows : usedRows), unusedFields: fieldsOf(unusedRows), unused: unusedRows.length, invalid, settings, changed, shownProblems};
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
  const feedback = useFeedback();
  const {fields, unusedFields, unused, invalid, settings, shownProblems} = useOptionRows(group, null, false, feedback);
  const close = () => session.store.dispatch({type: 'optionsPanel', panel: null});
  const apply = (e: Event) => {
    e.preventDefault();
    if (invalid) {
      feedback.attempt();
      queueMicrotask(() => focusFirstError(document.querySelector('[data-options-panel]')));
      return;
    }
    const command = settings();
    if (!command || session.send(command)) close();
  };
  return <Dialog title={group.label} name={group.label} onClose={close} onSubmit={apply} problems={shownProblems}
    action={{label: 'Apply'}} className="registry-form options-panel" attrs={{'data-options-panel': group.group}}>
    {fields}
    {unused > 0 && <details class="unused-settings"><summary>Settings not used by this method or model</summary>{unusedFields}</details>}
  </Dialog>;
}
