/* What a view's place shows while the view is in a window of its own (popout.ts, W645): where it is, and the two things to do
   about it. */
import {viewLabel, type ViewKey} from '../popout';
import {useSession} from './context';

export function PoppedOut({view}: {view: ViewKey}) {
  const session = useSession();
  return (
    <div class="popped-out" role="status">
      <p>“{viewLabel(view)}” is in a window of its own.</p>
      <div class="popped-out-actions">
        <button onClick={() => void session.popouts.show(view)}>Bring it forward</button>
        <button onClick={() => session.popouts.popIn(view)}>Pop in</button>
      </div>
    </div>
  );
}
