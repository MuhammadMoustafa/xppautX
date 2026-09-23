/* Text views (docs/ui-v2.md T16): the model's equations, its source with
   comment actions, and the last Sing pts equilibrium, in one panel with a
   tab strip (R6: a side panel from 48rem, a full-screen sheet with a Back
   button under that, like ui/TableView.tsx). Opened by the title bar's Text
   button or a tab's own toggle; each tab asks the core to (re)send its data
   when picked (session.ts's openText/selectTextTab), since the model, the
   source or the initial conditions may have changed since it was last
   shown. Equations and Source are read-only monospace text, wrapped so a
   long line never causes sideways scroll (R1); Source adds a button on
   every line that is a comment with a `{par=value,...}` action
   (docs/protocol.md `action`, `source.comments`) -- choosing one runs it,
   exactly as the classic page's button does (web/xpp-client.js
   showSource). Equilibrium shows the last Sing pts result (docs/protocol.md
   `equilibrium`): its type, eigenvalue counts, and values, six significant
   digits (A14) with the full value on hover/title; Import
   (docs/protocol.md `eqimport`) makes it the initial conditions. */
import {useEffect, useLayoutEffect, useReducer, useRef} from 'preact/hooks';
import type {Session} from '../session';
import type {AppState} from '../store/state';
import {sixSig} from '../store/values';
import type {SourceLine, TextTab} from '../store/text';
import {useSession, useStore} from './context';

const FOCUSABLE = 'button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';

/** like ui/context.ts's useStore, but subscribes with useLayoutEffect
    (synchronous, right after this render commits) instead of useEffect
    (deferred): a tab's data here comes from a command sent the instant it
    mounts (session.ts refreshText), a local round trip fast enough that the
    response can arrive and be dispatched before a *deferred* effect has
    subscribed -- missing the one notification that would show it, and
    nothing later nudges it (found by web2check.mjs: the view stayed on "no
    data yet" until an unrelated command forced another render). Scoped to
    this file rather than fixed in context.ts to avoid touching shared code
    other in-flight work also depends on. */
function useLiveStore<T>(select: (s: AppState) => T): T {
  const session = useSession();
  const [, force] = useReducer((n: number) => n + 1, 0);
  const selected = select(session.store.getState());
  const last = useRef(selected), selector = useRef(select);
  last.current = selected;
  selector.current = select;
  useLayoutEffect(() => session.store.subscribe(() => {
    if (!Object.is(selector.current(session.store.getState()), last.current)) force(0);
  }), [session]);
  return selected;
}

const TABS: {id: TextTab; label: string}[] = [
  {id: 'equations', label: 'Equations'},
  {id: 'source', label: 'Source'},
  {id: 'equilibrium', label: 'Equilibrium'},
];

function EquationsView() {
  const lines = useLiveStore(s => s.text.equations);
  if (!lines) return <p class="text-empty">No equations yet.</p>;
  if (!lines.length) return <p class="text-empty">The model has no equations.</p>;
  return <pre class="text-equations" tabIndex={0} aria-label="The model's equations">{lines.join('\n')}</pre>;
}

function SourceLineRow({line, session}: {line: SourceLine; session: Session}) {
  return (
    <div class="source-line">
      <span class="source-text">{line.text || ' '}</span>
      {line.comment?.hasAction && (
        <button class="source-action" onClick={() => session.runAction(line.comment!.index)}
          title="A comment action: sets the parameters it names">
          {line.comment.text.trim() || `Action ${line.comment.index + 1}`}
        </button>
      )}
    </div>
  );
}

function SourceView() {
  const session = useSession();
  const source = useLiveStore(s => s.text.source);
  if (!source) return <p class="text-empty">No source yet.</p>;
  const actionCount = source.comments.filter(c => c.hasAction).length;
  return (
    <div class="text-source">
      {actionCount > 0 && (
        <p class="text-hint" role="status">
          {actionCount} action{actionCount > 1 ? 's' : ''} on the lines marked with a button below.
        </p>
      )}
      <div class="source-lines" aria-label="The model's source, with line numbers">
        {source.lines.map((line, i) => (
          <div class="source-row" key={i}>
            <span class="source-lineno" aria-hidden="true">{i + 1}</span>
            <SourceLineRow line={line} session={session} />
          </div>
        ))}
      </div>
    </div>
  );
}

/** the stability class (docs/protocol.md `equilibrium` `type`: core/xpp_util.c eq_stability) */
function stabilityClass(type: string): string {
  const t = type.toLowerCase();
  return t === 'stable' ? 'eq-stable' : t === 'unstable' ? 'eq-unstable' : 'eq-neutral';
}

function EquilibriumView() {
  const session = useSession();
  const eq = useLiveStore(s => s.text.equilibrium);
  return (
    <div class="text-equilibrium">
      <div class="text-tools">
        <button onClick={() => session.findEquilibrium()}
          title="Sing pts / Go: find the equilibrium closest to the current initial conditions">
          Find equilibrium
        </button>
        <button onClick={() => session.importEquilibrium()} disabled={!eq}
          title="Make this equilibrium the initial conditions">
          Import
        </button>
      </div>
      {!eq ? (
        <p class="text-empty">No equilibrium yet: choose Find equilibrium, or Sing pts from the menu.</p>
      ) : (
        <>
          <p class={'eq-type ' + stabilityClass(eq.type)} role="status">{eq.type}</p>
          <table class="eq-counts">
            <caption class="visually-hidden">Eigenvalue counts: complex/real with positive/negative real part, purely imaginary</caption>
            <tbody>
              <tr><th scope="row">c+</th><td>{eq.cplus}</td><th scope="row">c-</th><td>{eq.cminus}</td></tr>
              <tr><th scope="row">r+</th><td>{eq.rplus}</td><th scope="row">r-</th><td>{eq.rminus}</td></tr>
              <tr><th scope="row">im</th><td>{eq.im}</td><td></td><td></td></tr>
            </tbody>
          </table>
          <table class="eq-values">
            <caption>Values</caption>
            <thead><tr><th scope="col">Name</th><th scope="col">Value</th></tr></thead>
            <tbody>
              {eq.values.map(([name, v]) => (
                <tr key={name}><td>{name}</td><td title={String(v)}>{sixSig(v)}</td></tr>
              ))}
            </tbody>
          </table>
          <table class="eq-values">
            <caption>Eigenvalues</caption>
            <thead><tr><th scope="col">Real</th><th scope="col">Imaginary</th></tr></thead>
            <tbody>
              <tr><td colSpan={2} class="text-hint">
                Not sent by the server yet (the core computes them but the `equilibrium` event does not carry them).
              </td></tr>
            </tbody>
          </table>
        </>
      )}
    </div>
  );
}

/* ---- the panel: a side panel from 48rem, a full-screen sheet under that (R6) ---- */

export function TextViews() {
  const session = useSession();
  const open = useStore(s => s.text.open);
  const tab = useStore(s => s.text.tab);
  const panel = useRef<HTMLElement>(null);
  const close = () => session.closeText();

  useEffect(() => {
    if (!open) {
      if (panel.current?.contains(document.activeElement)) document.querySelector<HTMLElement>('.text-toggle')?.focus();
      return;
    }
    panel.current?.querySelector<HTMLElement>(FOCUSABLE)?.focus();
    /* Escape closes the sheet wherever the focus is inside it (narrow only: CSS keeps it open elsewhere) */
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || session.store.getState().ask) return;
      e.preventDefault();
      e.stopPropagation();
      close();
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open]);

  return (
    <section id="text-panel" ref={panel} class={'text-panel' + (open ? ' open' : '')} aria-label="Text views">
      <div class="text-header">
        <button class="text-back" onClick={close}>Back</button>
        <h2>Text</h2>
      </div>
      <div class="text-tabs" role="group" aria-label="Which text view to show">
        {TABS.map(t => (
          <button key={t.id} aria-pressed={tab === t.id} class={'text-tab' + (tab === t.id ? ' active' : '')}
            onClick={() => session.selectTextTab(t.id)}>
            {t.label}
          </button>
        ))}
      </div>
      <div class="text-body" aria-label={`${TABS.find(t => t.id === tab)!.label} view`}>
        {tab === 'equations' && <EquationsView />}
        {tab === 'source' && <SourceView />}
        {tab === 'equilibrium' && <EquilibriumView />}
      </div>
    </section>
  );
}
