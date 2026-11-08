/* The one dialog shell (W641, docs/ui-v2.md "Dialogs"): every dialog and every core ask is this. A title, the description under it, an
   optional ? that opens the manual's section and a × (Escape and a click outside do the same); the body; and a footer with the one action,
   named for what it does, beside a count of the fields left to fix. Drawn in the browser's top layer by Modal (W632); Tab stays inside it
   (cycleDialogFocus), the focus goes in on open and back on close (useDialogFocus). Two widths: a form's and a list's. On a phone
   (below 48 rem) it is a full-screen sheet with the title and × on top and the action at the bottom. A dialog with fields passes `onSubmit`:
   its body and footer are one <form>, the action its submit button, which is never disabled. */
import type {ComponentChildren} from 'preact';
import {useRef} from 'preact/hooks';
import type {HelpTarget} from '../help/links';
import {cycleDialogFocus, useDialogFocus} from './dialogFocus';
import {HelpButton} from './HelpButton';
import {Modal} from './Modal';

export interface DialogAction {
  /** the action named for what it does ("Apply", "Save set", "Load"), from the command's row where it has one */
  label: string;
  /** a button that is not the form's submit (a dialog with no form) */
  onClick?: () => void;
  /** a mark a check finds it by */
  attrs?: Record<string, string>;
}

export interface DialogProps {
  title: ComponentChildren;
  /** what the title is as text: the dialog's accessible name */
  name: string;
  description?: ComponentChildren;
  help?: HelpTarget | null;
  /** about 36 rem for a form, 52 rem for a list or a table */
  width?: 'form' | 'list';
  onClose(): void;
  action?: DialogAction;
  /** other buttons of the footer, before the action (All, None) */
  extra?: ComponentChildren;
  /** the count of fields left to fix, in the footer */
  problems?: number;
  /** the dialog is a form: its body and footer are submitted by the action */
  onSubmit?: (e: Event) => void;
  role?: 'dialog' | 'alertdialog';
  /** data attributes a check finds the dialog by */
  attrs?: Record<string, string | number | undefined>;
  className?: string;
  /** the focus goes to the first field again when this changes (another ask in the same dialog) */
  focusKey?: unknown;
  /** a click outside closes it (default); false for a question that must be answered through its buttons */
  outside?: boolean;
  /** false for a dialog nothing closes (a question a recording's step asks): no ×, no Escape */
  closable?: boolean;
  /** the dialog's own other keys; return true when taken */
  onKeyDown?: (e: KeyboardEvent) => boolean | void;
  children?: ComponentChildren;
}

export function Dialog(p: DialogProps) {
  const box = useRef<HTMLDivElement>(null);
  useDialogFocus(box, [p.focusKey ?? null]);
  const labelId = useRef(`dialog-title-${Math.random().toString(36).slice(2, 8)}`).current;
  const keys = (e: KeyboardEvent) => {
    if (p.onKeyDown?.(e)) return;
    /* Enter in a field submits the form, a dropdown included (A3) */
    const target = e.target as HTMLElement;
    if (e.key === 'Enter' && !e.defaultPrevented && p.onSubmit && (target.tagName === 'SELECT' || (target.tagName === 'INPUT' && !['checkbox', 'radio', 'button', 'file', 'search'].includes((target as HTMLInputElement).type)))) {
      e.preventDefault();
      target.closest('form')?.requestSubmit();
      return;
    }
    if (e.key === 'Escape') {
      if (e.defaultPrevented || p.closable === false) return;
      e.preventDefault();
      e.stopPropagation();
      p.onClose();
    } else cycleDialogFocus(e, box.current);
  };
  const footer = (p.action || p.extra || p.problems) ? (
    <div class="dialog-actions" data-dialog-footer>
      {p.problems ? <span class="dialog-count" role="status" data-problems={p.problems}>{p.problems === 1 ? '1 field to fix' : `${p.problems} fields to fix`}</span> : null}
      {p.extra}
      {p.action && <button type={p.onSubmit && !p.action.onClick ? 'submit' : 'button'} class="primary" data-dialog-action data-autofocus={p.onSubmit ? undefined : ''} onClick={p.action.onClick} {...p.action.attrs}>{p.action.label}</button>}
    </div>
  ) : null;
  const inner = <>
    <div class="dialog-body">{p.children}</div>
    {footer}
  </>;
  const attrs = Object.fromEntries(Object.entries(p.attrs ?? {}).filter(([, v]) => v !== undefined));
  return (
    <Modal onBackdrop={p.outside === false || p.closable === false ? undefined : e => { e.preventDefault(); p.onClose(); }}>
      <div class={'dialog' + (p.className ? ` ${p.className}` : '')} data-dialog data-width={p.width ?? 'form'} ref={box} role={p.role ?? 'dialog'}
        aria-modal="true" aria-labelledby={labelId} onKeyDown={keys} {...attrs}>
        <header class="dialog-head">
          <div class="dialog-titles">
            <h2 id={labelId}>{p.title}</h2>
            {p.description ? <p class="dialog-desc">{p.description}</p> : null}
          </div>
          {p.help && <HelpButton target={p.help} label={p.name} />}
          {p.closable !== false && <button type="button" class="dialog-close" aria-label={`Close ${p.name}`} title="Close (Esc)" onClick={p.onClose}>×</button>}
        </header>
        {p.onSubmit ? <form class="dialog-form" noValidate onSubmit={p.onSubmit}>{inner}</form> : inner}
      </div>
    </Modal>
  );
}
