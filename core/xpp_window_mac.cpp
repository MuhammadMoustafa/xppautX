/* The window's platform primitives (xpp_window_internal.h) on macOS, through the
   Objective-C runtime (message sends from C++, as the webview library makes
   them; no .mm file): the native file panels, the close box, Cmd+Q and the app
   menu, Finder's documents, the pop-out's raise and the window's place. Cocoa
   runs the window on the main thread and the session on another. Not tested:
   written without a Mac (CI's macos-core). */
#include "xpp_window_internal.h"
#include "xpp_window.h"
#include "xpp_files.h"
#include <utility>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#include <vector>

#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <unistd.h>

namespace {

/* the launch's document and whether the session has begun, which tells open_documents what to do with one */
struct DocumentState {
    std::mutex mu;
    bool session_started = false; /* a document to open is opened, not the model to load */
    std::string launch_document;  /* the file the launch's open-documents event named */
};
DocumentState documents_state;

} // namespace

bool xpp::window::adopt_launch_document(std::vector<std::string> &args)
{
    std::string doc;
    {
        std::lock_guard<std::mutex> lk(documents_state.mu);
        doc = documents_state.launch_document;
    }
    if (!doc.empty()) {
        const std::pair<std::string, std::string> where = xpp::files::split_path(doc);
        if (!where.first.empty()) xpp::files::change_dir(where.first.c_str());
        args.push_back(where.second);
        return true;
    }
    if (xpp::files::working_dir() == "/") {
        const char *home = getenv("HOME");
        if (home && *home) xpp::files::change_dir(home);
    }
    return false;
}

namespace xpp::window::platform {

bool has_file_dialog() { return true; }

namespace {

/* an Objective-C message from C++, as the webview library sends them */
template <typename R = id, typename... A> R msg(id self, const char *sel, A... a)
{
    return reinterpret_cast<R (*)(id, SEL, A...)>(objc_msgSend)(self, sel_registerName(sel), a...);
}
id cls(const char *name) { return reinterpret_cast<id>(objc_getClass(name)); }
id ns_string(const std::string &s) { return msg(cls("NSString"), "stringWithUTF8String:", s.c_str()); }

} // namespace

/* the library centres the window and Cocoa keeps it on its screen (the
   restored size); W230: zoom it to the screen's visible frame (untested) */
void place_window(webview_t w)
{
    msg<void>(static_cast<id>(webview_get_window(w)), "zoom:", static_cast<id>(nullptr));
}

namespace {

/* ---- leaving (W110) -----------------------------------------------------
   The window's close box and Cmd+Q (the app menu's Quit, the Dock's)
   answer "not yet" and go through the leave question (quit_asking), as
   WM_CLOSE and GTK's delete-event do: -windowShouldClose: on the library's
   window delegate (WebviewNSWindowDelegate) and
   -applicationShouldTerminate: on its application delegate
   (WebviewAppDelegate), both added to the library's classes once it has
   made them (in xpp::webview_create). The core's own exit after its bye
   stops the run loop (webview_terminate), which asks neither. On the main
   thread, from the run loop. Not tested: written without a Mac (CI's
   macos-core). */
constexpr unsigned long TERMINATE_CANCEL = 0; /* NSTerminateCancel */

/* -windowShouldClose: (called from Cocoa: nothing may throw) */
BOOL window_should_close(id, SEL, id window) { return close_asking(window) ? static_cast<BOOL>(NO) : static_cast<BOOL>(YES); }

/* -applicationShouldTerminate: never terminates the application itself
   (its exit() would run the core's atexit handlers on the window's
   thread): asked, or once the core is exiting, the window closes, as its
   close box would, and window_closed ends the process */
unsigned long application_should_terminate(id, SEL, id)
{
    if (!quit_asking()) {
        webview_t w = main_view();
        if (w) webview_terminate(w);
    }
    return TERMINATE_CANCEL;
}

} // namespace

/* a pop-out: the window delegate's windowShouldClose: (prepare_window added it to the library's class) is its close box;
   nothing to set up, and no place of ours: the system's */
void prepare_popout(webview_t) {}

/* a pop-out brought to the front (untested: written without a Mac) */
void raise_window(void *native)
{
    msg<void>(msg(cls("NSApplication"), "sharedApplication"), "activateIgnoringOtherApps:", static_cast<BOOL>(YES));
    msg<void>(static_cast<id>(native), "makeKeyAndOrderFront:", static_cast<id>(nullptr));
}

namespace {

/* the method on the library's class name (a no-op when it has none) */
void add_method(const char *name, const char *sel, IMP imp, const char *types)
{
    Class c = objc_lookUpClass(name);
    if (c) class_replaceMethod(c, sel_registerName(sel), imp, types);
}

void about_action(id, SEL, id)
{
    msg<void>(msg(cls("NSApplication"), "sharedApplication"), "orderFrontStandardAboutPanel:", static_cast<id>(nullptr));
}

} // namespace

/* The app menu's Quit and About. The icon comes
   with the .app bundle (W13b). */
void prepare_window(webview_t)
{
    add_method("WebviewNSWindowDelegate", "windowShouldClose:", reinterpret_cast<IMP>(window_should_close), "c@:@");
    add_method("WebviewAppDelegate", "applicationShouldTerminate:", reinterpret_cast<IMP>(application_should_terminate),
               "Q@:@");
    add_method("WebviewAppDelegate", "xppAbout:", reinterpret_cast<IMP>(about_action), "v@:@");
    id pool = msg(cls("NSAutoreleasePool"), "new");
    id bar = msg(msg(cls("NSMenu"), "alloc"), "init");
    id app_item = msg(msg(cls("NSMenuItem"), "alloc"), "init");
    id app_menu = msg(msg(cls("NSMenu"), "alloc"), "init");
    id quit = msg(msg(cls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", ns_string("Quit xppautX"),
                  sel_registerName("terminate:"), ns_string("q"));
    if (bar && app_item && app_menu && quit) {
        msg<void>(app_menu, "addItem:", quit);
        msg<void>(app_item, "setSubmenu:", app_menu);
        msg<void>(bar, "addItem:", app_item);
        msg<void>(msg(cls("NSApplication"), "sharedApplication"), "setMainMenu:", bar);
    }
    id about_item = msg(msg(cls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:",
                        ns_string("About xppautX"), sel_registerName("xppAbout:"), ns_string(""));
    msg<void>(about_item, "setTarget:", msg(msg(cls("NSApplication"), "sharedApplication"), "delegate"));
    msg<void>(app_menu, "insertItem:atIndex:", about_item, static_cast<long>(0));
    msg<void>(pool, "drain");
}

/* NSOpenPanel, NSSavePanel, application-modal on the main thread (not
   tested: written without a Mac). An open panel has no type menu of its
   own, so it filters nothing and every file stays pickable (All files);
   a save panel takes the types, the first added to a name without one. */
std::optional<std::string> pick_file(void *, const FileDialog &d)
{
    id panel = d.save ? msg(cls("NSSavePanel"), "savePanel") : msg(cls("NSOpenPanel"), "openPanel");
    if (!panel) return std::nullopt;
    if (!d.title.empty()) {
        msg<void>(panel, "setTitle:", ns_string(d.title));
        msg<void>(panel, "setMessage:", ns_string(d.title));
    }
    if (!d.dir.empty())
        msg<void>(panel, "setDirectoryURL:",
                  msg(cls("NSURL"), "fileURLWithPath:isDirectory:", ns_string(d.dir), static_cast<BOOL>(YES)));
    if (d.save && !d.file.empty()) msg<void>(panel, "setNameFieldStringValue:", ns_string(d.file));
    if (d.save && !d.exts.empty()) {
        id types = msg(cls("NSMutableArray"), "array");
        for (const std::string &e : d.exts) msg<void>(types, "addObject:", ns_string(e.substr(1)));
        msg<void>(panel, "setAllowedFileTypes:", types);
        msg<void>(panel, "setAllowsOtherFileTypes:", static_cast<BOOL>(YES));
    }
    constexpr long MODAL_RESPONSE_OK = 1; /* NSModalResponseOK */
    if (msg<long>(panel, "runModal") != MODAL_RESPONSE_OK) return std::string();
    id url = msg(panel, "URL");
    const char *path = url ? msg<const char *>(msg(url, "path"), "UTF8String") : nullptr;
    if (!path) return std::nullopt;
    return std::string(path);
}

namespace {

/* ---- Finder's documents (W91) -------------------------------------------
   Double-clicking a .ode, .odex, .snapx or .recx (xppautX.app's Info.plist
   declares them), dropping one on the app or `open -a xppautX file` sends
   the app an open-documents Apple Event, not an argument. Our handler
   replaces NSApplication's own (which would hand the files to the
   delegate, and webview's WebviewAppDelegate is left as it is): installed
   when NSApplicationWillFinishLaunchingNotification is posted, where Apple
   says a handler replaces the standard one, and before the event a launch
   brings, which Cocoa dispatches before applicationDidFinishLaunching:.
   That all happens inside xpp::webview_create (webview runs the app until
   it has finished launching), so a launch's document is known before the
   session starts, and becomes its model (xpp::window::launch_document);
   one that comes later is opened as File > Open model opens one (W61),
   asking first. On the main thread, from the run loop. Not tested: written
   without a Mac. */

/* the Apple Event Manager's four-character codes (AE/AppleEvents.h) */
constexpr uint32_t four_cc(const char (&c)[5])
{
    return static_cast<uint32_t>(static_cast<unsigned char>(c[0])) << 24 |
           static_cast<uint32_t>(static_cast<unsigned char>(c[1])) << 16 |
           static_cast<uint32_t>(static_cast<unsigned char>(c[2])) << 8 |
           static_cast<uint32_t>(static_cast<unsigned char>(c[3]));
}
constexpr uint32_t CORE_EVENT_CLASS = four_cc("aevt"); /* kCoreEventClass */
constexpr uint32_t OPEN_DOCUMENTS = four_cc("odoc");   /* kAEOpenDocuments */
constexpr uint32_t DIRECT_OBJECT = four_cc("----");    /* keyDirectObject */
constexpr const char *HANDLE_DOCUMENTS = "handleOpenDocuments:withReplyEvent:";
constexpr const char *WILL_FINISH_LAUNCHING = "applicationWillFinishLaunching:";

/* the files an open-documents event names, in its order */
std::vector<std::string> document_paths(id event)
{
    std::vector<std::string> paths;
    id list = event ? msg(event, "paramDescriptorForKeyword:", DIRECT_OBJECT) : nullptr;
    if (!list) return paths;
    auto add = [&paths](id item) {
        id url = item ? msg(item, "fileURLValue") : nullptr; /* coerced from an alias if need be */
        id path = url ? msg(url, "path") : nullptr;
        const char *s = path ? msg<const char *>(path, "UTF8String") : nullptr;
        if (s && *s) paths.emplace_back(s);
    };
    const long n = msg<long>(list, "numberOfItems"); /* NSInteger; not a list: 0 */
    if (n <= 0) add(list);
    for (long i = 1; i <= n; i++) add(msg(list, "descriptorAtIndex:", i)); /* from 1 */
    return paths;
}

/* -handleOpenDocuments:withReplyEvent: (called from Cocoa: nothing may
   throw) */
void open_documents(id, SEL, id event, id)
{
    try {
        std::vector<std::string> paths = document_paths(event);
        if (paths.empty()) return;
        if (paths.size() > 1)
            host_api()->log(XPP_LOG_WARN, "xppautX: {:d} files to open, one model at a time: only the first\n", paths.size());
        host_api()->log(XPP_LOG_INFO, "xppautX: macOS asked to open {}\n", paths[0].c_str());
        {
            std::lock_guard<std::mutex> lk(documents_state.mu);
            if (!documents_state.session_started) {
                documents_state.launch_document = paths[0];
                return;
            }
        }
        host_api()->open_model(paths[0].c_str());
    } catch (const std::exception &e) {
        host_api()->log(XPP_LOG_WARN, "xppautX: a document to open was lost: {}\n", e.what());
    } catch (...) {
        host_api()->log(XPP_LOG_WARN, "xppautX: a document to open was lost\n");
    }
}

/* the handler, on the object below */
void install_documents_handler(id handler)
{
    id events = msg(cls("NSAppleEventManager"), "sharedAppleEventManager");
    if (events)
        msg<void>(events, "setEventHandler:andSelector:forEventClass:andEventID:", handler,
                  sel_registerName(HANDLE_DOCUMENTS), CORE_EVENT_CLASS, OPEN_DOCUMENTS);
}

/* -applicationWillFinishLaunching:, the notification's */
void will_finish_launching(id self, SEL, id) { install_documents_handler(self); }

/* An object of a class of our own (XppDocumentsHandler, NSObject's) that
   observes the launch and handles the event; kept for the process's life.
   Before the web view is created. */
void watch_for_documents()
{
    constexpr const char *CLASS_NAME = "XppDocumentsHandler";
    id pool = msg(cls("NSAutoreleasePool"), "new");
    Class c = objc_lookUpClass(CLASS_NAME);
    if (!c) {
        c = objc_allocateClassPair(objc_lookUpClass("NSObject"), CLASS_NAME, 0);
        if (c) {
            class_addMethod(c, sel_registerName(HANDLE_DOCUMENTS),
                            reinterpret_cast<IMP>(open_documents), "v@:@@");
            class_addMethod(c, sel_registerName(WILL_FINISH_LAUNCHING),
                            reinterpret_cast<IMP>(will_finish_launching), "v@:@");
            objc_registerClassPair(c);
        }
    }
    id handler = c ? msg(reinterpret_cast<id>(c), "new") : nullptr;
    if (handler) {
        msg<void>(msg(cls("NSNotificationCenter"), "defaultCenter"), "addObserver:selector:name:object:", handler,
                  sel_registerName(WILL_FINISH_LAUNCHING),
                  ns_string("NSApplicationWillFinishLaunchingNotification"), static_cast<id>(nullptr));
        /* and now, should the application have finished launching already */
        install_documents_handler(handler);
    }
    msg<void>(pool, "drain");
}

void (*session_fn)(void);

void *session_main(void *)
{
    session_fn();
    return nullptr;
}

} // namespace

/* Cocoa: the window on the main thread, the session on another, with the main thread's 8 MB of stack (a new
   thread gets 512 KB). Finder's documents first: the launch's comes while the view is created */
bool run_window_ui(void (*session)())
{
    watch_for_documents();
    webview_t w = open_view();
    if (!w) {
        host_api()->log(XPP_LOG_WARN, "{}", no_view_message());
        return false;
    }
    set_main_view(w);
    {
        std::lock_guard<std::mutex> lk(documents_state.mu);
        documents_state.session_started = true; /* a document from now on is opened */
    }
    std::atexit(on_exit);
    pthread_attr_t attr;
    pthread_t core;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    session_fn = session;
    pthread_create(&core, &attr, session_main, nullptr);
    webview_run(w);
    window_closed(w); /* never returns */
    return true;
}

} // namespace xpp::window::platform
