/* The model's folder over HTTP (core/xpp_http.cpp, docs/protocol.md "Files"):
   GET /files lists it, GET /files/NAME reads a file, PUT /files/NAME
   writes one. Token-protected like /cmd. Thin: the decisions are in
   store/files.ts and session.ts. */
import type {FolderFile} from '../store/files';

export interface FilesApi {
  list(): Promise<FolderFile[]>;
  get(name: string): Promise<Blob | null>;
  /** the file's size and SHA-256 as the core stored them */
  put(name: string, data: Blob): Promise<{name: string; size: number; sha256: string}>;
}

export class HttpFiles implements FilesApi {
  constructor(private readonly token: string = location.search, private readonly base = '/') {}

  private url(name?: string): string {
    return `${this.base}files${name === undefined ? '' : '/' + encodeURIComponent(name)}${this.token}`;
  }

  async list(): Promise<FolderFile[]> {
    const r = await fetch(this.url(), {cache: 'no-store'});
    if (!r.ok) throw new Error(`listing the model's folder: ${r.status} ${await r.text()}`);
    return ((await r.json()) as {files: FolderFile[]}).files;
  }

  async get(name: string): Promise<Blob | null> {
    const r = await fetch(this.url(name), {cache: 'no-store'});
    if (r.status === 404) return null;
    if (!r.ok) throw new Error(`reading ${name}: ${r.status} ${await r.text()}`);
    return r.blob();
  }

  async put(name: string, data: Blob): Promise<{name: string; size: number; sha256: string}> {
    const r = await fetch(this.url(name), {method: 'PUT', body: data});
    if (!r.ok) throw new Error(`copying ${name} into the model's folder: ${await r.text()}`);
    return r.json();
  }
}

/** a file's SHA-256 as lowercase hex, as GET /files lists them */
export async function sha256Hex(data: Blob): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', await data.arrayBuffer());
  return [...new Uint8Array(digest)].map(b => b.toString(16).padStart(2, '0')).join('');
}
