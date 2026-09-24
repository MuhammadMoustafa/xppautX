/* The small "?" every menu and dialog gets (docs/roadmap.md W12b step 3):
   opens the Help view (Help.tsx) at the section docs/manual/README.md's
   map names for it (help/links.ts). */
import type {HelpTarget} from '../help/links';
import {useSession} from './context';

export function HelpButton({target, label}: {target: HelpTarget; label?: string}) {
  const session = useSession();
  const title = label ? `Help: ${label}` : 'Help for this';
  return (
    <button type="button" class="help-link icon-button" aria-label={title} title={title}
      onClick={() => session.store.dispatch({type: 'help', action: {type: 'open', target}})}>
      ?
    </button>
  );
}
