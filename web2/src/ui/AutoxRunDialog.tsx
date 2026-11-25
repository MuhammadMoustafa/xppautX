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
import {NUM_FIRST_STEP, NUM_FOLLOW_ALL, NUM_MESH_INTERVALS, NUM_MESH_POINTS, NUM_OUTPUT_EVERY, NUM_PARAMETER_MAX, NUM_PARAMETER_MIN, NUM_STEP_MAX, NUM_STEP_MIN, NUM_STEPS} from '../store/steady';
import {sixSig} from '../store/values';
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
const NUMERIC_ELSEWHERE = [NUM_PARAMETER_MIN, NUM_PARAMETER_MAX, NUM_FOLLOW_ALL];
/** the kinds that take the homoclinic fields (dimensions and the two equilibria), shown for that kind only */
const HOMOCLINIC = 'homoclinic';
/** a two-parameter run's name from the point it starts at, as a person says it (the point's own name is hello's: pointLabel) */
const CURVE_NAMES: Record<string, string> = {hopf: 'Hopf curve', fold: 'Fold curve', cycle_fold: 'Fold-of-cycles curve',
  period_doubling: 'Period-doubling curve', neimark_sacker: 'Torus curve'};
const DIRECTIONS = ['increase', 'decrease', 'both'] as const;
type Direction = (typeof DIRECTIONS)[number];

/** the kind a point starts by its type, first and picked: a Hopf point its periodic orbits, a fold or branch point equilibria, a torus or
    period-doubling point its own curve (a point of another type has none, the core's order stands) */
const FIRST_KIND: Record<string, string> = {hopf: 'periodic', fold: 'equilibrium', branch_point: 'equilibrium', neimark_sacker: 'two_parameter',
  period_doubling: 'two_parameter'};
/** the numerics the group's summary line shows when nothing changed: the key values in force, in the order a person reads them */
const SUMMARY_KEYS = [NUM_FIRST_STEP, NUM_STEP_MIN, NUM_STEP_MAX, NUM_STEPS, NUM_MESH_INTERVALS, NUM_MESH_POINTS, NUM_OUTPUT_EVERY];

/** a number a field's text holds, or null when it is not one */
function numberOf(text: string): number | null {
  const v = Number(text);
  return text.trim() !== '' && Number.isFinite(v) ? v : null;
}

/** a dimension a field's text holds: a whole number, 0 or more; null when it is not one */
function dimensionOf(text: string): number | null {
  const v = numberOf(text);
  return v !== null && Number.isInteger(v) && v >= 0 ? v : null;
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
  const view = activeView(state.diagram);
  const labels = view.labels;
  const parameters = argumentNames({names: 'parameter'} as RegistryArgument, state);
  const feedback = useFeedback();
  const wide = typeof matchMedia === 'function' && matchMedia('(min-width: 80rem)').matches;

  /* 1 where to start: a labelled point (the grabbed one first), or the current state */
  const [fromLabel, setFromLabel] = useState(!!start);
  const [label, setLabel] = useState<number>(start?.label ?? labels[0]?.lab ?? 0);
  const chosen = labels.find(l => l.lab === label);
  /* 2 what to compute: the grabbed point's kinds once autoxinfo's start is for the label picked, else the current state's */
  const startKinds = fromLabel ? (start && start.label === label ? start.kinds : []) : (table?.current ?? []);
  const first = fromLabel && start && start.label === label ? FIRST_KIND[start.type] : undefined;
  const kinds = first && startKinds.includes(first) ? [first, ...startKinds.filter(k => k !== first)] : startKinds;
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
  const summary = SUMMARY_KEYS.every(k => k in numerics)
    ? `step ${inForce(NUM_FIRST_STEP)} (${inForce(NUM_STEP_MIN)} to ${inForce(NUM_STEP_MAX)}) · at most ${inForce(NUM_STEPS)} points · mesh ${inForce(NUM_MESH_INTERVALS)} × ${inForce(NUM_MESH_POINTS)} · label every ${inForce(NUM_OUTPUT_EVERY)}` : '';

  /* what is sent: the kind, the start, the clear answer, every value the person changed, the first step with the direction's sign */
  const problems: string[] = [];
  const runArguments = (sign: 1 | -1, first: boolean): Record<string, unknown> => {
    const args: Record<string, unknown> = {kind: kindNow};
    if (fromLabel) args.label = label;
    args.replace_diagram = first && clear; // the core asks for it whenever the diagram holds points: the second way of Both adds to the first's
    for (const key of Object.keys(edits)) {
      if (key === NUM_FIRST_STEP) continue;
      const row = rows.find(r => r.key === key);
      if (row?.boolean) args[key] = edits[key] === 'true';
      else if (edits[key] !== undefined) args[key] = numberOf(edits[key]) ?? edits[key];
    }
    const step = numberOf(inForce(NUM_FIRST_STEP));
    if (step !== null) args[NUM_FIRST_STEP] = sign * Math.abs(step);
    if (kindNow === HOMOCLINIC) {
      const unstable = dimensionOf(dimensions.unstable), stable = dimensionOf(dimensions.stable);
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
  if (numberOf(inForce(NUM_FIRST_STEP)) === 0) problems.push('The first step must not be 0');
  if (kindNow === HOMOCLINIC) for (const [name, text] of [['Unstable', dimensions.unstable], ['Stable', dimensions.stable]]) {
    if (text.trim() !== '' && dimensionOf(text) === null) problems.push(`${name} dimension must be a whole number, 0 or more`);
  }
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

  /* a labelled point's name in the list, as the key names it: its mark, label and type, the parameter's value and its branch */
  /* the branch's direction at a label, shown under the list (a closed list cuts a long name): the sign of x at the next point of the same branch
     minus x here (the previous point's, when the label ends its branch); nothing when the label has neither */
  const directionAt = (point: number): string => {
    const {x, br} = view.points, next = point + 1, prev = point - 1;
    const other = next < br.length && br[next] === br[point] ? next : prev >= 0 && br[prev] === br[point] ? prev : -1;
    if (other < 0) return '';
    const dx = other === next ? x[next] - x[point] : x[point] - x[prev];
    return dx === 0 || !Number.isFinite(dx) ? '' : `Along branch ${br[point]}, ${view.axes?.xlabel ?? parameter} ${dx > 0 ? 'rises' : 'falls'} from this point.`;
  };
  const labelName = (l: {lab: number; sym: string; point: number}): string => {
    const x = view.points.x[l.point], where = Number.isFinite(x) ? ` · ${view.axes?.xlabel ?? parameter} = ${sixSig(x)}` : '';
    return `${pointMark(l.sym) || l.sym || 'point'} ${l.lab} · ${pointLabel(l.sym) || 'Point'}${where} · branch ${view.points.br[l.point]}`;
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
  /* a box before its words, the words the whole row; the hint under it */
  const checkRow = (name: string, text: string, checked: boolean, change: (on: boolean) => void, hint?: string, title?: string) =>
    <div class="run-check" data-argument={name}>
      <label title={title}><input type="checkbox" aria-label={text} checked={checked} onChange={e => change(e.currentTarget.checked)} /><span>{text}</span></label>
      {hint ? <p class="value-hint">{hint}</p> : null}
    </div>;
  const numericField = (row: OptionRow): ComponentChildren => {
    if (row.boolean) return <div key={row.key}>{checkRow(row.key, row.label, inForce(row.key) === 'true', on => edit(row.key, String(on)), undefined, row.help)}</div>;
    const t = inForce(row.key), bad = numberOf(t) === null && t !== '' ? `${row.label} must be a number` : null;
    return <FormRow key={row.key} name={row.key} label={row.label} error={bad} id={`run-${row.key}`}>
      <input class="run-number" id={`run-${row.key}`} aria-label={row.label} title={row.help} value={t} onInput={e => edit(row.key, e.currentTarget.value)} />
    </FormRow>;
  };
  const kindCards = kinds.map(name => card(kindNow === name, () => setKind(name), name === 'two_parameter' && fromLabel && start ? CURVE_NAMES[start.type] ?? KIND_WORDS[name].label : KIND_WORDS[name]?.label ?? choiceLabel(name),
    KIND_WORDS[name]?.help ?? '', undefined, {'data-choice': name}));
  const homoclinic = kindNow === HOMOCLINIC;

  return <Dialog title="Run continuation" name="Run continuation" description="Follow a branch of solutions while a parameter changes."
    onClose={onClose} onSubmit={submit} problems={fixes} action={{label: 'Run', attrs: {'data-autox-run': ''}}} className="registry-form run-form"
    attrs={{'data-command-form': 'autox.run'}}>
    <div class="run-columns" data-autox-run-columns>
      <div class="run-column">
        <fieldset class="form-group run-step" data-run-step="start">
          <legend>1 · Start from</legend>
          {card(fromLabel, () => setFromLabel(true), 'A labelled point in the diagram', 'The grabbed point is chosen for you. Click any label on the plot to choose another.',
            labels.length ? <>
              <select aria-label="Labelled point" data-argument="label" value={String(label)} disabled={!fromLabel}
                onChange={e => pick(Number(e.currentTarget.value))}>
                {labels.map(l => <option key={`${l.point}-${l.lab}`} value={String(l.lab)}>{labelName(l)}</option>)}
              </select>
              {chosen && fromLabel && directionAt(chosen.point) ? <span class="value-hint" data-run-direction>{directionAt(chosen.point)}</span> : null}
            </> : null, {'data-argument-start': 'label'})}
          {card(!fromLabel, () => setFromLabel(false), 'The current state', "The Values' initial conditions and parameters, as a steady state")}
        </fieldset>
        <fieldset class="form-group run-step" data-run-step="compute">
          <legend>2 · Compute</legend>
          <div class="run-kinds" role="radiogroup" aria-label="Compute" data-argument="kind">{kindCards.length ? kindCards : <span class="value-hint">Looking up what this point starts</span>}</div>
          <p class="value-hint">{start && fromLabel ? `Only the kinds this point can start are offered.` : 'Only the kinds the current state can start are offered.'} Homoclinic orbits start from a long-period orbit, and torus curves from a TR label.</p>
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
          <FormRow name={NUM_PARAMETER_MIN} label="Stop outside">
            <span class="range-pair" title="The run stops when the parameter leaves this range">
              <input class="run-number" aria-label="Lowest parameter" value={inForce(NUM_PARAMETER_MIN)} onInput={e => edit(NUM_PARAMETER_MIN, e.currentTarget.value)} />
              <span class="range-word">≤ {parameter} ≤</span>
              <input class="run-number" aria-label="Highest parameter" value={inForce(NUM_PARAMETER_MAX)} onInput={e => edit(NUM_PARAMETER_MAX, e.currentTarget.value)} />
            </span>
          </FormRow>
        </fieldset>
        <fieldset class="form-group run-step" data-run-step="branches">
          <legend>4 · Branches</legend>
          {checkRow(NUM_FOLLOW_ALL, 'Also follow the branches this run finds', inForce(NUM_FOLLOW_ALL) === 'true', on => edit(NUM_FOLLOW_ALL, String(on)),
            'A Hopf point starts its periodic orbits, and a branch point starts the other branch. Each becomes a branch of its own.')}
          {checkRow('replace_diagram', 'Clear the diagram first', clear, setClear, 'Start a new diagram instead of adding to this one')}
        </fieldset>
        <details class="run-numerics" open={numericsOpen} onToggle={e => setNumericsOpen(e.currentTarget.open)} data-run-numerics>
          <summary>Numerics <span class="value-hint">{[summary, changed ? `${changed} changed` : summary ? '' : 'values in force'].filter(Boolean).join(' · ')}</span></summary>
          <div class="run-sections">{sections.map(section => <fieldset class="form-group" key={section}><legend>{section}</legend>
            {numericRows.filter(row => row.section_label === section).map(numericField)}
          </fieldset>)}</div>
        </details>
      </div>
    </div>
    {feedback.attempted && problems.map(problem => <p key={problem} class="field-error" role="alert">{problem}</p>)}
  </Dialog>;
}
