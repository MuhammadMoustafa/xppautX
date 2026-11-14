/* A model's own files: see model_files.h. */
#include "odex.h"
#include "model_files.h"
#include "model_place.h"
#include "model.h"
#include "model_record.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"
#include "xpp_ui.h"
#include "xpp_zip.h"
#include "model_options.h"
#include "session.h"
#include "expr.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>

namespace xpp {

std::string include_path(const std::string &including, const std::string &name)
{
    const size_t slash = including.find_last_of("/\\");
    if (files::is_absolute(name) || slash == std::string::npos) return name;
    return including.substr(0, slash + 1) + name;
}

std::string relative_to_model(const std::string &including, const std::string &path)
{
    const size_t slash = including.find_last_of("/\\");
    if (slash == std::string::npos || !path.starts_with(including.substr(0, slash + 1))) return path;
    return path.substr(slash + 1);
}

Result<> approve_model_path(Model &m, const std::string &name, const Place &at)
{
    /* a saved model reads its own copies; nothing is read from the disk */
    if (!m.saved_in.empty() || name.empty()) return {};
    const files::PathCheck found = files::resolve_path(name, files::split_path(m.this_file).first);
    if (found.kind == files::PathKind::inside) return {};
    const Place place = at.file.empty() ? Place{m.this_file} : at;
    if (found.kind == files::PathKind::refused)
        return fail(std::string(PATH_REFUSED), xpp::format("names {}, a link or a path that is not a plain name: refused", name), place);
    if (std::ranges::find(m.outside_paths, found.path) != m.outside_paths.end()) return {};
    /* The approval owner is the absolute native-model path, including
       the .odex a foreign .ode becomes. */
    const std::string model = files::absolute(files::has_extension(m.this_file, ".ode") ? odex::odex_name(m.this_file) : m.this_file);
    bool remembered = false;
    if (!m.outside_once_only) {
        const Result<bool> kept = prefs::outside_path_approved(model, found.path);
        if (!kept) return std::unexpected(kept.error());
        remembered = *kept;
    }
    if (!remembered) {
        const std::string source = at.source.empty() && at.line > 0 ? model_source_line(m, at.file, at.line) : at.source;
        const std::string who = at.line > 0 ? xpp::format("line {} of {}", at.line, at.file) : std::string("the command line");
        const std::string question =
            xpp::format("{} names \"{}\"{}, outside the folder of {}{}. Allow it?", who, name,
                        name == found.path ? std::string() : xpp::format(" ({})", found.path), model,
                        source.empty() ? std::string() : xpp::format(" [{}]", source));
        const std::array<std::string, 3> choices{"Allow always", "Allow once", "Refuse"};
        const std::array<std::string, 2> once_choices{"Allow once", "Refuse"};
        const int key = m.outside_once_only ? Choose("Path outside the model's folder", question, once_choices, OUTSIDE_PATH_KEYS_ONCE_ONLY, OUTSIDE_PATH_ONCE)
                                            : Choose("Path outside the model's folder", question, choices, OUTSIDE_PATH_KEYS, OUTSIDE_PATH_ONCE);
        if (key != OUTSIDE_PATH_ONCE && key != OUTSIDE_PATH_ALWAYS)
            return fail(std::string(PATH_REFUSED), xpp::format("names {}, outside the model's folder: refused", found.path), place);
        if (key == OUTSIDE_PATH_ALWAYS) {
            const Result<> kept = prefs::approve_outside_path(model, found.path);
            if (!kept) return std::unexpected(kept.error());
        }
    }
    m.outside_paths.push_back(found.path);
    return {};
}

namespace {

/* the index of the model's file name in m.files, or -1 */
long file_index(const Model &m, const std::string &name)
{
    const auto it = std::find_if(m.record->files.begin(), m.record->files.end(), [&name](const ModelFile &f) { return f.name == name; });
    return it == m.record->files.end() ? -1 : it - m.record->files.begin();
}

/* The bytes a replacement or a new companion may use, excluding its earlier
   copy. Reads and later validated picks share this archive budget. */
std::size_t remaining_file_bytes(const Model &m, const std::string &name)
{
    std::size_t available = m.record->files_bytes_limit;
    for (const auto &file : m.record->files) {
        if (file.name == name) continue;
        if (file.bytes.size() > available) return 0;
        available -= file.bytes.size();
    }
    return available;
}

/* Decode a recorded file once, while loading; workers only read the index. */
void index_source_lines(Model &m, const ModelFile &file)
{
    m.record->source_lines.try_emplace(file.name, "model", file.name, file.bytes);
}

} // namespace

Result<> remember_model_file(Model &m, const std::string &name, std::string bytes)
{
    const long i = file_index(m, name);
    if (i < 0 && m.record->files.size() >= zip::archive_entries_limit)
        return fail_reading("model files", "too many companion files for a session", name);
    if (bytes.size() > remaining_file_bytes(m, name))
        return fail_reading("model files", "companion files exceed the session byte budget", name);
    if (i < 0) m.record->files.push_back({name, std::move(bytes)});
    else m.record->files[static_cast<std::size_t>(i)].bytes = std::move(bytes);
    m.record->source_lines.erase(name);
    index_source_lines(m, m.record->files[static_cast<std::size_t>(file_index(m, name))]);
    return {};
}

bool read_model_file(Model &m, const std::string &name, std::string &bytes)
{
    bytes.clear();
    const long i = file_index(m, name);
    if (i >= 0) {
        bytes = m.record->files[static_cast<size_t>(i)].bytes;
        return true;
    }
    if (!m.saved_in.empty()) return false;
    if (name.empty()) return false;
    if (m.record->files.size() >= zip::archive_entries_limit) return false;
    const std::size_t available = remaining_file_bytes(m, name);
    UniqueFile fp(files::open_read_within(name, files::split_path(m.this_file).first, m.outside_paths));
    if (!read_bytes(fp.get(), bytes, available)) return false;
    return remember_model_file(m, name, bytes).has_value();
}

UniqueFile open_model_file(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return UniqueFile();
    if (!m.saved_copies) m.saved_copies = std::make_shared<TempDir>();
    if (m.saved_copies->path().empty()) return UniqueFile();
    const std::string copy = m.saved_copies->file(std::to_string(file_index(m, name)));
    Writer w = Writer::binary(copy.c_str());
    if (!w || !w.write(bytes) || !w.commit()) return UniqueFile();
    return open_read(copy.c_str());
}

LineReader model_file_lines(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return LineReader();
    return LineReader::of_text(std::move(bytes));
}

std::string model_source_line(const Model &m, const std::string &name, int n)
{
    if (n <= 0) return {};
    const auto lines = m.record->source_lines.find(name);
    if (lines != m.record->source_lines.end()) return std::string(lines->second.text(n));
    /* Before the builder has indexed a source (a converter diagnostic),
       use the recorded bytes through the same line reader. No disk read. */
    const long i = file_index(m, name);
    return i < 0 ? std::string() : LineReader::of_text(m.record->files[static_cast<size_t>(i)].bytes).line(n);
}

std::vector<std::string> model_source_lines(const Model &m, const std::string &name)
{
    std::vector<std::string> lines;
    const long i = file_index(m, name);
    if (i < 0) return lines;
    LineReader r = LineReader::of_text(m.record->files[static_cast<size_t>(i)].bytes);
    while (const std::optional<std::string_view> line = r.next()) lines.emplace_back(*line);
    return lines;
}

Place model_place(const Model &m, const odex::Pos &pos)
{
    const size_t f = static_cast<size_t>(pos.file);
    Place p{f < m.record->statement_files.size() ? m.record->statement_files[f] : m.this_file, pos.line, pos.col};
    p.source = model_source_line(m, p.file, p.line);
    return p;
}

void set_model_source(Model &m, std::string_view text)
{
    m.source.clear();
    LineReader lr = LineReader::of_text(std::string(text));
    while (std::optional<std::string_view> line = lr.next()) m.source.push_back(std::string(*line) + "\n");
}

void replace_model_text(Model &m, std::string bytes)
{
    set_model_source(m, bytes);
    const long i = file_index(m, m.this_file);
    if (i < 0) return;
    ModelFile &file = m.record->files[static_cast<size_t>(i)];
    file.bytes = std::move(bytes);
    m.record->source_lines.erase(file.name);
    index_source_lines(m, file);
}

void index_model_places(Model &m)
{

    // Data tables need no source index; only files that supplied statements do.
    for (const std::string &file : m.record->statement_files) {
        const long i = file_index(m, file);
        if (i >= 0) index_source_lines(m, m.record->files[static_cast<size_t>(i)]);
    }
    /* a statement that defines name first (an equation, a function, a
       table ...), then the lists that name it (par, aux, wiener ...).
       Reverse assignment keeps the first defining statement, including
       repeated .ode Markov declarations that share one state variable. */
    for (auto it=m.record->statements.rbegin();it!=m.record->statements.rend();++it) {
        const odex::Statement &st=*it;
        switch (st.kind) {
        case odex::Statement::Kind::Ode:
        case odex::Statement::Kind::Map:
        case odex::Statement::Kind::Volterra:
        case odex::Statement::Kind::Fixed:
        case odex::Statement::Kind::Fun:
        case odex::Statement::Kind::Table:
        case odex::Statement::Kind::Markov:
        case odex::Statement::Kind::Network:
        case odex::Statement::Kind::Vector:
        case odex::Statement::Kind::Solv:
            m.record->names.try_emplace(converted(st.name)).first->second.pos = st.pos;
            break;
        case odex::Statement::Kind::Set:
            if (auto entry=m.record->names.find(converted(st.name));entry!=m.record->names.end() && entry->second.kind==ModelRecord::NameKind::Set)
                entry->second.pos=st.pos;
            break;
        default:
            break;
        }
    }
    for (const odex::Statement &st : m.record->statements) {
        for (const odex::Binding &b : st.bindings)
            if (auto [it, inserted] = m.record->names.try_emplace(converted(b.name)); inserted || it->second.pos.line == 0)
                it->second.pos = b.pos.line > 0 ? b.pos : st.pos;
        if(st.kind==odex::Statement::Kind::Wiener)
            for (size_t k = 0; k < st.more().names.size(); k++)
                if (auto [it, inserted] = m.record->names.try_emplace(converted(st.more().names[k])); inserted || it->second.pos.line == 0)
                    it->second.pos = k < st.more().name_positions.size() && st.more().name_positions[k].line > 0 ? st.more().name_positions[k] : st.pos;
    }
    for (auto &[name, entry] : m.record->names) {
        decltype(entry.origins){}.swap(entry.origins);
        decltype(entry.array_members){}.swap(entry.array_members);
        entry.load_constant.reset();
    }
}

Model::Model() : record(std::make_unique<ModelRecord>()) {}
Model::~Model() = default;
ModelRecord::ModelRecord() = default;
ModelRecord::~ModelRecord() = default;

ModelRecord::Name &ModelRecord::define_name(std::string_view text,NameKind kind,int index,const odex::Pos &pos)
{
    Name &entry=names[converted(text)];
    if(entry.written.empty())entry.written=text;
    if(entry.pos.line==0)entry.pos=pos;
    entry.kind=kind;
    entry.index=index;
    return entry;
}

const ModelRecord::Name *ModelRecord::name(std::string_view text) const
{
    const auto it = names.find(converted(text));
    return it == names.end() ? nullptr : &it->second;
}

Place model_place(const Model &m, std::string_view name)
{
    const ModelRecord::Name *entry = m.record->name(name);
    if(entry)return model_place(m, entry->pos);
    const std::string option=odex_option_key(name);
    for(auto line=m.options.rbegin();line!=m.options.rend();++line)
        for(const auto &[key,value]:line->items)
            if(!option.empty() && odex_option_key(key)==option)return line->where;
    return Load::place();
}

Place model_statement_place(const Model &m, odex::Statement::Kind kind, int k)
{
    for (const odex::Statement &st : m.record->statements)
        if (st.kind == kind && k-- == 0) return model_place(m, st.pos);
    return {};
}

bool is_model_text(std::string_view bytes)
{
    return !zip::is_zip(bytes) && bytes.find('\0') == std::string_view::npos;
}

std::string model_title(const Model &m)
{
    if (m.saved_in.empty()) return m.this_file;
    return m.this_file + " (saved in " + xpp::files::split_path(m.saved_in).second + ")";
}

std::unique_ptr<Model> scratch_model(const Model &m)
{
    std::unique_ptr<Model> scratch = std::make_unique<Model>();
    scratch->this_file = m.this_file;
    scratch->outside_paths = m.outside_paths;
    scratch->load_dir = m.load_dir;
    if (!m.saved_in.empty()) { /* its files are the session's copies, never the disk's */
        scratch->saved_in = m.saved_in;
        scratch->record->files = m.record->files;
    }
    return scratch;
}

} // namespace xpp
