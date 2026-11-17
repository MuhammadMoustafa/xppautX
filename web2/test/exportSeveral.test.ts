/* File > Export > Several (W774): the window's rows, what each tick sends, the stem a typed name gives, the folder paths it writes to, and the window's state
   (store/exportSeveral.ts). The rows are the registry's: a format is a choice's value, its file the core's output name. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {EXPORT_ROWS, defaultFileName, exportCommand, exportDialogTitle, exportRows, folderOf, joinFolder, restem, typedStem, type ExportRow} from '../src/exportSeveral';
import {initialExportSeveral, reduceExportSeveral} from '../src/store/exportSeveral';
import type {CommandRow} from '../src/protocol/kinds';
import {setFileFormats} from '../src/store/fieldKinds';

/* hello's file formats: the labels of a format that is a file are its name as written */
setFileFormats(['pdf', 'svg', 'dat', 'csv', 'csv.gz', 'npz', 'txt', 'gif', 'ppm']);

const names: Record<string, string> = {
  'session.export_info': 'lecar.txt', 'plot.export.pdf': 'lecar.pdf', 'plot.export.svg': 'lecar.svg',
  'data.export.dat': 'lecar.dat', 'data.export.csv': 'lecar.csv', 'data.export.npz': 'lecar.npz',
  'model.export.python': 'lecar.py', 'model.export.python_numpy': 'lecar-python_numpy.py', 'model.export.python_numba': 'lecar-python_numba.py',
  'animation.export.animated_gif': 'lecar-animation.gif', 'animation.export.frames': 'lecar-animation-frames.ppm',
  'kinescope.export.animated_gif': 'lecar-kinescope.gif', 'kinescope.export.frames': 'lecar-kinescope-frames.gif',
  'array_plot.export.pdf': 'lecar-array.pdf', 'array_plot.export.gif': 'lecar-array.gif',
};

const SEP = ' \u203a ';
const choice = (name: string, values: string[]) => ({name, type: 'choice', required: true, choices: values});
const row = (id: string, label: string, menu_choice: string, values: string[], extra: Partial<CommandRow> = {}): CommandRow => ({
  id, cmd: id, label, name: label, menu_path: 'File/Export', menu_choice, arguments: [choice(menu_choice, values)], ...extra,
} as CommandRow);
const rows: CommandRow[] = [
  {id: 'session.export_info', cmd: 'session.export_info', label: 'Simulation information', name: 'Simulation information', menu_path: 'File/Export', arguments: []} as unknown as CommandRow,
  row('plot.export', 'Graph as', 'format', ['pdf', 'svg']),
  row('data.export', 'Data as', 'format', ['dat', 'csv', 'csv.gz', 'npz'], {menu_fixed: {argument: 'target', value: 'table'}}),
  row('model.export', 'Model as', 'language', ['python', 'python_numpy', 'python_numba']),
  row('animation.export', 'Animation as', 'output', ['animated_gif', 'frames']),
  row('kinescope.export', 'Kinescope as', 'output', ['animated_gif', 'frames']),
  row('array_plot.export', 'Array plot as', 'format', ['pdf', 'gif']),
  row('plot.export_other', 'Not an export', 'format', ['pdf']),
];

test('the window lists the exports in its order, each format a tick, the file its core name', () => {
  const out = exportRows(rows, SEP, names, []);
  assert.deepEqual(out.map(r => r.id), EXPORT_ROWS.filter(id => id !== 'plot.export_other'));
  const graph = out.find(r => r.id === 'plot.export')!;
  assert.deepEqual(graph.items.map(i => [i.label, i.file, i.key]), [['pdf', 'lecar.pdf', 'plot.export|pdf'], ['svg', 'lecar.svg', 'plot.export|svg']]);
  const data = out.find(r => r.id === 'data.export')!;
  assert.deepEqual(data.items.map(i => i.label), ['dat', 'csv', 'npz']); /* csv.gz has no output name in this fixture: left out */
  assert.deepEqual(data.items.map(i => i.args), [{target: 'table', format: 'dat'}, {target: 'table', format: 'csv'}, {target: 'table', format: 'npz'}]);
  const model = out.find(r => r.id === 'model.export')!;
  assert.deepEqual(model.items.map(i => [i.label, i.file]), [['Python', 'lecar.py'], ['Python numpy', 'lecar-python_numpy.py'], ['Python numba', 'lecar-python_numba.py']]);
  const info = out.find(r => r.id === 'session.export_info')!;
  assert.deepEqual([info.items[0].label, info.items[0].file, info.items[0].args], ['txt', 'lecar.txt', {}]);
});

test('a file question with no name of its own starts with the command\'s default: its choice\'s value, or the command alone', () => {
  const name = (cmd: Record<string, unknown>) => defaultFileName(rows, names, cmd as never);
  assert.equal(name({cmd: 'plot.export', format: 'svg', file_picker: true}), 'lecar.svg');
  assert.equal(name({cmd: 'data.export', target: 'table', format: 'csv', file_picker: true}), 'lecar.csv');
  assert.equal(name({cmd: 'kinescope.export', output: 'animated_gif', file_picker: true}), 'lecar-kinescope.gif');
  assert.equal(name({cmd: 'session.export_info', file_picker: true}), 'lecar.txt');
  assert.equal(name({cmd: 'session.save_as', file_picker: true}), '');
});

test('a row the session cannot run now is greyed with the reason the core gave', () => {
  const out = exportRows(rows, SEP, names, [{cmd: 'animation.export', reason: 'needs an animation loaded'}]);
  const animation = out.find(r => r.id === 'animation.export')!;
  assert.equal(animation.reason, 'needs an animation loaded');
  assert.equal(out.find(r => r.id === 'kinescope.export')!.reason, null);
});

test('a tick sends its own command with its format and the file, and the replace answer when given', () => {
  const item = exportRows(rows, SEP, names, []).find(r => r.id === 'data.export')!.items[1];
  assert.deepEqual(exportCommand(item, '/home/u/out/lecar.csv', 'export-1'),
    {cmd: 'data.export', target: 'table', format: 'csv', file: '/home/u/out/lecar.csv', id: 'export-1'});
  assert.deepEqual(exportCommand(item, 'lecar.csv', 'export-2', 1),
    {cmd: 'data.export', target: 'table', format: 'csv', file: 'lecar.csv', id: 'export-2', replace: 1});
});

test('a folder path joins a name with its own separator, and a model path gives its folder', () => {
  assert.equal(joinFolder('/home/u/out', 'lecar.csv'), '/home/u/out/lecar.csv');
  assert.equal(joinFolder('/home/u/out/', 'lecar.csv'), '/home/u/out/lecar.csv');
  assert.equal(joinFolder('C:\\Users\\u\\out\\', 'lecar.csv'), 'C:\\Users\\u\\out\\lecar.csv');
  assert.equal(folderOf('/home/u/m/lecar.ode'), '/home/u/m');
  assert.equal(folderOf('C:\\m\\lecar.ode'), 'C:\\m');
  assert.equal(folderOf('lecar.ode'), '');
});

test('the name written in the save dialog is the stem of every file: its folder and extension dropped, a bad one refused with the reason', () => {
  assert.deepEqual(typedStem('/home/u/out/fig1'), {stem: 'fig1'});
  assert.deepEqual(typedStem('C:\\out\\fig1.csv'), {stem: 'fig1'});
  assert.deepEqual(typedStem('/o/my.v2.pdf'), {stem: 'my.v2'});
  assert.deepEqual(typedStem('/o/'), {error: 'no name was written'});
  assert.deepEqual(typedStem('/o/con'), {error: '“con” is not a plain file name'});
  assert.equal(restem('lecar.csv', 'lecar', 'fig1'), 'fig1.csv');
  assert.equal(restem('lecar-kinescope-frames.gif', 'lecar', 'fig1'), 'fig1-kinescope-frames.gif');
  assert.equal(restem('lecar-array.pdf', 'lecar', 'a'.repeat(300)), null);
  assert.equal(exportDialogTitle(3), 'Export 3 files');
  assert.equal(exportDialogTitle(1), 'Export 1 file');
});

test('the window opens with nothing ticked and the ticks toggle', () => {
  let s = reduceExportSeveral(initialExportSeveral, {type: 'open', open: true});
  s = reduceExportSeveral(s, {type: 'tick', key: 'plot.export|pdf', on: true});
  s = reduceExportSeveral(s, {type: 'tick', key: 'plot.export|pdf', on: true});
  assert.deepEqual(s.ticks, ['plot.export|pdf']);
  s = reduceExportSeveral(s, {type: 'tick', key: 'plot.export|pdf', on: false});
  assert.deepEqual(s.ticks, []);
  s = reduceExportSeveral(s, {type: 'tick', key: 'data.export|csv', on: true});
  s = reduceExportSeveral(s, {type: 'open', open: false});
  s = reduceExportSeveral(s, {type: 'open', open: true});
  assert.deepEqual(s.ticks, []); /* not remembered between uses */
});

test('a stored row with no format in a fixture the registry lacks is left out, not thrown', () => {
  assert.deepEqual(exportRows([], SEP, names, []), []);
  const out: ExportRow[] = exportRows(rows, SEP, {}, []);
  assert.ok(out.every(r => r.items.length === 0));
});
