import {test} from 'node:test';
import assert from 'node:assert/strict';
import {registryRows, menuTree, paletteRows} from '../src/ui/navigation';
import {argumentSpec, argumentNames, argumentProblem, commandValues, formValues, argumentControl, blankArgument} from '../src/ui/argumentFields';
import {REGISTRY_HELLO} from './hello';
import {initialState} from '../src/store/state';
import type {RegistryArgument} from '../src/protocol/kinds';

const rows = registryRows(REGISTRY_HELLO);
test('W517: every registry command with a menu place appears exactly once in the tree with its declared path (W637: the others are a view\'s)', () => {
  const found: string[] = [];
  const walk = (nodes: ReturnType<typeof menuTree>, path: string[]) => nodes.forEach(n => {
    if (n.row) {found.push(n.row.id); assert.equal(path.join('/'), n.row.menu_path); assert.equal(n.name, n.row.label);}
    walk(n.children, [...path, n.name]);
  });
  walk(menuTree(rows), []);
  const placed = rows.filter(r => r.menu_path);
  assert.deepEqual(found.sort(), placed.map(r => r.id).sort());
  assert.ok(placed.length > 150);
  assert.ok(placed.every(r => ['File','Run','Analysis','Plot','Data','autoX','Windows','Help'].includes(r.menu_path!.split('/')[0])));
});
test('W647: the window (its __xppCloseWindow bound) has no Quit; a browser has it, and About xppautX is a Help command in both', () => {
  const w = globalThis as {__xppCloseWindow?: unknown};
  try {
    assert.ok(registryRows(REGISTRY_HELLO).some(r => r.id === 'session.quit'));
    w.__xppCloseWindow = async () => {};
    assert.ok(!registryRows(REGISTRY_HELLO).some(r => r.id === 'session.quit'));
    assert.ok(registryRows(REGISTRY_HELLO).length > 150);
  } finally { delete w.__xppCloseWindow; }
  const about = rows.find(r => r.id === 'tool.about');
  assert.equal(about?.label, 'About xppautX');
  assert.equal(about?.menu_path, 'Help');
});
test('W636: a row with no menu place has a label but no place in the tree, and a row is shown by its label', () => {
  const bare = {...rows[0], id: 'x.bare', menu_path: '', label: 'Bare'};
  assert.deepEqual(menuTree([bare]), []);
  assert.deepEqual(menuTree([{...bare, menu_path: 'File/Sets'}]).map(n => [n.name, n.children.map(c => c.name), n.children[0].children.map(c => c.name)]), [['File', ['Sets'], ['Bare']]]);
});
test('W517: palette matches each word in names, menu places and descriptions', () => {
  assert.ok(paletteRows(rows, 'session save').some(r => r.id === 'session.save'));
  assert.ok(paletteRows(rows, 'annotation move').some(r => r.id === 'annotation.move'));
  assert.equal(paletteRows(rows, 'not-a-command').length, 0);
});
const arg = (type: string): RegistryArgument => ({name: 'value', type, required: true, choices: ['last_state','initial_state'], min: 0, max: 3});
test('W517: form fields use registry types, choices, bounds and model/window names', () => {
  assert.deepEqual(argumentSpec(arg('choice')), {kind: 'choice', choices: ['last_state','initial_state']});
  assert.equal(argumentSpec(arg('boolean')).kind, 'boolean');
  assert.equal(argumentSpec(arg('color')).kind, 'color');
  assert.equal(argumentSpec(arg('file')).kind, 'file');
  assert.ok(argumentProblem(arg('integer'), '4'));
  assert.equal(argumentProblem(arg('integer'), '2'), null);
  assert.ok(argumentProblem(arg('real'), '-'));
  const state = {...initialState, hello: {...REGISTRY_HELLO, lists: [['T'], ['x'], ['p']]}, windows: [{name: 'Named graph', kind: 'graph' as const, shown: false}]};
  assert.deepEqual(argumentNames(arg('window'), state), ['Named graph']);
  assert.deepEqual(argumentNames({...arg('name'), names:'parameter'}, state), ['p']);
  const list = {...arg('list'), element:'real'};
  const map = {...arg('map'), element:'real'};
  assert.deepEqual(commandValues([list], {value:['1','2']}), {value:[1,2]});
  assert.deepEqual(commandValues([map], {value:{p:'2'}}), {value:{p:2}});
  assert.ok(argumentProblem(list, ['wrong']));
  assert.deepEqual(commandValues([{...arg('text'), required:false}], {}), {});
});

test('W517: widget selection uses the actual registry wire types, including name and fixed lists', () => {
  for (const type of ['name','window','choice']) assert.equal(argumentControl(arg(type)), 'select');
  for (const type of ['list','map','list[real, 2]','list[text]']) assert.equal(argumentControl(arg(type)), 'rows');
  assert.equal(argumentControl(arg('boolean')), 'checkbox');
  assert.equal(argumentControl(arg('file')), 'file');
  assert.equal(argumentControl(arg('color')), 'color');
  assert.deepEqual(commandValues([{...arg('map'), element:'list[real, 2]'}], {value:{x:['1','2']}}), {value:{x:[1,2]}});
  assert.deepEqual(blankArgument(arg('list[real, 2]')), ['', '']);
  assert.deepEqual(blankArgument(arg('list[text]')), []);
  assert.throws(() => argumentControl(arg('unknown')));
});

test('W323: the active graph a form fills in goes with the scopes all and plot only', () => {
  const args = [arg('choice'), arg('window')].map((a, i) => ({...a, name: ['scope', 'window'][i]}));
  assert.deepEqual(formValues(args, {scope: 'parameters', window: 'graph_1'}), {scope: 'parameters'});
  assert.deepEqual(formValues(args, {scope: 'plot', window: 'graph_1'}), {scope: 'plot', window: 'graph_1'});
  assert.deepEqual(formValues(args, {scope: 'all', window: 'graph_1'}), {scope: 'all', window: 'graph_1'});
  assert.deepEqual(formValues([args[1]], {window: 'graph_1'}), {window: 'graph_1'});
});
