(() => {
  const FORM = location.hash.slice(1) || 'settings';
  const row = (label, ctl, help, err) => `<div class="fx-row"><label>${label}</label><div class="fx-ctl">${ctl}${err ? `<div class="fx-err">⚠ ${err}</div>` : ''}${help ? `<div class="fx-help">${help}</div>` : ''}</div></div>`;
  const num = (v, unit, bad) => `<div class="fx-input${bad ? ' bad' : ''}"><input value="${v}">${unit ? `<span class="unit">${unit}</span>` : ''}</div>`;
  const seg = (opts, on) => `<div class="fx-seg">${opts.map(o => `<span class="${o === on ? 'on' : ''}">${o}</span>`).join('')}</div>`;
  const sel = v => `<select class="fx-select"><option>${v}</option></select>`;
  const sw = (on, text) => `<span class="fx-switch${on ? '' : ' off'}"><i></i>${text}</span>`;
  const forms = {
    settings: {title: 'Integration settings', sub: 'How the next run integrates. Leave a field as it is to keep its value.', action: 'Apply',
      body: `<fieldset class="fx-group"><legend>Time</legend>${row('Total time', num('30', 'time units'))}${row('Start time', num('0'))}${row('Time step', num('-0.05', '', true), 'Dt in XPPAUT', 'Must be more than 0; got -0.05')}${row('Store every', num('1', 'steps'))}</fieldset>
        <fieldset class="fx-group"><legend>Method</legend>${row('Method', sel('Runge–Kutta (4th order)'))}${row('Relative tolerance', num('0.001'))}${row('Absolute tolerance', num('0.001'))}</fieldset>
        <fieldset class="fx-group"><legend>Limits</legend>${row('State limit', num('100'), 'A run stops when a state grows past it')}${row('At the limit', seg(['Stop', 'Keep going'], 'Stop'))}</fieldset>`},
    save_set: {title: 'Save as new set', sub: 'Write the current values into the model file as a named set.', action: 'Save set',
      body: `${row('Name', num(''), 'Shown in the Sets menus')}${row('Values', seg(['All', 'Parameters', 'Initial values', 'Numerics', 'Plot'], 'All'))}${row('States from', seg(['Initial values', 'Last state'], 'Initial values'))}${row('Window', sel('graph_1'))}`},
    load_file: {title: 'Load from file', sub: 'Read an .odex, or XPPAUT’s .par, .ic or .set. Nothing is applied until you choose a part.', action: 'Load',
      body: `${row('File', `<div class="fx-drop"><span style="font-size:1.4rem">📄</span><div><b>Choose a file</b> or drop it here<div class="fx-help">.odex, .par, .ic, .set in the model’s folder</div></div></div>`)}${row('Values', seg(['All', 'Parameters', 'Initial values', 'Numerics'], 'Parameters'))}`},
  };
  const f = forms[FORM];
  document.querySelector('.fx-backdrop')?.remove();
  const d = document.createElement('div'); d.className = 'fx-backdrop';
  d.innerHTML = `<div class="fx" role="dialog" aria-modal="true"><div class="fx-head"><div style="flex:1"><h2>${f.title}</h2><p>${f.sub}</p></div><button class="fx-icon" title="Help">?</button><button class="fx-icon" title="Close (Esc)">✕</button></div>
    <div class="fx-body">${f.body}</div><div class="fx-foot">${FORM === 'settings' ? '<span class="note">1 field to fix</span>' : ''}<button class="fx-primary">${f.action}</button></div></div>`;
  document.body.append(d); return 1;
})()
