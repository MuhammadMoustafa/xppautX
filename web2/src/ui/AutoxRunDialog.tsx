/* autoX > Run as a dialog for a person (W855, docs/studies/ui-review-2026-11-23/w855_run_dialog.jpg): five steps in the order a person
   thinks in. 1 where to start (a labelled point of the diagram, or the current state), 2 what to compute (only the kinds that start
   allows: hello.autox_run for the current state, the grabbed point's kinds from autoxinfo's start), 3 the parameter, its direction (the
   sign of the first step) and the range it stops outside (limits.parameter_min/max), 4 branches (follow what the run finds, clear the
   diagram first), 5 the numerics, one collapsed group prefilled with the values in force. What it sends is the same autox.run a script
   sends (session.autoxRunSteps); a changed parameter goes first as autox.configure_axes, through the page's settings commands. */
import type {ComponentChildren} from 'preact';
import {useState} from 'preact/hooks';
import type {RegistryArgument} from '../protocol/kinds';
import type {OptionRow} from '../protocol/types';
import {autoxSettingsCommands, shownSettings, type AutoxSettingsPatch} from '../store/autoxOptions';
import {activeView, type AutoxRunStart} from '../store/diagram';
import {pointLabel, pointMark} from '../store/pointTypes';
import {choiceLabel} from '../store/fieldKinds';
import {useSession, useStore} from './context';
import {argumentNames} from './argumentFields';
import {FormRow, useFeedback} from './FormFields';
import {Dialog} from './Dialog';

/** what each kind of run computes, in a person's words (its name is the core's: autox_codes.h's autox_run_kind_names) */
const KIND_WORDS: Record<string, {label: string; help: string}> = {
  equilibrium: {label: 'Equilibria', help: 'The steady states through this point'},
  periodic: {label: 'Periodic orbits', help: 'The cycles born at this point'},
  two_parameter: {label: 'Two-parameter curve', help: 'How this point moves with a second parameter'},
  fixed_period: {label: 'Fixed-period orbits', help: 'The cycles of a chosen period through this point'},
  bvp: {label: 'Boundary value problem', help: 'Solutions of a two-point problem through this point'},
  homoclinic: {label: 'Homoclinic orbits', help: 'The orbits that leave and return to an equilibrium: start from a long-period orbit'},
  heteroclinic: {label: 'Heteroclinic orbits', help: 'The orbits that join two equilibria'},
};

/** the numerics that another step of the dialog shows: the first step (step.initial) takes the direction's sign, the limits are the
    parameter's range, and follow_all_branches is the branches step's switch */
const NUMERIC_ELSEWHERE = ['limits.parameter_min', 'limits.parameter_max', 'follow_all_branches'];
const PARAMETER_MIN = 'limits.parameter_min', PARAMETER_MAX = 'limits.parameter_max', FOLLOW = 'follow_all_branches', FIRST_STEP = 'step.initial';
/** the kinds that take the homoclinic fields (dimensions and the two equilibria), shown for that kind only */
const HOMOCLINIC = 'homoclinic';
const DIRECTIONS = ['increase', 'decrease', 'both'] as const;
type Direction = (typeof DIRECTIONS)[number];

/** a number a field's text holds, or null when it is not one */
function numberOf(text: string): number | null {
  const v = Number(text);
  return text.trim() !== '' && Number.isFinite(v) ? v : null;
}

/** a list of reals typed as "a, b": null when a part is not a number */
function realsOf(text: string): number[] | null {
  if (text.trim() === '') return [];
  const parts = text.split(',').map(numberOf);
  return parts.every(v => v !== null) ? parts as number[] : null;
}

export function AutoxRunDialog({onClose}: {onClose(): void}) {
  const session = useSession(), state = useStore(s => s);
  const table = state.hello?.autox_run;
  const settings = shownSettings(state.autoxSettings);
  const numerics = (settings?.numerics ?? {}) as Record<string, unknown>;
  const start: AutoxRunStart | null = state.diagram.start;
  const labels = activeView(state.diagram).labels;
  const parameters = argumentNames({names: 'parameter'} as RegistryArgument, state);
  const feedback = useFeedback();
  const wide = typeof matchMedia === 'function' && matchMedia('(min-width: 80rem)').matches;

  /* 1 where to start: a labelled point (the grabbed one first), or the current state */
  const [fromLabel, setFromLabel] = useState(!!start);
  const [label, setLabel] = useState<number>(start?.label ?? labels[0]?.lab ?? 0);
  /* 2 what to compute: the grabbed point's kinds once autoxinfo's start is for the label picked, else the current state's */
  const kinds = fromLabel ? (start && start.label === label ? start.kinds : []) : (table?.current ?? []);
  const [kind, setKind] = useState('');
  const kindNow = kinds.includes(kind) ? kind : (kinds[0] ?? '');
  const [dimensions, setDimensions] = useState({unstable: '', stable: ''});
  const [equilibria, setEquilibria] = useState({left: '', right: ''});
  /* 3 the parameter, its direction and the range it stops outside */
  const [parameter, setParameter] = useState(state.autoxSettings.core?.axes.par1 ?? parameters[0] ?? '');
  const [direction, setDirection] = useState<Direction>('increase');
  /* every Numerics value the person changed, by its key (the values in force are the core's) */
  const [edits, setEdits] = useState<Record<string, string>>({});
  const [clear, setClear] = useState(false);
  const [numericsOpen, setNumericsOpen] = useState(wide);

  const inForce = (key: string): string => (key in edits ? edits[key] : String(numerics[key] ?? ''));
  const edit = (key: string, text: string) => setEdits(old => ({...old, [key]: text}));
  const rows = (state.hello?.options.find(g => g.group === 'continuation')?.rows ?? []) as OptionRow[];
  const numericRows = rows.filter(row => row.key in numerics && !NUMERIC_ELSEWHERE.includes(row.key));
  const sections = [...new Set(numericRows.map(row => row.section_label))];
  const changed = Object.keys(edits).length;

  /* what is sent: the kind, the start, the clear answer, every value the person changed, the first step with the direction's sign */
  const problems: string[] = [];
  const runArguments = (sign: 1 | -1, first: boolean): Record<string, unknown> => {
    const args: Record<string, unknown> = {kind: kindNow};
    if (fromLabel) args.label = label;
    if (first && clear) args.replace_diagram = true;
    for (const key of Object.keys(edits)) {
      if (key === FIRST_STEP) continue;
      const row = rows.find(r => r.key === key);
      if (row?.boolean) args[key] = edits[key] === 'true';
      else if (edits[key] !== undefined) args[key] = numberOf(edits[key]) ?? edits[key];
    }
    const step = numberOf(inForce(FIRST_STEP));
    if (step !== null) args[FIRST_STEP] = sign * Math.abs(step);
    if (kindNow === HOMOCLINIC) {
      const unstable = numberOf(dimensions.unstable), stable = numberOf(dimensions.stable);
      const left = realsOf(equilibria.left), right = realsOf(equilibria.right);
      if (unstable !== null) args.unstable_dimension = unstable;
      if (stable !== null) args.stable_dimension = stable;
      if (left?.length) args.left_equilibrium = left;
      if (right?.length) args.right_equilibrium = right;
    }
    return args;
  };
  for (const key of Object.keys(edits)) {
    const row = rows.find(r => r.key === key);
    if (row && !row.boolean && edits[key] !== undefined && numberOf(edits[key]) === null) problems.push(`${row.label} must be a number`);
  }
  if (!kindNow) problems.push('Choose what to compute');
  if (numberOf(inForce(FIRST_STEP)) === 0) problems.push('The first step must not be 0');
  if (kindNow === HOMOCLINIC && (realsOf(equilibria.left) === null || realsOf(equilibria.right) === null)) problems.push('Enter the equilibria as numbers');
  const fixes = problems.length;

  const submit = (e?: Event) => {
    e?.preventDefault();
    if (fixes) {
      feedback.attempt();
      return;
    }
    const axes = parameter && parameter !== state.autoxSettings.core?.axes.par1
      ? autoxSettingsCommands({axes: {par1: parameter}} as AutoxSettingsPatch)[0]?.command : undefined;
    if (axes) session.send(axes);
    const signs: (1 | -1)[] = direction === 'both' ? [1, -1] : [direction === 'decrease' ? -1 : 1];
    session.autoxRunSteps(signs.map((sign, i) => ({
      label: `autox.run ${kindNow} ${sign > 0 ? 'increase' : 'decrease'}`,
      args: runArguments(sign, i === 0),
    })));
    onClose();
  };

  /* a labelled point's name in the list: the grabbed one with its type, the others by their symbol and label */
  const labelName = (lab: number, sym: string): string => {
    if (start && start.label === lab) return `${pointMark(start.type) || sym} ${lab} · ${pointLabel(start.type)}`;
    return `${sym || 'point'} ${lab}`;
  };
  const pick = (lab: number) => {
    setLabel(lab);
    if (start?.label !== lab) session.send({cmd: 'autox.grab', label: lab});
  };
  const seg = (name: string, value: string, change: (v: string) => void, options: readonly string[]) => <div class="seg" role="radiogroup" aria-label={name}>
    {options.map(option => <button type="button" role="radio" key={option} class={'seg-item' + (value === option ? ' on' : '')} aria-checked={value === option}
      data-choice={option} onClick={() => change(option)}>{choiceLabel(option)}</button>)}
  </div>;
  const card = (checked: boolean, pickIt: () => void, title: string, body: ComponentChildren, extra?: ComponentChildren, attrs?: Record<string, string>) =>
    <label class={'run-card' + (checked ? ' on' : '')} {...attrs}>
      <input type="radio" checked={checked} onChange={pickIt} />
      <span class="run-card-text"><strong>{title}</strong><span class="value-hint">{body}</span>{extra}</span>
    </label>;
  const numericField = (row: OptionRow): ComponentChildren => {
    const bad = (() => { const t = inForce(row.key); return !row.boolean && numberOf(t) === null && t !== '' ? `${row.label} must be a number` : null; })();
    if (row.boolean) return <FormRow key={row.key} name={row.key} label={row.label} help={row.help} inline>
      <input type="checkbox" aria-label={row.label} checked={inForce(row.key) === 'true'} onChange={e => edit(row.key, String(e.currentTarget.checked))} />
    </FormRow>;
    return <FormRow key={row.key} name={row.key} label={row.label} help={row.help} error={bad}>
      <input class="run-number" aria-label={row.label} value={inForce(row.key)} onInput={e => edit(row.key, e.currentTarget.value)} />
    </FormRow>;
  };
  const kindCards = kinds.map(name => card(kindNow === name, () => setKind(name), KIND_WORDS[name]?.label ?? choiceLabel(name),
    KIND_WORDS[name]?.help ?? '', undefined, {'data-choice': name}));
  const homoclinic = kindNow === HOMOCLINIC;
  const pointWord = start ? pointLabel(start.type) : '';

  return <Dialog title="Run continuation" name="Run continuation" description="Follow a branch of solutions while a parameter changes."
    onClose={onClose} onSubmit={submit} problems={fixes} action={{label: 'Run', attrs: {'data-autox-run': ''}}} className="registry-form run-form"
    attrs={{'data-command-form': 'autox.run'}}>
    <div class="run-columns" data-autox-run-columns>
      <div class="run-column">
        <fieldset class="form-group run-step" data-run-step="start">
          <legend>1 · Start from</legend>
          {card(fromLabel, () => setFromLabel(true), 'A labelled point in the diagram', 'The grabbed point is chosen for you. Click any label on the plot to choose another.',
            labels.length ? <select aria-label="Labelled point" data-argument="label" value={String(label)} disabled={!fromLabel}
              onChange={e => pick(Number(e.currentTarget.value))}>
              {labels.map(l => <option key={`${l.point}-${l.lab}`} value={String(l.lab)}>{labelName(l.lab, l.sym)}</option>)}
            </select> : null, {'data-argument-start': 'label'})}
          {card(!fromLabel, () => setFromLabel(false), 'The current state', "The Values' initial conditions and parameters, as a steady state")}
        </fieldset>
        <fieldset class="form-group run-step" data-run-step="compute">
          <legend>2 · Compute</legend>
          <div class="run-kinds" role="radiogroup" aria-label="Compute" data-argument="kind">{kindCards.length ? kindCards : <span class="value-hint">Looking up what this point starts</span>}</div>
          <p class="value-hint">{start && fromLabel ? `Only the kinds a ${pointWord} point can start are offered.` : 'Only the kinds the current state can start are offered.'} Homoclinic orbits start from a long-period orbit, and torus curves from a TR label.</p>
          {homoclinic && <div class="run-homoclinic" data-homoclinic>
            <FormRow name="unstable_dimension" label="Unstable dimension" help="How many unstable directions the equilibrium has">
              <input aria-label="Unstable dimension" value={dimensions.unstable} onInput={e => setDimensions({...dimensions, unstable: e.currentTarget.value})} />
            </FormRow>
            <FormRow name="stable_dimension" label="Stable dimension" help="How many stable directions the equilibrium has">
              <input aria-label="Stable dimension" value={dimensions.stable} onInput={e => setDimensions({...dimensions, stable: e.currentTarget.value})} />
            </FormRow>
            <FormRow name="left_equilibrium" label="Left equilibrium" help="The equilibrium the orbit leaves, as a list of values">
              <input aria-label="Left equilibrium" value={equilibria.left} onInput={e => setEquilibria({...equilibria, left: e.currentTarget.value})} />
            </FormRow>
            <FormRow name="right_equilibrium" label="Right equilibrium" help="The equilibrium the orbit returns to, as a list of values">
              <input aria-label="Right equilibrium" value={equilibria.right} onInput={e => setEquilibria({...equilibria, right: e.currentTarget.value})} />
            </FormRow>
          </div>}
        </fieldset>
      </div>
      <div class="run-column">
        <fieldset class="form-group run-step" data-run-step="parameter">
          <legend>3 · Parameter</legend>
          <FormRow name="parameter1" label="Parameter" required>
            <select aria-label="Parameter" value={parameter} onChange={e => setParameter(e.currentTarget.value)}>
              {parameters.map(name => <option key={name} value={name}>{name}</option>)}
            </select>
          </FormRow>
          <FormRow name="direction" label="Direction">{seg('Direction', direction, v => setDirection(v as Direction), DIRECTIONS)}</FormRow>
          <FormRow name={PARAMETER_MIN} label="Stop outside" help="The run stops when the parameter leaves this range">
            <span class="range-pair">
              <input aria-label="Lowest parameter" value={inForce(PARAMETER_MIN)} onInput={e => edit(PARAMETER_MIN, e.currentTarget.value)} />
              <span class="range-word">≤ {parameter} ≤</span>
              <input aria-label="Highest parameter" value={inForce(PARAMETER_MAX)} onInput={e => edit(PARAMETER_MAX, e.currentTarget.value)} />
            </span>
          </FormRow>
        </fieldset>
        <fieldset class="form-group run-step" data-run-step="branches">
          <legend>4 · Branches</legend>
          <FormRow name={FOLLOW} label="Also follow the branches this run finds" inline>
            <input type="checkbox" aria-label="Also follow the branches this run finds" checked={(inForce(FOLLOW) === 'true')}
              onChange={e => edit(FOLLOW, String(e.currentTarget.checked))} />
          </FormRow>
          <p class="value-hint">A Hopf point starts its periodic orbits, and a branch point starts the other branch. Each becomes a branch of its own.</p>
          <FormRow name="replace_diagram" label="Clear the diagram first" inline>
            <input type="checkbox" aria-label="Clear the diagram first" checked={clear} onChange={e => setClear(e.currentTarget.checked)} />
          </FormRow>
          <p class="value-hint">Start a new diagram instead of adding to this one</p>
        </fieldset>
        <details class="run-numerics" open={numericsOpen} onToggle={e => setNumericsOpen(e.currentTarget.open)} data-run-numerics>
          <summary>Numerics <span class="value-hint">{changed ? `${changed} changed` : 'values in force'}</span></summary>
          {sections.map(section => <fieldset class="form-group" key={section}><legend>{section}</legend>
            {numericRows.filter(row => row.section_label === section).map(numericField)}
          </fieldset>)}
        </details>
      </div>
    </div>
    {feedback.attempted && problems.map(problem => <p class="field-error" role="alert">{problem}</p>)}
  </Dialog>;
}
