/* The wire: events by Server-Sent Events, commands by POST, on the same
   endpoints core/xpp_http.cpp serves. xppautX puts a token in the page's
   address; both URLs need it. */
import type {Command, XppEvent} from './types';

export interface Transport {
  send(cmd: Command): void;
  /** starts the event stream; `onEvent` gets every event, `onStatus` the connection's state */
  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void): void;
  close(): void;
}

export class HttpTransport implements Transport {
  private source: EventSource | null = null;

  constructor(private readonly token: string = location.search, private readonly base = '/') {}

  send(cmd: Command): void {
    void fetch(`${this.base}cmd${this.token}`, {method: 'POST', body: JSON.stringify(cmd)});
  }

  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void): void {
    const source = new EventSource(`${this.base}events${this.token}`);
    source.onopen = () => onStatus(true);
    source.onerror = () => onStatus(false); /* EventSource reconnects by itself */
    source.onmessage = m => {
      onEvent(JSON.parse(m.data) as XppEvent);
    };
    this.source = source;
  }

  close(): void {
    this.source?.close();
    this.source = null;
  }
}
