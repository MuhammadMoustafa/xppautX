(() => {
  const MODE = location.hash.slice(1);
  const dockTabs = on => { const t = document.createElement('div'); t.className = 'mock-tabs';
    t.innerHTML = ['Values', 'Table', 'Model', 'Help'].map(n => `<span class="${n === on ? 'on' : ''}">${n}</span>`).join('') + '<span class="x" title="Hide the dock">✕</span>'; return t; };
  const vp = document.querySelector('.values-panel'); vp.querySelector('.mock-tabs')?.remove(); vp.prepend(dockTabs('Values'));
  const work = (on, autox) => { const t = document.createElement('div'); t.className = 'mock-worktabs';
    t.innerHTML = `<span class="${on === 'graph_1' ? 'on' : ''}">graph_1</span>` + (autox ? `<span class="${on === 'autoX' ? 'on' : ''}">autoX · diagram_1 <i title="Close autoX">✕</i></span>` : '') + '<span class="new" title="New graph or autoX view">＋</span>'; return t; };
  document.querySelectorAll('.mock-worktabs').forEach(e => e.remove());
  const ws = document.querySelector('main.workspace'); ws.prepend(work('graph_1', MODE === 'autox' || MODE === 'graph'));
  if (MODE === 'table') { document.body.classList.add('mock-table'); const tp = document.querySelector('.table-panel'); tp.querySelector('.mock-tabs')?.remove(); tp.prepend(dockTabs('Table')); }
  if (MODE === 'autox') { document.body.classList.add('mock-autox'); const ap = document.querySelector('.autox-panel'); ap.querySelector('.mock-worktabs')?.remove(); ap.prepend(work('autoX', true));
    for (const b of ap.querySelectorAll('.autox-header button')) if (b.textContent.trim() === 'Close') b.style.display = 'none'; }
  return 1;
})()
