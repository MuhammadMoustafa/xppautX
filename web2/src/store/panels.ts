/* The Values panel (a column from 80rem, a row below that) can be collapsed
   and resized by its inner edge. Kept per viewer (store/prefs.ts). */
import {readPref, writePref} from './prefs';

/** sizes in CSS px; null is the stylesheet's default size */
export interface Panels {
  valuesCollapsed: boolean;
  /** the Values panel as a right column */
  valuesWidth: number | null;
  /** the Values panel as a row under the work area */
  valuesHeight: number | null;
}

export const defaultPanels: Panels = {
  valuesCollapsed: false, valuesWidth: null, valuesHeight: null,
};

/** the limits of each panel's size (px); a panel is also held to a share of the window by the
    stylesheet (`min(..., 40vw)`), so a narrowed window never leaves the plot without room */
export const PANEL_LIMITS = {
  valuesWidth: {min: 288, max: 800, share: 0.5, label: 'Values panel width'},
  valuesHeight: {min: 96, max: 480, share: 0.5, label: 'Values panel height'},
} as const;
export type PanelSize = keyof typeof PANEL_LIMITS;

/** one arrow-key step of a splitter (a quarter of the narrowest menu), px */
export const SPLITTER_STEP = 16;

const KEY = 'xppPanels';

/** the limits of `size` in a window of `room` px along the panel's axis */
export function panelRange(size: PanelSize, room: number): {min: number; max: number} {
  const l = PANEL_LIMITS[size];
  return {min: l.min, max: Math.max(l.min, Math.min(l.max, Math.floor(room * l.share)))};
}

const isSize = (v: unknown, size: PanelSize): v is number | null =>
  v === null || (typeof v === 'number' && v >= PANEL_LIMITS[size].min && v <= PANEL_LIMITS[size].max);

const parsePanels = (v: any): Panels | null =>
  v && typeof v.valuesCollapsed === 'boolean'
    && isSize(v.valuesWidth, 'valuesWidth') && isSize(v.valuesHeight, 'valuesHeight')
    ? {valuesCollapsed: v.valuesCollapsed,
      valuesWidth: v.valuesWidth, valuesHeight: v.valuesHeight}
    : null;

export const savedPanels = (): Panels => readPref(KEY, parsePanels, defaultPanels);
export const savePanels = (p: Panels): void => writePref(KEY, p);
