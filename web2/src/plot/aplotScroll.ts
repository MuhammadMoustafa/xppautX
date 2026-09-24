/* The array plot's time scroll (docs/ui-v2.md T12, docs/protocol.md `aplot`
   `scroll`): the classic page (web/xpp-client.js buildArrayPlot) sends a
   drag's raw pointer-Y delta as `dy`, which the core subtracts straight
   from the first row shown (core/ui_json.cpp aplot_command: "aplot.nstart -=
   dy") -- not a real pixel-to-row conversion, just the number of rows to
   shift. This view keeps that convention for a drag, and picks comparable
   `dy` amounts for a wheel notch and the keyboard, so all three feel the
   same. Positive `dy` moves toward earlier rows (dragging down, ArrowUp,
   PageUp); negative moves toward later ones (dragging up, wheel down,
   ArrowDown, PageDown) -- the same direction a dragged pointer's Y and the
   resulting `dy` already agree on. Pure: no DOM, no timers; session.ts owns
   the queue (it only sends while the core is idle). */

/** a wheel notch's `dy`: scrolling down (positive deltaY) moves forward in
    time, like ArrowDown/PageDown */
export function wheelScroll(deltaY: number): number {
  return -deltaY || 0; /* -0 (deltaY 0) back to plain 0 */
}

/** a pointer dragged from `fromY` to `toY` (client pixels): the same sign
    the classic page's mouse drag sends */
export function dragScroll(fromY: number, toY: number): number {
  return toY - fromY;
}

/** ArrowUp/ArrowDown/PageUp/PageDown as a `dy`, a page being `ny` rows (the
    grid's own height, so a page always shows fresh rows); null for a key
    this view does not use */
export function keyScroll(key: string, ny: number): number | null {
  const page = Math.max(1, ny);
  switch (key) {
    case 'ArrowUp': return 1;
    case 'ArrowDown': return -1;
    case 'PageUp': return page;
    case 'PageDown': return -page;
    default: return null;
  }
}

/** several scroll gestures made while the core is busy collapse into one
    `aplot` `scroll`: accumulates `delta` onto `pending`, and says how much
    to send now (0 while busy) and what stays queued */
export function accumulateScroll(pending: number, delta: number, busy: boolean): {send: number; pending: number} {
  const total = pending + delta;
  return busy ? {send: 0, pending: total} : {send: total, pending: 0};
}
