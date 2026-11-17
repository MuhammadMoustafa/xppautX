/* File > Export > Several (W774, docs/ui-v2.md): one window listing every export, each format of it a tick (a chip), one folder
   picked for the files, and the one replace question for the files that exist. Every tick is one ordinary export command when Export
   runs (session.ts exportSeveralStart); the window shows the batch's outcome after its last command, the core's reason for each file
   that failed. A row whose export cannot run now is greyed with the reason the core gives (state.unavailable). */
import {canPickFolder} from '../pickers';
import {exportRows, type ExportRow} from '../exportSeveral';
import {registryRows} from './navigation';
import {useSession, useStore} from './context';
import {Dialog} from './Dialog';
import {choiceLabel} from '../store/fieldKinds';
import type {BatchOutcome} from '../store/batch';
import {EXPORT_TITLE} from '../exportSeveral';

export function ExportSeveral() {
  const open = useStore(s => s.exportSeveral.open);
  return open ? <Window /> : null;
}

function Window() {
  const session = useSession();
  const st = useStore(s => s.exportSeveral);
  const batch = useStore(s => s.batch);
  const hello = useStore(s => s.hello);
  const unavailable = useStore(s => s.core?.unavailable);
  const rows = exportRows(registryRows(hello), hello?.menu_separator ?? '', hello?.output_names ?? {}, unavailable ?? []);
  const close = () => session.exportSeveralClose();
  /* the batch is the runner's (store/batch.ts): its replace question is BatchQuestion's, its report and progress are shown here */
  if (batch.title === EXPORT_TITLE) {
    if (batch.phase === 'running') return <Running total={batch.steps.length} done={batch.outcomes.length} />;
    if (batch.phase === 'report') return <Report outcomes={batch.outcomes} onClose={close} />;
    if (batch.phase === 'exists') return null;
  }
  return <Choose rows={rows} ticks={st.ticks} folder={st.folder} onClose={close} />;
}

/** the items of the rows the ticks name, in the window's order, when they can run */
function ticked(rows: ExportRow[], ticks: string[]) {
  return rows.filter(row => !row.reason).flatMap(row => row.items).filter(item => ticks.includes(item.key));
}

function Choose({rows, ticks, folder, onClose}: {rows: ExportRow[]; ticks: string[]; folder: string | null; onClose(): void}) {
  const session = useSession();
  const picks = canPickFolder();
  const files = ticked(rows, ticks);
  const ready = files.length > 0 && (!picks || !!folder);
  return (
    <Dialog title="Export several" name="export several" width="list" onClose={onClose} attrs={{'data-export-several': 'choose'}}
      extra={<>
        <span class="export-count" data-export-count>{files.length === 1 ? '1 file' : `${files.length} files`}</span>
        <button type="button" data-export-cancel onClick={onClose}>Cancel</button>
        <button type="button" class="primary" data-export-start disabled={!ready} title={picks && !folder ? 'Choose a folder first' : undefined}
          onClick={() => void session.exportSeveralStart()}>Export</button>
      </>}>
      {picks
        ? <div class="export-folder">
          <span>Folder</span>
          <code data-export-folder>{folder ?? 'none chosen'}</code>
          <button type="button" data-export-pick onClick={() => void session.exportSeveralPick()}>Choose…</button>
        </div>
        : <p class="export-note" data-export-note>Each file is downloaded under its name.</p>}
      {rows.map(row => (
        <section class={'export-row' + (row.reason ? ' off' : '')} data-export-row={row.id} key={row.id}>
          <div class="export-name">{row.label}</div>
          {row.reason
            ? <div class="export-why" data-export-reason>{row.reason}</div>
            : <>
              <div class="export-formats">{row.items.map(item => {
                const on = ticks.includes(item.key);
                return (
                  <label class={'chip' + (on ? ' on' : '')} key={item.key}>
                    <input type="checkbox" data-export-tick={item.key} checked={on}
                      onChange={e => session.exportSeveralTick(item.key, e.currentTarget.checked)} />
                    {item.label}
                  </label>);
              })}</div>
              <div class="export-files">{ticked([row], ticks).map(item => item.file).join(' · ')}</div>
            </>}
        </section>))}
    </Dialog>
  );
}

function Running({total, done}: {total: number; done: number}) {
  return (
    <Dialog title="Export several" name="export several" width="form" onClose={() => undefined} closable={false} attrs={{'data-export-several': 'running'}}>
      <p role="status" data-export-progress>{`Writing ${Math.min(done + 1, total)} of ${total}…`}</p>
    </Dialog>
  );
}

function Report({outcomes, onClose}: {outcomes: BatchOutcome[]; onClose(): void}) {
  const count = (status: string) => outcomes.filter(o => o.status === status).length;
  return (
    <Dialog title="Export several" name="export several" width="list" onClose={onClose} attrs={{'data-export-several': 'report'}}
      action={{label: 'Close', onClick: onClose}}>
      <p data-export-summary>{`${count('written') + count('downloaded')} ${count('downloaded') ? 'downloaded' : 'written'}, ${count('failed')} failed, ${count('skipped')} skipped.`}</p>
      <ul class="export-report">{outcomes.map((o, i) => (
        <li key={i} data-export-outcome={o.status} class={o.status}>
          <strong>{o.step}</strong> <span class="export-status">{choiceLabel(o.status)}</span>
          {o.text && (o.status !== 'written' || o.text !== o.step) && <span class="export-text"> {o.text}</span>}
        </li>))}</ul>
    </Dialog>
  );
}
