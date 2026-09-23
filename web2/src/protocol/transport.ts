/* The wire: events by Server-Sent Events, commands by POST, on the same
   endpoints web/index.html uses (core/xpp_http.cpp, web/serve.js). xppautX
   puts a token in the page's address; both URLs need it. */
import type {Command, XppEvent} from './types';

export interface Transport {
  send(cmd: Command): void;
  /** starts the event stream; `onEvent` gets every event, `onStatus` the connection's state */
  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void): void;
  close(): void;
}

/** how the server starts a draw event (flush_ops in core/ui_json.c) */
const DRAW = '{"ev":"draw",';

export class HttpTransport implements Transport {
  private source: EventSource | null = null;

  constructor(private readonly token: string = location.search, private readonly base = '/') {}

  send(cmd: Command): void {
    void fetch(`${this.base}cmd${this.token}`, {method: 'POST', body: JSON.stringify(cmd)});
  }

  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void): void {
    /* draw=0: xppautX leaves the classic page's drawing ops out of this stream */
    const source = new EventSource(`${this.base}events${this.token}${this.token ? '&' : '?'}draw=0`);
    source.onopen = () => onStatus(true);
    source.onerror = () => onStatus(false); /* EventSource reconnects by itself */
    source.onmessage = m => {
      /* drawing ops are the classic page's (web/): this page draws from data,
         and a long run's redraw is tens of megabytes of them, not worth parsing */
      if ((m.data as string).startsWith(DRAW)) return;
      onEvent(JSON.parse(m.data) as XppEvent);
    };
    this.source = source;
  }

  close(): void {
    this.source?.close();
    this.source = null;
  }
}
