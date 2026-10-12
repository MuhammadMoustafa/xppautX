/* A model's own files: see model_files.h. */
#include "model_files.h"
#include "model.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"
#include "xpp_ui.h"
#include "xpp_zip.h"
#include "xpp_util.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

namespace xpp {

std::string include_path(const std::string &including, const std::string &name)
{
    const size_t slash = including.find_last_of("/\\");
    if (files::is_absolute(name) || slash == std::string::npos) return name;
    return including.substr(0, slash + 1) + name;
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
    const Result<bool> remembered = prefs::outside_path_approved(model, found.path);
    if (!remembered) return std::unexpected(remembered.error());
    if (!*remembered) {
        const std::string source = at.source.empty() && at.line > 0 ? model_source_line(m, at.file, at.line) : at.source;
        const std::string who = at.line > 0 ? xpp::format("line {} of {}", at.line, at.file) : std::string("the command line");
        const std::string question =
            xpp::format("{} names \"{}\"{}, outside the folder of {}{}. Allow it?", who, name,
                        name == found.path ? std::string() : xpp::format(" ({})", found.path), model,
                        source.empty() ? std::string() : xpp::format(" [{}]", source));
        const std::array<std::string, 3> choices{"Allow always", "Allow once", "Refuse"};
        const int key = Choose("Path outside the model's folder", question, choices, OUTSIDE_PATH_KEYS, OUTSIDE_PATH_ONCE);
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
    const auto it = std::find_if(m.files.begin(), m.files.end(), [&name](const ModelFile &f) { return f.name == name; });
    return it == m.files.end() ? -1 : it - m.files.begin();
}

/* Decode a recorded file once, while loading; workers only read the index. */
void index_source_lines(Model &m, const ModelFile &file)
{
    m.source_lines.try_emplace(file.name, "model", file.name, file.bytes);
}

} // namespace

bool read_model_file(Model &m, const std::string &name, std::string &bytes)
{
    bytes.clear();
    const long i = file_index(m, name);
    if (!m.saved_in.empty()) {
        if (i < 0) return false;
        bytes = m.files[static_cast<size_t>(i)].bytes;
        return true;
    }
    if (name.empty()) return false;
    if (i < 0 && m.files.size() >= zip::archive_entries_limit) return false;
    size_t available = zip::archive_bytes_limit;
    for (const ModelFile &file : m.files)
        if (file.name != name) available -= file.bytes.size();
    UniqueFile fp(files::open_read_within(name, files::split_path(m.this_file).first, m.outside_paths));
    if (!read_bytes(fp.get(), bytes, available)) return false;
    if (i < 0) m.files.push_back({name, bytes});
    return true;
}

UniqueFile open_model_file(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return UniqueFile();
    if (m.saved_in.empty()) return UniqueFile(files::open_read_within(name, files::split_path(m.this_file).first, m.outside_paths));
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
    const auto lines = m.source_lines.find(name);
    if (lines != m.source_lines.end()) return std::string(lines->second.text(n));
    /* Before the builder has indexed a source (a converter diagnostic),
       use the recorded bytes through the same line reader. No disk read. */
    const long i = file_index(m, name);
    return i < 0 ? std::string() : LineReader::of_text(m.files[static_cast<size_t>(i)].bytes).line(n);
}

std::vector<std::string> model_source_lines(const Model &m, const std::string &name)
{
    std::vector<std::string> lines;
    const long i = file_index(m, name);
    if (i < 0) return lines;
    LineReader r = LineReader::of_text(m.files[static_cast<size_t>(i)].bytes);
    while (const std::optional<std::string_view> line = r.next()) lines.emplace_back(*line);
    return lines;
}

Place model_place(const Model &m, const odex::Pos &pos)
{
    const size_t f = static_cast<size_t>(pos.file);
    Place p{f < m.statement_files.size() ? m.statement_files[f] : m.this_file, pos.line, pos.col};
    p.source = model_source_line(m, p.file, p.line);
    return p;
}

void index_model_places(Model &m)
{

    // Data tables need no source index; only files that supplied statements do.
    for (const std::string &file : m.statement_files) {
        const long i = file_index(m, file);
        if (i >= 0) index_source_lines(m, m.files[static_cast<size_t>(i)]);
    }
    /* a statement that defines name first (an equation, a function, a
       table ...), then the lists that name it (par, aux, wiener ...).
       Reverse assignment keeps the first defining statement, including
       repeated .ode Markov declarations that share one state variable. */
    for (auto it=m.statements.rbegin();it!=m.statements.rend();++it) {
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
            m.names.try_emplace(converted(st.name)).first->second.pos = st.pos;
            break;
        case odex::Statement::Kind::Set:
            if (auto entry=m.names.find(converted(st.name));entry!=m.names.end() && entry->second.kind==Model::NameKind::Set)
                entry->second.pos=st.pos;
            break;
        default:
            break;
        }
    }
    for (const odex::Statement &st : m.statements) {
        for (const odex::Binding &b : st.bindings)
            if (auto [it, inserted] = m.names.try_emplace(converted(b.name)); inserted || it->second.pos.line == 0)
                it->second.pos = b.pos.line > 0 ? b.pos : st.pos;
        if(st.kind==odex::Statement::Kind::Wiener)
            for (size_t k = 0; k < st.names.size(); k++)
                if (auto [it, inserted] = m.names.try_emplace(converted(st.names[k])); inserted || it->second.pos.line == 0)
                    it->second.pos = k < st.name_positions.size() && st.name_positions[k].line > 0 ? st.name_positions[k] : st.pos;
    }
    for (auto &[name, entry] : m.names) {
        decltype(entry.origins){}.swap(entry.origins);
        decltype(entry.array_members){}.swap(entry.array_members);
        entry.load_constant.reset();
    }
}

Model::Name &Model::define_name(std::string_view text,NameKind kind,int index,const odex::Pos &pos)
{
    Name &entry=names[converted(text)];
    if(entry.written.empty())entry.written=text;
    if(entry.pos.line==0)entry.pos=pos;
    entry.kind=kind;
    entry.index=index;
    return entry;
}

const Model::Name *Model::name(std::string_view text) const
{
    const auto it = names.find(converted(text));
    return it == names.end() ? nullptr : &it->second;
}

Place model_place(const Model &m, std::string_view name)
{
    const Model::Name *entry = m.name(name);
    return entry ? model_place(m, entry->pos) : Load::place();
}

Place model_place(const Model &m, odex::Statement::Kind kind, int k)
{
    for (const odex::Statement &st : m.statements)
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

} // namespace xpp
