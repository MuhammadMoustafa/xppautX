/* File > Export > Several (W774): the rows of its window and the commands it sends. Every format of an export is one tick, and each
   ticked format is one ordinary registry command (`<command>` with its format and the file), so a script, a recording or the Python
   client writes the same files with the same commands. The file names are hello's output_names (the core's rule); nothing here names
   a file. Pure: the session sends and the store holds the results (session.ts, store/exportSeveral.ts). */
import type {CommandRow} from './protocol/kinds';
import {choiceItems} from './ui/navigation';
import type {Command} from './protocol/types';
import {stepCommand} from './batch';
import {choiceLabel} from './store/fieldKinds';
import {baseName, safeName} from './store/files';
import {unavailableReason} from './ui/navigation';

/** the batch's title (store/batch.ts): what the status bar and Messages call it, and how the window knows the batch is its own */
export const EXPORT_TITLE = 'Export several';

/** the exports the window lists, in its order: a command with a format each (its choice), or the one that has none */
export const EXPORT_ROWS = ['session.export_info', 'plot.export', 'data.export', 'model.export', 'animation.export', 'kinescope.export', 'array_plot.export'];

/** one tick: a format of one export, and the command and arguments that write it (without the file) */
export interface ExportItem {
  /** the tick's key, unique in the window: the command and its format */
  key: string;
  /** the command (its registry name) */
  cmd: string;
  /** the arguments the format and any fixed argument of the menu give */
  args: Record<string, string>;
  /** the format as the window shows it (choiceLabel), the file's own extension for an export with none */
  label: string;
  /** the key of the file's default name in hello's output_names */
  outputKey: string;
  /** the default file name (a base name, the core's rule) */
  file: string;
}

/** one export's row: its label, the formats it can write, and why it cannot run now (null when it can) */
export interface ExportRow {
  id: string;
  label: string;
  reason: string | null;
  items: ExportItem[];
}

/** the window's rows from the registry rows and the session's unavailable commands (state.unavailable), in EXPORT_ROWS' order: a
    row's formats are its menu's items (choiceItems: the one builder of a choice's items), its file names hello's output_names. A row
    the registry does not have, or a format that names no file, is left out */
export function exportRows(rows: CommandRow[], separator: string, outputNames: Record<string, string>, unavailable: {cmd: string; reason: string}[]): ExportRow[] {
  return EXPORT_ROWS.flatMap(id => {
    const row = rows.find(r => r.id === id);
    if (!row) return [];
    const reason = unavailableReason(unavailable, id);
    /* a format is one item of the menu (choiceItems); an export with none is the command with no arguments but its file */
    const formats = row.menu_choice
      ? choiceItems(row, separator).map(item => ({value: item.choice!.value, args: stringArgs(item.choice!.arguments)}))
      : [{value: null, args: {}}];
    const items = formats.flatMap(({value, args}): ExportItem[] => {
      const outputKey = value === null ? id : `${id}.${value}`;
      const file = outputNames[outputKey];
      if (!file) return [];
      return [{key: `${id}|${value ?? ''}`, cmd: id, args, label: value === null ? file.slice(file.lastIndexOf('.') + 1) : choiceLabel(value), outputKey, file}];
    });
    return [{id, label: row.label, reason, items}];
  });
}

/** an item's arguments as the window sends them: its choice and fixed values, without the picker's flag (the file is given) */
const stringArgs = (args: Record<string, string | boolean>): Record<string, string> =>
  Object.fromEntries(Object.entries(args).filter(([name]) => name !== 'file_picker').map(([name, value]) => [name, String(value)]));

/** the command that writes `item` to `file` (a base name in the model's folder, or a full path in the picked folder), with the batch's id
    and the replace answer when there is one (1: replace an existing file; left out: the file is new, or asked about as any save) */
export function exportCommand(item: ExportItem, file: string, id: string, replace?: 1): Command {
  return stepCommand({cmd: item.cmd, ...item.args}, file, id, replace);
}

/** the default file name of `command` as hello's output_names has it (the core's one rule: `<command>.<its choice's value>`, or `<command>` when it
    has no choice), for a file question the core asks with no name of its own; '' when the command has none */
export function defaultFileName(rows: CommandRow[], outputNames: Record<string, string>, command: Command): string {
  const choice = rows.find(r => r.id === command.cmd)?.menu_choice;
  const value = choice ? (command as Record<string, unknown>)[choice] : undefined;
  return outputNames[typeof value === 'string' ? `${command.cmd}.${value}` : command.cmd] ?? '';
}

/** the name the save dialog answered (a path) as the stem of the window's files: its folder and its extension dropped (the core's output_stem
    rule: a typed `fig1.csv` is `fig1`), or why it cannot name files */
export function typedStem(path: string): {stem: string} | {error: string} {
  const base = baseName(path);
  if (!base) return {error: 'no name was written'};
  const dot = base.lastIndexOf('.');
  const stem = dot > 0 ? base.slice(0, dot) : base;
  return safeName(stem) ? {stem} : {error: `“${stem}” is not a plain file name`};
}

/** a default file name (hello's output_names) with `stem` in place of the model's: the one rule's text after the stem is kept (`-curves`, `.csv`);
    null when the result is not a name the core accepts */
export function restem(name: string, modelStem: string, stem: string): string | null {
  const named = stem + name.slice(modelStem.length);
  return name.startsWith(modelStem) && safeName(named) ? named : null;
}

/** the title of the window's save dialog: how many files the one name serves */
export const exportDialogTitle = (count: number): string => `Export ${count} ${count === 1 ? 'file' : 'files'}`;

/** `name` in `folder`: the folder's own separator (a Windows folder has backslashes), the folder with no trailing separator */
export function joinFolder(folder: string, name: string): string {
  const sep = folder.includes('\\') && !folder.includes('/') ? '\\' : '/';
  return folder.replace(/[\\/]+$/, '') + sep + name;
}

/** the folder of a model's path (hello.file), for the save dialog to open in: '' when the path has none */
export function folderOf(path: string): string {
  const cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
  return cut > 0 ? path.slice(0, cut) : '';
}
