#ifndef _scrngif_h_
#define _scrngif_h_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAKE_ONE_GIF 2
#define GET_GLOBAL_CMAP 1
#define FIRST_ANI_GIF 3
#define NEXT_ANI_GIF 4

/* scrngif.cpp: GIF encoding into a stream the caller owns */
void set_global_map(int flag);
void end_ani_gif(FILE *fp);
void gif_stuff_ppm(unsigned char *ppm, int w, int h, FILE *fp, int task);

#ifdef __cplusplus
}
#endif
#endif
