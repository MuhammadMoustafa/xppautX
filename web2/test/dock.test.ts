import {strict as assert} from 'node:assert';
import {test} from 'node:test';
import {dockIsShown, dockTab, withDock} from '../src/store/dock';

const state = (open: Partial<Record<'table' | 'text' | 'help', boolean>> = {}, dockHidden = false) => ({
  table: {open: !!open.table}, text: {open: !!open.text}, help: {open: !!open.help}, panels: {dockHidden},
});

test('the tab shown is the panel that is open, else Values', () => {
  assert.equal(dockTab(state()), 'values');
  assert.equal(dockTab(state({text: true})), 'text');
});

test('opening a panel closes the others and shows a hidden dock; nothing opened changes nothing', () => {
  const before = state({table: true});
  const next = withDock(before, {...before, help: {open: true}});
  assert.deepEqual([next.table.open, next.help.open], [false, true]);
  const hidden = state({}, true);
  assert.equal(withDock(hidden, {...hidden, table: {open: true}}).panels.dockHidden, false);
  assert.equal(withDock(before, before), before);
});

test('beside autoX the dock is there for a panel, not for Values', () => {
  assert.equal(dockIsShown(false, true, 'values'), false);
  assert.equal(dockIsShown(false, true, 'table'), true);
  assert.equal(dockIsShown(false, false, 'values'), true);
  assert.equal(dockIsShown(true, false, 'table'), false);
});
