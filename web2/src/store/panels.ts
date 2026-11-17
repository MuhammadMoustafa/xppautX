/* The Values panel (a right column from 48rem, a sheet below that) can be collapsed
   and resized by its inner edge. Kept in settings.json (session.setPage, the page group). */

/** sizes in CSS px; null is the stylesheet's default size */
export interface Panels {
  dockHidden: boolean;
  /** the Values panel as a right column */
  dockWidth: number | null;
}

export const defaultPanels: Panels = {
  dockHidden: false, dockWidth: null,
};

/** the limits of each panel's size (px); a panel is also held to a share of the window by the
    stylesheet (`min(..., 40vw)`), so a narrowed window never leaves the plot without room */
export const PANEL_LIMITS = {
  dockWidth: {min: 288, max: 800, share: 0.5, label: 'Dock width'},
} as const;
export type PanelSize = keyof typeof PANEL_LIMITS;

/** one arrow-key step of a splitter (a quarter of the narrowest menu), px */
export const SPLITTER_STEP = 16;

/** the limits of `size` in a window of `room` px along the panel's axis */
export function panelRange(size: PanelSize, room: number): {min: number; max: number} {
  const l = PANEL_LIMITS[size];
  return {min: l.min, max: Math.max(l.min, Math.min(l.max, Math.floor(room * l.share)))};
}

