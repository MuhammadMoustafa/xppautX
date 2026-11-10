#include "userbut.h"

#include <algorithm>
#include <iterator>

#include "command_def.h"
#include "odex_block.h"
#include "odex.h"
#include "session.h"
#include "xpp_io.h"

namespace xpp {

namespace {

/* XPPAUT's menu key sequences (its kbs list, import only) that a command of ours runs; the others (a mouse click, a question XPPAUT
   asked in a window of its own) have none */
struct KeyCommand {
  std::string_view keys, command;
};
constexpr KeyCommand key_commands[] = {
  {"ig", "run.initial"}, {"il", "run.last"}, {"io", "run.old"}, {"is", "run.shift"}, {"ir", "run.range"}, {"i2", "run.double_range"},
  {"c", "run.continue"}, {"sg", "equilibrium.find"}, {"sr", "equilibrium.range"},
  {"nn", "nullcline.compute"}, {"nr", "nullcline.redraw"}, {"ns", "nullcline.export"},
  {"dd", "direction_field.compute"}, {"df", "direction_field.flow"}, {"dn", "direction_field.clear"}, {"dc", "direction_field.colorize"},
  {"ww", "window.set_bounds"}, {"wf", "window.fit"}, {"mc", "window.new"}, {"mk", "window.close_extra"}, {"md", "window.close"}, {"mb", "window.send_to_bottom"}, {"r", "window.redraw"}, {"e", "window.clear"}, {"x", "plot.variable_vs_time"},
  {"kc", "kinescope.capture"}, {"kr", "kinescope.reset"}, {"kp", "kinescope.play"}, {"ka", "kinescope.autoplay"}, {"ks", "kinescope.export_gif"},
  {"ga", "plot.add_curve"}, {"gd", "plot.delete_curve"}, {"gr", "plot.delete_all_curves"}, {"ge", "plot.edit_curve"},
  {"p", "value.set_parameters"}, {"fa", "autox.open"}, {"fc", "calculator.evaluate"}, {"fs", "session.export_info"},
  {"fr", "session.import_xppaut_state"}, {"fq", "session.quit"}, {"ft", "data.transpose"}, {"fg", "set.apply"}};

} // namespace

std::vector<UserButton> read_buttons(odex::Block &file)
{
  std::vector<odex::Block> items;
  file.groups("buttons", items, odex::max_file_tokens);
  std::vector<UserButton> buttons;
  for (odex::Block &g : items) {
    for (const std::string_view key : {"label", "command"})
      if (!g.has(key)) g.fail(xpp::format("a button requires {}", key));
    UserButton button;
    g.text("label", button.label);
    if (button.label.empty()) g.fail(*g.member("label"), "a button's label is not empty");
    g.word("command", button.command);
    if (!command::find(button.command)) g.fail(*g.member("command"), xpp::format("`{}` is no command of xppautX", button.command));
    if (std::ranges::any_of(buttons, [&](const UserButton &b) { return b.label == button.label; }))
      g.fail(*g.member("label"), xpp::format("two buttons are labelled \"{}\"", button.label));
    g.done();
    buttons.push_back(std::move(button));
  }
  return buttons;
}

std::string buttons_text(const std::vector<UserButton> &buttons)
{
  std::string text = "[";
  for (const UserButton &b : buttons) text += xpp::format("{} {{ label: {}, command: {} }}", text.size() > 1 ? "," : "", odex::quote_string(b.label), b.command);
  return text + (buttons.empty() ? "]" : " ]");
}

std::string_view xppaut_button_command(std::string_view keys)
{
  const auto at = std::ranges::find(key_commands, keys, &KeyCommand::keys);
  return at == std::end(key_commands) ? std::string_view() : at->command;
}

const char *add_xppaut_button(Session &s, std::string_view spec, bool apply)
{
  const std::size_t colon = spec.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon + 1 == spec.size()) return "must be a name and a command sequence, name:keys";
  const std::string_view command = xppaut_button_command(spec.substr(colon + 1));
  if (command.empty() || std::ranges::any_of(s.user_buttons, [&](const UserButton &b) { return b.command == command; })) return nullptr;
  if (apply) s.user_buttons.push_back({std::string(spec.substr(0, colon)), std::string(command)});
  return nullptr;
}

} // namespace xpp
