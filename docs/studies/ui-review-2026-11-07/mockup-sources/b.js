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
