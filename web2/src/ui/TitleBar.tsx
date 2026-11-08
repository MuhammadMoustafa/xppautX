/* The top bar: the menu drawer's button (phones), model, theme, the Values panel's button and the Data, Model and
   Help buttons (on a phone the last three are in the menu bar: Windows and Help). Common run actions belong to RunToolbar. */
import type {Theme} from '../store/state';
import {useSession, useStore} from './context';
import {CommandLabel} from './CommandButton';
import {saveTheme} from './theme';
import {baseName} from '../store/files';
import {MenuButton} from './MenuBar';

const NEXT_THEME: Record<Theme, Theme> = {system: 'light', light: 'dark', dark: 'system'};
/* the theme is an icon, not a word: "autoX" beside the feature buttons read as autoX (T21) */
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

/** The Values panel can still be collapsed independently of the menu bar. */
function PanelToggle({collapsed}: {collapsed: boolean}) {
  const session = useSession();
  return <button class="panel-toggle icon-button values-collapse" aria-controls="values-panel"
    aria-expanded={!collapsed} aria-label="Values panel" title={`${collapsed ? 'Show' : 'Hide'} the values panel`}
    onClick={() => session.store.dispatch({type:'panels', panels:{valuesCollapsed:!collapsed}})}>
    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><rect x="3" y="4" width="18" height="16" rx="2"/><path d="M15 4v16"/></svg>
  </button>;
}

export function TitleBar() {
  const session = useSession();
  const title = useStore(s => s.title);
  const file = useStore(s => s.hello?.file ?? '');
  const changed = useStore(s => !!s.core?.changed);
  const theme = useStore(s => s.theme);
  const valuesOpen = useStore(s => s.valuesOpen);
  const tableOpen = useStore(s => s.table.open);
  const textOpen = useStore(s => s.text.open);
  const panels = useStore(s => s.panels);
  const helpOpen = useStore(s => s.help.open);
  const setTheme = () => {
    const t = NEXT_THEME[theme];
    saveTheme(t);
    session.store.dispatch({type: 'theme', theme: t});
  };
  /* Data, Model and Help: the title bar's buttons (css/layout.css .title-only: hidden on a phone, where the menu bar has them) */
  const panelButtons = [
    <button key="table" class="table-toggle title-only" aria-controls="table-panel" aria-expanded={tableOpen}
      onClick={() => (tableOpen ? session.closeTable() : session.openTable())}><CommandLabel command="tool.browser" /></button>,
    <button key="text" class="text-toggle title-only" aria-controls="text-panel" aria-expanded={textOpen}
      onClick={() => (textOpen ? session.closeText() : session.openText())}
      title="Equations, source and the last equilibrium">Model</button>,
    <button key="help" class="help-toggle title-only" aria-controls="help-panel" aria-expanded={helpOpen}
      onClick={() => session.store.dispatch({type: 'help', action: {type: 'open'}})}
      title="The manual">Help</button>,
  ];
  return (
    <header class="title-bar">

      <MenuButton />
      <h1 title={file}>{baseName(file) || 'xppautX'}</h1>
      {changed && <span class="unsaved" role="img" aria-label="Unsaved changes" title="Changed since the session was loaded or saved (Ctrl+S saves)">&#9679;</span>}
      <span class="muted file">{title}</span>
      <span class="spacer" />
      <button class="theme-toggle icon-button" onClick={setTheme} data-theme-choice={theme}
        aria-label={`Theme: ${THEME_NAME[theme]}`} title={`Theme: ${THEME_NAME[theme]} (click for ${THEME_NAME[NEXT_THEME[theme]]})`}>
        <ThemeIcon theme={theme} />
      </button>
      <PanelToggle collapsed={panels.valuesCollapsed} />
      <button class="values-toggle" aria-controls="values-panel" aria-expanded={valuesOpen}
        onClick={() => session.store.dispatch({type: 'valuesPanel', open: !valuesOpen})}><CommandLabel command="tool.values" /></button>
      {panelButtons}
    </header>
  );
}
