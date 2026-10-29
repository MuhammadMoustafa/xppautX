/* Save values into the model file (model_save.h). */
#include "model_save.h"

#include "browse.h"
#include "expr.h"
#include "load_eqn.h"
#include "model.h"
#include "model_files.h"
#include "odex_write.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "xpp_util.h"

#include <algorithm>
#include <functional>
#include <memory>

namespace xpp {

namespace {

/* what a save of the model file needs to know of the live model after it wrote */
struct Adopt {
    /* the defaults that changed: parameter and state numbers with their new values */
    std::vector<std::pair<int, double>> parameters, states;
};

Error save_error(std::string what, const Place &place = {}) { return Error{"save values", std::move(what), place}; }

/* text written through w (opened by open_writer_*) and put in place */
Result<> write_text(Writer &w, const std::string &text)
{
    if (!w.write(text)) {
        abort_save(w);
        return std::unexpected(files::write_error("save values", w.path()));
    }
    return commit_save(w);
}

/* the model file's text as the model loaded it, or why this model has no file to write */
Result<const ModelFile *> own_file(const Session &s)
{
    const Model &m = s.model();
    if (!m.loaded()) return std::unexpected(save_error("no model is open"));
    if (!odex::is_odex(m.this_file))
        return std::unexpected(save_error(xpp::format("{} is not an .odex model: only an .odex takes saved values", m.this_file), Place{m.this_file}));
    if (m.files.empty() || m.files.front().name != m.this_file)
        return std::unexpected(save_error("the model's file was not kept as it was loaded", Place{m.this_file}));
    return &m.files.front();
}

/* a Model of its own to parse the text in: the folder, the paths the user
   allowed and (for a saved model) the files are the live model's, so an import is read as the load read it */
std::unique_ptr<Model> scratch_model(const Model &m)
{
    std::unique_ptr<Model> scratch = std::make_unique<Model>();
    scratch->this_file = m.this_file;
    scratch->outside_paths = m.outside_paths;
    scratch->load_dir = m.load_dir;
    if (!m.saved_in.empty()) { /* its files are the session's copies, never the disk's */
        scratch->saved_in = m.saved_in;
        scratch->files = m.files;
    }
    return scratch;
}

/* a place of the model file moved to where its line went */
void remap(Place &place, const std::string &file, const odex::LineMap &lines)
{
    if (place.file == file && place.line > 0) place.line = lines.map(place.line);
}

/* the places of the model file moved to where their lines went (the lines the edit
   removed or added shift what follows them) */
void remap_places(Model &m, const odex::LineMap &lines)
{
    const auto shift = [&lines](odex::Pos &p) {
        if (p.file == 0 && p.line > 0) p.line = lines.map(p.line);
    };
    for (auto &[name, entry] : m.names) shift(entry.pos);
    for (odex::Statement &st : m.statements) {
        shift(st.pos);
        shift(st.name_pos);
        for (odex::Binding &b : st.bindings) shift(b.pos);
    }
    for (Model::InitialValue &v : m.initial_values) remap(v.where, m.this_file, lines);
    for (Model::DerivedQuantity &d : m.derived) remap(d.where, m.this_file, lines);
    for (int i = 0; i < m.nsvar; i++) remap(m.svars[static_cast<std::size_t>(i)].where, m.this_file, lines);
    for (int i = 0; i < m.naeqn; i++) remap(m.aeqns[static_cast<std::size_t>(i)].where, m.this_file, lines);
    remap(m.jacobian.requested, m.this_file, lines);
}

/* the live model made to be what the file now says */
void adopt(Session &s, const odex::Edited &edited, const odex::Parsed &now, const Adopt &changed)
{
    Model &m = s.model();
    remap_places(m, edited.lines);
    replace_model_text(m, edited.text);
    std::erase_if(m.statements, [](const odex::Statement &st) { return st.kind == odex::Statement::Kind::Set; });
    for (const odex::Statement &st : now.statements)
        if (st.kind == odex::Statement::Kind::Set) m.statements.push_back(st);
    replace_intern_sets(m, now);
    replace_model_options(m, now);
    s.options_applied = m.options.size(); /* the values in use are the ones saved */
    for (const auto &[i, value] : changed.parameters) m.default_val[static_cast<std::size_t>(i)] = value;
    for (const auto &[i, value] : changed.states) {
        m.default_ic[static_cast<std::size_t>(i)] = value;
        std::erase_if(m.initial_values, [&](const Model::InitialValue &v) { return v.index == i; });
    }
}

using Edit = std::function<Result<odex::Edited>(Model &scratch, const std::string &text, const std::string &file)>;

/* The one writer of the model file: edit builds the new text from the one the
   model was loaded from (parsed and checked in a model of its own), which must be what
   is on the disk still; it replaces the file in one step and then the live model
   follows. Nothing is written, and nothing changes, when any step fails. */
Result<bool> write_model_file(Session &s, const Edit &edit, const Adopt &changed)
{
    const Result<const ModelFile *> file = own_file(s);
    if (!file) return std::unexpected(file.error());
    Model &m = s.model();
    const std::string path = m.this_file;
    const std::string &text = (*file)->bytes;
    const std::unique_ptr<Model> scratch = scratch_model(m);
    const Result<odex::Edited> edited = edit(*scratch, text, path);
    if (!edited) return std::unexpected(edited.error());
    if (edited->text == text) return false; /* the file already says it */
    odex::Parsed now;
    try {
        now = odex::ready(*scratch, odex::parse(*scratch, edited->text, path));
    } catch (Error &e) {
        return std::unexpected(std::move(e));
    }
    /* a model opened from a session (or played from a recording) has no file of its own to write: the
       session's copy of the model changes, and saving the session keeps it */
    if (m.saved_in.empty()) {
        /* the file as it is now must be the file the model loaded: else it was changed by something else */
        std::string disk;
        if (!read_bytes(path, disk))
            return std::unexpected(save_error(xpp::format("{} cannot be read to check it has not changed: nothing was written", path), Place{path}));
        if (disk != text)
            return std::unexpected(save_error(xpp::format("{} changed on disk since the model was loaded: nothing was written (reload the model, then save again)", path), Place{path}));
        Result<> opened;
        Writer w = open_writer_replacing(path, true, &opened);
        if (!w) return std::unexpected(opened ? save_error(xpp::format("{} cannot be written", path), Place{path}) : opened.error());
        if (const Result<> done = write_text(w, edited->text); !done) return std::unexpected(done.error());
    }
    adopt(s, *edited, now, changed);
    return true;
}

Result<> written(const Result<bool> &r)
{
    if (!r) return std::unexpected(r.error());
    return {};
}

Result<> save(Session &s, const Edit &edit, const Adopt &changed = {}) { return written(write_model_file(s, edit, changed)); }

}

Result<> save_new_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph, bool last_state)
{
    Result<odex::Values> values = captured_values(s, scope, graph, last_state);
    if (!values) return std::unexpected(values.error());
    return save(s, [&](Model &scratch, const std::string &text, const std::string &file) {
        return odex::add_set(scratch, text, file, name, *values);
    });
}

Result<> save_into_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph, bool last_state)
{
    Result<odex::Values> values = captured_values(s, scope, graph, last_state);
    if (!values) return std::unexpected(values.error());
    return save(s, [&](Model &scratch, const std::string &text, const std::string &file) {
        return odex::replace_in_set(scratch, text, file, name, *values, [&](std::string_view key) { return value_kind(s.model(), key); });
    });
}

Result<> save_defaults(Session &s, std::string_view scope, std::optional<int> graph, bool last_state)
{
    Result<odex::Values> values = captured_values(s, scope, graph, last_state);
    if (!values) return std::unexpected(values.error());
    const Model &m = s.model();
    /* only the values that differ from the defaults are written: the lines of the others stay as they are */
    Adopt changed;
    odex::Values differing = *values;
    differing.parameter_values.clear();
    differing.state_values.clear();
    for (std::size_t i = 0; i < values->parameter_values.size(); i++) {
        double now = 0;
        get_val(s, m.upar_names[i], &now);
        if (now == m.default_val[i]) continue;
        differing.parameter_values.push_back(values->parameter_values[i]);
        changed.parameters.emplace_back(static_cast<int>(i), now);
    }
    for (std::size_t i = 0; i < values->state_values.size(); i++) {
        const double now = last_state ? s.data_store.current[i] : s.last_ic[i];
        if (now == m.default_ic[i]) continue;
        differing.state_values.push_back(values->state_values[i]);
        changed.states.emplace_back(static_cast<int>(i), now);
    }
    return save(s, [&](Model &scratch, const std::string &text, const std::string &file) {
        return odex::set_defaults(scratch, text, file, differing);
    }, changed);
}

Result<> rename_intern_set(Session &s, std::string_view from, std::string_view to)
{
    return save(s, [&](Model &scratch, const std::string &text, const std::string &file) {
        return odex::rename_set(scratch, text, file, from, to);
    });
}

Result<> delete_intern_set(Session &s, std::string_view name)
{
    return save(s, [&](Model &scratch, const std::string &text, const std::string &file) {
        return odex::delete_set(scratch, text, file, name);
    });
}

Result<bool> export_values(Session &s, std::string_view name, std::string_view scope, std::string_view file, std::optional<int> graph,
                           bool last_state)
{
    const Result<const ModelFile *> own = own_file(s);
    if (!own) return std::unexpected(own.error());
    const Model &m = s.model();
    if (files::absolute(file) == files::absolute(m.this_file))
        return std::unexpected(save_error(xpp::format("{} is the model file: Save values writes into it", file), Place{std::string(file)}));
    if (!odex::is_odex(file)) return std::unexpected(save_error(xpp::format("{} is not an .odex file", file), Place{std::string(file)}));
    Result<odex::Values> values = captured_values(s, scope, graph, last_state);
    if (!values) return std::unexpected(values.error());
    const std::unique_ptr<Model> scratch = scratch_model(m);
    const std::string target(file);
    const odex::Classify classify = [&m](std::string_view key) { return value_kind(m, key); };
    Result<odex::Edited> edited = std::unexpected(Error{});
    const bool exists = files::exists(target);
    if (!exists) {
        edited = odex::add_set(*scratch, odex::export_header(files::split_path(m.this_file).second, utc_now()), target, name, *values, true);
    } else {
        std::string existing;
        if (!read_bytes(target, existing))
            return std::unexpected(save_error(xpp::format("{} cannot be read", file), Place{target}));
        const Result<std::vector<std::pair<std::string, odex::Pos>>> sets = odex::local_sets(*scratch, existing, target);
        if (!sets) return std::unexpected(sets.error());
        const bool has = std::ranges::any_of(*sets, [&](const auto &entry) { return entry.first == name; });
        if (has) {
            const std::string question = xpp::format("{} has a set {}. Replace its {} with the current values?", file, name, scope == set_scope::all ? "values" : scope);
            if (!confirm(question)) return false;
            edited = odex::replace_in_set(*scratch, existing, target, name, *values, classify, true);
        } else {
            edited = odex::add_set(*scratch, existing, target, name, *values, true);
        }
    }
    if (!edited) return std::unexpected(edited.error());
    Result<> opened;
    Writer w = exists ? open_writer_replacing(target, true, &opened) : open_writer_asking(target, true, &opened);
    if (!w) {
        if (opened) return false; /* the replace question was answered no */
        return std::unexpected(opened.error());
    }
    if (const Result<> done = write_text(w, edited->text); !done) return std::unexpected(done.error());
    return true;
}

}
