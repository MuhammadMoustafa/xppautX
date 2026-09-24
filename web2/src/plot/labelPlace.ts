/* Where the AUTO diagram writes its labels' names (docs/ui-v2.md T21):
   beside the cross, moved down or up a line at a time when that spot is
   taken by a name already written, so two labels at one place ("LP 6" and
   "MX 9" at a branch's end) are both readable. Greedy, in the order given.
   Pure. */

export interface LabelBox {
  /** where the name would go (its top left) and its size, in canvas pixels */
  x: number;
  y: number;
  w: number;
  h: number;
}

const overlaps = (a: LabelBox, b: LabelBox) => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;

/** the top of each name: its own y when free, else the nearest free line (down first), else its own y */
export function placeLabels(boxes: LabelBox[], tries = 6): number[] {
  const placed: LabelBox[] = [];
  return boxes.map(b => {
    let y = b.y;
    for (let k = 0; k <= 2 * tries; k++) {
      const step = k === 0 ? 0 : (k % 2 ? 1 : -1) * Math.ceil(k / 2);
      const c = {...b, y: b.y + step * b.h};
      if (!placed.some(p => overlaps(p, c))) {
        y = c.y;
        break;
      }
    }
    placed.push({...b, y});
    return y;
  });
}
