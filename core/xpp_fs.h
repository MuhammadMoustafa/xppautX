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
   autoX's run files are streamed through Sinks as a run goes and copied into place in pieces when it ends (W739); the writers and exporters stream through it too (W740). */

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
#include "xpp_text.h"

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
    /* the bytes appended, or the error naming the file when the system refused. A write that failed also
       fails commit(), so a writer that formats row after row may leave each result to the commit */
    virtual Result<> write(std::string_view bytes) = 0;
    /* text formatted as xpp::format does and appended, for a writer of many small pieces: its failure is the
       commit's to report (see write), so it returns nothing */
    template <class... Args>
    void print(std::format_string<Args...> fmt, Args &&...args)
    {
        static_cast<void>(write(xpp::format(fmt, std::forward<Args>(args)...)));
    }
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
    /* up to length bytes of the file at path from byte offset on: fewer at its end, none past it; length is at most
       file_range_limit. The error names the file and `where`. What a run reads of a file too long to hold whole */
    virtual Result<std::string> read_range(std::string where, std::string_view path, std::size_t offset, std::size_t length) = 0;
    /* a plain file inside folder ("" the working folder), or one outside it that the caller has the
       user's approval for (allowed_outside: resolve_path's text of each): its bytes, at most limit.
       Nothing for a file that is absent, linked, outside without approval, or longer than limit:
       a model's companion is read within the folder it names */
    virtual std::optional<std::string> read_within(std::string_view path, std::string_view folder,
                                                   std::span<const std::string> allowed_outside, std::size_t limit) = 0;
    /* path against folder: inside, outside or refused (see PathKind); the file need not exist */
    virtual PathCheck resolve_path(std::string_view path, std::string_view folder) = 0;

    /* a Sink for a file replaced at commit, in binary or text mode; the error (naming the file, with the
       advice for a refused folder, and `at` as its place when that names one and the file system is not serving
       reads) when it cannot be opened */
    virtual Result<std::unique_ptr<Sink>> open_sink(std::string_view path, bool binary, const Place &at) = 0;
    /* nothing when path may be written (status OK); else the error naming the file and why (and `at` as its place
       as above, else the command's), after the replay's path guard was told (write_path_ok) */
    virtual Result<> check_destination(std::string_view path, const Place &at) = 0;
    /* path is a safe destination: a plain base name, and no link, folder or device already there */
    virtual bool destination_ok(std::string_view path) = 0;

    /* path names a file or a folder */
    virtual bool exists(std::string_view path) = 0;
    /* path names a folder */
    virtual bool is_dir(std::string_view path) = 0;
    /* the length in bytes of the plain file at path (a command's file just written, read back), or the error
       naming the file and why it cannot be opened */
    virtual Result<unsigned long long> size_of(std::string_view path) = 0;
    /* a file can be created in the folder dir (probed), false for "" */
    virtual bool dir_writable(std::string_view dir) = 0;
    /* deletes the file path: 0 on success */
    virtual int remove_file(std::string_view path) = 0;
    /* a scratch folder of its own (its path, "" when none could be made), and removing it with what it holds */
    virtual std::string make_temp_dir() = 0;
    virtual void remove_temp_dir(std::string_view dir) = 0;
    /* the per-user settings folder's files (settings.json, state.json): the path of `name` in it ("" when the system names no
       folder), its whole text (nullopt when it is not there; an error when it cannot be read, is longer than
       limit, or there is no folder), written whole (the folder made first, temp-then-rename), and removed (no
       file is all defaults; an error when it stays). `where` is the Error's */
    virtual std::string config_path(std::string_view name) = 0;
    virtual Result<std::optional<std::string>> read_config(std::string_view where, std::string_view name, std::size_t limit) = 0;
    virtual Result<> write_config(std::string_view where, std::string_view name, std::string_view bytes) = 0;
    virtual Result<> remove_config(std::string_view where, std::string_view name) = 0;
    /* a recording's replay supplies the files it reads (files::serve_reads): nothing is imported from or written to
       the disk behind its back */
    virtual bool serving_reads() = 0;
    /* path itself when it is absolute, else under the working folder */
    virtual std::string absolute(std::string_view path) = 0;
    /* the working folder ("" when it cannot be had), and moving it: 0 on success */
    virtual std::string working_dir() = 0;
    virtual int change_dir(std::string_view path) = 0;
};

/* the most a read_range gives, and the piece the files are streamed in: a solution of autoX's is far below the one, a
   copy or a pass over a file never holds more than the other */
inline constexpr std::size_t file_range_limit = 512u * 1024 * 1024;
inline constexpr std::size_t file_chunk_bytes = 64u * 1024;

/* the settings file `name` read and parsed, parse(text, path) giving the Result<T>; T{} (the defaults) when the
   file is not there */
template <class T, class Parse>
Result<T> load_config(FileSystem &fs, std::string_view where, std::string_view name, std::size_t limit, Parse parse)
{
    const Result<std::optional<std::string>> text = fs.read_config(where, name, limit);
    if (!text) return std::unexpected(text.error());
    if (!*text) return T{};
    return parse(**text, fs.config_path(name));
}

/* the file at path in pieces of at most file_chunk_bytes, each handed to piece(std::string_view) in order; the error when it
   cannot be read. Memory stays one piece, whatever the file's length */
template <class F>
Result<> for_each_chunk(FileSystem &fs, const std::string &where, std::string_view path, F &&piece)
{
    for (std::size_t offset = 0;;) {
        Result<std::string> chunk = fs.read_range(where, path, offset, file_chunk_bytes);
        if (!chunk) return std::unexpected(chunk.error());
        if (chunk->empty()) return {};
        piece(std::string_view(*chunk));
        offset += chunk->size();
    }
}

/* the file at path appended to sink, streamed in pieces */
inline Result<> copy_into(FileSystem &fs, Sink &sink, std::string_view path)
{
    Result<> written;
    const Result<> read = for_each_chunk(fs, "copy", path, [&](std::string_view piece) {
        if (written) written = sink.write(piece);
    });
    return read ? written : read;
}

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
