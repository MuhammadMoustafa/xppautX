/* A hello with the kinds, ids and key layers the tests use (the core's own:
   core/menus.cpp, core/ui_json.cpp's command table). Not a test itself. */
import {PROTOCOL, type HelloEvent} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';

const layer = (keys: string, kinds: string, ids: string[]) => ({items: ids, keys, kinds, hints: ids, ids});

export const HELLO: HelloEvent = {
  ev: 'hello', protocol: PROTOCOL, features: [], title: 't', file: 'f.ode', about: '',
  output_names: {par: 'lecar.par', ic: 'lecar.ic', csv: 'lecar.csv', curves: 'lecar-curves.csv'},
  quit: {question: 'Quit?', recording: 'Quit and stop recording?', choices: ['Save session', "Don't save"], keys: 'sd'},
  lists: [], userbuttons: [], defaults: {pars: [], ics: []},
  menus: {
    main: [], main_keys: 'icndwakgufpemtsvxr3b', main_hints: [], main_kinds: 'xxvvvdvvvvsvvvxvvvvx',
    file: [], file_keys: 'pracshqtglxuomevndy', file_hints: [], file_kinds: 'vdvvdvcdsddvvdddddd',
    num: [], num_keys: 'tsrdniobmechpukva\x1b', num_hints: [], num_kinds: 'ssssssssssvdssdsdv',
    names: ['main', 'file', 'num'],
    main_ids: ['initialconds', 'continue', 'nullcline', 'dirfield', 'window', 'phasespace', 'kinescope', 'graphic',
      'numerics', 'file', 'parameters', 'erase', 'makewindow', 'text', 'singpts', 'viewaxes', 'xivst', 'restore',
      '3dparams', 'bndryval'],
    file_ids: ['source', 'importset', 'auto', 'calculator', 'saveinfo', 'help', 'quit', 'transpose',
      'getparset', 'clone', 'xpprc', 'tutorial', 'copyset', 'openmodel', 'reload', 'savesession', 'opensession',
      'record', 'play'],
    num_ids: ['total', 'start', 'transient', 'dt', 'ncline', 'singpt', 'noutput', 'bounds', 'method', 'delay',
      'colorcode', 'stochastic', 'poincare', 'ruelle', 'lookup', 'bndval', 'averaging', 'exit'],
  },
  windows: {
    auto: layer('panrgucdf', 'svsxdsvvv', ['param', 'axes', 'numerics', 'run', 'grab', 'usr', 'clear', 'redraw', 'file']),
    ani: layer('fgrsmoa', 'vvvvdvd', ['file', 'go', 'reset', 'skip', 'mpeg', 'fly', 'grab']),
    browser: layer('fgru', 'vvdd', ['find', 'get', 'replace', 'unreplace']),
    aplot: layer('refrpg', 'vvvvdd', ['redraw', 'edit', 'fit', 'range', 'print', 'gif']),
    equilibrium: layer('i', 's', ['import']),
  },
  commands: [
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
