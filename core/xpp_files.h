#ifndef XPP_FILES_H
#define XPP_FILES_H

#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The model's folder as the page's workspace (xpp_files.cpp; docs/ui-v2.md
   section 4, docs/protocol.md "Files"). The browser's file dialogs cannot
   tell a page where a file lives, so the page copies what the user picks
   into the folder XPP reads and writes (the working directory) and fetches
   back what XPP wrote there. xpp_http.cpp serves this as /files, and the
   `file` command of the protocol does the same for --server clients.

   Only base names in the working directory are reachable: no separators,
   no "..", no leading dot, no control or reserved characters, at most 255
   bytes, not a Windows device name (xpp_files_name_ok). A name that is a
   symbolic link, a folder or anything but a plain file is refused, so
   nothing outside the folder is ever read or written through it. A write
   goes to a hidden temporary file in the same folder first and is renamed
   into place only when complete: a failed or cut upload leaves nothing.

   Thread-safe (the HTTP thread and the core thread both call it). This
   header includes no core header, so xpp_http.cpp can use it. */

#define XPP_FILES_CAP (64ULL << 20) /* the largest upload, bytes */

/* what a call gives back */
#define XPP_FILES_OK 0
#define XPP_FILES_BAD_NAME 1  /* not a safe base name */
#define XPP_FILES_NOT_FOUND 2
#define XPP_FILES_REFUSED 3   /* a link, a folder or a device: not a plain file */
#define XPP_FILES_TOO_LARGE 4 /* more than the cap */
#define XPP_FILES_IO 5        /* the system refused, or the data was cut short */

int xpp_files_name_ok(const char *name);
const char *xpp_files_status_text(int status); /* in words, for an error */

/* opens a file for reading ("rb"); *size its length */
int xpp_files_open(const char *name, FILE **fp, unsigned long long *size);

/* a write in steps: begin, write the bytes as they come (more than `cap`
   in all fails with XPP_FILES_TOO_LARGE), then commit, or abort. Commit
   and abort end the XppFilePut whatever they return; after a failed
   write, abort. Commit (below, C++) gives the size and SHA-256 of what
   was written.
   The write is xpp_io.h's writer (binary): the temp file beside the name
   and its rename are the same as every other replace's. */
typedef struct XppFilePut XppFilePut;
int xpp_files_put_begin(const char *name, unsigned long long cap, XppFilePut **put);
int xpp_files_put_write(XppFilePut *put, const void *data, size_t n);
void xpp_files_put_abort(XppFilePut *put);

/* "read" when a file selector with this title opens a file, "write" when
   it saves one (the `mode` of the `file` ask) */
const char *xpp_files_ask_mode(const char *title);

/* Atomically replaces `to` with `from` (POSIX rename(), which already
   replaces; xpp_replace_file on Windows, where rename() does not): 0 on
   success. The one place that knows the platform difference; core/xpp_io.cpp's
   writer (core/xpp_io.h) calls this for its own temp-then-rename commit
   instead of duplicating it. */
int xpp_files_replace_file(const char *from, const char *to);

/* ---- the core's own files, by any path (W32b) -------------------------------
   The one place the core opens, copies, moves, deletes and probes files
   and makes its temp folders: the calls above are the page's, limited to
   base names in the model's folder; these are the core's (the model's
   outputs, AUTO's fort.* and diagram files, its scratch folder).
   tools/filecheck.sh counts every direct fopen/remove/rename/mkdir/...
   left elsewhere. Reading and writing go through xpp_io.h's handles
   (xpp::Writer: write, binary, append, a replace only on commit;
   xpp::LineReader, xpp::TokenReader and xpp::open_read for reading),
   which open their files here. */

/* A stream the caller keeps open across calls and closes itself with
   fclose (AUTO's fort.3/7/8/9 during a run, the array plot's GIF movie,
   an input script): fopen's modes, NULL on failure. A file opened and
   closed in one scope uses a handle of xpp_io.h instead. */
FILE *xpp_files_open_stream(const char *path, const char *mode);
/* the descriptor a standard stream (stdout, stderr) writes through, given
   one (the null device) if it has none: the Windows exe is a GUI-subsystem
   program, and started with no console (Explorer, a shortcut,
   Start-Process) the C library leaves stdout and stderr without a
   descriptor (_fileno -2), where a dup2 onto 1 and 2 never reaches them
   and all the core prints, AUTO's table included, was lost to the page
   (T27). -1 when it cannot be given one. */
int xpp_files_stream_fd(FILE *f);
/* Creates path for writing, failing when it exists already (a link
   included, which is never followed): the temp files of a replace.
   binary 0 is text mode ("w"), 1 binary ("wb"). NULL on failure. */
FILE *xpp_files_create_new(const char *path, int binary);
/* 1 when path names a file or a folder */
int xpp_files_exists(const char *path);
/* 1 when path names a folder (a link to one included) */
int xpp_files_is_dir(const char *path);
/* 1 when a file can be created in dir: probed by creating one and
   removing it (a folder can exist without being writable) */
int xpp_files_dir_writable(const char *dir);
/* the file selector's current folder (core/read_dir.cpp's old cur_dir,
   folded in here, W46b): "" until refreshed. The pointer is valid until
   the next call to xpp_files_refresh_cur_dir or xpp_files_change_dir. */
const char *xpp_files_cur_dir(void);
/* sets it from getcwd(): 1 on success; on failure a WARN and "" */
int xpp_files_refresh_cur_dir(void);
/* chdir(path), then refreshes the current folder; path NULL clears the
   current folder without touching the process's own (form_ode.cpp's
   console (c)d and json_prompts.cpp's file selector "cd" answer).
   0 on success, 1 on failure (the chdir failed, a WARN, or the refresh
   after it did) -- the historical change_directory()'s own convention. */
int xpp_files_change_dir(const char *path);
/* deletes the file path: 0 on success */
int xpp_files_remove(const char *path);
/* to becomes a byte-for-byte copy of from, written beside it and renamed
   into place, so it is either the whole copy or left as it was; a WARN
   when from cannot be read or to written */
void xpp_files_copy(const char *from, const char *to);
/* to becomes from's bytes followed by its own, the same way (AUTO's run
   output put ahead of the diagram files it keeps: its "append"); a copy
   when to does not exist */
void xpp_files_prepend(const char *from, const char *to);
/* from becomes to, replacing it; when the system refuses (Windows, a
   source still open elsewhere) a copy, then from is removed if it can be */
void xpp_files_move(const char *from, const char *to);

/* Removes every file directly in dir (no folders are expected there),
   then dir itself. NULL does nothing. */
void xpp_files_remove_temp_dir(const char *dir);
/* issue #32: removes the scratch folders of runs that were killed before
   they could remove their own (xpp_files_make_temp_dir's naming, whose
   pid names no running process); called once at start */
void xpp_files_cleanup_stale_temp_dirs(void);

/* the protocol's {"cmd":"file","op":..,"name":..,"data":..}: op "list",
   "get" or "put"; name_json and data_json point at the JSON values of
   "name" and "data" in the command (NULL when absent). Sends one `file`
   event through emit. */
void xpp_files_command(const char *op, const char *name_json, const char *data_json,
                       void (*emit)(const char *line, size_t n));

#ifdef __cplusplus
}

#include <string>
#include <vector>

/* {"files":[{"name":..,"size":..,"mtime":..,"sha256":".."},...]}: the plain
   files of the working directory whose names are reachable, sorted by
   name; mtime in seconds since 1970 */
std::string xpp_files_list_json();

/* the put's commit (above): its size in *size, the SHA-256 of what was
   written in sha256 (64 hex digits) */
int xpp_files_put_commit(XppFilePut *put, unsigned long long *size, std::string &sha256);

/* AUTO's private scratch folder: "xppautoX-<pid>-<N>", mode 0700, under
   $TMPDIR or /tmp (POSIX) or the system temp path (Windows): its absolute
   path, or empty on failure */
std::string xpp_files_make_temp_dir();

/* a folder's entry: its name, and whether it is a folder itself (a link
   followed) */
struct XppDirEntry {
    std::string name;
    bool folder;
};
/* the entries of the folder dir ("." and ".." included, in the order the
   system gives them) into out; false (out empty) when dir cannot be read */
bool xpp_files_list_dir(const char *dir, std::vector<XppDirEntry> &out);

/* the working directory, whatever its length ("" when it cannot be had) */
std::string xpp_files_working_dir();
/* the folders of direct and its files that match the Unix-style wildcard
   wild (*, ?, [..]), each list sorted; false (dirs/files left empty, a
   WARN) when direct cannot be read. core/read_dir.cpp's old list_folder,
   folded in here (W46b). */
bool xpp_files_list_matching(const char *wild, const char *direct,
                             std::vector<std::string> &dirs, std::vector<std::string> &files);
#endif
#endif
