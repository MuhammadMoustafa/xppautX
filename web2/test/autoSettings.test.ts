/* AUTO's settings as data (docs/ui-v2.md T22, store/autoSettings.ts): the
   event into the store, edits pending while the core computes and merged
   into one set, what the forms show, the core's rules checked in the page,
   and the settings file (version 1 files still load). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  formatSettings, mergePatch, NUM_FIELDS, numError, pairError, pairErrors, parseSettings, pendingFields, plainName, setCommand,
  shownSettings, type AutoSettings,
} from '../src/store/autoSettings';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const act = (s: AppState, action: object) => reduce(s, {type: 'autoSettings', action: action as never});

const settings: AutoSettings = {
  numerics: {ntst: 15, nmx: 200, npr: 50, ncol: 4, ds: 0.02, dsmin: 0.001, dsmax: 0.5, rl0: 0, rl1: 2, a0: 0, a1: 1000,
    epsl: 1e-4, epsu: 1e-4, epss: 1e-4, iad: 3, mxbf: 5, iid: 2, itmx: 8, itnw: 7, nwtn: 3, iads: 1, suppbp: 0},
  pars: ['iapp', 'phi', 'gca'],
  axes: {plot: 2, var: 'V', par1: 'iapp', par2: 'phi', xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4},
  marks: [],
};

test('the event is the store; an edit while busy waits, merged, and shows as pending', () => {
  let s = ev(initialState, {ev: 'autosettings', ...settings});
  assert.deepEqual(s.autoSettings.core, {ev: 'autosettings', ...settings});
  s = act(s, {type: 'queue', patch: {numerics: {nmx: 20}}});
  s = act(s, {type: 'queue', patch: {numerics: {npr: 5}, axes: {plot: 1, fit: true}}});
  s = act(s, {type: 'queue', patch: {numerics: {nmx: 25}}});
  assert.deepEqual(s.autoSettings.queued, {numerics: {nmx: 25, npr: 5}, axes: {plot: 1, fit: true}});
  const shown = shownSettings(s.autoSettings)!;
  assert.deepEqual([shown.numerics.nmx, shown.numerics.npr, shown.numerics.ntst, shown.axes.plot], [25, 5, 15, 1]);
  assert.ok(!('fit' in shown.axes));
  assert.deepEqual([...pendingFields(s.autoSettings)].sort(), ['axes.plot', 'numerics.nmx', 'numerics.npr']);
  /* the queue goes out as one set; its idle ends the pending, the event has the core's values */
  s = act(s, {type: 'sent', patch: s.autoSettings.queued});
  assert.equal(s.autoSettings.queued, null);
  assert.deepEqual(setCommand(s.autoSettings.sent!), {cmd: 'auto', op: 'set', numerics: {nmx: 25, npr: 5},
    axes: {plot: 1, fit: true}});
  s = ev(s, {ev: 'autosettings', ...settings, numerics: {...settings.numerics, nmx: 25, npr: 5}});
  s = ev(s, {ev: 'idle'});
  assert.equal(s.autoSettings.sent, null);
  assert.equal(pendingFields(s.autoSettings).size, 0);
  assert.equal(shownSettings(s.autoSettings)!.numerics.nmx, 25);
});

test("a refused set: the core's error is the forms', until the next edit", () => {
  let s = ev(initialState, {ev: 'autosettings', ...settings});
  s = act(s, {type: 'sent', patch: {numerics: {ncol: 9}}});
  s = ev(s, {ev: 'message', error: 'AUTO settings: Ncol must be a whole number from 2 to 7'});
  s = ev(s, {ev: 'idle'});
  assert.match(s.autoSettings.error!, /Ncol/);
  assert.equal(shownSettings(s.autoSettings)!.numerics.ncol, 4);
  s = act(s, {type: 'queue', patch: {numerics: {ncol: 5}}});
  assert.equal(s.autoSettings.error, null);
  /* an error with no set of the forms' waiting is not theirs */
  s = ev(s, {ev: 'message', error: 'AUTO settings: nope'});
  assert.equal(s.autoSettings.error, null);
});

test("the core's rules, in the page", () => {
  assert.equal(NUM_FIELDS.length, 22);
  assert.equal(numError('ncol', '8'), 'Collocation points (NCOL) must be a whole number from 2 to 7');
  assert.equal(numError('ntst', '2.5'), 'Mesh intervals (NTST) must be a whole number, not 2.5');
  assert.equal(numError('nmx', '1e2'), 'Max points (NMX) must be a whole number, not 1e2', 'digits only (T31)');
  assert.equal(numError('nmx', 'ab'), 'Max points (NMX) must be a whole number');
  assert.equal(numError('ntst', '0'), 'Mesh intervals (NTST) must be a whole number of at least 1');
  assert.equal(numError('ds', '0'), 'First step (DS) must be a number other than 0');
  assert.equal(numError('dsmin', '-1'), 'Smallest step (DSMIN) must be a number above 0');
  assert.equal(numError('rl0', 'x'), 'Par Min (RL0) must be a number');
  assert.deepEqual([numError('ds', '-0.01'), numError('mxbf', '-3'), numError('suppbp', '1')], [null, null, null]);
  assert.equal(pairError({...settings.numerics, rl0: 3}), 'Par Min must be below Par Max');
  assert.equal(pairError({...settings.numerics, dsmin: 1}), 'DSMIN must be at most DSMAX');
  assert.equal(pairError(settings.numerics), null);
  /* T23: DSMIN <= |DS| <= DSMAX, whatever the direction; each message by its field */
  const n = settings.numerics;
  assert.deepEqual(pairErrors({...n, ds: -n.dsmax}), {});
  assert.deepEqual(pairErrors({...n, ds: -2 * n.dsmax}), {ds: 'DS must be from DSMIN to DSMAX in size'});
  assert.deepEqual(pairErrors({...n, ds: n.dsmin / 2, a0: 5, a1: 1}),
    {ds: 'DS must be from DSMIN to DSMAX in size', a0: 'Norm Min must be below Norm Max'});
  /* every field is named plainly with AUTO's short name, and explained */
  for (const f of NUM_FIELDS) {
    assert.match(f.name, /\([A-Z0-9]+[a-zA-Z]*\)$/, f.key);
    assert.ok(f.help.length > 40, f.key);
  }
  assert.equal(NUM_FIELDS.find(f => f.key === 'nmx')!.name, 'Max points (NMX)');
  assert.deepEqual(mergePatch({pars: ['a']}, {marks: [['a', 1]]}), {pars: ['a'], marks: [['a', 1]]});
});

test('the settings file: written from the data, read back as one set; version 1 files load', () => {
  const text = formatSettings({...settings, marks: [['iapp', 0.25], ['T', 30]]});
  const o = JSON.parse(text);
  assert.equal(o.version, 2);
  assert.equal(Object.keys(o.numerics).length, 22);
  assert.deepEqual([o.numerics.Nmax, o.numerics['Par Min'], o.plot, o.axes['Main Parm'], o.axes.Ymax],
    [200, 0, 2, 'iapp', 0.4]);
  const {patch, error} = parseSettings(text);
  assert.equal(error, null);
  assert.deepEqual(patch!.numerics, settings.numerics);
  assert.deepEqual(patch!.axes, {plot: 2, var: 'V', par1: 'iapp', par2: 'phi', xmin: -0.2, ymin: -0.5, xmax: 0.5, ymax: 0.4});
  assert.deepEqual([patch!.pars, patch!.marks], [['iapp', 'phi', 'gca'], [['iapp', 0.25], ['T', 30]]]);
  /* T21's version 1: the forms' text, names with list markers, any case */
  const v1 = parseSettings('{"xppautX":"auto-settings","version":1,"numerics":{"NMAX":"321","Ds":"0.01","Other":"1"},'
    + '"plot":1,"axes":{"*1Y-axis":"W","Xmin":"0.01"}}');
  assert.deepEqual(v1.patch, {numerics: {nmx: 321, ds: 0.01}, axes: {plot: 1, var: 'W', xmin: 0.01}});
  assert.equal(plainName('*2Secnd Parm'), 'Secnd Parm');
  assert.match(parseSettings('{').error!, /not JSON/);
  assert.match(parseSettings('{"xppautX":"other"}').error!, /auto-settings/);
  assert.match(parseSettings('{"xppautX":"auto-settings","plot":7,"numerics":{"Ntst":"1"}}').error!, /plot/);
  assert.match(parseSettings('{"xppautX":"auto-settings","numerics":{"Ntst":[1]}}').error!, /Ntst is not a number/);
  assert.match(parseSettings('{"xppautX":"auto-settings","numerics":[1]}').error!, /map names/);
  assert.match(parseSettings('{"xppautX":"auto-settings","marks":[["a"]]}').error!, /marks/);
  assert.match(parseSettings('{"xppautX":"auto-settings","numerics":{"Other":"1"}}').error!, /no values/);
});
