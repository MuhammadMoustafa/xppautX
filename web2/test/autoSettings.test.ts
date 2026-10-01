/* AUTO's settings as data (docs/ui-v2.md T22, store/autoSettings.ts): the
   event into the store, edits sent at once (settings, W106) and shown
   until their set's idle, what the forms show, the core's rules (the event's) checked in the
   page with the core's messages. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  NUM_FIELDS, numError, pairError, pairErrors, pendingFields, autoSettingsSetCommand, shownSettings, type AutoSettings,
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
const rules: Record<NumKey, NumRule> = {
  ntst: whole('Ntst', 1), nmx: whole('Nmax', 1), npr: whole('NPr', 1), ncol: whole('Ncol', 2, 7),
  ds: {label: 'Ds', nonzero: true, message: 'Ds must be a number other than 0'}, dsmin: positive('Dsmin'),
  dsmax: positive('Dsmax'), rl0: any('Par Min'), rl1: any('Par Max'), a0: any('Norm Min'), a1: any('Norm Max'),
  epsl: positive('EPSL'), epsu: positive('EPSU'), epss: positive('EPSS'), iad: whole('IAD', 0), mxbf: whole('MXBF'),
  iid: whole('IID', 0, 5), itmx: whole('ITMX', 1), itnw: whole('ITNW', 1), nwtn: whole('NWTN', 1), iads: whole('IADS', 0),
  suppbp: whole('SuppBP', 0, 1),
};

const settings: AutoSettings = {
  numerics: {ntst: 15, nmx: 200, npr: 50, ncol: 4, ds: 0.02, dsmin: 0.001, dsmax: 0.5, rl0: 0, rl1: 2, a0: 0, a1: 1000,
    epsl: 1e-4, epsu: 1e-4, epss: 1e-4, iad: 3, mxbf: 5, iid: 2, itmx: 8, itnw: 7, nwtn: 3, iads: 1, suppbp: 0},
  pars: ['iapp', 'phi', 'gca'],
  axes: {plot: 2, var: 'V', par1: 'iapp', par2: 'phi', xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4},
  marks: [],
  rules,
  pairs: [{lo: 'dsmin', hi: 'dsmax', strict: false, message: 'Dsmin must be at most Dsmax'},
    {lo: 'rl0', hi: 'rl1', strict: true, message: 'Par Min must be below Par Max'},
    {lo: 'a0', hi: 'a1', strict: true, message: 'Norm Min must be below Norm Max'}],
  step: {key: 'ds', lo: 'dsmin', hi: 'dsmax', message: 'Ds must be from Dsmin to Dsmax in size (its sign is the direction)'},
};

test('the event is the store; an edit sent during a run shows until its own idle', () => {
  let s = ev(READY, {ev: 'autosettings', ...settings});
  assert.deepEqual(s.autoSettings.core, {ev: 'autosettings', ...settings});
  /* sent during a computation: one idle (the run's) ahead of each set's own */
  s = act(s, {type: 'sent', patch: {numerics: {nmx: 20}}, ahead: 1});
  s = act(s, {type: 'sent', patch: {numerics: {npr: 5}, axes: {plot: 1, fit: true}}, ahead: 2});
  s = act(s, {type: 'sent', patch: {numerics: {nmx: 25}}, ahead: 3});
  const shown = shownSettings(s.autoSettings)!;
  assert.deepEqual([shown.numerics.nmx, shown.numerics.npr, shown.numerics.ntst, shown.axes.plot], [25, 5, 15, 1]);
  assert.ok(!('fit' in shown.axes));
  assert.deepEqual([...pendingFields(s.autoSettings)].sort(), ['axes.plot', 'numerics.nmx', 'numerics.npr']);
  assert.deepEqual(autoSettingsSetCommand({numerics: {nmx: 25}}), {cmd: 'auto', op: 'set', numerics: {nmx: 25}});
  /* the run's idle, then each set's own: the event has the core's values */
  s = ev(s, {ev: 'idle'});
  assert.equal(shownSettings(s.autoSettings)!.numerics.nmx, 25, 'still shown after the run ends');
  s = ev(s, {ev: 'autosettings', ...settings, numerics: {...settings.numerics, nmx: 20}});
  s = ev(s, {ev: 'idle'});
  s = ev(s, {ev: 'autosettings', ...settings, numerics: {...settings.numerics, nmx: 20, npr: 5}});
  s = ev(s, {ev: 'idle'});
  s = ev(s, {ev: 'autosettings', ...settings, numerics: {...settings.numerics, nmx: 25, npr: 5}});
  s = ev(s, {ev: 'idle'});
  assert.deepEqual(s.autoSettings.inflight, []);
  assert.equal(pendingFields(s.autoSettings).size, 0);
  assert.equal(shownSettings(s.autoSettings)!.numerics.nmx, 25);
});

test("a refused set: the core's error is the forms', until the next edit", () => {
  let s = ev(READY, {ev: 'autosettings', ...settings});
  s = act(s, {type: 'sent', patch: {numerics: {ncol: 9}}, ahead: 0});
  s = ev(s, {ev: 'message', error: 'AUTO settings: Ncol must be a whole number from 2 to 7'});
  s = ev(s, {ev: 'idle'});
  assert.match(s.autoSettings.error!, /Ncol/);
  assert.equal(shownSettings(s.autoSettings)!.numerics.ncol, 4);
  s = act(s, {type: 'sent', patch: {numerics: {ncol: 5}}, ahead: 1});
  assert.equal(s.autoSettings.error, null);
  /* an error with no set of the forms' running is not theirs */
  s = ev(s, {ev: 'message', error: 'AUTO settings: nope'});
  assert.equal(s.autoSettings.error, null);
});

test("the core's rules, in the page, with its messages", () => {
  assert.equal(NUM_FIELDS.length, 22);
  assert.deepEqual(NUM_FIELDS.map(f => f.key).sort(), Object.keys(rules).sort());
  assert.equal(numError(rules.ncol, '8'), 'Ncol must be a whole number from 2 to 7');
  assert.equal(numError(rules.ntst, '2.5'), 'Ntst must be a whole number of at least 1');
  assert.equal(numError(rules.nmx, '1e2'), 'Nmax must be a whole number of at least 1', 'digits only (T31)');
  assert.equal(numError(rules.ntst, '0'), 'Ntst must be a whole number of at least 1');
  assert.equal(numError(rules.ds, '0'), 'Ds must be a number other than 0');
  assert.equal(numError(rules.dsmin, '-1'), 'Dsmin must be a number above 0');
  assert.equal(numError(rules.rl0, 'x'), 'Par Min must be a number');
  assert.deepEqual([numError(rules.ds, '-0.01'), numError(rules.mxbf, '-3'), numError(rules.suppbp, '1')], [null, null, null]);
  assert.equal(pairError(settings, {...settings.numerics, rl0: 3}), 'Par Min must be below Par Max');
  assert.equal(pairError(settings, {...settings.numerics, dsmin: 1}), 'Dsmin must be at most Dsmax');
  assert.equal(pairError(settings, settings.numerics), null);
  /* T23: DSMIN <= |DS| <= DSMAX, whatever the direction; each message by its field */
  const n = settings.numerics, step = settings.step.message;
  assert.deepEqual(pairErrors(settings, {...n, ds: -n.dsmax}), {});
  assert.deepEqual(pairErrors(settings, {...n, ds: -2 * n.dsmax}), {ds: step});
  assert.deepEqual(pairErrors(settings, {...n, ds: n.dsmin / 2, a0: 5, a1: 1}), {ds: step, a0: 'Norm Min must be below Norm Max'});
  /* every field is named plainly with AUTO's short name, and explained */
  for (const f of NUM_FIELDS) {
    assert.match(f.name, /\([A-Z0-9]+[a-zA-Z]*\)$/, f.key);
    assert.ok(f.help.length > 40, f.key);
  }
  assert.equal(NUM_FIELDS.find(f => f.key === 'nmx')!.name, 'Max points (NMX)');
});
