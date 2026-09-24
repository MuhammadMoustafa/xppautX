/* The manual's shape (docs/manual/*.md, W12): built once at web2 build
   time into dist/manual.json (tools/manualBuild.mjs, run from build.mjs,
   never bundled into app.js) and fetched by the page itself the first
   time Help opens (ui/Help.tsx useManual) rather than imported, so a
   session that never opens Help never downloads the manual's own text. */
export interface ManualHeading {
  id: string;
  text: string;
  level: number;
}

export interface ManualChapter {
  /** the file's basename without extension, e.g. "04-using-the-interface" */
  id: string;
  title: string;
  /** already has each heading's `id` attribute set (manualBuild.mjs slugify) */
  html: string;
  headings: ManualHeading[];
}
