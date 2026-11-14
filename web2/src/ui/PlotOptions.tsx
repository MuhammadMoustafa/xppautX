/* The collapsed "Window options" of a plot's bar (closed by default): a disclosure holding the plot's less used controls, the graph's in
   ui/Plots.tsx and, in a group of the split view (W720), autoX's in ui/AutoxView.tsx. */
import type {ComponentChildren} from 'preact';

export function PlotOptions({label = 'Window options', children}: {label?: string; children: ComponentChildren}) {
  return (
    <details class="plot-window-options">
      <summary>{label}</summary>
      <div class="plot-window-options-menu">{children}</div>
    </details>
  );
}
