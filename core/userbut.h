#pragma once
/* The buttons of a model: each has a label and runs one registry command (W663). A .odex lists them under `interface`:
   `buttons: [ { label: "AUTO", command: autox.open } ]`; an XPPAUT .ode's `@ but=name:keys` (the letters of its menu
   keys) is read on import only and written as that list. userbut.cpp. */
#include <string>
#include <string_view>
#include <vector>

namespace xpp {
namespace odex { class Block; }
struct LoadServices; /* model_file_types.h */

/* a button of the model's: its label and the registry command it runs */
struct UserButton {
  std::string label, command;
};

/* the buttons of the `buttons` list of file: each has a label (not empty) and a command (a registry command), and no two have one
   label; ReadFailed at the member that is wrong. The count has no limit but the file's. `services` gives the registry's answer
   (LoadServices::command_exists) to whether a command exists; the Model of a state file read apart from its model has none and
   checks the shape only: the read that follows with the real Model checks the names. */
std::vector<UserButton> read_buttons(odex::Block &file, const LoadServices &services);

/* the list as the options block holds it, `[ { label: "AUTO", command: autox.open }, ... ]` */
std::string buttons_text(const std::vector<UserButton> &buttons);

/* the registry command an XPPAUT key sequence ("fa") runs, "" when no command of ours does (the .ode's button is dropped, said once) */
std::string_view xppaut_button_command(std::string_view keys);

} // namespace xpp
