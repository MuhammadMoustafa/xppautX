/* A hello the tests use: the registry's rows read from the core's source. Not a test itself. */
import {readdirSync, readFileSync} from 'node:fs';
import {PROTOCOL, type HelloEvent} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';
import {registryRows} from '../src/ui/navigation';
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

/** the rows of the registry: every resource's table (core/commands_<resource>.cpp) and the help row, read from
    the source so that no copy can drift (npm test runs in web2/) */
function registrySource(): string {
  const files = ['command_registry.cpp', ...readdirSync('../core').filter(f => /^commands_.*\.cpp$/.test(f))];
  return files.map(f => readFileSync(`../core/${f}`, 'utf8')).join('\n');
}

const RESERVED = [...readFileSync('../core/xpp_keymap.h', 'utf8').match(/RESERVED_KEYS = \{([^}]*)\}/)![1].matchAll(/"([^"]+)"/g)].map(m => m[1]);

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
  {name: 'snic', label: 'SNIC (saddle-node on invariant circle)', mark: 'SNIC'},
  {name: 'homoclinic', label: 'Homoclinic', mark: 'HOC'},
  {name: 'heteroclinic', label: 'Heteroclinic', mark: 'HEC'},
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
  model_names: [], lists: [], userbuttons: [], defaults: {pars: [], ics: []},
  /* no keymap.json: every command with the table's keys (core/xpp_keymap.cpp effective_json) */
  keymap: {
    ok: true, path: '', preset: 'default', pinned: [], reserved: RESERVED, limits: {keys: 8, parts: 2, key_bytes: 64},
    commands: [],
  },
  commands: [
    {cmd: 'run.configure', kind: 's', step: true, arguments: [{name: 'total_time', type: 'real', required: false, choices: []}, {name: 'dt', type: 'real', required: false, choices: []},
      {name: 'method', type: 'text', required: false, choices: []}, {name: 'store_every', type: 'integer', required: false, choices: []}]},
    {cmd: 'bvp.configure', kind: 's', step: true, arguments: [{name: 'max_iterates', type: 'integer', required: false, choices: []}]},
    {cmd: 'key', kind: '', step: true},
    {cmd: 'answer', kind: 'c', step: false}, {cmd: 'run.abort', kind: 'c', step: false},
    {cmd: 'display', kind: 'v', step: true},
    ...[...registrySource().matchAll(/Command\{"([^"]+)",[\s\S]*?XPP_KIND_(\w+)[\s\S]*?,\s*(true|false),/g)]
      .map(m => ({cmd: m[1], kind: ({VIEW: 'v', COMPUTE: 'x', SETTING: 's', CONTROL: 'c', DATA: 'd'} as Record<string, string>)[m[2]], step: m[3] === 'true'})),
    {cmd: 'set', kind: 's', step: true},
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
/** the hello with every registry row (kind, menu path, legacy keys, default keys), for the key and menu tests */
export const REGISTRY_HELLO: HelloEvent = {...HELLO, commands: []};
for (const file of readdirSync('../core').filter(f => /^commands_.*\.cpp$/.test(f))) {
  const source = readFileSync(`../core/${file}`, 'utf8');
  for (const match of source.matchAll(/Command\{"([^"]+)",([\s\S]*?)\},?\n/g)) {
    const body = match[2];
    const metadata = /XPP_KIND_(\w+),\s*CommandCategory::\w+,\s*"([^"]*)", "([^"]*)",/.exec(body);
    if (!metadata) continue;
    const keyName = /, (\w+)\s*$/.exec(body)?.[1];
    const keys = keyName ? new RegExp('constexpr std::array ' + keyName + ' = \\{([^}]*)\\}').exec(source)?.[1] : '';
    REGISTRY_HELLO.commands.push({cmd: match[1], arguments: [], kind: ({VIEW:'v', COMPUTE:'x', SETTING:'s', CONTROL:'c', DATA:'d'} as Record<string,string>)[metadata[1]], step: true,
      menu_path: metadata[2], legacy_keys: metadata[3], legacy_context: /,\s*"(auto|animation|array_plot|browser)"(?:,\s*"(?:read|write)",\s*"[^"]*")?\s*$/.exec(body)?.[1] ?? '', default_keys: [...(keys ?? '').matchAll(/"([^"]+)"/g)].map(m => m[1]), description: match[1]});
  }
}
REGISTRY_HELLO.keymap = {...HELLO.keymap, commands: REGISTRY_HELLO.commands.map(c => ({id: c.cmd, keys: c.default_keys ?? [], source: 'default'}))};
/** the rows the page lists commands from (navigation.ts registryRows) */
export const ROWS = registryRows(REGISTRY_HELLO);
