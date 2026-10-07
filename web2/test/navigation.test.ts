import {test} from 'node:test';
import assert from 'node:assert/strict';
import {kindOf, menuCommand} from '../src/protocol/kinds';
import {COMMAND_GROUPS, LAYER_SWITCHES, contextualShortcut, layerLabel, navigationGroups, navigationProblems} from '../src/ui/navigation';
import {initialState, reduce} from '../src/store/state';
import {initialValues, reduceValues} from '../src/store/values';
import {HELLO} from './hello';

const hello = {...HELLO, menus: {...HELLO.menus}};

test('shortcut hints describe the sequence from the active layer', () => {
  assert.equal(contextualShortcut('F, R', 'file', 'main'), 'F, R');
  assert.equal(contextualShortcut('F, R', 'file', 'file'), 'R');
  assert.equal(contextualShortcut('I', 'main', 'file'), 'Esc, I');
  assert.equal(contextualShortcut('U, D', 'num', 'file'), 'Esc, U, D');
  assert.equal(contextualShortcut('F, R', 'file', 'num'), 'Esc, F, R');
  assert.equal(contextualShortcut('U, D', 'num', 'num'), 'D');
  assert.equal(contextualShortcut('I', 'main', null), 'I');
});
for (const menu of ['main', 'file', 'num'] as const) {
  hello.menus[menu] = hello.menus[`${menu}_ids`].map(id => id);
  hello.menus[`${menu}_hints`] = hello.menus[menu].map(id => `Action ${id}`);
}

test('grouped navigation exposes every actionable legacy item exactly once', () => {
  const items = navigationGroups(hello, '').flatMap(group => group.items.map(item => `${item.menu}:${item.id}`));
  const expected = (['main', 'file', 'num'] as const).flatMap(menu => hello.menus[`${menu}_ids`]
    .filter(id => !(menu === 'main' && ['file', 'numerics'].includes(id)) && !(menu === 'num' && id === 'exit'))
    .map(id => `${menu}:${id}`));
  assert.deepEqual([...items].sort(), [...expected].sort());
  assert.equal(new Set(items).size, items.length);
});

test('search finds file and analysis commands across legacy menu boundaries', () => {
  assert.deepEqual(navigationGroups(hello, 'save session').flatMap(group => group.items.map(item => item.id)), ['savesession']);
  assert.deepEqual(navigationGroups(hello, 'SING PTS').flatMap(group => group.items.map(item => item.id)), ['singpts']);
  assert.equal(navigationGroups(hello, 'no such action').flatMap(group => group.items).length, 0);
});

test('stable menu actions retain their kind in every legacy shortcut mode', () => {
  for (const current of [0, 1, 2]) {
    assert.equal(kindOf(HELLO, current, menuCommand('file', 'savesession')), 'data');
    assert.equal(kindOf(HELLO, current, menuCommand('main', 'parameters')), 'setting');
    assert.equal(kindOf(HELLO, current, menuCommand('main', 'initialconds')), 'computation');
  }
  assert.equal(kindOf(HELLO, 0, menuCommand('file', 'does-not-exist')), null);
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', menu: 'invalid', item: 'quit'}), null);
  for (const fields of [{menu: 'main'}, {item: 'erase'}, {menu: 42, item: 'erase'},
    {menu: 'main', item: 'erase', key: 'Escape'}, {menu: 'main', item: 'erase', win: 'auto'}]) {
    assert.equal(kindOf(HELLO, 0, {cmd: 'key', ...fields}), null);
  }
});

test('working-value checkpoints copy authoritative values and clear on any model hello', () => {
  const pars: [string, number][] = [['iapp', 0.1234567890123456]];
  const ics: [string, number][] = [['V', -0.144]];
  const values = reduceValues(initialValues, {type: 'checkpoint', pars, ics});
  pars[0][1] = 99;
  assert.equal(values.checkpoint?.pars[0][1], 0.1234567890123456);
  assert.equal(values.checkpoint?.ics[0][1], -0.144);
  const next = reduce({...initialState, values}, {type: 'event', ev: HELLO});
  assert.equal(next.values.checkpoint, null);
});

test('every hello menu id is in exactly one group, or is a shortcut-layer switch', () => {
  assert.deepEqual(navigationProblems(HELLO), []);
  const seen = new Map<string, number>();
  for (const group of COMMAND_GROUPS) for (const menu of ['main', 'file', 'num'] as const)
    for (const id of group[menu]) seen.set(`${menu}:${id}`, (seen.get(`${menu}:${id}`) ?? 0) + 1);
  for (const menu of ['main', 'file', 'num'] as const) for (const id of HELLO.menus[`${menu}_ids`])
    assert.equal(seen.get(`${menu}:${id}`) ?? 0, LAYER_SWITCHES[menu].includes(id) ? 0 : 1, `${menu}/${id}`);
});

test('a command the groups and the core disagree on is a problem, never skipped silently', () => {
  const extra = {...HELLO, menus: {...HELLO.menus, main_ids: [...HELLO.menus.main_ids, 'brandnew']}};
  assert.equal(navigationProblems(extra).length, 1);
  assert.match(navigationProblems(extra)[0], /main\/brandnew.*0 command groups/);
  const missing = {...HELLO, menus: {...HELLO.menus, file_ids: HELLO.menus.file_ids.filter(id => id !== 'reload')}};
  assert.match(navigationProblems(missing).join(' '), /file\/reload, which the core does not have/);
});

test('a shortcut layer is named by the core\'s own label of the item that enters it', () => {
  const labelled = {...HELLO, menus: {...HELLO.menus, main: HELLO.menus.main_ids.map(id => id === 'file' ? 'File' : id === 'numerics' ? 'Numerics' : id)}};
  assert.equal(layerLabel(labelled, 'file'), 'File');
  assert.equal(layerLabel(labelled, 'num'), 'Numerics');
});
