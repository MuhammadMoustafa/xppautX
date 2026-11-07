(() => {
  const bar = document.querySelector('.plot-bar'), tools = document.querySelector('.plot-window-tools');
  if (bar && tools && !bar.querySelector('.plot-window-tools')) bar.append(tools.cloneNode(true));
  const pt = document.querySelector('.plot-tools');
  if (pt && !pt.querySelector('.mock-more')) { const m = document.createElement('button'); m.className = 'mock-more'; m.textContent = '⋯'; m.title = 'Reset view, Use this view, CSV, Sets'; pt.append(m); }
  const st = document.querySelector('.status-bar, footer.status, .shell > footer');
  if (st && !st.querySelector('.mock-status-messages')) { const m = document.createElement('button'); m.className = 'small mock-status-messages'; m.textContent = 'Messages (1)'; st.append(m); }
  for (const b of document.querySelectorAll('.values-panel button')) if (/^(Load from file…|Restore defaults)$/.test(b.textContent.trim())) b.classList.add('mock-hide-btn');
  for (const g of document.querySelectorAll('.values-panel .value-group')) { const row = [...g.querySelectorAll('button')].find(b => b.textContent.trim().startsWith('Save')); const r = row?.closest('details') ?? row; if (r && !g.querySelector('.mock-vmore')) { const m = document.createElement('button'); m.className = 'small mock-vmore'; m.textContent = '⋯'; m.title = 'Load from file…, Restore defaults'; r.after(m); } }
  const par = [...document.querySelectorAll('.value-group')].find(g => /PARAMETERS/i.test(g.textContent));
  if (par && !par.querySelector('.mock-slider')) { const m = document.createElement('button'); m.className = 'small mock-slider'; m.textContent = 'Add slider'; (par.querySelector('.mock-vmore') ?? par).after(m); }
  const tb = document.querySelector('.title-bar, header');
  if (tb && !tb.querySelector('.mock-menu')) { const m = document.createElement('button'); m.className = 'mock-menu'; m.textContent = '☰ Menu'; tb.prepend(m); }
  return 1;
})()
;(() => {
  const fmt = n => { const a = Math.abs(n); const sci = n !== 0 && (a < 1e-4 || a >= 1e6);
    const m = sci ? Number(n.toExponential(5)) : Number(n.toPrecision(6)); let s = sci ? n.toExponential(5).replace(/\.?0+e/, 'e') : String(m); return [s, m !== n, sci]; };
  const list = document.querySelector('.value-state'); const rows = [...list.children]; const tpl = rows[rows.length - 1];
  for (const [name, val] of [['tiny', 3.2e-9], ['big', 123456789.25], ['calcium_activation_gate', 0.0417734]]) { const r = tpl.cloneNode(true); r.querySelector('.value-name').textContent = name; r.querySelector('.value-now').title = 'Now: ' + val; list.append(r); }
  for (const o of list.querySelectorAll('.value-now')) { const n = Number((o.title || '').replace(/^Now: /, '')); if (!Number.isFinite(n)) continue;
    const [s, cut, sci] = fmt(n); o.classList.toggle('cut', cut);
    const mark = cut ? '<span class="more" aria-hidden="true">…</span>' : '';
    o.innerHTML = sci ? s.replace(/e/, mark + 'e') : s + mark; }
  for (const l of document.querySelectorAll('.value-name')) if (l.scrollWidth > l.clientWidth) l.classList.add('cut');
  const v = list.querySelector('.value-now.cut'); v.classList.add('tipped'); if ('copied') v.classList.add('copied');
  const t = document.createElement('div'); t.className = 'mock-tip line ok-tip'; t.innerHTML = '<span>' + v.title.replace(/^Now: /, '') + '</span>' + '<span class="ok"><svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M20 6 9 17l-5-5"/></svg> Copied</span>'; v.append(t);
  const ln = [...list.querySelectorAll('.value-name.cut')].pop(); if (ln && location.hash !== '#x' && document.querySelector('.mock-tip.line:not(.ok-tip)')) {
    const nt = document.createElement('div'); nt.className = 'mock-tip name-tip'; nt.textContent = ln.textContent; ln.closest('.value-field').append(nt); }
  return 1;
})()
