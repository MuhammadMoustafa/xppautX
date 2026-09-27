#ifndef _kinescope_h_
#define _kinescope_h_
#ifdef __cplusplus
extern "C" {
#endif

void do_movie_com(int c);

#ifdef __cplusplus
}

/* The kinescope, a Session's (session.h): the frames it holds (the front
   end keeps their pictures) and Autoplay's settings, how many times it
   runs the film and how fast */
struct XppKinescope {
    int frames = 0;     /* frames captured */
    int cycles = 1;     /* times through the film */
    int frame_ms = 50;  /* milliseconds between frames */
};
#endif
#endif

