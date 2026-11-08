import {choiceLabel} from '../store/fieldKinds';
import type {ValuesFileOffer, ValuesPart} from '../store/state';
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';

/** what a part of a file of values is called: the file's defaults, one of its sets, or the whole XPPAUT file */
function partLabel(part: ValuesPart, format: string): string {
  if (part.from === 'defaults') return 'Defaults (its par and init lines and settings)';
  if (part.from === 'set') return `Set “${part.name}”`;
  return `The whole .${format} file`;
}

/** Load values from a file by scope (W321): the parts of the picked file that hold the section's kind, as the core's
    `set.file_contents` listed them, one to load with `set.load_file`; a part that cannot be loaded says why */
export function ValuesFileDialog() {
  const offer = useStore(s => s.valuesFile);
  return offer ? <Offer offer={offer} /> : null;
}

function Offer({offer}: {offer: ValuesFileOffer}) {
  const session = useSession();
  const close = () => session.store.dispatch({type: 'valuesFile', offer: null});
  const what = choiceLabel(offer.scope);
  const title = `Load ${what.toLowerCase()} from ${offer.file}`;
  return (
    <Dialog title={title} name={`Load ${what} from a file`} description="Choose the part of the file to load; it replaces the current values it holds." onClose={close}
      attrs={{'data-values-file-dialog': offer.scope}}>
      {offer.parts.length === 0
        ? <p>The file has nothing of this kind to load.</p>
        : <ul class="values-parts">{offer.parts.map(part => (
          <li key={part.from + ':' + part.name}>
            <button type="button" data-values-part={`${part.from}:${part.name}`} disabled={!!part.problem}
              onClick={() => session.loadValuesPart(offer, part)}>{partLabel(part, offer.format)}</button>
            {part.problem && <span class="field-error">{part.problem}</span>}
          </li>))}</ul>}
    </Dialog>
  );
}
