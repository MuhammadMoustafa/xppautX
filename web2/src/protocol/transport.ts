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
  /* the last command's POST: the next one goes out once it was answered */
  private sending: Promise<unknown> = Promise.resolve();

  constructor(private readonly token: string = location.search, private readonly base = '/') {}

  /** commands reach the core in the order they were sent: each POST waits
      for the one before it (xppautX answers each connection on a thread of
      its own, so two in flight at once could be taken in either order); a
      failed one does not hold up the next */
  send(cmd: Command): void {
    const body = JSON.stringify(cmd);
    this.sending = this.sending
      .then(() => fetch(`${this.base}cmd${this.token}`, {method: 'POST', body}))
      .catch(() => undefined);
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
