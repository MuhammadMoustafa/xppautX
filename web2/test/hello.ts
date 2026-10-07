/* A hello with the command table and key layers the tests use (the core's own, the command table read from
   core/command_table.h, core/menus.cpp, core/ui_json.cpp's command table). Not a test itself. */
import {readFileSync} from 'node:fs';
import type {CommandCategory, CommandRow} from '../src/protocol/kinds';
import {PROTOCOL, type HelloEvent, type MenuName} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';

/** the string literal `"..."` of C++ source as its text */
const cString = (literal: string): string => JSON.parse(literal);

/** the core's own table, read from its source so that no copy can drift (npm test runs in web2/):
    the kind letters from menus.h, the categories and rows from command_table.h */
function coreTable(): {categories: CommandCategory[]; rows: CommandRow[]} {
  const source = (file: string) => readFileSync(`../core/${file}`, 'utf8');
  const kinds = new Map([...source('menus.h').matchAll(/#define (XPP_KIND_\w+) '(\w)'/g)].map(m => [m[1], m[2]]));
  const text = '(?:\\\\.|[^"\\\\])*';
  /* a row may name its text by a string constant of the header (STORE_EVERY_LABEL): read as the text */
  const constants = [...source('command_table.h').matchAll(new RegExp(`inline constexpr std::string_view (\\w+) = ("${text}");`, 'g'))];
  const header = constants.reduce((h, m) => h.replaceAll(`, ${m[1]},`, `, ${m[2]},`), source('command_table.h'));
  const categories = [...header.matchAll(new RegExp(`\\{CommandCategory::\\w+, ("${text}"), ("${text}"), (true|false), (true|false)\\}`, 'g'))]
    .map((m): CommandCategory => ({id: cString(m[1]), label: cString(m[2]), listed: m[3] === 'true', expanded: m[4] === 'true'}));
  const rows = [...header.matchAll(new RegExp(
    `\\{(MAIN|FILE|NUM)_MENU, '((?:\\\\.|[^'\\\\])+)', ("${text}"), ("${text}"), ("${text}"), (XPP_KIND_\\w+), CommandCategory::(\\w+), (true|false), (true|false), \\{([^}]*)\\}\\}`, 'g'))]
    .map((m): CommandRow => {
      const menu = m[1].toLowerCase() as MenuName;
      const key = m[2] === '\\033' ? '\x1b' : m[2];
      return {menu, id: cString(m[3]), key, label: cString(m[4]), description: cString(m[5]), kind: kinds.get(m[6])!, category: m[7].toLowerCase(),
        pinnable: m[8] === 'true', primary: m[9] === 'true', default_keys: [...m[10].matchAll(new RegExp(`"(${text})"`, 'g'))].map(k => k[1]),
        legacy_keys: []};
    });
  /* the XPPAUT sequence: the layer's key, then the item's (core/ui_json.cpp buf_command_table) */
  for (const row of rows) {
    const entry = rows.find(r => r.menu === 'main' && r.id === (row.menu === 'file' ? 'file' : 'numerics'));
    row.legacy_keys = [...row.menu === 'main' ? [] : [entry!.key.toUpperCase()], row.key === '\x1b' ? 'Esc' : row.key.toUpperCase()];
  }
  return {categories, rows};
}

const CORE = coreTable();

const layer = (keys: string, kinds: string, ids: string[]) => ({items: ids, keys, kinds, hints: ids, ids});

export const HELLO: HelloEvent = {
  ev: 'hello',
  state_inspection: {tail_intervals: 10}, protocol: PROTOCOL, features: [], title: 't', file: 'f.ode', about: '',
  output_names: {par: 'lecar.par', ic: 'lecar.ic', csv: 'lecar.csv', curves: 'lecar-curves.csv'},
  continue: {grid_tolerance: 1e-6},
  steady: {max_decimals: 15, default_decimals: 9, default_hold: 1},
  quit: {question: 'Quit?', recording: 'Quit and stop recording?', choices: ['Save session', "Don't save"], keys: 'sd'},
  lists: [], userbuttons: [], defaults: {pars: [], ics: []},
  menu_names: ['main', 'file', 'num'],
  command_categories: CORE.categories,
  command_table: CORE.rows,
  windows: {
    auto: layer('panrgucdf', 'svsxdsvvv', ['param', 'axes', 'numerics', 'run', 'grab', 'usr', 'clear', 'redraw', 'file']),
    ani: layer('fgrsmoa', 'vvvvdvd', ['file', 'go', 'reset', 'skip', 'mpeg', 'fly', 'grab']),
    browser: layer('fgru', 'vvdd', ['find', 'get', 'replace', 'unreplace']),
    aplot: layer('refrpg', 'vvvvdd', ['redraw', 'edit', 'fit', 'range', 'print', 'gif']),
    equilibrium: layer('i', 's', ['import']),
  },
  commands: [
    {cmd: 'steady', kind: 'x', step: true}, {cmd: 'continue', kind: 'x', step: true},
    {cmd: 'key', kind: '', step: true},
    {cmd: 'answer', kind: 'c', step: false}, {cmd: 'abort', kind: 'c', step: false},
    {cmd: 'state', kind: 'v', step: false}, {cmd: 'data', kind: 'v', step: false},
    {cmd: 'display', kind: 'v', step: true}, {cmd: 'click', kind: 'v', step: true},
    {cmd: 'browser', op: 'write', kind: 'd', step: true}, {cmd: 'browser', kind: 'v', step: false},
    {cmd: 'set', kind: 's', step: true}, {cmd: 'slider', kind: 's', step: true},
    {cmd: 'auto', op: 'set', kind: 's', step: true}, {cmd: 'auto', op: 'grab', kind: 'd', step: true},
    {cmd: 'auto', kind: 'v', step: true},
    {cmd: 'values', op: 'write', kind: 'd', step: true}, {cmd: 'values', kind: 's', step: true},
    {cmd: 'userbut', kind: 'x', step: true},
  ],
  upload_error: 'larger than 64 MB',
  player_speed: {min: 0.25, max: 8},
  limits: {upload: 64 * 1024 * 1024, browser_rows: 2000, browser_cols: 500},
  window_ids: {plots: 21, auto: 101, ani: 104, aplot: 105},
};

/** the state after the server's hello: every event but hello comes after one */
export const READY = reduce(initialState, {type: 'event', ev: HELLO});
