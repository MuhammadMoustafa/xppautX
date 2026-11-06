/* A section's sets row (W323, docs/studies/w309-values-design/sets-design.md sections 5 and 6): Sets, Save, Load from
   file and Restore defaults, each acting on that section's scope only. The page decides nothing: every button sends or
   opens the core's `set.*` command with the section's scope (and, for a graph's, its window) settled, so the window,
   `--server` and a recording do the same thing; File > Sets asks for the scope and the graph itself. */
import {useEffect, useRef} from 'preact/hooks';
import type {ComponentChildren} from 'preact';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';

/** what each scope's section holds, in the words of its buttons */
const WHAT: Record<string, string> = {
  parameters: 'parameters', initial: 'initial values', integration: 'numerics', continuation: 'autoX settings',
  plot: 'plot settings', sweep: 'range settings', analysis: 'histogram settings',
};

/** a drop-down (a details element) that closes when a click lands outside it */
function useDropDown() {
  const box = useRef<HTMLDetailsElement>(null);
  useEffect(() => {
    const outside = (e: PointerEvent) => {if (box.current && !box.current.contains(e.target as Node)) box.current.open = false;};
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
  return box;
}

/** a drop-down of buttons: it closes when one is picked or a click lands outside it */
function Menu({id, label, children}: {id: string; label: string; children: ComponentChildren}) {
  const box = useDropDown();
  return (
    <details class="sets-menu" ref={box} data-sets-menu={id}>
      <summary class="small">{label} ▾</summary>
      <ul role="menu" onClick={() => {if (box.current) box.current.open = false;}}>{children}</ul>
    </details>
  );
}

/** Load values from a file by scope (W321): the picked file is read whole by the core, which lists its parts that hold
    this section's kind; the dialog offers them (ValuesFileDialog.tsx) */
function LoadFromFile({scope, what}: {scope: string; what: string}) {
  const session = useSession();
  const input = useRef<HTMLInputElement>(null);
  const off = !useMay()({cmd: 'set.file_contents'});
  return (
    <>
      <button class="small" onClick={() => input.current?.click()} disabled={off} data-values-file={scope}
        title={off ? BUSY_TITLE : `Load ${what} from a file: an .odex's defaults or one of its sets, or an XPPAUT file`}>Load from file…</button>
      <input ref={input} id={`values-file-${scope}`} type="file" hidden accept=".odex,.par,.ic,.set"
        onChange={async e => {
          const el = e.target as HTMLInputElement, file = el.files?.[0];
          if (file) await session.offerValuesFile(scope, file);
          el.value = '';
        }} />
    </>
  );
}

export function SetsRow({scope, window}: {scope: string; window?: string}) {
  const session = useSession(), may = useMay();
  const sets = useStore(s => s.core?.sets ?? []).filter(set => set.scopes.includes(scope));
  const what = WHAT[scope];
  const target = window ? {window} : {};
  /* the form leaves out what the place settles: the scope, the graph, and the saved state unless these are the initial values */
  const fixed = ['scope', 'window', ...(scope === 'initial' ? [] : ['state'])];
  const ask = (cmd: string, values: Record<string, unknown> = {}) => session.registryAction(cmd, {scope, ...target, ...values}, fixed.concat(Object.keys(values)));
  const off = (cmd: string) => !may({cmd});
  const item = (cmd: string, label: string, run: () => void, title?: string) =>
    <li key={label}><button role="menuitem" data-command={cmd} disabled={off(cmd)} title={off(cmd) ? BUSY_TITLE : title} onClick={run}>{label}</button></li>;
  const own = sets.filter(set => !set.file);
  /* a graph's toolbar has no room for the row: it folds into one Sets drop-down, whose list of sets is Apply */
  const folded = scope === 'plot';
  const fold = useDropDown();
  const row = (
    <div class="sets-row" role="group" aria-label={`Sets of ${what}`} data-sets-row={scope}>
      <Menu id="sets" label={folded ? 'Apply' : 'Sets'}>
        {sets.length === 0 ? <li class="value-hint">No set holds {what}</li>
          : sets.map(set => item('set.apply', set.file ? `${set.name} (from ${set.file})` : set.name,
            () => session.send({cmd: 'set.apply', name: set.name, scope, ...target}), `Apply only the ${what} of the set ${set.name}`))}
      </Menu>
      <Menu id="save" label="Save">
        {item('set.save_new', 'New set…', () => ask('set.save_new'), `Write the current ${what} into the model file as a new set`)}
        {own.length === 0 ? <li class="value-hint">No set of the model file to replace in</li>
          : own.map(set => item('set.save_into', `Replace ${what} in set ${set.name}…`, () => ask('set.save_into', {name: set.name}),
            `Replace the ${what} of the set ${set.name} by the current values; its other parts stay`))}
        {item('set.save_defaults', 'As defaults…', () => ask('set.save_defaults'), `Write the current ${what} as the model's own`)}
        {item('set.export', 'Export…', () => ask('set.export'), `Write the current ${what} as a set of another .odex file`)}
      </Menu>
      <LoadFromFile scope={scope} what={what} />
      <button class="small" data-command="set.restore_defaults" disabled={off('set.restore_defaults')}
        title={off('set.restore_defaults') ? BUSY_TITLE : `Put the model's own ${what} back`}
        onClick={() => session.restoreDefaults(scope, window)}>Restore defaults</button>
    </div>
  );
  return folded ? <details class="sets-menu sets-fold" ref={fold}><summary class="small">Sets ▾</summary>{row}</details> : row;
}

/** the dialogs that hold a settings group of their own (range, histogram), by command: their form carries the group's sets row */
export const SETS_OF_FORM: Record<string, string> = {'run.range': 'sweep', 'analysis.histogram': 'analysis', 'analysis.histogram_2d': 'analysis'};
