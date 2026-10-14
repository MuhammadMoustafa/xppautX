/* .odex text written, and a model file's text edited (odex_write.h). */
#include "odex_write.h"

#include "expr.h"
#include "model.h"
#include "model_options.h"
#include "xpp_io.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace xpp::odex {

namespace {

/* a line of the model file is at most this wide before a list breaks */
constexpr std::size_t line_width = 100;

/* the statements the file itself wrote, not those an import brought */
bool local(const Statement &s) { return s.pos.file == 0 && s.imported_at.line == 0; }

/* one piece of the old text replaced */
struct Replacement {
  std::size_t begin = 0, end = 0;
  std::string text;
};

int newlines_in(std::string_view text) { return static_cast<int>(std::ranges::count(text, '\n')); }

/* the text with the replacements made (they do not overlap), and where the lines went */
Edited apply(std::string_view text, std::vector<Replacement> replacements)
{
  std::ranges::stable_sort(replacements, {}, &Replacement::begin);
  Edited out;
  std::size_t at = 0;
  for (const Replacement &r : replacements) {
    out.text.append(text.substr(at, r.begin - at));
    out.text += r.text;
    const std::string_view old = text.substr(r.begin, r.end - r.begin);
    const int removed = newlines_in(old), added = newlines_in(r.text);
    if (removed != added) out.lines.changes.push_back({1 + newlines_in(text.substr(0, r.begin)), removed, added});
    at = r.end;
  }
  out.text.append(text.substr(at));
  return out;
}

/* what is inserted, in the file's own line ends */
std::string in_file_newlines(std::string_view text, bool crlf)
{
  std::string out;
  for (std::size_t i = 0; i < text.size(); i++) {
    if (text[i] == '\n' && crlf && (i == 0 || text[i - 1] != '\r')) out += '\r';
    out += text[i];
  }
  return out;
}

bool uses_crlf(std::string_view text) { return text.find("\r\n") != std::string_view::npos; }

Error error_in(const std::string &file, const Pos &at, std::string what)
{
  return Error{"save values", std::move(what), Place{file, at.line, at.col}};
}

/* the file's statements, parsed; the error is the parser's */
Result<Parsed> read(Model &m, std::string_view text, const std::string &file)
{
  try {
    return parse(m, text, file);
  } catch (Error &e) {
    return std::unexpected(std::move(e));
  }
}

/* the file's set called name; nullptr when there is none, and then imported
   says whether an import brought a set of that name */
const Statement *find_set(const Parsed &p, std::string_view name, const Statement *&imported)
{
  imported = nullptr;
  for (const Statement &s : p.statements) {
    if (s.kind != Statement::Kind::Set || s.name != name) continue;
    if (local(s)) return &s;
    imported = &s;
  }
  return nullptr;
}

/* why name cannot be the name of a new set, an empty Error text when it can */
std::string name_problem(const Parsed &p, std::string_view name)
{
  if (!is_name(name) || is_reserved(name)) return xpp::format("{} is not a valid name for a set", name);
  for (const Statement &s : p.statements)
    if (s.kind == Statement::Kind::Set && s.name == name)
      return xpp::format("{} is already a set of the model (line {} of {})", name, s.pos.line, p.files[static_cast<std::size_t>(s.pos.file)]);
  return "";
}

/* the set to change, or why it cannot be: not there, or an import's */
Result<const Statement *> local_set(const Parsed &p, const std::string &file, std::string_view name)
{
  const Statement *imported = nullptr;
  if (const Statement *s = find_set(p, name, imported)) return s;
  if (imported)
    return std::unexpected(Error{"save values",
                                 xpp::format("the set {} comes from {} and is read-only here: save under a name of your own",
                                             name, p.files[static_cast<std::size_t>(imported->pos.file)]),
                                 Place{p.files[static_cast<std::size_t>(imported->pos.file)], imported->pos.line, imported->pos.col}});
  return std::unexpected(Error{"save values", xpp::format("the model has no set {}", name), Place{file}});
}

/* a `key: value` entry */
std::string entry(std::string_view key, std::string_view value) { return xpp::format("{}: {}", key, value); }

/* the settings values holds, as the entries of the groups it covers */
std::vector<GroupItem> new_settings(const Values &values)
{
  std::vector<GroupItem> out;
  for (const Setting &s : values.settings) out.push_back({s.group, entry(s.key, s.value)});
  return out;
}

/* the text of a binding of a set as it stands */
std::string slice(std::string_view text, std::size_t begin, std::size_t end) { return std::string(text.substr(begin, end - begin)); }

/* the set's entries after replacing the kinds values covers: the kept ones as
   they were written, the covered kinds as values has them; parameters, then
   states, then what is neither */
void set_items(const Statement &old, std::string_view text, const Values &values, const Classify &classify,
               std::vector<std::string> &items, std::vector<GroupItem> &settings)
{
  std::vector<std::string> parameters, states, others;
  for (const Binding &b : old.bindings) {
    const ValueKind kind = classify(b.name);
    if (kind == ValueKind::parameter && !values.parameters) parameters.push_back(slice(text, b.begin, b.end));
    else if (kind == ValueKind::state && !values.states) states.push_back(slice(text, b.begin, b.end));
    else if (kind == ValueKind::other) others.push_back(slice(text, b.begin, b.end));
  }
  if (values.parameters) for (const auto &[name, value] : values.parameter_values) parameters.push_back(entry(name, value));
  if (values.states) for (const auto &[name, value] : values.state_values) states.push_back(entry(name, value));
  for (std::vector<std::string> *kind : {&parameters, &states, &others}) items.insert(items.end(), kind->begin(), kind->end());
  for (const Option &o : old.options)
    if (!values.covers_group(o.group)) settings.push_back({o.group, slice(text, o.begin, o.end)});
  for (GroupItem &g : new_settings(values)) settings.push_back(std::move(g));
}

/* the edit's text, checked as a model of its own */
Result<Edited> checked(Model &m, Edited edited, const std::string &file, bool partial)
{
  if (Result<> ok = check_text(m, edited.text, file, partial); !ok) return std::unexpected(std::move(ok.error()));
  return edited;
}

/* the end of the statement's last line when only blanks follow it there, else its end */
std::size_t through_line_end(std::string_view text, std::size_t end)
{
  std::size_t i = end;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r')) i++;
  return i < text.size() && text[i] == '\n' ? i + 1 : (i == text.size() ? i : end);
}

/* the text with a block appended as a statement of its own, a blank line before it */
Replacement appended(std::string_view text, const std::string &block)
{
  std::string add;
  if (!text.empty() && text.back() != '\n') add += '\n';
  if (!text.empty()) add += '\n';
  add += block + '\n';
  return {text.size(), text.size(), in_file_newlines(add, uses_crlf(text))};
}

/* the number text of a binding's value when it is one, else nothing */
std::optional<double> number_of(std::string_view text)
{
  double v = 0;
  if (!xpp::parse_number(xpp::trim_blanks(text), v)) return std::nullopt;
  return v;
}

/* the value of the binding of `name` among the file's own statements of kind,
   nullptr when none is there (an array's element, an import's, none at all) */
const Binding *own_binding(const Parsed &p, Statement::Kind kind, std::string_view name)
{
  const std::string wanted = xpp::converted(name);
  for (const Statement &s : p.statements) {
    if (s.kind != kind || !local(s) || !s.range.index.empty()) continue;
    for (const Binding &b : s.bindings)
      if (!b.index && xpp::converted(b.name) == wanted) return &b;
  }
  return nullptr;
}

/* where name is declared by a statement of kind, for the error of a default
   that cannot be written there; nothing when no statement declares it */
std::optional<std::string> declared_at(const Parsed &p, Statement::Kind kind, std::string_view name)
{
  const std::string wanted = xpp::converted(name);
  for (const Statement &s : p.statements) {
    if (s.kind != kind) continue;
    for (const Binding &b : s.bindings)
      if (xpp::converted(b.name) == wanted) {
        const std::string &file = p.files[static_cast<std::size_t>(s.pos.file)];
        return xpp::format("{}:{}{}", file, b.pos.line, s.range.index.empty() ? (s.imported_at.line ? " (an import)" : "") : " (an array statement)");
      }
  }
  return std::nullopt;
}

}

bool Values::covers_group(std::string_view group) const { return std::ranges::find(groups, group) != groups.end(); }

int LineMap::map(int line) const
{
  int delta = 0;
  for (const Change &c : changes) {
    if (line > c.first + c.removed) delta += c.added - c.removed;
    else if (line >= c.first) return c.first + delta + std::min(line - c.first, c.added);
  }
  return line + delta;
}

std::string braced(const std::vector<std::string> &items, std::size_t indent)
{
  std::string line = "{ ";
  for (std::size_t i = 0; i < items.size(); i++) line += (i ? ", " : "") + items[i];
  line += " }";
  if (indent + line.size() <= line_width) return line;
  const std::string pad(indent + 2, ' ');
  std::string out = "{\n", row;
  for (const std::string &item : items) {
    if (!row.empty() && pad.size() + row.size() + 2 + item.size() > line_width) {
      out += pad + row + ",\n";
      row.clear();
    }
    row += (row.empty() ? "" : ", ") + item;
  }
  return out + pad + row + ",\n" + std::string(indent, ' ') + "}";
}

std::vector<std::string> grouped(const std::vector<GroupItem> &items, std::size_t indent)
{
  std::vector<std::string> out;
  for (std::string_view group : xpp::option_groups) {
    std::vector<std::string> in_group;
    for (const GroupItem &e : items)
      if (e.group == group) in_group.push_back(e.text);
    if (!in_group.empty()) out.push_back(std::string(group) + ": " + braced(in_group, indent));
  }
  return out;
}

std::string options_block(const std::vector<GroupItem> &items)
{
  std::string out;
  for (const std::string &group : grouped(items, 2)) out += "  " + group + ",\n";
  return out.empty() ? std::string() : "options {\n" + out + "}\n";
}

std::string set_block(std::string_view name, const std::vector<std::string> &items, const std::vector<GroupItem> &settings)
{
  std::vector<std::string> all = items;
  if (!settings.empty()) all.push_back("options: " + braced(grouped(settings, 8 + name.size()), 6 + name.size()));
  return "set " + std::string(name) + " " + braced(all, 4 + name.size());
}

Result<> check_text(Model &m, std::string_view text, const std::string &file, bool partial)
{
  try {
    const Parsed p = parse(m, text, file);
    if (!partial) static_cast<void>(ready(m, p));
  } catch (Error &e) {
    return std::unexpected(std::move(e));
  }
  return {};
}

Result<std::vector<std::pair<std::string, Pos>>> local_sets(Model &m, std::string_view text, const std::string &file)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  std::vector<std::pair<std::string, Pos>> out;
  for (const Statement &s : p->statements)
    if (s.kind == Statement::Kind::Set && local(s)) out.emplace_back(s.name, s.pos);
  return out;
}

std::string export_header(std::string_view file, std::string_view time)
{
  return xpp::format("# Saved from {} at {}\n", file, time);
}

Result<Edited> add_set(Model &m, std::string_view text, const std::string &file, std::string_view name, const Values &values, bool partial)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  if (const std::string why = name_problem(*p, name); !why.empty()) return std::unexpected(Error{"save values", why, Place{file}});
  std::vector<std::string> items;
  for (const auto &[n, v] : values.parameter_values) items.push_back(entry(n, v));
  for (const auto &[n, v] : values.state_values) items.push_back(entry(n, v));
  return checked(m, apply(text, {appended(text, set_block(name, items, new_settings(values)))}), file, partial);
}

Result<Edited> replace_in_set(Model &m, std::string_view text, const std::string &file, std::string_view name, const Values &values,
                              const Classify &classify, bool partial)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  const Result<const Statement *> old = local_set(*p, file, name);
  if (!old) return std::unexpected(old.error());
  std::vector<std::string> items;
  std::vector<GroupItem> settings;
  set_items(**old, text, values, classify, items, settings);
  const std::string block = in_file_newlines(set_block(name, items, settings), uses_crlf(text));
  return checked(m, apply(text, {{(*old)->begin, (*old)->end, block}}), file, partial);
}

Result<Edited> rename_set(Model &m, std::string_view text, const std::string &file, std::string_view from, std::string_view to)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  const Result<const Statement *> old = local_set(*p, file, from);
  if (!old) return std::unexpected(old.error());
  if (const std::string why = name_problem(*p, to); !why.empty()) return std::unexpected(error_in(file, (*old)->pos, why));
  return checked(m, apply(text, {{(*old)->name_begin, (*old)->name_begin + from.size(), std::string(to)}}), file, false);
}

Result<Edited> delete_set(Model &m, std::string_view text, const std::string &file, std::string_view name)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  const Result<const Statement *> old = local_set(*p, file, name);
  if (!old) return std::unexpected(old.error());
  return checked(m, apply(text, {{(*old)->begin, through_line_end(text, (*old)->end), ""}}), file, false);
}

Result<Edited> set_defaults(Model &m, std::string_view text, const std::string &file, const Values &values)
{
  const Result<Parsed> p = read(m, text, file);
  if (!p) return std::unexpected(p.error());
  std::vector<Replacement> edits;
  std::string added, added_inits;
  /* a value is replaced where its line gives it; a state with no line gets one */
  const auto write = [&](Statement::Kind kind, std::string_view keyword, const std::vector<std::pair<std::string, std::string>> &list) -> Result<> {
    for (const auto &[name, value] : list) {
      const Binding *b = own_binding(*p, kind, name);
      if (!b) {
        const std::optional<std::string> where = declared_at(*p, kind, name);
        if (!where && kind == Statement::Kind::Init) {
          added_inits += (added_inits.empty() ? "" : "\n") + xpp::format("{} {} = {}", keyword, name, value);
          continue;
        }
        return std::unexpected(Error{"save values", xpp::format("the default of {} is declared at {}: it cannot be saved here", name, where.value_or("no line")), Place{file}});
      }
      const std::optional<double> now = number_of(slice(text, b->value_begin, b->end));
      const std::optional<double> wanted = number_of(value);
      if (now && wanted && *now == *wanted) continue;
      edits.push_back({b->value_begin, b->end, value});
    }
    return {};
  };
  if (values.parameters)
    if (Result<> r = write(Statement::Kind::Par, "par", values.parameter_values); !r) return std::unexpected(r.error());
  if (values.states)
    if (Result<> r = write(Statement::Kind::Init, "init", values.state_values); !r) return std::unexpected(r.error());
  /* an options block with the groups values covers replaced, the others as written */
  if (!values.groups.empty()) {
    const Statement *block = nullptr;
    for (const Statement &s : p->statements)
      if (s.kind == Statement::Kind::Options && local(s)) block = &s;
    std::vector<GroupItem> items;
    if (block)
      for (const Option &o : block->options)
        if (!values.covers_group(o.group)) items.push_back({o.group, slice(text, o.begin, o.end)});
    for (GroupItem &g : new_settings(values)) items.push_back(std::move(g));
    const std::string text_block = options_block(items);
    if (block) edits.push_back({block->begin, text_block.empty() ? through_line_end(text, block->end) : block->end,
                                in_file_newlines(text_block.empty() ? "" : text_block.substr(0, text_block.size() - 1), uses_crlf(text))});
    else if (!text_block.empty()) added += text_block;
  }
  /* a state with no init line gets one after the last init of the file, else at its end */
  if (!added_inits.empty()) {
    const Statement *last_init = nullptr;
    for (const Statement &st : p->statements)
      if (st.kind == Statement::Kind::Init && local(st)) last_init = &st;
    if (last_init) edits.push_back({last_init->end, last_init->end, in_file_newlines("\n" + added_inits, uses_crlf(text))});
    else added = added_inits + "\n" + added;
  }
  if (!added.empty()) edits.push_back(appended(text, added.substr(0, added.size() - 1)));
  if (edits.empty()) return Edited{std::string(text), {}};
  return checked(m, apply(text, std::move(edits)), file, false);
}

}
