/* The one field renderer (W641): a labelled row and the control its kind gets. Command forms (CommandForm), the option groups' forms
   (OptionsPanel, Values' Numerics) and the core's form asks draw their fields here, so a field looks and behaves the same in every
   dialog. The control is decided from the field's kind in one place (store/fieldKinds.ts controlOf); the row puts the label left of it
   where the dialog is wide and above it where it is narrow (a container query on the dialog's body), marks a required field with an
   asterisk only, shows the field's help under it and its error after the field was left or the action was pressed. */
import type {ComponentChildren} from 'preact';
import {useState} from 'preact/hooks';
import {choiceLabel, controlOf, rangeNote, type FieldSpec} from '../store/fieldKinds';
import {ColorField} from './ColorField';
import {Field} from './Field';

export interface FormRowProps {
  /** a check finds the row by it (data-argument) */
  name: string;
  label: string;
  required?: boolean;
  help?: ComponentChildren;
  /** shown under the field: the rule it broke, once the field was left or the action pressed */
  error?: string | null;
  /** the control is wider than a line: it takes the row's whole width (a list of rows, a table) */
  wide?: boolean;
  /** the id of the control, for the label */
  id?: string;
  className?: string;
  children: ComponentChildren;
}

export function FormRow({name, label, required, help, error, wide, id, className, children}: FormRowProps) {
  return (
    <div class={'form-row registry-argument' + (wide ? ' wide' : '') + (error ? ' bad' : '') + (className ? ` ${className}` : '')} data-argument={name} data-num={name}>
      <label class="form-label" htmlFor={id}>{label}{required && <span class="required" title="Required" aria-hidden="true"> *</span>}</label>
      <div class="form-control">
        {children}
        {help ? <small class="value-hint form-help">{help}</small> : null}
        {error ? <span class="field-error" role="alert">{error}</span> : null}
      </div>
    </div>
  );
}

export interface ControlProps {
  spec: FieldSpec;
  value: string;
  onChange(text: string): void;
  /** the field was left */
  onBlur?: () => void;
  label: string;
  id?: string;
  disabled?: boolean;
  dark: boolean;
  /** the dropdown's and the segmented control's names that cannot be chosen, with why (a method the model cannot use) */
  unavailable?: Record<string, string>;
  /** a dropdown's names shown as they are, not as a choice's words (the model's names) */
  verbatim?: boolean;
  /** an invalid text is marked on the box (aria-invalid) */
  invalid?: boolean;
  /** the field names a file: it is the one file control, whatever its spec (a `file`'s is) */
  file?: boolean;
  /** a dropped file fills the name (read-mode files) */
  onDropFile?: (file: File) => void;
  placeholder?: string;
}

export function Control({spec, value, onChange, onBlur, label, id, disabled, dark, unavailable = {}, verbatim, invalid, file, onDropFile, placeholder}: ControlProps) {
  const kind = file ? 'file' : controlOf(spec);
  if (kind === 'switch') {
    return <label class="switch"><input id={id} type="checkbox" role="switch" checked={value === 'true'} disabled={disabled} aria-label={label}
      onChange={e => onChange(String(e.currentTarget.checked))} /><i aria-hidden="true" /></label>;
  }
  if (kind === 'segmented' && spec.kind === 'choice') {
    return <div class="seg" role="radiogroup" aria-label={label} id={id}>
      {spec.choices.map(name => <button type="button" role="radio" key={name} class={'seg-item' + (value === name ? ' on' : '')} aria-checked={value === name}
        disabled={disabled || !!unavailable[name]} title={unavailable[name]} data-choice={name} onClick={() => { onChange(name); }}>{choiceLabel(name)}</button>)}
    </div>;
  }
  if (kind === 'select') {
    const names = spec.kind === 'choice' ? spec.choices : spec.kind === 'name' ? spec.names : [];
    const listed = names.includes(value) || !value ? names : [value, ...names];
    return <select id={id} value={value} disabled={disabled} aria-label={label} aria-invalid={invalid ? 'true' : undefined} onBlur={onBlur} onChange={e => onChange(e.currentTarget.value)}>
      {!value ? <option value="">Choose…</option> : null}
      {listed.map(name => <option key={name} value={name} disabled={!!unavailable[name]} title={unavailable[name]}>{spec.kind === 'choice' && !verbatim ? choiceLabel(name) : name}</option>)}
    </select>;
  }
  if (kind === 'checklist' && spec.kind === 'selection') {
    /* the checked names are the text, a name a line, in the order of the choices (the model's sets) */
    const checked = value.split('\n');
    if (!spec.choices.length) return <span class="value-hint" id={id}>The model has no sets.</span>;
    return <div class="check-list" role="group" aria-label={label} id={id}>
      {spec.choices.map(name => <label key={name} class="check-item"><input type="checkbox" checked={checked.includes(name)} disabled={disabled} data-choice={name}
        onChange={e => onChange(spec.choices.filter(n => n === name ? e.currentTarget.checked : checked.includes(n)).join('\n'))} /><span>{name}</span></label>)}
    </div>;
  }
  if (kind === 'color') return <ColorField value={value} dark={dark} label={label} onChange={onChange} />;
  if (kind === 'file') {
    /* the one file control (every file argument and every file option row): a name in the model's folder, typed or filled by a dropped file */
    return <span class="input-box file-box" onDragOver={onDropFile ? e => e.preventDefault() : undefined}
      onDrop={onDropFile ? e => { const dropped = e.dataTransfer?.files?.[0]; if (!dropped) return; e.preventDefault(); onDropFile(dropped); } : undefined}>
      <Field id={id} spec={spec} value={value} disabled={disabled} aria-label={label} aria-invalid={invalid ? 'true' : undefined} placeholder={placeholder}
        check={() => null} onInput={onChange} onBlur={onBlur} />
    </span>;
  }
  const note = rangeNote(spec);
  return <span class={'input-box' + (kind === 'mono' ? ' mono' : '')}>
    <Field id={id} spec={spec} value={value} disabled={disabled} aria-label={label} aria-invalid={invalid ? 'true' : undefined}
      check={() => null} onInput={onChange} onBlur={onBlur} />
    {note && <span class="unit">{note}</span>}
  </span>;
}

/** which fields of a form show their error: those left (touched) and, once the action was pressed, all */
export function useFeedback() {
  const [touched, setTouched] = useState<string[]>([]);
  const [attempted, setAttempted] = useState(false);
  return {
    attempted,
    touch: (name: string) => setTouched(old => old.includes(name) ? old : [...old, name]),
    attempt: () => setAttempted(true),
    shows: (name: string) => attempted || touched.includes(name),
  };
}
export type Feedback = ReturnType<typeof useFeedback>;

/** the focus to the first field that shows an error (the action pressed with errors) */
export function focusFirstError(box: Element | null): void {
  box?.querySelector<HTMLElement>('.form-row.bad :is(input, select, button)')?.focus();
}
