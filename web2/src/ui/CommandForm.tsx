import {choiceLabel} from '../store/fieldKinds';
import {useRef, useState} from 'preact/hooks';
import type {RegistryArgument} from '../protocol/kinds';
import {useSession, useStore} from './context';
import {registryRows} from './navigation';
import {argumentNames, argumentProblem, argumentSpec, argumentValue, currentArguments, commandValues, groupProblems, argumentControl, blankArgument} from './argumentFields';
import {Field} from './Field';
import {ColorField} from './ColorField';
import {useDark} from './theme';
import {cycleDialogFocus, useDialogFocus} from './dialogFocus';

export function CommandForm() {
  const form = useStore(s => s.commandForm);
  return form ? <Form key={form.id} id={form.id} initial={form.values} /> : null;
}
function Form({id, initial}: {id: string; initial: Record<string, unknown>}) {
  const session = useSession(), state = useStore(s => s);
  const row = registryRows(state.hello).find(row => row.id === id)!;
  const [values, setValues] = useState<Record<string, unknown>>({...Object.fromEntries((row.arguments ?? []).filter(a => a.required && ['boolean','map','list','list[text]'].includes(a.type)).map(a => [a.name, blankArgument(a)])), ...currentArguments(row, state), ...initial});
  const dark = useDark(state.theme), box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, []);
  const close = () => session.store.dispatch({type: 'commandForm', form: null});
  const set = (name: string, value: unknown) => setValues(old => ({...old, [name]: value}));
  const args = row.arguments ?? [];
  const problems = [...args.map(arg => argumentProblem(arg, values[arg.name])), ...groupProblems(row, values)];
  const control = (arg: RegistryArgument, value: unknown, change: (value: unknown) => void) => {
    const text = value === undefined ? '' : String(value);
    const kind = argumentControl(arg);
    if (kind === 'checkbox') return <input type="checkbox" checked={value === true} onChange={e => change(e.currentTarget.checked)} />;
    if (kind === 'color') return <ColorField value={text} dark={dark} label={choiceLabel(arg.name)} onChange={change} />;
    if (kind === 'select') {
      const names = arg.type === 'choice' ? arg.choices : argumentNames(arg, state);
      return <select value={text} onChange={e => change(e.currentTarget.value)}><option value="">Choose…</option>
        {names.map(name => <option key={name} value={name}>{arg.type === 'choice' ? choiceLabel(name) : name}</option>)}</select>;
    }
    if (kind === 'rows') {
      const map = arg.type === 'map', pair = arg.type === 'list[real, 2]';
      const entries: [string, unknown][] = map ? Object.entries((value ?? {}) as Record<string, unknown>)
        : ((value ?? (pair ? ['', ''] : [])) as unknown[]).map((v, i) => [String(i), v]);
      const update = (next: [string, unknown][]) => change(map ? Object.fromEntries(next) : next.map(e => e[1]));
      const element = {...arg, type: pair ? 'real' : arg.type === 'list[text]' ? 'text' : arg.element ?? 'text', required: true};
      return <div class="argument-rows">{entries.map(([key, entry], i) => <div class="argument-row" key={i}>
        {map && <input aria-label="Name" list={`names-${arg.name}`} value={key} onInput={e => update(entries.map((old, at) => at === i ? [e.currentTarget.value, old[1]] : old))} />}
        {control(element, entry, next => update(entries.map((old, at) => at === i ? [old[0], next] : old)))}
        {!pair && <button type="button" aria-label="Remove row" onClick={() => update(entries.filter((_, at) => at !== i))}>×</button>}
      </div>)}
      {map && <datalist id={`names-${arg.name}`}>{argumentNames(arg, state).map(name => <option value={name} />)}</datalist>}
      {!pair && <button type="button" disabled={entries.length >= (arg.max_items ?? Infinity)} onClick={() => update([...entries, ['', blankArgument(element)]])}>Add row</button>}</div>;
    }
    if (kind === 'file' || (kind === 'path' && arg.required)) return <span>{arg.type !== 'path' && <Field spec={argumentSpec(arg)} value={text} onInput={change} />}
      {arg.type !== 'path' && row.file_mode !== 'write' && <input type="file" aria-label={`Choose ${choiceLabel(arg.name)}`} onChange={async e => {
        const file = e.currentTarget.files?.[0]; if (!file) return;
        const name = await session.uploadArgument(file); if (name) change(name);
      }} />}<button type="button" disabled={[...args.filter(a => a !== arg).map(a => argumentProblem(a, values[a.name])), ...groupProblems(row, {...values, [arg.name]: 'selected_file'})].some(Boolean)} onClick={() => {
        const command = commandValues(args, values); delete command[arg.name];
        if (session.send({cmd: id, ...command, file_picker: true})) close();
      }}>Choose file and run…</button></span>;
    if (kind === 'path' && !arg.required) return <button type="button" onClick={() => {close(); session.send({cmd: id});}}>Choose file…</button>;
    return <Field spec={argumentSpec(arg)} value={text} onInput={text => change(argumentValue(arg, text))} />;
  };
  return <div class="dialog-backdrop"><div class="dialog registry-form" ref={box} role="dialog" aria-modal="true" aria-label={row.label} data-command-form={id}
    onKeyDown={e => {cycleDialogFocus(e, box.current);if (e.key === 'Escape') {e.stopPropagation(); close();}}}>
    <h2>{row.label}</h2><p>{row.description}</p>
    <form onSubmit={e => {e.preventDefault(); if (problems.some(Boolean)) return; if (session.send({cmd: id, ...commandValues(args, values)})) close();}}>
      {args.map((arg, i) => <div class="registry-argument" data-argument={arg.name} key={arg.name}>
        <label><span>{choiceLabel(arg.name)}{arg.required ? ' *' : ''}</span>
          {!arg.required && arg.type !== 'path' && <input type="checkbox" aria-label={`Set ${choiceLabel(arg.name)}`} checked={values[arg.name] !== undefined}
            onChange={e => set(arg.name, e.currentTarget.checked ? blankArgument(arg) : undefined)} />}
          {control(arg, values[arg.name], value => set(arg.name, value))}
        </label>{problems[i] && <span class="field-error">{problems[i]}</span>}
      </div>)}
      {groupProblems(row, values).map(problem => <p class="field-error">{problem}</p>)}
      <div class="dialog-actions"><button type="button" onClick={close}>Cancel</button><button type="submit" disabled={problems.some(Boolean)}>Run</button></div>
    </form>
  </div></div>;
}
