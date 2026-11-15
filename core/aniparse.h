#pragma once

#include "ani_state.h"
#include "odex.h"
#include "xpp_error.h"
#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace xpp {
struct Session; /* session.h */
class FileSystem; /* xpp_fs.h */
int run_now_grab(const Session &s);

void reset_comets(Session &s);

/* The animation window of the session s: its buttons, the mouse, a frame
   drawn (render_ani), an animation file chosen and loaded (get_ani_file), and the
   frames shown while integrating (on_the_fly) */
void update_ani_motion_stuff(Session &s, double x, double y);
void on_the_fly(Session &s, int task);
void ani_view_created(Session &s);
void ani_grab_start(Session &s);
void ani_reset(Session &s);
void ani_grab_mouse(Session &s, int flag, double x, double y);
void ani_flip1(Session &s, int n);
void ani_zero(Session &s);
/* animation.load's work: the animation file `file` loaded, and its first frame shown when there is data to show
   (`file` by value, like load_animation's) */
Result<> load_chosen_animation(Session &s, std::string file);
/* Go's frames also written: as numbered PPM files from `file`, or as one animated GIF `file` */
enum class AniExportKind { none, ppm, gif };
struct AniExport {
    AniExportKind kind = AniExportKind::none;
    std::string file;
};
/* the animation loaded and data stored to play, else why not (`where` names the command) */
Result<> animation_ready(const Session &s, const std::string &where);
/* Go from the frame shown to the last, also written when `out` says so: the frames shown, or why none were */
Result<int> play_animation(Session &s, const AniExport &out);
/* the frame of stored row `row` */
void ani_seek(Session &s, int row);
/* Native parsing and legacy import both return a whole value before applying it. */
Result<AnimationState> read_animation(Session &s, std::string_view text, const std::string &file);
/* the model's animation file `file` loaded; `file` by value, since a caller passes the animation's own
   name, which the load replaces */
Result<> load_animation(Session &s, std::string file);
/* loads the animation the `animation` option set (nothing when none is), after the model; one that is missing or does not
   fit is warned of with its place and left out, the model loading without it */
void load_option_animation(Session &s);
Result<std::string> animation_model(FileSystem &fs, const std::string &file);
Result<std::string> convert_animation(Session &s, std::string_view text, const std::string &file);
int convert_animation_file(FileSystem &fs, const std::string &file, const std::string &model = {});
void render_ani(Session &s);
void set_ani_perm(Session &s);

} // namespace xpp
