/* autoX's info strip and stability circle (docs/ui-v2.md T11b), from the
   `autoxinfo` data: the point the grab's cursor is on in words, and a small
   unit circle with the eigenvalues (as e^λ) or Floquet multipliers of the
   point autoX last drew. A point inside the circle is a filled dot, one
   outside a cross, a cycle's trivial multiplier an open diamond (A7: not
   color alone); each mark's value is its tooltip, and a summary line
   beside the circle counts them.
   autoX computes them from a run's second point on: a first point's
   circle (unless the run restarts from a label of the same kind) says so
   instead of listing zeros. */
import {ShownNumber} from './CutText';
import {circlePoints, infoRows, stabComputed, stabilitySummary} from '../plot/autoxInfo';
import {activeView} from '../store/diagram';
import {useStore} from './context';

const R = 2; /* the circle's view box: -2..2, as XPP's */

function StabilityCircle() {
  const stab = useStore(s => s.diagram.stab);
  if (!stab || !stab.circle.length) return null;
  const pts = circlePoints(stab);
  const summary = stabilitySummary(stab);
  return (
    <figure class="autox-stab">
      <svg viewBox={`${-R} ${-R} ${2 * R} ${2 * R}`} role="img" aria-label={`Stability circle: ${summary}`}>
        <line class="autox-stab-axis" x1={-R} y1={0} x2={R} y2={0} />
        <line class="autox-stab-axis" x1={0} y1={-R} x2={0} y2={R} />
        <circle class="autox-stab-unit" cx={0} cy={0} r={1} />
        {pts.map((p, i) => (
          <g key={i} class="autox-stab-mark">
            <title>{p.text}</title>
            {p.trivial
              ? <path class="autox-stab-trivial" d={`M${p.x},${-p.y - 0.16}L${p.x + 0.16},${-p.y}`
                + `L${p.x},${-p.y + 0.16}L${p.x - 0.16},${-p.y}Z`} />
              : p.inside
                ? <circle class="autox-stab-in" cx={p.x} cy={-p.y} r={0.13} />
                : <path class="autox-stab-out" d={`M${p.x - 0.13},${-p.y - 0.13}L${p.x + 0.13},${-p.y + 0.13}`
                  + `M${p.x - 0.13},${-p.y + 0.13}L${p.x + 0.13},${-p.y - 0.13}`} />}
          </g>))}
      </svg>
      <figcaption>
        {stabComputed(stab) ? (
          <p class="autox-stab-summary">{summary}</p>
        ) : (
          <>
            <span class="muted">{stab.periodic ? 'Multipliers' : 'Eigenvalues'}</span>
            <p class="muted autox-stab-none">Not computed at this point: autoX computes them from the second point of a branch on.</p>
          </>
        )}
      </figcaption>
    </figure>
  );
}

export function AutoxInfo() {
  const info = useStore(s => s.diagram.info);
  const stab = useStore(s => s.diagram.stab);
  const axes = useStore(s => activeView(s.diagram).axes);
  if (!info && !stab) return null;
  return (
    <aside class="autox-info" aria-label="Point information">
      {info ? (
        <dl class="autox-info-rows">
          {infoRows(info, axes).map(([k, v]) => (
            <div key={k}><dt>{k}</dt><dd>{typeof v === 'number' ? <ShownNumber value={v} /> : v}</dd></div>
          ))}
        </dl>
      ) : <p class="muted autox-info-rows">Grab shows a point's branch, label and values here.</p>}
      <StabilityCircle />
    </aside>
  );
}
