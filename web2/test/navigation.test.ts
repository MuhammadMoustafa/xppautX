import {test} from 'node:test';
import assert from 'node:assert/strict';
import {registryRows, menuTree, paletteRows} from '../src/ui/navigation';
import {argumentSpec, argumentNames, argumentProblem, commandValues, argumentControl, blankArgument} from '../src/ui/argumentFields';
import {REGISTRY_HELLO} from './hello';
import {initialState} from '../src/store/state';
import {withPreset} from '../src/store/keymap';
import type {RegistryArgument} from '../src/protocol/kinds';

const rows = registryRows(REGISTRY_HELLO);
test('W517: every registry command appears exactly once in the tree with its declared path', () => {
  const found: string[] = [];
  const walk = (nodes: ReturnType<typeof menuTree>, path: string[]) => nodes.forEach(n => {
    if (n.row) {found.push(n.row.id); assert.equal([...path, n.name].join('/'), n.row.menu_path);}
    walk(n.children, [...path, n.name]);
  });
  walk(menuTree(rows), []);
  assert.deepEqual(found.sort(), rows.map(r => r.id).sort());
  assert.ok(rows.length > 150);
  assert.ok(rows.every(r => ['File','Run','Analysis','Plot','Data','AUTO','Windows','Help'].includes(r.menu_path!.split('/')[0])));
});
test('W517: palette matches each word in names, paths and descriptions', () => {
  assert.ok(paletteRows(rows, 'session save').some(r => r.id === 'session.save'));
  assert.ok(paletteRows(rows, 'plot annotation').some(r => r.id === 'annotation.move'));
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
test('W517: both preset buttons retain pins and identify the core preset', () => {
  const info = {...REGISTRY_HELLO.keymap, pinned:['session.save']};
  assert.equal(withPreset(info, 'xppaut').preset, 'xppaut');
  assert.deepEqual(withPreset(info, 'default').pinned, ['session.save']);
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
