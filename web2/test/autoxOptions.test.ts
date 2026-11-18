/* autoX's options as data (docs/ui-v2.md T22, store/autoxOptions.ts): the
   event into the store, edits sent at once (options, W106) and shown
   until their set's done, what the forms show, the core's rules (the event's) checked in the
   page with the core's messages. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  pendingFields, autoxSettingsCommands, shownSettings, type AutoxSettings,
  type NumKey, type NumRule,
} from '../src/store/autoxOptions';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const act = (s: AppState, action: object) => reduce(s, {type: 'autoxSettings', action: action as never});

/* the core's rules as its event sends them (core/autox_options.cpp num_fields) */
const whole = (label: string, min?: number, max?: number): NumRule => ({label, integer: true, min, max,
  message: `${label} must be a whole number${min === undefined ? '' : max === undefined ? ` of at least ${min}` : ` from ${min} to ${max}`}`});
const positive = (label: string): NumRule => ({label, positive: true, message: `${label} must be a number above 0`});
const any = (label: string): NumRule => ({label, message: `${label} must be a number`});
const bool = (label: string): NumRule => ({label, boolean: true, message: `${label} must be true or false`});
const levels = ['none', 'little', 'normal', 'start_jacobian', 'extensive', 'debugging'];
const rules: Record<NumKey, NumRule> = {
  'mesh.intervals': whole('Mesh intervals', 1), 'limits.steps': whole('Max steps', 1), 'output.every': whole('Output every', 1),
  'mesh.collocation_points': whole('Collocation points', 2, 7),
  'step.initial': {label: 'Step size', nonzero: true, message: 'Step size must be a number other than 0'},
  'step.min': positive('Min step size'), 'step.max': positive('Max step size'), 'limits.parameter_min': any('Parameter min'),
  'limits.parameter_max': any('Parameter max'), 'limits.norm_min': any('Norm min'), 'limits.norm_max': any('Norm max'),
  'newton.parameter_tolerance': positive('Parameter tolerance'), 'newton.state_tolerance': positive('State tolerance'),
  'newton.special_point_tolerance': positive('Special point tolerance'), 'mesh.adapt_every': whole('Mesh adapt every', 0),
  max_branch_switches: whole('Max branch switches', 0), 'one_way_branch_switch': bool('Switch one direction'),
  'output.detail': {label: 'Output detail', choices: levels, message: `Output detail must be one of ${levels.join(', ')}`},
  'newton.max_location_iterations': whole('Max location iterations', 1), 'newton.max_iterations': whole('Max newton iterations', 1),
  'newton.full_after_steps': whole('Full newton after', 1), 'step.adapt_every': whole('Step adapt every', 0),
  suppress_branch_points: bool('Suppress branch points'), follow_all_branches: bool('Follow all branches'),
};

const options: AutoxSettings = {
  special_points: {bogdanov_takens:true,cusp:true,zero_hopf:true,generalized_hopf:true,resonance_1_1:true,resonance_1_2:true,resonance_1_3:true,resonance_1_4:true,fold_flip:true,fold_neimark_sacker:true,flip_neimark_sacker:true,double_neimark_sacker:true},
  numerics: {'mesh.intervals': 15, 'limits.steps': 200, 'output.every': 50, 'mesh.collocation_points': 4, 'step.initial': 0.02, 'step.min': 0.001,
    'step.max': 0.5, 'limits.parameter_min': 0, 'limits.parameter_max': 2, 'limits.norm_min': 0, 'limits.norm_max': 1000, 'newton.parameter_tolerance': 1e-4,
    'newton.state_tolerance': 1e-4, 'newton.special_point_tolerance': 1e-4, 'mesh.adapt_every': 3, max_branch_switches: 5,
    'one_way_branch_switch': false, 'output.detail': 'normal', 'newton.max_location_iterations': 8, 'newton.max_iterations': 7,
    'newton.full_after_steps': 3, 'step.adapt_every': 1, suppress_branch_points: false, follow_all_branches: true},
  axes: {plot: 'maximum_and_minimum', var: 'V', par1: 'iapp', par2: 'phi', xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4},
  marks: [],
  threads: 'auto',
  threads_auto: 4,
  threads_max: 16,
  rules,
  pairs: [{lo: 'step.min', hi: 'step.max', strict: false, message: 'Min step size must be at most Max step size'},
    {lo: 'limits.parameter_min', hi: 'limits.parameter_max', strict: true, message: 'Parameter min must be below Parameter max'},
    {lo: 'limits.norm_min', hi: 'limits.norm_max', strict: true, message: 'Norm min must be below Norm max'}],
  step: {key: 'step.initial', lo: 'step.min', hi: 'step.max', message: 'The size of Step size must be from Min step size to Max step size (its sign is the direction)'},
};

test('codim-2 detection booleans stay named while queued', () => {
  const core = ev(READY, {ev: 'autox.options', ...options});
  const sent = act(core, {type: 'sent', patch: {special_points: {generalized_hopf: false}}, ahead: 0});
  assert.equal(shownSettings(sent.autoxSettings)!.special_points.generalized_hopf, false);
  assert.equal(shownSettings(sent.autoxSettings)!.special_points.bogdanov_takens, true);
  assert.ok(pendingFields(sent.autoxSettings).has('special_points.generalized_hopf'));
});

test('the event is the store; an edit sent during a run shows until its own done', () => {
  let s = ev(READY, {ev: 'autox.options', ...options});
  assert.deepEqual(s.autoxSettings.core, {ev: 'autox.options', ...options});
  /* sent during a computation: one done (the run's) ahead of each set's own */
  s = act(s, {type: 'sent', patch: {numerics: {'limits.steps': 20}}, ahead: 1});
  s = act(s, {type: 'sent', patch: {numerics: {'output.every': 5}, axes: {plot: 'norm', fit: true}}, ahead: 2});
  s = act(s, {type: 'sent', patch: {numerics: {'limits.steps': 25}}, ahead: 3});
  const shown = shownSettings(s.autoxSettings)!;
  assert.deepEqual([shown.numerics['limits.steps'], shown.numerics['output.every'], shown.numerics['mesh.intervals'], shown.axes.plot], [25, 5, 15, 'norm']);
  assert.ok(!('fit' in shown.axes));
  assert.deepEqual([...pendingFields(s.autoxSettings)].sort(), ['axes.plot', 'numerics.limits.steps', 'numerics.output.every']);
  assert.deepEqual(autoxSettingsCommands({numerics: {'limits.steps': 25}}).map(c => c.command), [{cmd: 'autox.configure_numerics', 'limits.steps': 25}]);
  /* the run's done, then each set's own: the event has the core's values */
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.equal(shownSettings(s.autoxSettings)!.numerics['limits.steps'], 25, 'still shown after the run ends');
  s = ev(s, {ev: 'autox.options', ...options, numerics: {...options.numerics, 'limits.steps': 20}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'autox.options', ...options, numerics: {...options.numerics, 'limits.steps': 20, 'output.every': 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  s = ev(s, {ev: 'autox.options', ...options, numerics: {...options.numerics, 'limits.steps': 25, 'output.every': 5}});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.deepEqual(s.autoxSettings.inflight, []);
  assert.equal(pendingFields(s.autoxSettings).size, 0);
  assert.equal(shownSettings(s.autoxSettings)!.numerics['limits.steps'], 25);
});

test("a refused set: the core's error is the forms', until the next edit", () => {
  let s = ev(READY, {ev: 'autox.options', ...options});
  s = act(s, {type: 'sent', patch: {numerics: {'mesh.collocation_points': 9}}, ahead: 0});
  s = ev(s, {ev: 'session.message', error: 'autoX options: Collocation points must be a whole number from 2 to 7'});
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.match(s.autoxSettings.error!, /Collocation points/);
  assert.equal(shownSettings(s.autoxSettings)!.numerics['mesh.collocation_points'], 4);
  s = act(s, {type: 'sent', patch: {numerics: {'mesh.collocation_points': 5}}, ahead: 1});
  assert.equal(s.autoxSettings.error, null);
  /* an error with no set of the forms' running is not theirs */
  s = ev(s, {ev: 'session.message', error: 'autoX options: nope'});
  assert.equal(s.autoxSettings.error, null);
});

