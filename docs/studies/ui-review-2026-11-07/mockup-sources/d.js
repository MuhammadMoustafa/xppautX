(() => {
  const d = [...document.querySelectorAll('.dialog')].filter(e => e.getBoundingClientRect().height > 0).pop(); if (!d) return 0;
  const head = d.querySelector('.dialog-title-row');
  const h = d.querySelector('h2, h3');
  if (!head && h) { const row = document.createElement('div'); row.className = 'mock-head'; h.before(row); row.append(h);
    const x = document.createElement('button'); x.className = 'mock-x'; x.textContent = '×'; x.setAttribute('aria-label', 'Close'); row.append(x); }
  for (const e of d.querySelectorAll('*')) if (e.children.length === 0 && /^(A number|A whole number|Required)$/.test(e.textContent.trim())) e.classList.add('mock-quiet');
  for (const e of d.querySelectorAll('*')) if (e.children.length === 0 && /^A file name only/.test(e.textContent.trim())) e.classList.add('mock-quiet');
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'Close' && !b.classList.contains('mock-x')) b.remove();
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'OK' && d.querySelector('h2')?.textContent.startsWith('Add slider')) b.textContent = 'Add slider';
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'Apply') { const f = document.createElement('div'); f.className = 'mock-foot'; const c = document.createElement('button'); c.textContent = 'Cancel'; b.parentElement.replaceWith(f); f.append(c, b); }
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'Run' && /Save as new set/.test(d.textContent)) b.textContent = 'Save set';
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'Run' && /^Load from file/.test(d.querySelector('h2,h3')?.textContent ?? '')) b.textContent = 'Load';
  for (const b of d.querySelectorAll('button')) if (b.textContent.trim() === 'Run' && /^Settings/.test(d.querySelector('h2,h3')?.textContent ?? '')) b.textContent = 'Apply';
  return 1;
})()
