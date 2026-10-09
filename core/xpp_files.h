#ifndef XPP_FILES_H
#define XPP_FILES_H

#include "xpp_error.h"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* The model's folder as the page's workspace (xpp_files.cpp; docs/ui-v2.md
   section 4, docs/protocol.md "Files"). The browser's file dialogs cannot
   tell a page where a file lives, so the page copies what the user picks
   into the folder XPP reads and writes (the working directory) and fetches
   back what XPP wrote there. xpp_http.cpp serves this as /files, and the
   `file` command of the protocol does the same for --server clients.

   Only base names in the working directory are reachable: no separators,
   no "..", no leading dot, no control or reserved characters, at most 255
   bytes, not a Windows device name (name_ok). A name that is a symbolic
   link, a folder or anything but a plain file is refused, so nothing
   outside the folder is ever read or written through it. A write goes to
   a hidden temporary file in the same folder first and is renamed into
   place only when complete: a failed or cut upload leaves nothing.

   Thread-safe (the HTTP thread and the core thread both call it). This
   header includes no core header, so xpp_http.cpp can use it. C++ in
   namespace xpp::files (W109a); a path a function only reads is a
   std::string_view. */

#define XPP_FILES_CAP (64ULL << 20) /* Bound upload memory and disk use to 64 MB per file. */

/* what a call gives back */
#define XPP_FILES_OK 0
#define XPP_FILES_BAD_NAME 1  /* not a safe base name */
#define XPP_FILES_NOT_FOUND 2
#define XPP_FILES_REFUSED 3   /* a link, a folder or a device: not a plain file */
#define XPP_FILES_TOO_LARGE 4 /* more than the cap */
#define XPP_FILES_IO 5        /* the system refused, or the data was cut short */
#define XPP_FILES_BUSY 6      /* a computation runs: nothing lands in the folder under it */

namespace xpp::files {

inline constexpr size_t NAME_MAX_BYTES = 255; /* Common filesystem basename limit; refuse longer names without cutting them. */

/* All offered defaults: model base without its extension, optional qualifier, extension. */
std::string output_name(std::string_view model_file, std::string_view ext,
                        std::string_view what = {});

/* First is an actual picked destination; later frames add their sequence number. */
std::string frame_name(std::string_view first, std::string_view ext, int frame);
/* Case-insensitive suffix on the basename; an extension alone is not a filename. */
bool has_extension(std::string_view path, std::string_view ext);
bool name_ok(std::string_view name, bool allow_hidden = false);
/* Validate the basename and refuse links, folders and devices at a save destination. */
int output_status(std::string_view path);
const char *status_text(int status); /* in words, for an error */

/* opens a file for reading ("rb"); size its length */
int open(std::string_view name, FILE *&fp, unsigned long long &size);

/* a write in steps: begin, write the bytes as they come (more than `cap`
   in all fails with XPP_FILES_TOO_LARGE), then commit, or abort. Commit
   and abort end the Put whatever they return; after a failed write,
   abort. Commit gives the size and SHA-256 of what was written (sha256,
   64 hex digits).
   The write is xpp_io.h's writer (binary): the temp file beside the name
   and its rename are the same as every other replace's.
   The one route a file takes into the model's folder, whichever way it
   came (HTTP PUT /files/NAME, the protocol's `file` put; W134), and the
   one decision whether it may land now: never while a computation runs
   (XPP_FILES_BUSY, logged as a refusal). Begin refuses before a byte is
   read; commit decides again inside xpp::job::OutsideComputation, so a
   computation that began meanwhile refuses the rename too and one cannot
   begin under it. The bytes go to the hidden temp file meanwhile, which
   no model reads. */
struct Put;
int put_begin(std::string_view name, unsigned long long cap, Put *&put);
int put_write(Put &put, std::string_view bytes);
void put_abort(Put *put);
int put_commit(Put *put, unsigned long long &size, std::string &sha256);

/* "read" when a file selector with this title opens a file, "write" when
   it saves one (the `mode` of the `file` ask) */
const char *ask_mode(std::string_view title);

/* Atomically replaces `to` with `from` (POSIX rename(), which already
   replaces; win32::move_over on Windows, where rename() does not): 0 on
   success. The one place that knows the platform difference; core/xpp_io.cpp's
   writer (core/xpp_io.h) calls this for its own temp-then-rename commit
   instead of duplicating it. */
int replace_file(std::string_view from, std::string_view to);

struct FileEntry {
    std::string name;
    unsigned long long size;
    long long mtime;
    std::string sha;
};
/* The operation's data, independent of HTTP and protocol event rendering. */
struct CommandResult {
    std::string name, sha, bytes;
    std::optional<Error> error;
    unsigned long long size = 0;
    std::vector<FileEntry> files;
};
CommandResult command(std::string_view op, const char *name_json, const char *data_json);
/* Every failed open names the requested file and the system or explicit reason. */
Error open_error(std::string where, std::string_view file, std::string_view why = {});

/* ---- the core's own files, by any path (W32b) -------------------------------
   The one place the core opens, copies, moves, deletes and probes files
   and makes its temp folders: the calls above are the page's, limited to
   base names in the model's folder; these are the core's (the model's
   outputs, AUTO's fort.* and diagram files, its scratch folder).
   tools/filecheck.sh counts every direct fopen/remove/rename/mkdir/...
   left elsewhere. Reading and writing go through xpp_io.h's handles
   (xpp::Writer: write, binary, a replace only on commit;
   xpp::LineReader, xpp::TokenReader and xpp::open_read for reading),
   which open their files here. */

/* A stream the caller keeps open across calls and closes itself with
   fclose (AUTO's fort.3/7/8/9 during a run, the array plot's GIF movie,
   an input script): fopen's modes, NULL on failure. A file opened and
   closed in one scope uses a handle of xpp_io.h instead. */
FILE *open_stream(std::string_view path, const char *mode);
/* Read a plain file inside folder; refuse traversal and linked descendants.
   The caller owns the returned stream. Replay's saved reads remain isolated. */
FILE *open_read_within(std::string_view path, std::string_view folder);
/* While set, observer(path) is called on every file open_stream (so
   xpp_io.h's readers too) has just opened for reading only (a mode "r" or
   "rb"): what a recording embeds (W59a, json_record.cpp). The core
   thread's alone; nullptr stops it. */
void observe_reads(void (*observer)(const std::string &path));
/* While set, a file outside the scratch folders that the core opens for
   reading with open_stream (so xpp_io.h's readers too) is the server's:
   server(path, &copy) is true with copy the file to open in its place,
   false for a file it does not hold, which is then not there: nothing is
   read from the disk. exists(path) is true for a file server(path,
   nullptr) holds, else asks the disk (a file about to be written). A
   recording's replay serves the files its steps read (W59b,
   json_player.cpp). The core thread's alone; nullptr stops it. */
void serve_reads(bool (*server)(const std::string &path, std::string *copy),
                 bool (*write)(std::string_view path, bool opening, int kind) = nullptr);
/* serve_reads is set: a recording's step is running */
bool serving_reads();
/* While replay serves reads, writes stay in private scratch or the server's
   allowed output folder. opening confirms an existing output only once;
   atomic create/replace recheck the same path without another question. */
bool write_path_ok(std::string_view path, bool opening = false);
/* the descriptor a standard stream (stdout, stderr) writes through, given
   one (the null device) if it has none: the Windows exe is a GUI-subsystem
   program, and started with no console (Explorer, a shortcut,
   Start-Process) the C library leaves stdout and stderr without a
   descriptor (_fileno -2), where a dup2 onto 1 and 2 never reaches them
   and all the core prints, AUTO's table included, was lost to the page
   (T27). -1 when it cannot be given one. */
int stream_fd(FILE *f);
/* Creates path for writing, failing when it exists already (a link
   included, which is never followed): the temp files of a replace.
   binary false is text mode ("w"), true binary ("wb"). NULL on failure. */
FILE *create_new(std::string_view path, bool binary);
/* path names a file or a folder */
bool exists(std::string_view path);
/* path names a folder (a link to one included) */
bool is_dir(std::string_view path);
/* a file can be created in dir: probed by creating one and removing it (a
   folder can exist without being writable); false for "" */
bool dir_writable(std::string_view dir);
/* the file selector's current folder (core/read_dir.cpp's old cur_dir,
   folded in here, W46b): "" until refreshed */
std::string cur_dir();
/* sets it from getcwd(): true on success; on failure a WARN and "" */
bool refresh_cur_dir();
/* chdir(path), then refreshes the current folder. 0 on success, 1 on
   failure (the chdir failed, a WARN, or the refresh after it did) -- the
   historical change_directory()'s own convention. */
int change_dir(std::string_view path);
/* deletes the file path: 0 on success */
int remove(std::string_view path);
/* to becomes a byte-for-byte copy of from, written beside it and renamed
   into place, so it is either the whole copy or left as it was; a WARN
   when from cannot be read or to written */
void copy(std::string_view from, std::string_view to);
/* to becomes from's bytes followed by its own, the same way (AUTO's run
   output put ahead of the diagram files it keeps: its "append"); a copy
   when to does not exist */
void prepend(std::string_view from, std::string_view to);
/* from becomes to, replacing it; when the system refuses (Windows, a
   source still open elsewhere) a copy, then from is removed if it can be */
void move(std::string_view from, std::string_view to);

/* AUTO's private scratch folder: "xppautoX-<pid>-<N>", mode 0700, under
   $TMPDIR or /tmp (POSIX) or the system temp path (Windows): its absolute
   path, or empty on failure */
std::string make_temp_dir();
std::string temp_base(); /* The platform temp folder, shared by scratch files and the window loader. */
/* The per-user settings folder (keymap.json, xpp_keymap.h): $XPP_CONFIG_DIR
   when set, else the system's: %APPDATA%\xppautX on Windows,
   $XDG_CONFIG_HOME/xppautX or ~/.config/xppautX on Linux, and
   ~/Library/Application Support/xppautX on macOS. Empty when the system
   names none (no HOME), and when $XPP_CONFIG_DIR is not an absolute path.
   The folder may not exist yet: make_dirs. */
std::string config_dir();
/* the file `name` in it ("" when there is no config folder), with the
   platform's separator */
std::string config_path(std::string_view name);
/* the override: tests and CI point the settings at a folder of their own so
   they never read or write the user's real file */
inline constexpr const char *CONFIG_DIR_ENV = "XPP_CONFIG_DIR";
/* The folder the program's own read-only files are in (the bundled
   examples): beside the running program's executable, found from the
   system (Windows GetModuleFileNameA, Linux /proc/self/exe, macOS
   _NSGetExecutablePath), never from the working folder or a name typed on
   the command line. Inside a macOS .app (the executable in Contents/MacOS)
   it is the bundle's Contents/Resources. Empty when the system does not say. */
std::string program_dir();
/* the file or folder `name` in the folder dir, with the platform's separator */
std::string join(std::string_view dir, std::string_view name);
/* path and its missing parents as folders, mode 0700 (POSIX); false when
   one cannot be made */
bool make_dirs(std::string_view path);
/* path is in one of this process's scratch folders (as the path above
   names them): a file the core itself keeps there, never the user's.
   root_only requires the folder itself, excluding nested directories. */
bool is_scratch(std::string_view path, bool root_only = false);
/* Removes every file directly in dir (no folders are expected there),
   then dir itself. "" does nothing. */
void remove_temp_dir(std::string_view dir);
/* issue #32: removes the scratch folders of runs that were killed before
   they could remove their own (make_temp_dir's naming, whose pid names no
   running process); called once at start */
void cleanup_stale_temp_dirs();

/* a folder's entry: its name, and whether it is a folder itself (a link
   followed) */
struct DirEntry {
    std::string name;
    bool folder;
};
/* the entries of the folder dir ("." and ".." included, in the order the
   system gives them) into out; false (out empty) when dir cannot be read */
bool list_dir(std::string_view dir, std::vector<DirEntry> &out);
/* the folders of direct and its files that match the Unix-style wildcard
   wild (*, ?, [..]), each list sorted; false (dirs/files left empty, a
   WARN) when direct cannot be read. core/read_dir.cpp's old list_folder,
   folded in here (W46b). */
bool list_matching(std::string_view wild, std::string_view direct, std::vector<std::string> &dirs,
                   std::vector<std::string> &files);

/* the working directory, whatever its length ("" when it cannot be had) */
std::string working_dir();
/* path's folder ("" when it names none: a bare name) and its last part,
   either separator ('/', or '\' too on Windows) */
std::pair<std::string, std::string> split_path(std::string_view path);
/* path starts at the root ("/x", and on Windows "C:\x" or "\x") */
bool is_absolute(std::string_view path);
/* path itself when it is absolute (is_absolute),
   else under the folder dir ("" the working directory) */
std::string absolute(std::string_view path, std::string_view dir = {});

/* the folder name names inside the folder dir ("..": its parent), or ""
   when it is not a folder; the process's current folder is not touched */
std::string folder_in(std::string_view dir, std::string_view name);

} // namespace xpp::files

namespace xpp {
/* a scratch folder of its own (files::make_temp_dir), removed with the
   files in it when this goes; path() is empty when none could be made */
class TempDir {
public:
    TempDir() : path_(files::make_temp_dir()) {}
    ~TempDir()
    {
        if (!path_.empty()) files::remove_temp_dir(path_);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;
    const std::string &path() const noexcept { return path_; }
    /* the path of the file name in it */
    std::string file(const std::string &name) const { return path_ + "/" + name; }
private:
    std::string path_;
};
} // namespace xpp

#endif
