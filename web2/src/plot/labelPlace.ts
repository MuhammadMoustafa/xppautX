/* Where the autoX diagram writes its labels' names (docs/ui-v2.md T21):
   beside the cross, moved down or up a line at a time when that spot is
   taken by a name already written, so two labels at one place ("LP 6" and
   "MX 9" at a branch's end) are both readable. Greedy, in the order given.
   Names keep `gap` canvas pixels clear of each other: side by side, and a line
   apart (a moved name steps by its height plus the gap), so "EP 1" and "AH 2"
   at one spot are not run together. Pure. */

export interface LabelBox {
  /** where the name would go (its top left) and its size, in canvas pixels */
  x: number;
  y: number;
  w: number;
  h: number;
}

/* two boxes collide when they are closer than `gap` on both axes (a line a box-height and a gap below is clear) */
const overlaps = (a: LabelBox, b: LabelBox, gap: number) =>
  a.x < b.x + b.w + gap && b.x < a.x + a.w + gap && a.y < b.y + b.h + gap && b.y < a.y + a.h + gap;

/** the top of each name: its own y when free, else the nearest free line (down first), else its own y */
export function placeLabels(boxes: LabelBox[], gap = 0, tries = 6): number[] {
  const placed: LabelBox[] = [];
  return boxes.map(b => {
    let y = b.y;
    for (let k = 0; k <= 2 * tries; k++) {
      const step = k === 0 ? 0 : (k % 2 ? 1 : -1) * Math.ceil(k / 2);
      const c = {...b, y: b.y + step * (b.h + gap)};
      if (!placed.some(p => overlaps(p, c, gap))) {
        y = c.y;
        break;
      }
    }
    placed.push({...b, y});
    return y;
  });
}
