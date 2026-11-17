/* A batch of ordinary commands (W774, W775): the one state of the page's batch runner (session.ts batchRun), which File > Export > Several
   and a pin of several steps both run through: the step now running (the status bar shows it), the files that already exist (asked about
   once), and each step's outcome. Pure: the commands themselves are the session's, so the state holds only what the page shows. */

/** `idle`: no batch; `exists`: the replace question waits; `running`: a step runs; `report`: it ended and its window shows what happened */
export type BatchPhase = 'idle' | 'exists' | 'running' | 'report';

/** one step when it is done: written (a file, by its name or full path), ran (a step with no file), failed (the core's reason), or skipped
    (its file existed and the user chose so) */
export interface BatchOutcome {
  step: string;
  status: 'written' | 'downloaded' | 'ran' | 'failed' | 'skipped';
  text: string;
}

export interface BatchState {
  /** what is running, as the status bar and the messages name it (`Export several`, `Pin "Figures"`); null with no batch */
  title: string | null;
  phase: BatchPhase;
  /** each step as the page shows it: its file for an export, its command for a pin */
  steps: string[];
  /** the step now running, from 0 */
  index: number;
  /** the files that already exist, in phase `exists` */
  exists: string[];
  /** the steps done so far, in order */
  outcomes: BatchOutcome[];
}

export const initialBatch: BatchState = {title: null, phase: 'idle', steps: [], index: 0, exists: [], outcomes: []};

export type BatchAction =
  | {type: 'ask'; title: string; steps: string[]; exists: string[]}
  | {type: 'run'; title: string; steps: string[]}
  | {type: 'step'; index: number}
  | {type: 'outcome'; outcome: BatchOutcome}
  | {type: 'end'; report: boolean}
  | {type: 'clear'};

export function reduceBatch(state: BatchState, action: BatchAction): BatchState {
  switch (action.type) {
    case 'ask':
      return {...initialBatch, title: action.title, phase: 'exists', steps: action.steps, exists: action.exists};
    case 'run':
      return {...initialBatch, title: action.title, phase: 'running', steps: action.steps};
    case 'step':
      return {...state, index: action.index};
    case 'outcome':
      return {...state, outcomes: [...state.outcomes, action.outcome]};
    case 'end':
      return action.report ? {...state, phase: 'report'} : initialBatch;
    case 'clear':
      return initialBatch;
  }
}

/** a batch's title for a narrow status bar: the pin's name from `Pin "Name"`, else the title as it is */
export function shortTitle(title: string | null): string {
  return title?.match(/^Pin "(.*)"$/)?.[1] ?? title ?? '';
}
