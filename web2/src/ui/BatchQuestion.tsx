/* The one replace question of a batch (store/batch.ts, session.ts batchRun): Export several's and a pin's. The files that already exist
   are named once, and the answer is Replace (every file), Skip those (write the others) or Cancel (nothing is sent). */
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';

export function BatchQuestion() {
  const session = useSession();
  const batch = useStore(s => s.batch);
  if (batch.phase !== 'exists') return null;
  const names = batch.exists;
  return (
    <Dialog title={batch.title ?? 'Batch'} name="batch replace" width="form" onClose={() => session.batchAnswer('cancel')} attrs={{'data-batch-question': 'exists'}}
      extra={<>
        <button type="button" data-batch-cancel onClick={() => session.batchAnswer('cancel')}>Cancel</button>
        <button type="button" data-batch-skip onClick={() => session.batchAnswer('skip')}>Skip those</button>
        <button type="button" class="primary" data-batch-replace onClick={() => session.batchAnswer('replace')}>Replace</button>
      </>}>
      <p>{names.length === 1 ? '1 file already exists' : `${names.length} files already exist`} in the folder:</p>
      <ul class="export-exists">{names.map(name => <li key={name}>{name}</li>)}</ul>
      <p>Replace them, skip those and write the rest, or cancel?</p>
    </Dialog>
  );
}
