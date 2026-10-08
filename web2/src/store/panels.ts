/* The side panels of the main screen (W226): the Commands panel on the left and the Values panel
   (a column on the right from 80rem, a row under the work area below that) can each be collapsed
   and resized by dragging their inner edge. Kept per viewer, like the theme (ui/theme.ts). */

/** sizes in CSS px; null is the stylesheet's default size */
export interface Panels {
  menuCollapsed: boolean;
  menuWidth: number | null;
  valuesCollapsed: boolean;
  /** the Values panel as a right column */
  valuesWidth: number | null;
  /** the Values panel as a row under the work area */
  valuesHeight: number | null;
}

export const defaultPanels: Panels = {
  menuCollapsed: false, menuWidth: null, valuesCollapsed: false, valuesWidth: null, valuesHeight: null,
};

/** the limits of each panel's size (px); a panel is also held to a share of the window by the
    stylesheet (`min(..., 40vw)`), so a narrowed window never leaves the plot without room */
export const PANEL_LIMITS = {
  menuWidth: {min: 160, max: 384, share: 0.4, label: 'Commands panel width'},
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

export function savedPanels(): Panels {
  try {
    const v = JSON.parse(localStorage.getItem(KEY) ?? 'null');
    if (v && typeof v.menuCollapsed === 'boolean' && typeof v.valuesCollapsed === 'boolean'
      && isSize(v.menuWidth, 'menuWidth') && isSize(v.valuesWidth, 'valuesWidth') && isSize(v.valuesHeight, 'valuesHeight')) {
      return {menuCollapsed: v.menuCollapsed, menuWidth: v.menuWidth, valuesCollapsed: v.valuesCollapsed,
        valuesWidth: v.valuesWidth, valuesHeight: v.valuesHeight};
    }
  } catch {
    /* storage blocked or not ours: the defaults */
  }
  return defaultPanels;
}

export function savePanels(p: Panels): void {
  try {
    localStorage.setItem(KEY, JSON.stringify(p));
  } catch {
    /* storage blocked: the layout lasts for this page */
  }
}
