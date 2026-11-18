/* The options panels' layout rules (W811, docs/studies/w808-advanced/): which rows of a section are one field (a range pair), and whether a
   section starts folded. Pure: the panels (ui/OptionsPanel.tsx, the command forms, Values' Numerics) draw what these say. */
import type {OptionRow} from '../protocol/types';
import {choiceLabel} from './fieldKinds';

/** a range pair's two rows: the low side first (min or low), the high side after; `label` is the shared part of the two names, the words
    (`min`, `max`, `low`, `high`) the small prefixes inside the two inputs */
export interface RangePair {
  kind: 'pair';
  label: string;
  low: OptionRow;
  high: OptionRow;
  lowWord: string;
  highWord: string;
}
/** a row of a panel, or the range pair of two rows that show as one field */
export type PanelItem = {kind: 'row'; row: OptionRow} | RangePair;

/** the two sides of each range: the word a key's last part ends in, and the side it is (low before high) */
const RANGE_WORDS: Record<string, {family: string; side: 0 | 1}> = {
  min: {family: 'min_max', side: 0}, max: {family: 'min_max', side: 1},
  low: {family: 'low_high', side: 0}, high: {family: 'low_high', side: 1},
};

/** a row's range word, by the one rule: its key's last part is `min`, `max`, `low` or `high`, alone or after an underscore
    (`step.min`, `parameter_min`, `range_low`); the part before the word is the stem ("" or "parameter_") */
function rangeWordOf(key: string): {stem: string; word: string; family: string; side: 0 | 1} | null {
  const last = key.slice(key.lastIndexOf('.') + 1);
  const match = /^(.*?)(min|max|low|high)$/.exec(last);
  if (!match || (match[1] !== '' && !match[1].endsWith('_'))) return null;
  const info = RANGE_WORDS[match[2]];
  return {stem: match[1], word: match[2], ...info};
}

/** the rows of one section as the panel draws them, in the table's order: a low and a high row of one range (same section, same stem,
    same family) become one range pair at the place of the first of them; every other row stays a row */
export function panelItems(rows: OptionRow[]): PanelItem[] {
  const sides = new Map<string, Map<number, OptionRow>>();
  rows.forEach(row => {
    const range = rangeWordOf(row.key);
    if (!range) return;
    const id = `${row.section}\u0000${range.stem}\u0000${range.family}`;
    if (!sides.has(id)) sides.set(id, new Map());
    sides.get(id)!.set(range.side, row);
  });
  const items: PanelItem[] = [];
  const placed = new Set<OptionRow>();
  for (const row of rows) {
    if (placed.has(row)) continue;
    const range = rangeWordOf(row.key);
    const pair = range && sides.get(`${row.section}\u0000${range.stem}\u0000${range.family}`);
    const low = pair?.get(0), high = pair?.get(1);
    if (range && low && high) {
      const stem = range.stem.replace(/_$/, '');
      items.push({kind: 'pair', label: stem ? choiceLabel(stem) : rowSectionLabel(row), low, high,
        lowWord: rangeWordOf(low.key)!.word, highWord: rangeWordOf(high.key)!.word});
      placed.add(low);
      placed.add(high);
    } else {
      items.push({kind: 'row', row});
      placed.add(row);
    }
  }
  return items;
}

/** a section with no word of its own to name a pair (step.min, step.max): the section's title */
function rowSectionLabel(row: OptionRow): string {
  return row.section_label;
}

/** a section starts folded when every one of its rows is advanced, or when none of its rows applies to the method (`applies`: the row is
    used, or the values are not known yet, which counts as used); a section with a basic row that applies starts open */
export function sectionStartsFolded(rows: OptionRow[], applies: (row: OptionRow) => boolean): boolean {
  return rows.length > 0 && (rows.every(row => row.advanced === true) || !rows.some(applies));
}

/** the key the page keeps a section's fold under: the group and the section, so one section is folded the same in every place it is drawn */
export const sectionFoldKey = (group: string, section: string): string => `${group}/${section}`;
