/* Colour table computation, split out of color.c so that PostScript and SVG
   export (which need the RGB values) work without an X display. color.c
   keeps the X colormap allocation and pixel lookup. */
#include "colormap.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "xpp_io.h"
#include <math.h>
#include <stdio.h>

#define C_NORM 0
#define C_PERIODIC 1
#define C_HOT 2
#define C_COOL 3
#define C_REDBLUE 4
#define C_GRAY 5
#define C_CUBHLX 6

#define RED 20
#define REDORANGE 21
#define ORANGE 22
#define YELLOWORANGE 23
#define YELLOW 24
#define YELLOWGREEN 25
#define GREEN 26
#define BLUEGREEN 27
#define BLUE 28
#define PURPLE 29

#define COL_TOTAL 150

int periodic = 0;
int custom_color = 0;

/* 16-bit RGB per colour index, same scale X11 uses */
unsigned short xpp_cmap_rgb[XPP_MAX_COLORS][3];
XppColorTable color_table;

int rfun(double y, int per)
{
    double x;
    x = y;
    if ((y > .666666) && (per == 1)) x = 1. - y;
    if (x > .33333333333) return 0;
    return (int)(3. * 255 * sqrt((.333334 - x) * (x + .33334)));
}

int gfun(double y, int per)
{
    (void)per;
    if (y > .666666) return 0;
    return (int)(3. * 255 * sqrt((.6666667 - y) * (y)));
}

int bfun(double y, int per)
{
    (void)per;
    if (y < .333334) return 0;
    return (int)(2.79 * 255 * sqrt((1.05 - y) * (y - .333333333)));
}

void make_cmaps(int *r, int *g, int *b, int n, int type)
{
    double x;
    int i, i1, i2, i3;
    double pii = 3.1415926;
    double start = .5, rots = -1.5, hue = 1.2, gamma = 1.;
    double angle, amp;
    double rr, gg, bb;
    switch (type) {
    case C_NORM:
        for (i = 0; i < n; i++) {
            x = (double)i / ((double)n);
            r[i] = rfun(1 - x, 0) << 8;
            g[i] = gfun(1 - x, 0) << 8;
            b[i] = bfun(1 - x, 0) << 8;
        }
        break;
    case C_PERIODIC:
        for (i = 0; i < n; i++) {
            x = (double)i / ((double)n);
            r[i] = rfun(x, 1) << 8;
            g[i] = gfun(x, 1) << 8;
            b[i] = bfun(x, 1) << 8;
        }
        break;
    case C_HOT:
        i1 = .375 * n;
        i2 = 2 * i1;
        i3 = n - i2;
        for (i = 0; i < i1; i++) {
            x = 256 * 255 * (double)i / ((double)i1);
            r[i] = (int)x;
            g[i] = 0;
            b[i] = 0;
            g[i + i1] = (int)x;
            b[i + i1] = 0;
        }
        for (i = i1; i < n; i++) r[i] = 256 * 255;
        for (i = i2; i < n; i++) {
            x = 256 * 255 * (double)(i - i2) / ((double)i3);
            g[i] = 256 * 255;
            b[i] = (int)x;
        }
        break;
    case C_COOL:
        for (i = 0; i < n; i++) {
            x = (double)i / ((double)n);
            r[i] = (int)(256 * 255 * x);
            b[i] = (int)(256 * 255 * (1 - x));
            g[i] = 256 * 255;
        }
        break;
    case C_REDBLUE:
        for (i = 0; i < n; i++) {
            x = (double)i / ((double)n);
            r[i] = (int)(256 * 255 * x);
            b[i] = (int)(256 * 255 * (1 - x));
            g[i] = 0;
        }
        break;
    case C_GRAY:
        for (i = 0; i < n; i++) {
            r[i] = i * 256 * 255 / n;
            b[i] = i * 256 * 255 / n;
            g[i] = i * 256 * 255 / n;
        }
        break;
    case C_CUBHLX:
        for (i = 0; i < n; i++) {
            x = (double)i / ((double)n);
            angle = 2 * pii * (start / 3.0 + 1 + rots * x);
            x = pow(x, gamma);
            amp = hue * x * (1 - x) / 2.0;
            rr = x + amp * (-.14861 * cos(angle) + 1.78277 * sin(angle));
            gg = x + amp * (-.29227 * cos(angle) - .90649 * sin(angle));
            bb = x + amp * (1.97294 * cos(angle));
            if (rr < 0.0) rr = 0.0;
            if (rr > 1.0) rr = 1.0;
            if (gg < 0.0) gg = 0.0;
            if (gg > 1.0) gg = 1.0;
            if (bb < 0.0) bb = 0.0;
            if (bb > 1.0) bb = 1.0;
            r[i] = 256 * 255 * rr;
            b[i] = 256 * 255 * bb;
            g[i] = 256 * 255 * gg;
        }
        break;
    }
}

/* Fill xpp_cmap_rgb and color_table's first/last/count. This is the
   device-independent half of what MakeColormap() in color.c used to do. */
void xpp_build_colormap(void)
{
    int i;
    int r[256], g[256], b[256];

    color_table.first = 30;
    color_table.last = XPP_MAX_COLORS - 1;
    color_table.count = color_table.last - color_table.first + 1;
    if (color_table.count > COL_TOTAL) color_table.count = COL_TOTAL;
    color_table.last = color_table.first + color_table.count;

    for (i = 0; i < XPP_MAX_COLORS; i++)
        xpp_cmap_rgb[i][0] = xpp_cmap_rgb[i][1] = xpp_cmap_rgb[i][2] = 0;

    /* the ten named colours, 8-bit values shifted up to 16 bit */
    xpp_cmap_rgb[RED][0] = 255;
    xpp_cmap_rgb[BLUE][2] = 255;
    xpp_cmap_rgb[GREEN][1] = 225;
    xpp_cmap_rgb[YELLOWGREEN][0] = 200;
    xpp_cmap_rgb[YELLOWGREEN][2] = 75;
    xpp_cmap_rgb[YELLOWGREEN][1] = 235;
    xpp_cmap_rgb[REDORANGE][0] = 240;
    xpp_cmap_rgb[REDORANGE][1] = 100;
    xpp_cmap_rgb[ORANGE][0] = 255;
    xpp_cmap_rgb[ORANGE][1] = 165;
    xpp_cmap_rgb[YELLOWORANGE][0] = 255;
    xpp_cmap_rgb[YELLOWORANGE][1] = 205;
    xpp_cmap_rgb[YELLOW][0] = 200;
    xpp_cmap_rgb[YELLOW][1] = 200;
    xpp_cmap_rgb[BLUEGREEN][2] = 200;
    xpp_cmap_rgb[BLUEGREEN][1] = 200;
    xpp_cmap_rgb[PURPLE][0] = 160;
    xpp_cmap_rgb[PURPLE][1] = 32;
    xpp_cmap_rgb[PURPLE][2] = 240;
    for (i = 20; i < 30; i++) {
        xpp_cmap_rgb[i][0] <<= 8;
        xpp_cmap_rgb[i][1] <<= 8;
        xpp_cmap_rgb[i][2] <<= 8;
    }

    make_cmaps(r, g, b, color_table.count + 1, custom_color);
    for (i = color_table.first; i <= color_table.last; i++) {
        xpp_cmap_rgb[i][0] = r[i - color_table.first];
        xpp_cmap_rgb[i][1] = g[i - color_table.first];
        xpp_cmap_rgb[i][2] = b[i - color_table.first];
    }
}

void get_ps_color(int i, float *r, float *g, float *b)
{
    float z = 1. / (65535);
    *r = z * (float)xpp_cmap_rgb[i][0];
    *g = z * (float)xpp_cmap_rgb[i][1];
    *b = z * (float)xpp_cmap_rgb[i][2];
}

void get_svg_color(int i, int *r, int *g, int *b)
{
    *r = xpp_cmap_rgb[i][0] / 255;
    *g = xpp_cmap_rgb[i][1] / 255;
    *b = xpp_cmap_rgb[i][2] / 255;
}
