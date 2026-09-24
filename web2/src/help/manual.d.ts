/* The type of build.mjs's `virtual:manual` module (its content comes from
   tools/manualBuild.mjs, which reads docs/manual/*.md at build time; tsc
   never runs esbuild's plugins, so this ambient declaration is what lets
   `import manual from 'virtual:manual'` typecheck). */
declare module 'virtual:manual' {
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

  const manual: ManualChapter[];
  export default manual;
}
