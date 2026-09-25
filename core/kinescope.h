#ifndef _kinescope_h_
#define _kinescope_h_
#ifdef __cplusplus
extern "C" {
#endif

void do_movie_com(int c);
void reset_film(void);
int film_clip(void);
int show_frame(int i, int h, int w);
void play_back(void);
void save_kine(void);
void make_anigif(void);
void save_movie(const char *basename, int fmat);
void auto_play(void);
void too_small(void);

/* The kinescope's Autoplay: how many times it runs the film, how fast */
typedef struct {
    int cycles;    /* times through the film */
    int frame_ms;  /* milliseconds between frames */
} XppMovieAutoPlay;
extern XppMovieAutoPlay movie_autoplay;

#ifdef __cplusplus
}
#endif
#endif


