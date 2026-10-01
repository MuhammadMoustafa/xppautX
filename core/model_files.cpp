/* A model's own files: see model_files.h. */
#include "model_files.h"
#include "model.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_zip.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace xpp {

std::string include_path(const std::string &including, const std::string &name)
{
    const size_t slash = including.find_last_of("/\\");
    const bool absolute = !name.empty() && (name[0] == '/' || name[0] == '\\' || (name.size() > 1 && name[1] == ':'));
    return (slash == std::string::npos || absolute) ? name : including.substr(0, slash + 1) + name;
}

namespace {

/* the index of the model's file name in m.files, or -1 */
long file_index(const Model &m, const std::string &name)
{
    const auto it = std::find_if(m.files.begin(), m.files.end(), [&name](const ModelFile &f) { return f.name == name; });
    return it == m.files.end() ? -1 : it - m.files.begin();
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
    if (!read_bytes(name.c_str(), bytes)) return false;
    if (i < 0) m.files.push_back({name, bytes});
    return true;
}

UniqueFile open_model_file(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return UniqueFile();
    if (m.saved_in.empty()) return open_read(name.c_str());
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
    const long i = file_index(m, name);
    if (i < 0 || n <= 0) return {};
    return LineReader::of_text(m.files[static_cast<size_t>(i)].bytes).line(n);
}

Place model_place(const Model &m, const odex::Pos &pos)
{
    const size_t f = static_cast<size_t>(pos.file);
    Place p{f < m.statement_files.size() ? m.statement_files[f] : m.this_file, pos.line, pos.col};
    p.source = model_source_line(m, p.file, p.line);
    return p;
}

namespace {

bool same_name(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::toupper(static_cast<unsigned char>(x)) == std::toupper(static_cast<unsigned char>(y));
           });
}

} // namespace

Place model_place(const Model &m, std::string_view name)
{
    /* a statement that defines name first (an equation, a function, a
       table ...), then the lists that name it (par, aux, wiener ...) */
    for (const odex::Statement &st : m.statements) {
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
            if (same_name(st.name, name)) return model_place(m, st.pos);
            break;
        default:
            break;
        }
    }
    for (const odex::Statement &st : m.statements) {
        for (const odex::Binding &b : st.bindings)
            if (same_name(b.name, name)) return model_place(m, b.pos.line > 0 ? b.pos : st.pos);
        for (size_t k = 0; k < st.names.size(); k++)
            if (same_name(st.names[k], name))
                return model_place(m, k < st.name_positions.size() && st.name_positions[k].line > 0 ? st.name_positions[k] : st.pos);
    }
    return Load::place();
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
