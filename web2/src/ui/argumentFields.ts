import {autoSettingsCommands, shownSettings} from '../store/autoSettings';
import {autoWindow} from '../store/diagram';
import {annotationRows} from '../store/marks';
import type {RegistryArgument, CommandRow} from '../protocol/kinds';
import type {AppState} from '../store/state';
import {specOfKind, TEXT, fieldError, type FieldSpec} from '../store/fieldKinds';

/** Registry names use the same vocabulary as choices, files and the protocol. */

export function argumentSpec(arg: RegistryArgument): FieldSpec {
  return ['text', 'integer', 'real', 'boolean', 'choice', 'expression', 'file', 'color'].includes(arg.type)
    ? specOfKind(arg.type, [], {options: arg.choices, range: {min: arg.min, max: arg.max}}) : TEXT;
}
export function argumentNames(arg: RegistryArgument, state: AppState): string[] {
  if (arg.type === 'window') return state.windows.map(w => w.name);
  if (arg.names === 'any') return state.hello?.model_names ?? [];
  if (!arg.names || arg.names === 'none') return [];
  const lists = state.hello?.lists ?? [];
  return lists[arg.names === 'parameter' ? 2 : arg.names === 'variable' ? 1 : arg.names === 'either' ? 3 : 0] ?? [];
}
export function currentArguments(row: CommandRow, state: AppState): Record<string, unknown> {
  const out: Record<string, unknown> = {};
  const graph = state.plots.windows.find(w => w.win === state.plots.active);
  const annotation = row.id.startsWith('annotation.') ? annotationRows(graph?.marks ?? null).find(a => a.id === state.selectedAnnotation) : undefined;
  for (const arg of row.arguments ?? []) {
    const numeric = state.numerics?.find(field => field.key === arg.name || field.key === `${row.id.split('.')[0]}_${arg.name}`);
    if (numeric && ['run', 'bvp'].includes(row.id.split('.')[0])) out[arg.name] = numeric.value;
    const auto = shownSettings(state.autoSettings);
    if (row.id === 'auto.run') {
      if (arg.name === 'label' && (state.diagram.start?.label ?? 0) > 0) out[arg.name] = state.diagram.start!.label;
      if (arg.name === 'replace_diagram' && !state.diagram.start && state.diagram.views.some(v => v.points.x.length > 1)) out[arg.name] = false;
    }
    if (auto && row.id === 'auto.configure_marks') {
      if (arg.name === 'parameters') out[arg.name] = auto.marks.map(mark => mark[0]);
      if (arg.name === 'values') out[arg.name] = auto.marks.map(mark => mark[1]);
    }
    if (auto && row.id === 'auto.set_threads' && arg.name === 'automatic' && auto.threads === 'auto') out[arg.name] = true;

    if (auto && row.id === 'auto.configure_numerics' && arg.name in auto.numerics) out[arg.name] = auto.numerics[arg.name as keyof typeof auto.numerics];
    if (auto) {
      const command = autoSettingsCommands({axes: auto.axes}).find(c => c.command.cmd === row.id)?.command;
      if (command && arg.name in command) out[arg.name] = command[arg.name];
    }
    if (auto && row.id === 'auto.set_threads' && auto.threads !== 'auto') out[arg.name] = auto.threads;
    if (arg.type === 'window') {
      const name = row.id.startsWith('auto.') ? autoWindow(state) : state.plots.active;
      if (state.windows.some(window => window.name === name)) out[arg.name] = name;
    }
    if (annotation && arg.name in annotation.values) out[arg.name] = annotation.values[arg.name];
    if (annotation && row.id === 'annotation.move') {
      if (arg.name === 'new_x') out[arg.name] = annotation.x;
      if (arg.name === 'new_y') out[arg.name] = annotation.y;
    }
    const info = graph?.info;
    if (info && (row.id.startsWith('window.') || row.id === 'plot.set_2d' || row.id === 'plot.set_3d')) {
      const values: Record<string, number> = row.id === 'plot.set_3d' ? {x_min:info.box.xmin,x_max:info.box.xmax,y_min:info.box.ymin,y_max:info.box.ymax,z_min:info.box.zmin,z_max:info.box.zmax} : {x_min: info.xlo, x_max: info.xhi, y_min: info.ylo, y_max: info.yhi};
      if (arg.name in values) out[arg.name] = values[arg.name];
      if (arg.name === 'x_label' && graph?.series) out[arg.name] = graph.series.labels.x;
      if (arg.name === 'y_label' && graph?.series) out[arg.name] = graph.series.labels.y;
      if (arg.name === 'z_label' && graph?.series) out[arg.name] = graph.series.labels.z;
    }
    if (arg.type === 'map' && arg.name === 'values') {
      if (row.id === 'value.set_parameters') out[arg.name] = Object.fromEntries(state.core?.pars ?? []);
      if (row.id === 'ic.set') out[arg.name] = Object.fromEntries(state.core?.ics ?? []);
    }
  }
  return out;
}
export function argumentValue(arg: RegistryArgument, text: string): unknown {
  if (arg.type === 'boolean') return text === 'true';

  return text;
}
export function argumentProblem(arg: RegistryArgument, value: unknown): string | null {
  if (value === undefined) return arg.required ? 'Required' : null;
  if (['name','window','new_name'].includes(arg.type) && value === '') return 'Choose or enter a name';
  if (arg.type === 'list' || arg.type === 'map') {
    const entries = arg.type === 'list' ? value as unknown[] : Object.values(value as Record<string, unknown>);
    if (entries.length > (arg.max_items ?? Infinity)) return 'Too many rows';
    return entries.map(entry => argumentProblem({...arg, type: arg.element ?? 'text', required: true}, entry)).find(Boolean) ?? null;
  }
  if (arg.type === 'list[real, 2]') return Array.isArray(value) && value.length === 2 && value.every(v => String(v).trim() !== '' && Number.isFinite(Number(v))) ? null : 'Enter two finite numbers';
  if (arg.type === 'list[text]') return Array.isArray(value) && value.every(v => typeof v === 'string') ? null : 'Enter text rows';
  if (typeof value === 'string' && new TextEncoder().encode(value).length > (arg.max_length ?? Infinity)) return 'Too long';
  return fieldError(argumentSpec(arg), String(value));
}

/** Convert only on submission; numeric drafts such as '-' stay editable. */
export function commandValues(args: RegistryArgument[], values: Record<string, unknown>): Record<string, unknown> {
  const convert = (arg: RegistryArgument, value: unknown): unknown => {
    if (value === undefined) return undefined;
    if (arg.type === 'integer' || arg.type === 'real') return Number(value);
    if (arg.type === 'list[real, 2]') return (value as unknown[]).map(Number);
    if (arg.type === 'list') return (value as unknown[]).map(v => convert({...arg, type: arg.element ?? 'text'}, v));
    if (arg.type === 'map') return Object.fromEntries(Object.entries(value as Record<string, unknown>).map(([k, v]) => [k, convert({...arg, type: arg.element ?? 'text'}, v)]));
    return value;
  };
  return Object.fromEntries(args.filter(a => values[a.name] !== undefined).map(a => [a.name, convert(a, values[a.name])]));
}

/** Cross-field requirements come from the row, never from a second list of commands. */
export function groupProblems(row: CommandRow, values: Record<string, unknown>): string[] {
  return (row.groups ?? []).flatMap(group => {
    const count = group.arguments.filter(name => values[name] !== undefined).length;
    const bad = group.rule === 'exactly_one' ? count !== 1 : group.rule === 'at_most_one' ? count > 1 : count !== 0 && count !== group.arguments.length;
    return bad ? [`${group.rule.replaceAll('_',' ')}: ${group.arguments.join(', ')}`] : [];
  });
}

/** The wire vocabulary is command_registry.cpp's type_name, including the two fixed list shapes. */
export function argumentControl(arg: RegistryArgument): string {
  switch (arg.type) {
    case 'boolean': return 'checkbox';
    case 'color': return 'color';
    case 'choice': case 'window': case 'name': return 'select';
    case 'list': case 'map': case 'list[real, 2]': case 'list[text]': return 'rows';
    case 'file': return 'file';
    case 'path': return 'path';
    case 'text': case 'integer': case 'real': case 'expression': case 'new_name': return 'field';
    default: throw new Error(`Unknown registry argument type ${arg.type}`);
  }
}

/** An empty draft preserves the shape its widget edits; it is not a session default. */
export function blankArgument(arg: RegistryArgument): unknown {
  if (arg.type === 'boolean') return false;
  if (arg.type === 'map') return {};
  if (arg.type === 'list' || arg.type === 'list[text]') return [];
  if (arg.type === 'list[real, 2]') return ['', ''];
  return '';
}
