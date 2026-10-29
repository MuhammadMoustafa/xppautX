/* AUTO's settings as data (docs/ui-v2.md T22, store/autoSettings.ts): the
   event into the store, edits sent at once (settings, W106) and shown
   until their set's done, what the forms show, the core's rules (the event's) checked in the
   page with the core's messages. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  fieldName, NUM_FIELDS, numError, pairError, pairErrors, pendingFields, autoSettingsCommands, shownSettings, type AutoSettings,
  type NumKey, type NumRule,
} from '../src/store/autoSettings';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const act = (s: AppState, action: object) => reduce(s, {type: 'autoSettings', action: action as never});

/* the core's rules as its event sends them (core/auto_settings.cpp num_fields) */
const whole = (label: string, min?: number, max?: number): NumRule => ({label, integer: true, min, max,
  message: `${label} must be a whole number${min === undefined ? '' : max === undefined ? ` of at least ${min}` : ` from ${min} to ${max}`}`});
const positive = (label: string): NumRule => ({label, positive: true, message: `${label} must be a number above 0`});
const any = (label: string): NumRule => ({label, message: `${label} must be a number`});
const bool = (label: string): NumRule => ({label, boolean: true, message: `${label} must be true or false`});
const levels = ['none', 'little', 'normal', 'start_jacobian', 'extensive', 'debugging'];
const rules: Record<NumKey, NumRule> = {
  mesh_intervals: whole('Mesh intervals', 1), max_steps: whole('Max steps', 1), output_every: whole('Output every', 1),
  collocation_points: whole('Collocation points', 2, 7),
  step_size: {label: 'Step size', nonzero: true, message: 'Step size must be a number other than 0'},
  min_step_size: positive('Min step size'), max_step_size: positive('Max step size'), parameter_min: any('Parameter min'),
  parameter_max: any('Parameter max'), norm_min: any('Norm min'), norm_max: any('Norm max'),
  parameter_tolerance: positive('Parameter tolerance'), state_tolerance: positive('State tolerance'),
  special_point_tolerance: positive('Special point tolerance'), mesh_adapt_every: whole('Mesh adapt every', 0),
  max_branch_switches: whole('Max branch switches', 0), switch_one_direction: bool('Switch one direction'),
  output_detail: {label: 'Output detail', choices: levels, message: `Output detail must be one of ${levels.join(', ')}`},
  max_location_iterations: whole('Max location iterations', 1), max_newton_iterations: whole('Max newton iterations', 1),
  full_newton_after: whole('Full newton after', 1), step_adapt_every: whole('Step adapt every', 0),
  suppress_branch_points: bool('Suppress branch points'),
};

const settings: AutoSettings = {
  special_points: {bogdanov_takens:true,cusp:true,zero_hopf:true,generalized_hopf:true,resonance_1_1:true,resonance_1_2:true,resonance_1_3:true,resonance_1_4:true,fold_flip:true,fold_neimark_sacker:true,flip_neimark_sacker:true,double_neimark_sacker:true},
  numerics: {mesh_intervals: 15, max_steps: 200, output_every: 50, collocation_points: 4, step_size: 0.02, min_step_size: 0.001,
    max_step_size: 0.5, parameter_min: 0, parameter_max: 2, norm_min: 0, norm_max: 1000, parameter_tolerance: 1e-4,
    state_tolerance: 1e-4, special_point_tolerance: 1e-4, mesh_adapt_every: 3, max_branch_switches: 5,
    switch_one_direction: false, output_detail: 'normal', max_location_iterations: 8, max_newton_iterations: 7,
    full_newton_after: 3, step_adapt_every: 1, suppress_branch_points: false},
  axes: {plot: 'maximum_and_minimum', var: 'V', par1: 'iapp', par2: 'phi', xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4},
  marks: [],
  threads: 'auto',
  threads_auto: 4,
  threads_max: 16,
  rules,
  pairs: [{lo: 'min_step_size', hi: 'max_step_size', strict: false, message: 'Min step size must be at most Max step size'},
    {lo: 'parameter_min', hi: 'parameter_max', strict: true, message: 'Parameter min must be below Parameter max'},
    {lo: 'norm_min', hi: 'norm_max', strict: true, message: 'Norm min must be below Norm max'}],
  step: {key: 'step_size', lo: 'min_step_size', hi: 'max_step_size', message: 'The size of Step size must be from Min step size to Max step size (its sign is the direction)'},
};

test('codim-2 detection booleans stay named while queued', () => {
  const core = ev(READY, {ev: 'auto.settings', ...settings});
  const sent = act(core, {type: 'sent', patch: {special_points: {generalized_hopf: false}}, ahead: 0});
  assert.equal(shownSettings(sent.autoSettings)!.special_points.generalized_hopf, false);
  assert.equal(shownSettings(sent.autoSettings)!.special_points.bogdanov_takens, true);
  assert.ok(pendingFields(sent.autoSettings).has('special_points.generalized_hopf'));
});

test('the event is the store; an edit sent during a run shows until its own done', () => {
  let s = ev(READY, {ev: 'auto.settings', ...settings});
  assert.deepEqual(s.autoSettings.core, {ev: 'auto.settings', ...settings});
  /* sent during a computation: one done (the run's) ahead of each set's own */
  s = act(s, {type: 'sent', patch: {numerics: {max_steps: 20}}, ahead: 1});
  s = act(s, {type: 'sent', patch: {numerics: {output_every: 5}, axes: {plot: 'norm', fit: true}}, ahead: 2});
  s = act(s, {type: 'sent', patch: {numerics: {max_steps: 25}}, ahead: 3});
  const shown = shownSettings(s.autoSettings)!;
  assert.deepEqual([shown.numerics.max_steps, shown.numerics.output_every, shown.numerics.mesh_intervals, shown.axes.plot], [25, 5, 15, 'norm']);
  assert.ok(!('fit' in shown.axes));
  assert.deepEqual([...pendingFields(s.autoSettings)].sort(), ['axes.plot', 'numerics.max_steps', 'numerics.output_every']);
  assert.deepEqual(autoSettingsCommands({numerics: {max_steps: 25}}).map(c => c.command), [{cmd: 'auto.configure_numerics', max_steps: 25}]);
  /* the run's done, then each set's own: the event has the core's values */
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.equal(shownSettings(s.autoSettings)!.numerics.max_steps, 25, 'still shown after the run ends');
  s = ev(s, {ev: 'auto.settings', ...settings, numerics: {...settings.numerics, max_steps: 20}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'auto.settings', ...settings, numerics: {...settings.numerics, max_steps: 20, output_every: 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'auto.settings', ...settings, numerics: {...settings.numerics, max_steps: 25, output_every: 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.deepEqual(s.autoSettings.inflight, []);
  assert.equal(pendingFields(s.autoSettings).size, 0);
  assert.equal(shownSettings(s.autoSettings)!.numerics.max_steps, 25);
});

test("a refused set: the core's error is the forms', until the next edit", () => {
  let s = ev(READY, {ev: 'auto.settings', ...settings});
  s = act(s, {type: 'sent', patch: {numerics: {collocation_points: 9}}, ahead: 0});
  s = ev(s, {ev: 'session.message', error: 'AUTO settings: Collocation points must be a whole number from 2 to 7'});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.match(s.autoSettings.error!, /Collocation points/);
  assert.equal(shownSettings(s.autoSettings)!.numerics.collocation_points, 4);
  s = act(s, {type: 'sent', patch: {numerics: {collocation_points: 5}}, ahead: 1});
  assert.equal(s.autoSettings.error, null);
  /* an error with no set of the forms' running is not theirs */
  s = ev(s, {ev: 'session.message', error: 'AUTO settings: nope'});
  assert.equal(s.autoSettings.error, null);
});

test("the core's rules, in the page, with its messages", () => {
  assert.equal(NUM_FIELDS.length, 23);
  assert.deepEqual(NUM_FIELDS.map(f => f.key).sort(), Object.keys(rules).sort());
  assert.equal(numError(rules.collocation_points, '8'), 'Collocation points must be a whole number from 2 to 7');
  assert.equal(numError(rules.mesh_intervals, '2.5'), 'Mesh intervals must be a whole number of at least 1');
  assert.equal(numError(rules.max_steps, '1e2'), 'Max steps must be a whole number of at least 1', 'digits only (T31)');
  assert.equal(numError(rules.mesh_intervals, '0'), 'Mesh intervals must be a whole number of at least 1');
  assert.equal(numError(rules.step_size, '0'), 'Step size must be a number other than 0');
  assert.equal(numError(rules.min_step_size, '-1'), 'Min step size must be a number above 0');
  assert.equal(numError(rules.parameter_min, 'x'), 'Parameter min must be a number');
  assert.deepEqual([numError(rules.step_size, '-0.01'), numError(rules.max_branch_switches, '3'), numError(rules.suppress_branch_points, 'true'),
    numError(rules.output_detail, 'extensive')], [null, null, null, null]);
  assert.equal(numError(rules.max_branch_switches, '-3'), 'Max branch switches must be a whole number of at least 0');
  assert.equal(numError(rules.output_detail, '3'), `Output detail must be one of ${levels.join(', ')}`);
  assert.equal(pairError(settings, {...settings.numerics, parameter_min: 3}), 'Parameter min must be below Parameter max');
  assert.equal(pairError(settings, {...settings.numerics, min_step_size: 1}), 'Min step size must be at most Max step size');
  assert.equal(pairError(settings, settings.numerics), null);
  /* T23: min step size <= |step size| <= max step size, whatever the direction; each message by its field */
  const n = settings.numerics, step = settings.step.message;
  assert.deepEqual(pairErrors(settings, {...n, step_size: -n.max_step_size}), {});
  assert.deepEqual(pairErrors(settings, {...n, step_size: -2 * n.max_step_size}), {step_size: step});
  assert.deepEqual(pairErrors(settings, {...n, step_size: n.min_step_size / 2, norm_min: 5, norm_max: 1}), {step_size: step, norm_min: 'Norm min must be below Norm max'});
  /* every field is named by its key, labelled from it, with AUTO's own name for a hover, and explained */
  for (const f of NUM_FIELDS) {
    assert.ok(f.auto.length >= 2, f.key);
    assert.ok(f.help.length > 40, f.key);
    assert.equal(fieldName(f), rules[f.key].label, f.key);
  }
  assert.equal(NUM_FIELDS.find(f => f.key === 'max_steps')!.auto, 'NMX');
});
