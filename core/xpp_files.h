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

/* {"files":[{"name":..,"size":..,"mtime":..,"sha256":".."},...]}: the plain
   files of the working directory whose names are reachable, sorted by
   name; mtime in seconds since 1970. xpp_malloc'd, length in *len. */
char *xpp_files_list_json(size_t *len);

/* opens a file for reading ("rb"); *size its length */
int xpp_files_open(const char *name, FILE **fp, unsigned long long *size);

/* a write in steps: begin, write the bytes as they come (more than `cap`
   in all fails with XPP_FILES_TOO_LARGE), then commit, or abort. Commit
   and abort end the XppFilePut whatever they return; after a failed
   write, abort. Commit gives the size and SHA-256 of what was written. */
typedef struct XppFilePut XppFilePut;
int xpp_files_put_begin(const char *name, unsigned long long cap, XppFilePut **put);
int xpp_files_put_write(XppFilePut *put, const void *data, size_t n);
int xpp_files_put_commit(XppFilePut *put, unsigned long long *size, char sha256[65]);
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

/* the protocol's {"cmd":"file","op":..,"name":..,"data":..}: op "list",
   "get" or "put"; name_json and data_json point at the JSON values of
   "name" and "data" in the command (NULL when absent). Sends one `file`
   event through emit. */
void xpp_files_command(const char *op, const char *name_json, const char *data_json,
                       void (*emit)(const char *line, size_t n));

#ifdef __cplusplus
}

#include <string>
namespace xpp {
/* xpp_files_list_json's JSON as a std::string, for C++ callers */
std::string files_list_json();
} // namespace xpp
#endif
#endif
