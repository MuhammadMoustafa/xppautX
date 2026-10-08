import {test} from 'node:test';
import assert from 'node:assert/strict';
import {kindOf, menuCommand} from '../src/protocol/kinds';
import {navigationGroups, navigationProblems} from '../src/ui/navigation';
import {HELLO} from './hello';

const hello = HELLO;

test('grouped navigation lists the primary commands, each once, in its category; a search reaches every command', () => {
  const ids = (query = '') => navigationGroups(hello, query, hello.keymap).flatMap(group => group.items.map(item => `${item.menu}:${item.id}`));
  const key = (row: {menu: string; id: string}) => `${row.menu}:${row.id}`;
  const listed = new Set(hello.command_categories.filter(c => c.listed).map(c => c.id));
  const rows = hello.command_table.filter(row => listed.has(row.category));
  assert.deepEqual(navigationGroups(hello, '', hello.keymap).map(group => group.name), hello.command_categories.filter(c => c.listed).map(c => c.label));
  assert.deepEqual(ids(), rows.filter(row => row.primary).map(key));
  for (const row of rows) {
    const hit = navigationGroups(hello, row.id, hello.keymap).find(group => group.items.some(item => item.menu === row.menu && item.id === row.id));
    assert.equal(hit?.id, row.category, key(row));
  }
});

test('a command lists its default key, and its XPPAUT sequence only when that preset is on', () => {
  const item = (preset: 'default' | 'xppaut', id: string) =>
    navigationGroups(hello, id, {...hello.keymap, preset}).flatMap(group => group.items).find(i => i.id === id)!;
  assert.equal(item('default', 'savesession').shortcut, 'Ctrl+S');
  assert.equal(item('default', 'savesession').firstKey, 'Ctrl+S');
  assert.equal(item('default', 'saveinfo').shortcut, '');
  assert.equal(item('xppaut', 'saveinfo').shortcut, 'F S');
});

test('search finds file and analysis commands across legacy menu boundaries', () => {
  const found = (query: string) => navigationGroups(hello, query, hello.keymap).flatMap(group => group.items.map(item => item.id));
  assert.deepEqual(found('session as'), ['savesession', 'savesessionas', 'savesessioncopy']);
  assert.deepEqual(found('EQUILIBRIA'), ['singpts']);
  assert.equal(found('no such action').length, 0);
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

test('the core table has no problems: every id once, in one known category', () => {
  assert.deepEqual(navigationProblems(HELLO), []);
  assert.equal(new Set(HELLO.command_table.map(row => `${row.menu}:${row.id}`)).size, HELLO.command_table.length);
  const categories = new Set(HELLO.command_categories.map(category => category.id));
  for (const row of HELLO.command_table) assert.ok(categories.has(row.category), `${row.menu}/${row.id}`);
  /* the shortcut-layer switches are exactly the unlisted category, and none is pinnable */
  assert.deepEqual(HELLO.command_table.filter(row => row.category === 'layer').map(row => `${row.menu}:${row.id}`),
    ['main:file', 'main:numerics', 'num:exit']);
  /* W229, W228: the Tools group is the Calculator; what lives in a panel or the title bar has a row but no group and no pin */
  assert.deepEqual(HELLO.command_table.filter(row => row.category === 'tools').map(row => row.id), ['calculator']);
  assert.deepEqual(HELLO.command_table.filter(row => row.category === 'panels').map(row => row.id).sort(),
    ['3dparams', 'copyset', 'getparset', 'keymapeditor', 'lookup', 'play', 'record', 'source', 'transpose', 'viewaxes', 'window']);
  assert.ok(HELLO.command_table.filter(row => row.category === 'panels').every(row => !row.pinnable));
  assert.ok(!HELLO.command_table.some(row => ['help', 'tutorial', 'xpprc', 'clone'].includes(row.id)));
});

test('a table the page and the core disagree on is a problem, never skipped silently', () => {
  const row = HELLO.command_table[0];
  const twice = {...HELLO, command_table: [...HELLO.command_table, {...row, key: '\0'}]};
  assert.match(navigationProblems(twice).join(' '), new RegExp(`${row.menu}/${row.id} is in the command table twice`));
  const unknown = {...HELLO, command_table: [...HELLO.command_table, {...row, id: 'brandnew', key: '\x01', category: 'nowhere'}]};
  assert.match(navigationProblems(unknown).join(' '), /main\/brandnew is in category nowhere/);
  const sameKey = {...HELLO, command_table: [...HELLO.command_table, {...row, id: 'another'}]};
  assert.match(navigationProblems(sameKey).join(' '), /main\/another shares its key/);
  const sameChord = {...HELLO, command_table: HELLO.command_table.map(r => r.id === 'reload' ? {...r, default_keys: ['Ctrl+O']} : r)};
  assert.match(navigationProblems(sameChord).join(' '), /both on Ctrl\+O/);
  const pinnableSwitch = {...HELLO, command_table: HELLO.command_table.map(r => r.id === 'exit' ? {...r, pinnable: true} : r)};
  assert.match(navigationProblems(pinnableSwitch).join(' '), /num\/exit is in a category that is not a sidebar group \(layer\) but can be pinned/);
  const reserved = {...HELLO, command_table: HELLO.command_table.map(r => r.id === 'reload' ? {...r, default_keys: ['Ctrl+K Ctrl+W']} : r)};
  assert.match(navigationProblems(reserved).join(' '), /file\/reload is on Ctrl\+K Ctrl\+W, which the system or the browser keeps/);
  const emptyGroup = {...HELLO, command_table: HELLO.command_table.filter(r => r.category !== 'plot')};
  assert.match(navigationProblems(emptyGroup).join(' '), /Category plot has no commands/);
});
