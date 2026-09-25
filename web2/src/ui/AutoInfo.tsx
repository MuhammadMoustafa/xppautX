/* AUTO's info strip and stability circle (docs/ui-v2.md T11b), from the
   `autoinfo` data: the point the grab's cursor is on in words, and a small
   unit circle with the eigenvalues (as e^λ) or Floquet multipliers of the
   point AUTO last drew. A point inside the circle is a filled dot, one
   outside a cross (A7: not colour alone), and the values are listed too.
   AUTO computes them from a run's second point on: a first point's
   circle (unless the run restarts from a label of the same kind) says so
   instead of listing zeros. */
import {circlePoints, infoRows, stabComputed, stabilitySummary} from '../plot/autoInfo';
import {useStore} from './context';

const R = 2; /* the circle's view box: -2..2, as XPP's */

function StabilityCircle() {
  const stab = useStore(s => s.diagram.stab);
  if (!stab || !stab.circle.length) return null;
  const pts = circlePoints(stab);
  const summary = stabilitySummary(stab);
  return (
    <figure class="auto-stab">
      <svg viewBox={`${-R} ${-R} ${2 * R} ${2 * R}`} role="img" aria-label={`Stability circle: ${summary}`}>
        <line class="auto-stab-axis" x1={-R} y1={0} x2={R} y2={0} />
        <line class="auto-stab-axis" x1={0} y1={-R} x2={0} y2={R} />
        <circle class="auto-stab-unit" cx={0} cy={0} r={1} />
        {pts.map((p, i) => (p.inside
          ? <circle key={i} class="auto-stab-in" cx={p.x} cy={-p.y} r={0.13} />
          : <path key={i} class="auto-stab-out" d={`M${p.x - 0.13},${-p.y - 0.13}L${p.x + 0.13},${-p.y + 0.13}`
            + `M${p.x - 0.13},${-p.y + 0.13}L${p.x + 0.13},${-p.y - 0.13}`} />))}
      </svg>
      <figcaption>
        <span class="muted">{stab.periodic ? 'Multipliers' : 'Eigenvalues'}</span>
        {stabComputed(stab) ? (
          <ul class="auto-stab-list">
            {pts.map((p, i) => <li key={i}>{p.inside ? '●' : '×'} {p.text}</li>)}
          </ul>
        ) : (
          <p class="muted auto-stab-none">Not computed at this point: AUTO computes them from the second point of a branch on.</p>
        )}
      </figcaption>
    </figure>
  );
}

export function AutoInfo() {
  const info = useStore(s => s.diagram.info);
  const stab = useStore(s => s.diagram.stab);
  const axes = useStore(s => s.diagram.axes);
  if (!info && !stab) return null;
  return (
    <aside class="auto-info" aria-label="Point information">
      {info ? (
        <dl class="auto-info-rows">
          {infoRows(info, axes).map(([k, v]) => (
            <div key={k}><dt>{k}</dt><dd>{v}</dd></div>
          ))}
        </dl>
      ) : <p class="muted auto-info-rows">Grab shows a point's branch, label and values here.</p>}
      <StabilityCircle />
    </aside>
  );
}
