#pragma once

/* The kinescope, a Session's (session.h): the frames it holds (the front
   end keeps their pictures) and Autoplay's settings, how many times it
   runs the film and how fast */
/* the most frames the kinescope holds: the page keeps each as its pixels, 250 of a 640 by 480 plot are
   230 MB (XPPAUT's own limit, which kept its frames in X pixmaps) */
inline constexpr int kinescope_max_frames = 250;

struct XppKinescope {
    int frames = 0;     /* frames captured */
    int cycles = 1;     /* times through the film */
    int frame_ms = 50;  /* milliseconds between frames */
};

namespace xpp {
struct Session; /* session.h */
}

/* the Kinescope menu's item c on s's kinescope (commands.cpp) */
void do_movie_com(xpp::Session &s, int c);

