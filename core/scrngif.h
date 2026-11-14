#pragma once

#include <string>

namespace xpp {

#define MAKE_ONE_GIF 2
#define FIRST_ANI_GIF 3
#define NEXT_ANI_GIF 4

/* scrngif.cpp: GIF encoding, the bytes of one frame (or the end mark) appended to out */
void end_ani_gif(std::string &out);
void gif_stuff_ppm(unsigned char *ppm, int w, int h, std::string &out, int task);

} // namespace xpp
