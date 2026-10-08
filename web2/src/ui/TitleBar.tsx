/* The top bar: the menu drawer's button (narrow screens), model, theme,
   workspace tools and panels. Common run actions belong to RunToolbar. */
import type {Theme} from '../store/state';
import {BUSY_TITLE, useMay, useSession, useStore} from './context';
import {saveTheme} from './theme';
import {baseName} from '../store/files';
import {useEffect, useRef} from 'preact/hooks';
import {closeTools} from './hotkeys';

const NEXT_THEME: Record<Theme, Theme> = {system: 'light', light: 'dark', dark: 'system'};
/* the theme is an icon, not a word: "Auto" beside the feature buttons read as AUTO (T21) */
const THEME_NAME: Record<Theme, string> = {system: 'follow system', light: 'light', dark: 'dark'};

function ThemeIcon({theme}: {theme: Theme}) {
  const common = {width: 18, height: 18, viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', 'stroke-width': 2,
    'stroke-linecap': 'round' as const, 'stroke-linejoin': 'round' as const, 'aria-hidden': 'true' as const, class: 'icon'};
  if (theme === 'light') {
    return (
      <svg {...common}>
        <circle cx="12" cy="12" r="4" />
        <path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4" />
      </svg>
    );
  }
  if (theme === 'dark') return <svg {...common}><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z" /></svg>;
  /* follow the system: a half-filled circle */
  return (
    <svg {...common}>
      <circle cx="12" cy="12" r="9" />
      <path d="M12 3a9 9 0 0 1 0 18z" fill="currentColor" />
    </svg>
  );
}

/** the Commands and Values toggles of the wide layouts (the narrow ones are the drawers' menu-toggle and values-toggle) */
function PanelToggle({panel, collapsed, name}: {panel: 'menu' | 'values'; collapsed: boolean; name: string}) {
  const session = useSession();
  const key = panel === 'menu' ? 'menuCollapsed' : 'valuesCollapsed';
  const common = {width: 18, height: 18, viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', 'stroke-width': 2,
    'stroke-linecap': 'round' as const, 'stroke-linejoin': 'round' as const, 'aria-hidden': 'true' as const, class: 'icon'};
  return (
    <button class={`panel-toggle icon-button ${panel}-collapse`} aria-controls={panel === 'menu' ? 'command-menu' : 'values-panel'}
      aria-expanded={!collapsed} aria-label={`${name} panel`} title={`${collapsed ? 'Show' : 'Hide'} the ${name.toLowerCase()} panel`}
      onClick={() => session.store.dispatch({type: 'panels', panels: {[key]: !collapsed}})}>
      {panel === 'menu'
        ? <svg {...common}><path d="M4 6h16M4 12h16M4 18h16" /></svg>
        : <svg {...common}><rect x="3" y="4" width="18" height="16" rx="2" /><path d="M15 4v16" /></svg>}
    </button>
  );
}

export function TitleBar() {
  const tools = useRef<HTMLDetailsElement>(null);
  /* a click outside closes the menu (Escape is hotkeys.ts's, the one handler) */
  useEffect(() => {
    const outside = (e: PointerEvent) => {
      if (tools.current?.open && !tools.current.contains(e.target as Node)) closeTools(tools.current, tools.current.contains(document.activeElement));
    };
    document.addEventListener('pointerdown', outside, true);
    return () => document.removeEventListener('pointerdown', outside, true);
  }, []);
  const session = useSession();
  const title = useStore(s => s.title);
  const file = useStore(s => s.hello?.file ?? '');
  const changed = useStore(s => !!s.core?.changed);
  const theme = useStore(s => s.theme);
  const drawer = useStore(s => s.drawerOpen);
  const valuesOpen = useStore(s => s.valuesOpen);
  const tableOpen = useStore(s => s.table.open);
  const textOpen = useStore(s => s.text.open);
  const aplotOpen = useStore(s => s.aplot.open);
  const panels = useStore(s => s.panels);
  const aniOpen = useStore(s => s.ani.open);
  const helpOpen = useStore(s => s.help.open);
  const may = useMay();
  const recording = useStore(s => !!s.core?.recording);
  const mayRecord = may({cmd: 'record', op: 'start'});
  const mayPlay = may({cmd: 'play', op: 'open'});
  const setTheme = () => {
    const t = NEXT_THEME[theme];
    saveTheme(t);
    session.store.dispatch({type: 'theme', theme: t});
  };
  return (
    <header class="title-bar">
      <PanelToggle panel="menu" collapsed={panels.menuCollapsed} name="Commands" />
      <button class="menu-toggle" aria-controls="command-menu" aria-expanded={drawer}
        onClick={() => session.store.dispatch({type: 'drawer', open: !drawer})}>Menu</button>
      <h1 title={file}>{baseName(file) || 'xppautX'}</h1>
      {changed && <span class="unsaved" role="img" aria-label="Unsaved changes" title="Changed since the session was loaded or saved (Ctrl+S saves)">&#9679;</span>}
      <span class="muted file">{title}</span>
      <span class="spacer" />
      <details class="workspace-tools" ref={tools}><summary aria-haspopup="true">Tools</summary>
      <div class="workspace-tools-menu" onClick={e => { if ((e.target as Element).closest('button') && tools.current) closeTools(tools.current, false); }}>
      {!recording && (
        <button class="play-open" aria-disabled={!mayPlay} onClick={() => { if (mayPlay) session.playOpen(); }}
          title={mayPlay ? 'Play a recording (.recx): its model, then its steps (File/plaY recording)' : BUSY_TITLE}>Play a recording…</button>
      )}
      <button class="aplot-toggle" aria-controls="aplot-panel" aria-expanded={aplotOpen}
        onClick={() => (aplotOpen ? session.closeAplot() : session.openAplot())}>Array plot</button>
      <button class="ani-toggle" aria-controls="ani-panel" aria-expanded={aniOpen}
        onClick={() => (aniOpen ? session.closeAni() : session.openAni())}>Animation</button>
      {!recording && (
        <button class="record-toggle" aria-disabled={!mayRecord} onClick={() => { if (mayRecord) session.startRecording(); }}
          title={mayRecord ? 'Record the steps you take to a .recx file (File/recorD)' : BUSY_TITLE}>Record</button>
      )}
      </div></details>
      <button class="theme-toggle icon-button" onClick={setTheme} data-theme-choice={theme}
        aria-label={`Theme: ${THEME_NAME[theme]}`} title={`Theme: ${THEME_NAME[theme]} (click for ${THEME_NAME[NEXT_THEME[theme]]})`}>
        <ThemeIcon theme={theme} />
      </button>
      <PanelToggle panel="values" collapsed={panels.valuesCollapsed} name="Values" />
      <button class="values-toggle" aria-controls="values-panel" aria-expanded={valuesOpen}
        onClick={() => session.store.dispatch({type: 'valuesPanel', open: !valuesOpen})}>Values</button>
      <button class="table-toggle" aria-controls="table-panel" aria-expanded={tableOpen}
        onClick={() => (tableOpen ? session.closeTable() : session.openTable())}>Data</button>
      <button class="text-toggle" aria-controls="text-panel" aria-expanded={textOpen}
        onClick={() => (textOpen ? session.closeText() : session.openText())}
        title="Equations, source and the last equilibrium">Model</button>
      <button class="help-toggle" aria-controls="help-panel" aria-expanded={helpOpen}
        onClick={() => session.store.dispatch({type: 'help', action: {type: 'open'}})}
        title="The manual (F1)">Help</button>
    </header>
  );
}
