/* Starts the WebAssembly core (core/xpp_wasm.cpp, docs/wasm.md): the glue
   the page's Web Worker (worker.js) and node's check (tools/wasmcheck.mjs)
   share, so both run the module the same way. Plain JavaScript: a worker
   loads it with importScripts, node with require. */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.xppCore = factory();
})(typeof self !== 'undefined' ? self : globalThis, function () {
  /* the module's working folder: the model's folder, and what IDBFS keeps */
  const WORK = '/work';

  /* files are named by base name only, as xpp_files.cpp takes them: no
     folder, no "..", no empty name (the module's file system is the page's
     own, but a name that climbs out of WORK is still refused) */
  function baseName(name) {
    if (typeof name !== 'string' || name === '' || name === '.' || name === '..' || /[\/\\\u0000]/.test(name))
      throw new Error('not a base name: ' + JSON.stringify(name));
    return name;
  }

  /* Starts the core on `args` (--server MODEL ...) with `files` (name ->
     string or Uint8Array) in its folder. Returns at once: send(line) queues
     a command line until the core runs, then hands it to xpp::wasm::push
     (the inbox's reader; an abort goes straight to the running job), and
     ready settles when the core's main() has started. onLine gets each
     event line, in order, on this thread; onLog the core's own text (stdout
     and stderr), a line each. persistent mounts IDBFS on the folder, so
     files written there survive a reload once save() has run. */
  function startXpp(createModule, o) {
    const queue = [];
    let module = null;
    const handle = {
      send(line) {
        if (module) module.push(line);
        else queue.push(line);
      },
      save: () => new Promise((ok, no) => module.FS.syncfs(false, e => (e ? no(e) : ok()))),
      get module() {
        return module;
      },
    };
    handle.ready = (async () => {
      const files = Object.entries(o.files || {}).map(([name, data]) => [baseName(name), data]);
      const options = {
        xppEmit: o.onLine,
        print: text => o.onLog && o.onLog(text),
        printErr: text => o.onLog && o.onLog(text),
      };
      /* a pthread starts this same script again, not the worker that loaded it */
      if (o.script) options.mainScriptUrlOrBlob = o.script;
      const m = await createModule(options);
      m.FS.mkdir(WORK);
      if (o.persistent) {
        m.FS.mount(m.FS.filesystems.IDBFS, {}, WORK);
        await new Promise((ok, no) => m.FS.syncfs(true, e => (e ? no(e) : ok())));
      }
      for (const [name, data] of files) m.FS.writeFile(WORK + '/' + name, data);
      m.FS.chdir(WORK);
      module = m;
      m.callMain(o.args);
      for (const line of queue.splice(0)) m.push(line);
    })();
    return handle;
  }

  return {startXpp, baseName, WORK};
});
