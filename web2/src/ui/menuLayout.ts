/* A long menu in the prompt dialog goes in columns so it fits a laptop's
   screen without scrolling (docs/ui-v2.md T21; one column on a phone, by
   CSS). Pure. */

/** menus up to this many items stay one column */
export const MENU_ONE_COLUMN = 8;

/** rows per column: columns of at most MENU_ONE_COLUMN items, filled evenly */
export function menuRows(n: number): number {
  const cols = Math.ceil(n / MENU_ONE_COLUMN);
  return Math.max(1, Math.ceil(n / Math.max(1, cols)));
}
