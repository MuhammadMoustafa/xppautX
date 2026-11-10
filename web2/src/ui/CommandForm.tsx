/* A registry command's form (W641): the one dialog shell (Dialog) with a field for each argument the command's row declares, drawn by the one
   field renderer (FormFields), its control decided from the argument's kind (store/fieldKinds.ts controlOf). Every field shows its current
   value; only the fields the person changed (and a required argument, and what the opener fixed) are sent by a settings form; a command that does something (a run, a save) is sent its current values too, which are its inputs. A command whose row has a
   `panel` shows that option group's rows in it too, through the same fields (W616), and leaves out the arguments those rows set. An error
   shows after a field was left or the action was pressed, names the rule and the value, and the footer counts the fields left to fix; the
   action is never disabled, and pressing it with errors moves the focus to the first. */
import {useState} from 'preact/hooks';
import type {RegistryArgument, RegistryGroup} from '../protocol/kinds';
import {choiceLabel} from '../store/fieldKinds';
import {Control, FormRow, focusFirstError, useFeedback} from './FormFields';
import {useSession, useStore} from './context';
import {panelSpec, registryRows} from './navigation';
import {useOpenOptions, useOptionRows} from './OptionsPanel';
import {argumentFieldSpec, argumentMessage, argumentValue, currentArguments, formValues, groupProblems, argumentControl, blankArgument} from './argumentFields';
import {Dialog} from './Dialog';
import {useDark} from './theme';
import {SETS_OF_FORM, SetsRow} from './SetsRow';

export function CommandForm() {
  const form = useStore(s => s.commandForm);
  return form ? <Form key={form.id} id={form.id} initial={form.values} fixed={form.fixed ?? []} /> : null;
}

/** the heading over a group of arguments that go together, in words */
const GROUP_WORDS: Record<RegistryGroup['rule'], string> = {exactly_one: 'Give one of these', at_most_one: 'Give at most one of these', together: 'Give all of these or none'};

function Form({id, initial, fixed}: {id: string; initial: Record<string, unknown>; fixed: string[]}) {
  const session = useSession(), state = useStore(s => s);
  const row = registryRows(state.hello).find(row => row.id === id)!;
  const dark = useDark(state.theme);
  const current = currentArguments(row, state);
  const blanks = Object.fromEntries((row.arguments ?? []).filter(a => a.required && ['boolean', 'map', 'list', 'list[text]'].includes(a.type)).map(a => [a.name, blankArgument(a)]));
  const [values, setValues] = useState<Record<string, unknown>>({...blanks, ...current, ...initial});
  /* the arguments the person changed, in the order they last changed them: what is sent beside the required ones and what the opener gave */
  const [edited, setEdited] = useState<string[]>([]);
  const feedback = useFeedback();
  const close = () => session.store.dispatch({type: 'commandForm', form: null});
  const spec = panelSpec(row.panel);
  const group = useStore(s => s.hello?.options.find(g => g.group === spec?.group));
  /* a command with a panel keeps in its form what no row sets: its own arguments, and the group's rows (W616) */
  const args = (row.arguments ?? []).filter(a => !spec || !a.option);
  useOpenOptions(group?.group);
  const options = useOptionRows(group ?? {group: '', label: '', rows: []}, spec?.sections ?? null, false, feedback);
  const set = (name: string, value: unknown) => {
    setValues(old => ({...old, [name]: value}));
    setEdited(old => [...old.filter(n => n !== name), name]);
  };
  const specOf = (arg: RegistryArgument) => argumentFieldSpec(arg, state);
  /* what is sent: the required arguments, what the opener gave, and what was changed (in a group of "at most one", only the one changed last) */
  const sentValues = () => {
    const out: Record<string, unknown> = {};
    for (const arg of args) {
      const v = values[arg.name];
      /* an empty box is not set, unless a text the person emptied (clearing a label is a value) */
      if (v === undefined || (v === '' && !(edited.includes(arg.name) && ['text', 'expression'].includes(arg.type)))) continue;
      if (arg.required || arg.name in initial || fixed.includes(arg.name) || edited.includes(arg.name) || row.kind !== 's') out[arg.name] = v;
    }
    for (const g of row.groups ?? []) {
      if (g.rule === 'together') continue;
      const last = [...edited].reverse().find(n => g.arguments.includes(n) && n in out);
      if (last) for (const n of g.arguments) if (n !== last) delete out[n];
    }
    return out;
  };
  /* the argument the core's file dialog fills (command_registry picker_argument): left empty, the action opens the dialog */
  const picker = args.find(a => a.type === 'file' || (a.type === 'path' && a.required));
  const pickerHelp = row.file_mode === 'write' ? 'Leave empty to choose where to save it' : 'Leave empty to choose a file, or drop one here';
  const sent = sentValues();
  const messages = args.map(arg => fixed.includes(arg.name) || arg === picker ? (arg === picker && sent[arg.name] ? argumentMessage(arg, sent[arg.name], specOf(arg)) : null) : argumentMessage(arg, arg.required ? values[arg.name] : sent[arg.name], specOf(arg)));
  const groupMessages = groupProblems(row, sent);
  const shown = (arg: RegistryArgument, i: number) => feedback.shows(arg.name) ? messages[i] : null;
  const fixes = args.filter((arg, i) => feedback.shows(arg.name) && messages[i]).length + (feedback.attempted ? groupMessages.length : 0) + options.shownProblems;
  const send = (command: {cmd: string; [k: string]: unknown}) => {
    const settings = options.settings();
    if (settings) session.send(settings);
    const given = Object.keys(command).filter(k => k !== 'cmd' && k !== 'window' && k !== 'file_picker');
    return row.kind === 's' || row.kind === 'v' ? (given.length || !settings ? session.send(command) : true) : session.send(command);
  };
  const submit = (e?: Event) => {
    e?.preventDefault();
    if (messages.some(Boolean) || groupMessages.length || options.invalid) {
      feedback.attempt();
      queueMicrotask(() => focusFirstError(document.querySelector('[data-command-form]')));
      return;
    }
    const picking = !!picker && !sent[picker.name];
    if (send({cmd: id, ...formValues(args, sent), ...(picking ? {file_picker: true} : {})})) close();
  };
  const control = (arg: RegistryArgument, value: unknown, change: (value: unknown) => void, label: string, invalid?: boolean) => {
    const text = value === undefined ? '' : String(value);
    const kind = argumentControl(arg);
    if (kind === 'rows') {
      const map = arg.type === 'map', pair = arg.type === 'list[real, 2]';
      const entries: [string, unknown][] = map ? Object.entries((value ?? {}) as Record<string, unknown>)
        : ((value ?? (pair ? ['', ''] : [])) as unknown[]).map((v, i) => [String(i), v]);
      const update = (next: [string, unknown][]) => change(map ? Object.fromEntries(next) : next.map(e => e[1]));
      const element = {...arg, type: pair ? 'real' : arg.type === 'list[text]' ? 'text' : arg.element ?? 'text', required: true};
      return <div class="argument-rows">{entries.map(([key, entry], i) => <div class="argument-row" key={i}>
        {map && <input aria-label="Name" list={`names-${arg.name}`} value={key} onInput={e => update(entries.map((old, at) => at === i ? [e.currentTarget.value, old[1]] : old))} />}
        {control(element, entry, next => update(entries.map((old, at) => at === i ? [old[0], next] : old)), label)}
        {!pair && <button type="button" aria-label="Remove row" onClick={() => update(entries.filter((_, at) => at !== i))}>×</button>}
      </div>)}
      {map && <datalist id={`names-${arg.name}`}>{state.hello?.model_names.map(name => <option value={name} />)}</datalist>}
      {!pair && <button type="button" disabled={entries.length >= (arg.max_items ?? Infinity)} onClick={() => update([...entries, ['', blankArgument(element)]])}>Add row</button>}</div>;
    }
    if (kind === 'path') return null;
    const fieldSpec = specOf(arg);
    const isFile = kind === 'file';
    /* a file argument: the one file control; a read-mode file takes a dropped one (uploaded into the folder) */
    const dropFile = isFile && row.file_mode !== 'write' ? async (f: File) => { const name = await session.uploadArgument(f); if (name) change(name); } : undefined;
    return <Control spec={fieldSpec} value={fieldSpec.kind === 'boolean' ? String(value === true) : text} label={label} dark={dark} invalid={invalid}
      file={isFile} placeholder={isFile ? pickerHelp : undefined} onDropFile={dropFile}
      onBlur={() => feedback.touch(arg.name)} onChange={t => change(fieldSpec.kind === 'boolean' ? t === 'true' : argumentValue(arg, t))} />;
  };
  /* the arguments in the order the row has them, those of one group together under a small heading */
  const rendered = new Set<string>();
  const field = (arg: RegistryArgument) => {
    const i = args.indexOf(arg);
    const label = choiceLabel(arg.name);
    return <FormRow key={arg.name} name={arg.name} label={label} required={arg.required && arg.type !== 'boolean' && arg !== picker} help={arg.description} error={shown(arg, i)}
      wide={['list', 'map', 'list[real, 2]', 'list[text]'].includes(arg.type)}>
      {control(arg, values[arg.name], value => set(arg.name, value), label, !!shown(arg, i))}
    </FormRow>;
  };
  const body = args.filter(a => !fixed.includes(a.name) && a.type !== 'path').map(arg => {
    if (rendered.has(arg.name)) return null;
    const g = (row.groups ?? []).find(g => g.arguments.includes(arg.name));
    if (!g) return field(arg);
    const members = args.filter(a => g.arguments.includes(a.name) && !fixed.includes(a.name) && !rendered.has(a.name) && a.type !== 'path');
    members.forEach(a => rendered.add(a.name));
    if (!members.length) return null;
    return <fieldset class="form-group" key={`group-${arg.name}`}><legend>{GROUP_WORDS[g.rule]}</legend>{members.map(field)}</fieldset>;
  });
  return <Dialog title={row.name} name={row.name} description={row.description} onClose={close} onSubmit={submit} problems={fixes}
    action={{label: row.action || row.label}} className="registry-form" attrs={{'data-command-form': id}}>
    {SETS_OF_FORM[id] && <SetsRow scope={SETS_OF_FORM[id]} />}
    {group && <div data-options-rows>{options.fields}{options.unused > 0 && <details class="unused-settings"><summary>Settings not used by this method or model</summary>{options.unusedFields}</details>}</div>}
    {group && body.some(Boolean) ? <fieldset class="form-group">{body}</fieldset> : body}
    {picker?.type === 'path' && <p class="form-help">{row.file_mode === 'write' ? 'Where to save it is chosen in the file dialog when you press the action.' : 'The file is chosen in the file dialog when you press the action.'}</p>}
    {feedback.attempted && groupMessages.map(problem => <p class="field-error">{problem}</p>)}
  </Dialog>;
}
