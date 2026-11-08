/* autoX's settings as data (docs/ui-v2.md T22, store/autoxSettings.ts): the
   event into the store, edits sent at once (settings, W106) and shown
   until their set's done, what the forms show, the core's rules (the event's) checked in the
   page with the core's messages. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  pendingFields, autoxSettingsCommands, shownSettings, type AutoxSettings,
  type NumKey, type NumRule,
} from '../src/store/autoxSettings';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const act = (s: AppState, action: object) => reduce(s, {type: 'autoxSettings', action: action as never});

/* the core's rules as its event sends them (core/autox_settings.cpp num_fields) */
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
  suppress_branch_points: bool('Suppress branch points'), follow_all_branches: bool('Follow all branches'),
};

const settings: AutoxSettings = {
  special_points: {bogdanov_takens:true,cusp:true,zero_hopf:true,generalized_hopf:true,resonance_1_1:true,resonance_1_2:true,resonance_1_3:true,resonance_1_4:true,fold_flip:true,fold_neimark_sacker:true,flip_neimark_sacker:true,double_neimark_sacker:true},
  numerics: {mesh_intervals: 15, max_steps: 200, output_every: 50, collocation_points: 4, step_size: 0.02, min_step_size: 0.001,
    max_step_size: 0.5, parameter_min: 0, parameter_max: 2, norm_min: 0, norm_max: 1000, parameter_tolerance: 1e-4,
    state_tolerance: 1e-4, special_point_tolerance: 1e-4, mesh_adapt_every: 3, max_branch_switches: 5,
    switch_one_direction: false, output_detail: 'normal', max_location_iterations: 8, max_newton_iterations: 7,
    full_newton_after: 3, step_adapt_every: 1, suppress_branch_points: false, follow_all_branches: true},
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
  const core = ev(READY, {ev: 'autox.settings', ...settings});
  const sent = act(core, {type: 'sent', patch: {special_points: {generalized_hopf: false}}, ahead: 0});
  assert.equal(shownSettings(sent.autoxSettings)!.special_points.generalized_hopf, false);
  assert.equal(shownSettings(sent.autoxSettings)!.special_points.bogdanov_takens, true);
  assert.ok(pendingFields(sent.autoxSettings).has('special_points.generalized_hopf'));
});

test('the event is the store; an edit sent during a run shows until its own done', () => {
  let s = ev(READY, {ev: 'autox.settings', ...settings});
  assert.deepEqual(s.autoxSettings.core, {ev: 'autox.settings', ...settings});
  /* sent during a computation: one done (the run's) ahead of each set's own */
  s = act(s, {type: 'sent', patch: {numerics: {max_steps: 20}}, ahead: 1});
  s = act(s, {type: 'sent', patch: {numerics: {output_every: 5}, axes: {plot: 'norm', fit: true}}, ahead: 2});
  s = act(s, {type: 'sent', patch: {numerics: {max_steps: 25}}, ahead: 3});
  const shown = shownSettings(s.autoxSettings)!;
  assert.deepEqual([shown.numerics.max_steps, shown.numerics.output_every, shown.numerics.mesh_intervals, shown.axes.plot], [25, 5, 15, 'norm']);
  assert.ok(!('fit' in shown.axes));
  assert.deepEqual([...pendingFields(s.autoxSettings)].sort(), ['axes.plot', 'numerics.max_steps', 'numerics.output_every']);
  assert.deepEqual(autoxSettingsCommands({numerics: {max_steps: 25}}).map(c => c.command), [{cmd: 'autox.configure_numerics', max_steps: 25}]);
  /* the run's done, then each set's own: the event has the core's values */
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.equal(shownSettings(s.autoxSettings)!.numerics.max_steps, 25, 'still shown after the run ends');
  s = ev(s, {ev: 'autox.settings', ...settings, numerics: {...settings.numerics, max_steps: 20}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'autox.settings', ...settings, numerics: {...settings.numerics, max_steps: 20, output_every: 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'autox.settings', ...settings, numerics: {...settings.numerics, max_steps: 25, output_every: 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.deepEqual(s.autoxSettings.inflight, []);
  assert.equal(pendingFields(s.autoxSettings).size, 0);
  assert.equal(shownSettings(s.autoxSettings)!.numerics.max_steps, 25);
});

test("a refused set: the core's error is the forms', until the next edit", () => {
  let s = ev(READY, {ev: 'autox.settings', ...settings});
  s = act(s, {type: 'sent', patch: {numerics: {collocation_points: 9}}, ahead: 0});
  s = ev(s, {ev: 'session.message', error: 'autoX settings: Collocation points must be a whole number from 2 to 7'});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.match(s.autoxSettings.error!, /Collocation points/);
  assert.equal(shownSettings(s.autoxSettings)!.numerics.collocation_points, 4);
  s = act(s, {type: 'sent', patch: {numerics: {collocation_points: 5}}, ahead: 1});
  assert.equal(s.autoxSettings.error, null);
  /* an error with no set of the forms' running is not theirs */
  s = ev(s, {ev: 'session.message', error: 'autoX settings: nope'});
  assert.equal(s.autoxSettings.error, null);
});

