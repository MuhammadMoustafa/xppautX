/* A hello with the kinds and key layers the tests use (the core's own:
   core/menus.cpp, core/ui_json.cpp's command table). Not a test itself. */
import type {HelloEvent} from '../src/protocol/types';

export const HELLO: HelloEvent = {
  ev: 'hello', protocol: 2, title: 't', file: 'f.ode', lists: [], userbuttons: [], sliders: [],
  menus: {
    main: [], main_keys: 'icndwakgufpemtsvxr3b', main_hints: [], main_kinds: 'xxvvvdvvvvsvvvxvvvvx',
    file: [], file_keys: 'pwracshqtglxuomevn', file_hints: [], file_kinds: 'vddvvdvcdsddvvdddd',
    num: [], num_keys: 'tsrdniobmechpukva\x1b', num_hints: [], num_kinds: 'ssssssssssvdssdsdv',
  },
  windows: {
    auto: {items: [], keys: 'panrgucdf', kinds: 'svsxdsvvv', hints: [],
      ids: ['param', 'axes', 'numerics', 'run', 'grab', 'usr', 'clear', 'redraw', 'file']},
    ani: {items: [], keys: 'fgrsmoa', kinds: 'vvvvdvd', hints: [], ids: ['file', 'go', 'reset', 'skip', 'mpeg', 'fly', 'grab']},
  },
  commands: [
    {cmd: 'answer', kind: 'c'}, {cmd: 'abort', kind: 'c'}, {cmd: 'display', kind: 'v'}, {cmd: 'click', kind: 'v'},
    {cmd: 'browser', op: 'write', kind: 'd'}, {cmd: 'browser', kind: 'v'}, {cmd: 'set', kind: 's'},
    {cmd: 'auto', op: 'set', kind: 's'}, {cmd: 'auto', op: 'grab', kind: 'd'}, {cmd: 'auto', kind: 'v'},
    {cmd: 'values', op: 'write', kind: 'd'}, {cmd: 'values', kind: 's'}, {cmd: 'userbut', kind: 'x'},
  ],
};

