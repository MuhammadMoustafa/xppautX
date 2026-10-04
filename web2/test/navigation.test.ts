import {test} from 'node:test';
import assert from 'node:assert/strict';
import {kindOf, menuCommand} from '../src/protocol/kinds';
import {navigationGroups} from '../src/ui/navigation';
import {initialState, reduce} from '../src/store/state';
import {initialValues, reduceValues} from '../src/store/values';
import {HELLO} from './hello';

const hello = {...HELLO, menus: {...HELLO.menus}};
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
