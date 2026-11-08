/* An axis title drawn by uPlot (W646): a title wider than its side of the plotting area is cut with … so the
   canvas text fits that side, and the overlay button over it (ui/PlotView.tsx's AxisTitle) shows the whole title.
   The axes of the phase plot (chart.ts) and of the autoX diagram (diagramChart.ts) are built here, once. */
import uPlot from 'uplot';
import {cssVar} from './css';

/** the mark that stands for the text a title lost (the same … as a cut number or name, store/values.ts and CutText.tsx) */
const CUT_MARK = '…';

/** the title as it is drawn in a side `max` long: the title whole when `width` (its length in the drawing's font)
    says it fits, else its longest start followed by … that fits (the longest such start, found by bisection:
    the width of a prefix only grows with its length) */
export function fitTitle(title: string, max: number, width: (text: string) => number): string {
  if (width(title) <= max) return title;
  let lo = 0, hi = title.length;
  while (lo < hi) {
    const mid = Math.ceil((lo + hi) / 2);
    if (width(title.slice(0, mid) + CUT_MARK) <= max) lo = mid;
    else hi = mid - 1;
  }
  return title.slice(0, lo) + CUT_MARK;
}

/** an axis of a plot with its title (the axis at index 0 is the horizontal one): the title is measured at each
    draw, so it follows the plotting area's size, and the canvas keeps the cut title (the frame, GIF and kinescope
    captures take its pixels) */
export function plotAxis(title: string): uPlot.Axis {
  /* the theme's colours when drawn: a theme switch sets the variables after the chart is made */
  const fg = () => cssVar('--fg-muted'), grid = () => cssVar('--grid'), font = cssVar('--plot-font');
  return {
    label: (self, axisIdx) => {
      /* uPlot sets the label's font before it calls this, and bbox is in device pixels as the font is */
      const side = axisIdx === 0 ? self.bbox.width : self.bbox.height;
      return fitTitle(title, side, text => self.ctx.measureText(text).width);
    },
    stroke: fg, font, labelFont: font, grid: {stroke: grid, width: 1}, ticks: {stroke: grid, width: 1},
  };
}
