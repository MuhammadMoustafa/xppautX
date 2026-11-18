/* A group's options panel (W616, docs/protocol.md "The options panels"): built from `hello.options`, the option table's rows, whatever
   the group. Its title is the group's label, its rows are in the table's order, a dotted section is a titled fieldset, and a field's
   kind, label, choices, default, limits and help are the row's: nothing about one option is written here. The values come from
   `options.get` and go out as one `options.configure` (or `output.configure`), all or nothing, of the rows changed; Apply closes the panel, as a command form's Run does. A field is the same
   one every command form has (Field, ColorField, a select), in the form's own markup; useOptionRows is the one place a row is turned into
   a field: a panel of its own, the options part of a command's form and Values' Numerics all use it. Its rows are in sections (a dotted
   name's first part): each section folds on its own, starting folded when all its rows are advanced or none of them applies to the method
   (store/optionSections.ts, W811), and a low and a high row of one range are one field (a range pair). */
import {useEffect, useLayoutEffect, useId, useState} from 'preact/hooks';
import type {ComponentChildren} from 'preact';
import type {OptionGroup, OptionRow} from '../protocol/types';
import {choiceLabel, ruleMessage, specOfOption} from '../store/fieldKinds';
import {panelItems, sectionFoldKey, sectionStartsFolded, type PanelItem, type RangePair} from '../store/optionSections';
import {Control, FormRow, focusFirstError, useFeedback, type Feedback} from './FormFields';
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';
import {featureLack} from './navigation';

/** the one registry command that holds a group's options; the output group's has the file dialog's picker besides (core commands_options.cpp) */
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
    A row the method or model does not use is drawn disabled, in its section (folded when none of the section's rows is used). */
export function useOptionRows(group: OptionGroup, sections: string[] | null, live = false, own?: Feedback) {
  const session = useSession();
  const stored = useStore(s => s.optionValues[group.group]);
  const lists = useStore(s => s.hello?.lists ?? []);
  const made = useFeedback();
  const feedback = own ?? made;
  const [drafts, setDrafts] = useState<Record<string, string>>({});
  const values = stored?.values ?? null;
  /* the core's values that differ from those shown replace the edits; an answer that changes nothing (a second options.get, as
     reopening the panel just after Apply sends) keeps them (macOS CI 42959548937: a click between the two answers was lost); reset before the
     browser takes input, never in an effect run after paint, where a click on the boxes just drawn is wiped (macOS CI 43150345945) */
  useLayoutEffect(() => setDrafts({}), [JSON.stringify(values)]);
  const unavailable = stored?.unavailable ?? {};
  const names = [...new Set([...(lists[0] ?? []), ...(lists[3] ?? [])])];
  const sets = useStore(s => s.core?.sets ?? []).map(set => set.name);
  const hello = useStore(s => s.hello);
  /* a row for a feature the model lacks is not shown (hello.model_features) */
  const rows = (sections ? group.rows.filter(row => sections.includes(row.section)) : group.rows).filter(row => !featureLack(hello, row.needs));
  const shown = (row: OptionRow) => drafts[row.key] ?? values?.[row.key] ?? '';
  const used = (row: OptionRow) => values !== null && row.key in values;
  const changed = rows.filter(row => row.key in drafts && drafts[row.key] !== values?.[row.key]);
  /* an empty box is wrong for every row but a text or a path, which may be empty (nothing: the default) */
  const message = (row: OptionRow) => shown(row).trim() === '' ? (row.kind === 'text' || row.kind === 'path' || row.kind === 'sets' ? null : 'Required') : ruleMessage(specOfOption(row, names, sets), shown(row));
  const invalid = changed.some(message);
  const shownProblems = changed.filter(row => feedback.shows(row.key) && message(row)).length;
  const command = (keys: OptionRow[], texts: Record<string, string>) => ({cmd: configureCommand(group.group), ...(group.group === OUTPUT_GROUP ? {} : {group: group.group}),
    values: Object.fromEntries(keys.map(row => [row.key, texts[row.key]]))});
  /** the command that sets the rows changed, none when nothing was */
  const options = (): {cmd: string; [k: string]: unknown} | null => changed.length ? command(changed, drafts) : null;
  /* one row's control and what goes with it (its help, notes and error), the same for a row and for each side of a range pair */
  const member = (row: OptionRow) => {
    const spec = specOfOption(row, names, sets), value = shown(row), disabled = values === null || !used(row);
    /* live: the text stays shown as sent until the core's values come (an option made during a run applies after it) */
    const set = (text: string) => {
      setDrafts(old => ({...old, [row.key]: text}));
      if (live) session.send(command([row], {[row.key]: text}));
    };
    const error = changed.includes(row) && feedback.shows(row.key) ? message(row) : null;
    /* the default as the control shows it (on/off for a switch, a choice's words), left out when it is empty */
    const shownDefault = row.boolean ? (row.default === 'true' ? 'on' : 'off') : row.kind === 'choice' ? choiceLabel(row.default) : row.default;
    const help = <>{row.help}{shownDefault ? ` Default: ${shownDefault}.` : ''}{used(row) || values === null ? '' : ' Not used by this method or model.'}</>;
    const notes = Object.entries(unavailable[row.key] ?? {}).map(([name, why]) => <span key={name} class="unavailable-note" data-unavailable={name}>{choiceLabel(name)} is not available: {why}</span>);
    const control = (prefix?: string) => <Control spec={spec} prefix={prefix} value={value} label={row.label} disabled={disabled} unavailable={unavailable[row.key]} verbatim={row.kind === 'name'} invalid={!!error}
      file={row.kind === 'path'} onDropFile={row.kind === 'path' ? async f => { const name = await session.uploadArgument(f); if (name) set(name); } : undefined}
      onChange={set} onBlur={() => feedback.touch(row.key)} />;
    return {help, notes, error, control};
  };
  const rowBox = (row: OptionRow) => {
    const {help, notes, error, control} = member(row);
    return <FormRow key={row.key} name={row.key} label={row.label} help={help} notes={notes.length ? notes : null} error={error} inline={row.boolean}>{control()}</FormRow>;
  };
  /* a range pair: one label, the low side's input first, both helps behind one "?", an error of either side under the pair */
  const pairBox = (pair: RangePair) => {
    const low = member(pair.low), high = member(pair.high);
    const notes = [...low.notes, ...high.notes];
    return <FormRow key={pair.low.key} name={pair.low.key} paired={pair.high.key} label={pair.label} help={<>{low.help} {high.help}</>}
      notes={notes.length ? notes : null} error={low.error ?? high.error}>
      <div class="range-pair">
        {low.control(pair.lowWord)}
        {high.control(pair.highWord)}
      </div>
    </FormRow>;
  };
  const itemBox = (item: PanelItem) => item.kind === 'pair' ? pairBox(item) : rowBox(item.row);
  /* the rows in sections: a section with a name is a folding box of its own; the rows with no section are one card of their own at the top */
  const fieldsOf = (shownRows: OptionRow[]) => sectionsOf({...group, rows: shownRows}).map(({section, rows: inSection}) => {
    const items = panelItems(inSection).map(itemBox);
    return section
      ? <OptionSection key={section} group={group.group} section={section} label={inSection[0].section_label} rows={inSection} applies={row => values === null || used(row)}>{items}</OptionSection>
      : <div class="form-group options-section" key="" data-plain>{items}</div>;
  });
  return {fields: fieldsOf(rows), invalid, options, changed, shownProblems};
}

/** a section of a group's rows: its title with the count of its options, folded by its chevron. It starts folded when every row is
    advanced or none applies (sectionStartsFolded); a fold the person made is kept in the store (optionFolds) while the page lives. */
function OptionSection({group, section, label, rows, applies, children}: {
  group: string; section: string; label: string; rows: OptionRow[]; applies: (row: OptionRow) => boolean; children: ComponentChildren;
}) {
  const session = useSession();
  const folds = useStore(s => s.optionFolds);
  const bodyId = useId();
  const key = sectionFoldKey(group, section);
  const folded = folds[key] === undefined ? sectionStartsFolded(rows, applies) : !folds[key];
  const toggle = () => session.store.dispatch({type: 'optionFolds', folds: {[key]: folded}});
  const count = `${rows.length} option${rows.length === 1 ? '' : 's'}${rows.every(row => row.advanced) ? ', advanced' : ''}`;
  return <div class={'form-group options-section' + (folded ? ' folded' : '')} data-section={section} data-folded={folded ? 'true' : 'false'}>
    <div class="options-section-head">
      <button type="button" class="options-section-fold" aria-expanded={!folded} aria-controls={bodyId} onClick={toggle}>
        <span class="value-fold-mark" aria-hidden="true">{folded ? '▸' : '▾'}</span>{label}<span class="options-section-count">{count}</span>
      </button>
    </div>
    <div class="options-section-body" id={bodyId} hidden={folded}>{children}</div>
  </div>;
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
  const {fields, invalid, options, shownProblems} = useOptionRows(group, null, false, feedback);
  const close = () => session.store.dispatch({type: 'optionsPanel', panel: null});
  const apply = (e: Event) => {
    e.preventDefault();
    if (invalid) {
      feedback.attempt();
      queueMicrotask(() => focusFirstError(document.querySelector('[data-options-panel]')));
      return;
    }
    const command = options();
    if (!command || session.send(command)) close();
  };
  return <Dialog title={group.label} name={group.label} onClose={close} onSubmit={apply} problems={shownProblems}
    action={{label: 'Apply'}} className="registry-form options-panel" attrs={{'data-options-panel': group.group}}>
    {fields}
  </Dialog>;
}
