#ifndef _kinescope_h_
#define _kinescope_h_
#ifdef __cplusplus
extern "C" {
#endif

void do_movie_com(int c);
int film_clip(void);

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


