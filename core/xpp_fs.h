#pragma once

/* The file system, as the inner rings see it (W731, Discussion #808: text in, sink out). Inner code
   never opens a file: it is handed a FileSystem, asks it for a file's text or for a Sink to write
   into, and works on the text. The kernel owns this interface; the files module implements it
   (xpp_fs_local.h: LocalFileSystem over xpp_io and xpp_files, the same bounds, link refusals and
   temp-then-rename every file of ours has always had), and a test implements it in memory.

   An abstract class, not a table of functions: a file system carries state (the folder it works
   in, the files an in-memory one holds), and a derived class holds it with its functions.

   Who holds it: passed by reference. The program's start makes the local one and gives it to
   load_model; a Model keeps the reference it was loaded with (Model::fs) for the commands that read
   or write later. Never a global. Only what a caller needs is here: the inner rings read whole files
   (text or bytes), write through a Sink, and ask the questions a loader asks of the folders.
   Streaming writers and autoX's scratch files go through the Sink later (W739, W740). */

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "xpp_error.h"
#include "xpp_lines.h"

namespace xpp {

/* Where a path a model names lies, against the folder of the model: inside it, outside it (the user
   decides, W286), or refused for good (a link on the way, an unsafe name). path is the path to open
   when not refused: absolute, normalised, so what is checked, shown, remembered and opened is one text. */
enum class PathKind { inside, outside, refused };
struct PathCheck {
    PathKind kind;
    std::string path;
};

/* The answers a user gave to "create this folder?", kept for the session that asked, so the same
   question is not asked twice (files::prepare_write_folder reads them through the XppUi). */
struct WriteFolders {
    std::map<std::string, bool> answers;
};

/* A file being written: the bytes go to a temporary file beside the destination, which replaces it
   only at commit(). A Sink that goes without commit() leaves the destination untouched, and so does
   abort(). The destructor discards. */
class Sink {
public:
    virtual ~Sink() = default;
    /* the bytes appended, or the error naming the file when the system refused */
    virtual Result<> write(std::string_view bytes) = 0;
    /* the file is complete: put it in place, or the error (naming the file) */
    virtual Result<> commit() = 0;
    /* the destination as it was, and the temporary file gone */
    virtual void abort() = 0;
    virtual const std::string &path() const = 0;
};

class FileSystem {
public:
    virtual ~FileSystem() = default;

    /* the whole file at path, at most limit bytes; the error names the file and `where`, and says
       why (not there, a folder, cannot be read, longer than limit) */
    virtual Result<std::string> read(std::string where, std::string_view path, std::size_t limit) = 0;
    /* a plain file inside folder ("" the working folder), or one outside it that the caller has the
       user's approval for (allowed_outside: resolve_path's text of each): its bytes, at most limit.
       Nothing for a file that is absent, linked, outside without approval, or longer than limit:
       a model's companion is read within the folder it names */
    virtual std::optional<std::string> read_within(std::string_view path, std::string_view folder,
                                                   std::span<const std::string> allowed_outside, std::size_t limit) = 0;
    /* path against folder: inside, outside or refused (see PathKind); the file need not exist */
    virtual PathCheck resolve_path(std::string_view path, std::string_view folder) = 0;

    /* a Sink for a file replaced at commit, in binary or text mode; the error (naming the file, with the
       advice for a refused folder) when it cannot be opened */
    virtual Result<std::unique_ptr<Sink>> open_sink(std::string_view path, bool binary) = 0;
    /* path is a safe destination: a plain base name, and no link, folder or device already there */
    virtual bool destination_ok(std::string_view path) = 0;

    /* path names a file or a folder */
    virtual bool exists(std::string_view path) = 0;
    /* path names a folder */
    virtual bool is_dir(std::string_view path) = 0;
    /* path itself when it is absolute, else under the working folder */
    virtual std::string absolute(std::string_view path) = 0;
    /* the working folder ("" when it cannot be had), and moving it: 0 on success */
    virtual std::string working_dir() = 0;
    virtual int change_dir(std::string_view path) = 0;
};

/* parse(lines) over the file at path, read whole through fs (at most limit bytes); the error when it
   cannot be read (its place the file, no line) or parse stops at a line (ReadFailed) */
template <class F>
auto read_file_lines(FileSystem &fs, std::string where, std::string_view path, F &&parse, std::size_t limit)
    -> Result<std::invoke_result_t<F &, Lines &>>
{
    Result<std::string> bytes = fs.read(where, path, limit);
    if (!bytes) return std::unexpected(bytes.error());
    return read_lines(std::move(where), std::string(path), *bytes, parse);
}

} // namespace xpp
