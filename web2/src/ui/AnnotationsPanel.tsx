import {useSession, useStore} from './context';
import {annotationRows} from '../store/marks';

export function AnnotationsPanel() {
  const session = useSession();
  const {annotationsOpen, selectedAnnotation, plots} = useStore(s => s);
  if (!annotationsOpen) return null;
  const rows = annotationRows(plots.windows.find(w => w.win === plots.active)?.marks ?? null);
  return <aside class="annotations-panel" aria-label="Annotations" data-annotations-panel>
    <h2>Annotations — {plots.active}</h2><button onClick={() => session.store.dispatch({type: 'annotations', open: false})}>Close</button>
    <ul>{rows.map(row => <li key={row.id} data-annotation={row.id} aria-selected={selectedAnnotation === row.id}>
      <button onClick={() => session.store.dispatch({type: 'annotations', open: true, selected: row.id})}>{row.kind}: {row.text} ({row.x}, {row.y}) {row.color}</button>
      <button data-annotation-edit={row.id} onClick={() => session.registryAction('annotation.edit', row.values)}>Edit</button>
      <button data-annotation-delete={row.id} onClick={() => session.send({cmd: 'annotation.delete', annotation_id: row.id})}>Delete</button>
    </li>)}</ul>{!rows.length && <p>No annotations.</p>}
  </aside>;
}
