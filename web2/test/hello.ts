/* A hello with the command table and key layers the tests use (the core's own, the command table read from
   core/command_table.h, core/menus.cpp, core/ui_json.cpp's command table). Not a test itself. */
import {readdirSync, readFileSync} from 'node:fs';
import type {CommandCategory, CommandRow} from '../src/protocol/kinds';
import {PROTOCOL, type HelloEvent, type MenuName} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';
import {setLineGeometry, type LineGeometry} from '../src/plot/linestyle';
import {setAnnotationGlyphs, type AnnotationGlyphs} from '../src/plot/marks';
import type {CoreColor, LineStyle} from '../src/protocol/types';

/** XPPAUT's colour 0..10 as the core says it (core/colormap.h xppaut_color) */
export const xppColor = (i: number): CoreColor =>
  ['foreground', '#ff0000', '#f06400', '#ffa500', '#ffcd00', '#c8c800', '#c8eb4b', '#00e100', '#00c8c8', '#0000ff', '#a020f0'][i];

/** the core's default curve style */
import type {PointType} from '../src/protocol/types';

export const LINE: LineStyle = {draw: 'line', dash: 'solid', width: 1, marker: 'circle', marker_size: 3};

/** the dash patterns and marker glyphs the core's hello carries (core/picture.cpp line_styles_json) */
export const LINE_GEOMETRY: LineGeometry = {
  dash: {solid: [], dashed: [6, 3], dotted: [1, 2], dash_dot: [6, 3, 1, 3]},
  markers: {
    circle: {circle: 1, polygon: [], strokes: []},
    square: {circle: 0, polygon: [[-1, -1], [1, -1], [1, 1], [-1, 1]], strokes: []},
    triangle: {circle: 0, polygon: [[0, -1.15], [1.05, 0.85], [-1.05, 0.85]], strokes: []},
    diamond: {circle: 0, polygon: [[0, -1.2], [1.2, 0], [0, 1.2], [-1.2, 0]], strokes: []},
    cross: {circle: 0, polygon: [], strokes: [[[-1, -1], [1, 1]], [[-1, 1], [1, -1]]]},
    plus: {circle: 0, polygon: [], strokes: [[[-1, 0], [1, 0]], [[0, -1], [0, 1]]]},
    star: {circle: 0, polygon: [], strokes: [[[-1, 0], [1, 0]], [[0, -1], [0, 1]], [[-0.7, -0.7], [0.7, 0.7]], [[-0.7, 0.7], [0.7, -0.7]]]},
  },
};
setLineGeometry(LINE_GEOMETRY);

/** the marker glyphs and arrow head the core's hello carries (core/grobs.cpp annotation_glyphs_json) */
export const ANNOTATION_GLYPHS: AnnotationGlyphs = {
  half_units: 6,
  arrow_head_spread: 0.5,
  markers: {
    box: [[[-6, -6], [6, -6], [6, 6], [-6, 6], [-6, -6]]],
    diamond: [[[8, 0], [0, -8], [-8, 0], [0, 8], [8, 0]]],
    triangle: [[[-6, -6], [6, -6], [0, 6], [-6, -6]]],
    plus: [[[-6, 0], [6, 0]], [[0, -6], [0, 6]]],
    cross: [[[-6, 6], [6, -6]], [[-6, -6], [6, 6]]],
    circle: [[[6, 0], [5, 3], [3, 5], [0, 6], [-3, 5], [-5, 3], [-6, 0], [-5, -3], [-3, -5], [0, -6], [3, -5], [5, -3], [6, 0]]],
  },
};
setAnnotationGlyphs(ANNOTATION_GLYPHS);

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
    `\\{(MAIN|FILE|NUM)_MENU, ('(?:\\\\.|[^'\\\\])+'|PAGE_KEY), ("${text}"), ("${text}"), ("${text}"), (XPP_KIND_\\w+), CommandCategory::(\\w+), (true|false), (true|false), \\{([^}]*)\\}\\}`, 'g'))]
    .map((m): CommandRow => {
      const menu = m[1].toLowerCase() as MenuName;
      /* a command the page runs itself has no legacy key (command_table.h PAGE_KEY) */
      const key = m[2] === 'PAGE_KEY' ? '' : m[2] === "'\\033'" ? '\x1b' : m[2].slice(1, -1);
      return {menu, id: cString(m[3]), key, label: cString(m[4]), description: cString(m[5]), kind: kinds.get(m[6])!, category: m[7].toLowerCase(),
        pinnable: m[8] === 'true', primary: m[9] === 'true', default_keys: [...m[10].matchAll(new RegExp(`"(${text})"`, 'g'))].map(k => k[1]),
        legacy_keys: []};
    });
  /* the XPPAUT sequence: the layer's key, then the item's (core/ui_json.cpp buf_command_table) */
  for (const row of rows.filter(r => r.key !== '')) {
    const entry = rows.find(r => r.menu === 'main' && r.id === (row.menu === 'file' ? 'file' : 'numerics'));
    row.legacy_keys = [...row.menu === 'main' ? [] : [entry!.key.toUpperCase()], row.key === '\x1b' ? 'Esc' : row.key.toUpperCase()];
  }
  return {categories, rows};
}

const CORE = coreTable();
/** the keys no command may take, read from core/xpp_keymap.h like the table (hello.keymap.reserved) */
/* the rows of the registry: every resource's table (core/commands_<resource>.cpp) and the help row */
function registrySource(): string {
  const files = ['command_registry.cpp', ...readdirSync('../core').filter(f => /^commands_.*\.cpp$/.test(f))];
  return files.map(f => readFileSync(`../core/${f}`, 'utf8')).join('\n');
}

const RESERVED = [...readFileSync('../core/xpp_keymap.h', 'utf8').match(/RESERVED_KEYS = \{([^}]*)\}/)![1].matchAll(/"([^"]+)"/g)].map(m => m[1]);

const layer = (keys: string, kinds: string, ids: string[]) => ({items: ids, keys, kinds, hints: ids, ids});

/** AUTO's point types as hello gives them (core/auto_codes.h's table) */
export const POINT_TYPES: PointType[] = [
  {name: 'regular', label: 'Regular', mark: ''},
  {name: 'branch_point', label: 'Branch point', mark: 'BP'},
  {name: 'fold', label: 'Fold', mark: 'LP'},
  {name: 'hopf', label: 'Andronov\u2013Hopf', mark: 'AH'},
  {name: 'output', label: 'Output point', mark: ''},
  {name: 'cycle_fold', label: 'Fold of cycles', mark: 'LPC'},
  {name: 'cycle_branch_point', label: 'Branch point of cycles', mark: 'BPC'},
  {name: 'period_doubling', label: 'Period doubling (flip)', mark: 'PD'},
  {name: 'neimark_sacker', label: 'Neimark\u2013Sacker (torus)', mark: 'NS'},
  {name: 'end', label: 'End of branch', mark: 'EP'},
  {name: 'user_point', label: 'User point', mark: 'UZ'},
  {name: 'bogdanov_takens', label: 'Bogdanov\u2013Takens', mark: 'BT'},
  {name: 'cusp', label: 'Cusp', mark: 'CP'},
  {name: 'zero_hopf', label: 'Zero-Hopf (fold\u2013Hopf)', mark: 'ZH'},
  {name: 'generalized_hopf', label: 'Generalized Hopf (Bautin)', mark: 'GH'},
  {name: 'resonance_1_1', label: 'Resonance 1:1', mark: 'R1'},
  {name: 'resonance_1_2', label: 'Resonance 1:2', mark: 'R2'},
  {name: 'resonance_1_3', label: 'Resonance 1:3', mark: 'R3'},
  {name: 'resonance_1_4', label: 'Resonance 1:4', mark: 'R4'},
  {name: 'fold_flip', label: 'Fold\u2013flip', mark: 'LPPD'},
  {name: 'fold_neimark_sacker', label: 'Fold\u2013Neimark\u2013Sacker', mark: 'LPNS'},
  {name: 'flip_neimark_sacker', label: 'Flip\u2013Neimark\u2013Sacker', mark: 'PDNS'},
  {name: 'double_neimark_sacker', label: 'Double Neimark\u2013Sacker', mark: 'NSNS'},
  {name: 'no_convergence', label: 'No convergence', mark: 'MX'},
];

export const HELLO: HelloEvent = {
  ev: 'hello',
  point_types: POINT_TYPES,
  auto_run: {kinds: ['equilibrium', 'periodic'], current: ['equilibrium'], rows: []},
  state_inspection: {tail_intervals: 10}, protocol: PROTOCOL, features: [], title: 't', file: 'f.ode', about: [],
  output_names: {par: 'lecar.par', ic: 'lecar.ic', csv: 'lecar.csv', curves: 'lecar-curves.csv'},
  continue: {grid_tolerance: 1e-6},
  steady: {max_decimals: 15, default_decimals: 9, default_hold: 1},
  quit: {question: 'Quit?', recording: 'Quit and stop recording?', choices: ['Save session', "Don't save"], keys: 'sd'},
  lists: [], userbuttons: [], defaults: {pars: [], ics: []},
  menu_names: ['main', 'file', 'num'],
  command_categories: CORE.categories,
  command_table: CORE.rows,
  /* no keymap.json: every command with the table's keys (core/xpp_keymap.cpp effective_json) */
  keymap: {
    ok: true, path: '', preset: 'default', pinned: [], reserved: RESERVED, limits: {keys: 8, parts: 2, key_bytes: 64},
    commands: CORE.rows.filter(r => r.category !== 'layer').map(r => ({id: r.id, keys: r.default_keys, source: 'default' as const})),
  },
  windows: {
    auto: layer('anrgucdf', 'vsxdsvvv', ['axes', 'numerics', 'run', 'grab', 'usr', 'clear', 'redraw', 'file']),
    ani: layer('fgrsmoa', 'vvvvdvd', ['file', 'go', 'reset', 'skip', 'mpeg', 'fly', 'grab']),
    browser: layer('fru', 'vdd', ['find', 'replace', 'unreplace']),
    aplot: layer('refrpg', 'vvvvdd', ['redraw', 'edit', 'fit', 'range', 'print', 'gif']),
  },
  commands: [
    {cmd: 'run.configure', kind: 's', step: true, arguments: [{name: 'total_time', type: 'real', required: false, choices: []}, {name: 'dt', type: 'real', required: false, choices: []},
      {name: 'method', type: 'text', required: false, choices: []}, {name: 'store_every', type: 'integer', required: false, choices: []}]},
    {cmd: 'bvp.configure', kind: 's', step: true, arguments: [{name: 'max_iterates', type: 'integer', required: false, choices: []}]},
    {cmd: 'key', kind: '', step: true},
    {cmd: 'answer', kind: 'c', step: false}, {cmd: 'run.abort', kind: 'c', step: false},
    {cmd: 'state', kind: 'v', step: false}, {cmd: 'data', kind: 'v', step: false},
    {cmd: 'display', kind: 'v', step: true},
    ...[...registrySource().matchAll(/Command\{"([^"]+)",[\s\S]*?XPP_KIND_(\w+)[\s\S]*?,\s*(true|false),\s*[\w:<>]+(?:,\s*\w+)?\}/g)]
      .map(m => ({cmd: m[1], kind: ({VIEW: 'v', COMPUTE: 'x', SETTING: 's', CONTROL: 'c', DATA: 'd'} as Record<string, string>)[m[2]], step: m[3] === 'true'})),
    {cmd: 'browser', op: 'write', kind: 'd', step: true}, {cmd: 'browser', kind: 'v', step: false},
    {cmd: 'set', kind: 's', step: true}, {cmd: 'slider', kind: 's', step: true},
        {cmd: 'values', op: 'write', kind: 'd', step: true}, {cmd: 'values', kind: 's', step: true},
    {cmd: 'userbut', kind: 'x', step: true},
  ],
  upload_error: 'larger than 64 MB',
  player_speed: {min: 0.25, max: 8},
  limits: {upload: 64 * 1024 * 1024, browser_rows: 2000, browser_cols: 500},
  text: {max_length: 1000, size_max: 4, default_size: 2},
  text_styles: ['normal', 'bold', 'italic', 'bold_italic'],
  line_styles: LINE_GEOMETRY,
  annotation_glyphs: ANNOTATION_GLYPHS,
  colormaps: ['jet', 'viridis', 'plasma'],
  window_names: {main:'graph_1',auto:'diagram_1',ani:'animation',aplot:'array_plot_1',browser:'browser'},
};

/** the state after the server's hello: every event but hello comes after one */
export const READY = reduce(initialState, {type: 'event', ev: HELLO});