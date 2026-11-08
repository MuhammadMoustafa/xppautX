/* A section's sets row (W323, docs/studies/w309-values-design/sets-design.md sections 5 and 6): Sets, Save, Load from
   file and Restore defaults, each acting on that section's scope only. The page decides nothing: every button sends or
   opens the core's `set.*` command with the section's scope (and, for a graph's, its window) settled, so the window,
   `--server` and a recording do the same thing; File > Sets asks for the scope and the graph itself. */
import {useEffect, useRef, useState} from 'preact/hooks';
import type {ComponentChildren} from 'preact';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {commandRow} from './navigation';

/** what each scope's section holds, in the words of its buttons */
const WHAT: Record<string, string> = {
  parameters: 'parameters', initial: 'initial values', integration: 'numerics', continuation: 'autoX settings',
  plot: 'plot settings', sweep: 'range settings', analysis: 'histogram settings',
};

/** calls `close` when a click lands outside `box` */
function useOutsideClose(box: {current: HTMLElement | null}, close: () => void) {
  useEffect(() => {
    const outside = (e: PointerEvent) => {if (box.current && !box.current.contains(e.target as Node)) close();};
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, []);
}

/** a drop-down (a details element) that closes when a click lands outside it */
export function useDropDown() {
  const box = useRef<HTMLDetailsElement>(null);
  useOutsideClose(box, () => {if (box.current) box.current.open = false;});
  return box;
}

/** the buttons a narrow Values panel has no room for (Load from file, Restore defaults): beside the others, or behind a ⋯
    pop-up when the panel is narrow (css/values.css, the `values` container) */
function SetsMore({children}: {children: ComponentChildren}) {
  const box = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  useOutsideClose(box, () => setOpen(false));
  return (
    <div class={'sets-more' + (open ? ' open' : '')} ref={box}>
      <button class="small sets-more-toggle" aria-expanded={open} aria-label="More: load from a file, restore defaults"
        title="Load from file…, restore defaults" onClick={() => setOpen(!open)}>⋯</button>
      <div class="sets-more-body" onClick={() => setOpen(false)}>{children}</div>
    </div>
  );
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
        title={off ? BUSY_TITLE : `Load ${what} from a file: an .odex's defaults or one of its sets, or an XPPAUT file`}><CommandLabel command="set.file_contents" /></button>
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
  const hello = useStore(s => s.hello);
  /* a set's own entries name the set; the rest are the command's label */
  const item = (cmd: string, named: string | null, run: () => void, title?: string) => {
    const label = named ?? commandRow(hello, cmd)?.label;
    return <li key={label}><button role="menuitem" data-command={cmd} disabled={off(cmd)} title={off(cmd) ? BUSY_TITLE : title} onClick={run}>{label}</button></li>;
  };
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
        {item('set.save_new', null, () => ask('set.save_new'), `Write the current ${what} into the model file as a new set`)}
        {own.length === 0 ? <li class="value-hint">No set of the model file to replace in</li>
          : own.map(set => item('set.save_into', `Replace ${what} in set ${set.name}…`, () => ask('set.save_into', {name: set.name}),
            `Replace the ${what} of the set ${set.name} by the current values; its other parts stay`))}
        {item('set.save_defaults', null, () => ask('set.save_defaults'), `Write the current ${what} as the model's own`)}
        {item('set.export', null, () => ask('set.export'), `Write the current ${what} as a set of another .odex file`)}
      </Menu>
      <SetsMore>
        <LoadFromFile scope={scope} what={what} />
        <button class="small" data-command="set.restore_defaults" disabled={off('set.restore_defaults')}
          title={off('set.restore_defaults') ? BUSY_TITLE : `Put the model's own ${what} back`}
          onClick={() => session.restoreDefaults(scope, window)}><CommandLabel command="set.restore_defaults" /></button>
      </SetsMore>
    </div>
  );
  return folded ? <details class="sets-menu sets-fold" ref={fold}><summary class="small">Sets ▾</summary>{row}</details> : row;
}

/** the dialogs that hold a settings group of their own (range, histogram), by command: their form carries the group's sets row */
export const SETS_OF_FORM: Record<string, string> = {'run.range': 'sweep', 'analysis.histogram': 'analysis', 'analysis.histogram_2d': 'analysis'};
