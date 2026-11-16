/* The File > Export > Several window's own state (W774): which formats are ticked, the folder shown, the files that already exist
   (asked about once), the phase, and each file's outcome. The ticks are not kept between uses: opening the window clears them.
   Pure: the batch itself (the commands sent, the folder's handle) is the session's (session.ts), so the state holds only what the
   window shows. */

export type ExportPhase = 'choose' | 'exists' | 'running' | 'report';

/** one file of the batch when it is done: written (its name or full path), failed (the core's reason), or skipped (it existed and the user chose so) */
export interface ExportOutcome {
  file: string;
  status: 'written' | 'failed' | 'skipped';
  text: string;
}

export interface ExportSeveralState {
  open: boolean;
  phase: ExportPhase;
  /** the ticked formats, each `exportKey`: the command and its format (exportKey in exportSeveral.ts) */
  ticks: string[];
  /** the folder the files go to: its full path on the desktop, a browser folder's name; null before one is picked */
  folder: string | null;
  /** the files that already exist, in phase `exists` */
  exists: string[];
  /** the batch's files, in order, as they are done */
  outcomes: ExportOutcome[];
  /** the batch's file count */
  total: number;
}

export const initialExportSeveral: ExportSeveralState = {open: false, phase: 'choose', ticks: [], folder: null, exists: [], outcomes: [], total: 0};

export type ExportSeveralAction =
  | {type: 'open'; open: boolean}
  | {type: 'tick'; key: string; on: boolean}
  | {type: 'folder'; folder: string | null}
  | {type: 'phase'; phase: ExportPhase; exists?: string[]; total?: number}
  | {type: 'outcome'; outcome: ExportOutcome};

export function reduceExportSeveral(state: ExportSeveralState, action: ExportSeveralAction): ExportSeveralState {
  switch (action.type) {
    case 'open':
      return action.open ? {...initialExportSeveral, open: true} : {...state, open: false};
    case 'tick': {
      const ticks = state.ticks.filter(key => key !== action.key);
      return {...state, ticks: action.on ? [...ticks, action.key] : ticks};
    }
    case 'folder':
      return {...state, folder: action.folder};
    case 'phase':
      return {...state, phase: action.phase, exists: action.exists ?? state.exists, total: action.total ?? state.total};
    case 'outcome':
      return {...state, outcomes: [...state.outcomes, action.outcome]};
  }
}
