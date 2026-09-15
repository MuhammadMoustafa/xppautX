/* xdrive: send keys to a running xppaut and grab screenshots, for
   before/after GUI comparisons (tools/guicheck.sh).
   usage: xdrive SCRIPT OUTDIR
   script lines:  key <keysym>  |  sleep <ms>  |  shot <name>  (-> OUTDIR/name.ppm)
   Keys go to the main window with XSendEvent; xppaut reads KeyPress from any
   of its windows, so no focus or XTest is needed. Screenshots are of the
   main window and its children, which includes the pop-up menus. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Window find_win(Display *d, Window w, const char *name)
{
  Window root, parent, *kids = NULL, r = 0;
  unsigned int n, i;
  char *wn = NULL;
  if (XFetchName(d, w, &wn) && wn) {
    int hit = strstr(wn, name) != NULL;
    XFree(wn);
    if (hit) return w;
  }
  if (!XQueryTree(d, w, &root, &parent, &kids, &n)) return 0;
  for (i = 0; i < n && !r; i++) r = find_win(d, kids[i], name);
  if (kids) XFree(kids);
  return r;
}

static void send_key(Display *d, Window w, const char *ks)
{
  XKeyEvent e;
  KeySym sym = XStringToKeysym(ks);
  memset(&e, 0, sizeof e);
  e.display = d; e.window = w; e.root = DefaultRootWindow(d);
  e.subwindow = None; e.time = CurrentTime; e.same_screen = True;
  e.keycode = XKeysymToKeycode(d, sym);
  e.state = 0;
  if (sym >= 'A' && sym <= 'Z') e.state = ShiftMask;
  e.type = KeyPress;
  XSendEvent(d, w, True, KeyPressMask, (XEvent *)&e);
  e.type = KeyRelease;
  XSendEvent(d, w, True, KeyReleaseMask, (XEvent *)&e);
  XFlush(d);
}

static void shot(Display *d, Window w, const char *file)
{
  XWindowAttributes a;
  int x, y;
  XImage *img;
  FILE *fp;
  XGetWindowAttributes(d, w, &a);
  img = XGetImage(d, w, 0, 0, a.width, a.height, AllPlanes, ZPixmap);
  if (!img) { fprintf(stderr, "XGetImage failed\n"); return; }
  fp = fopen(file, "wb");
  fprintf(fp, "P6\n%d %d\n255\n", a.width, a.height);
  for (y = 0; y < a.height; y++)
    for (x = 0; x < a.width; x++) {
      unsigned long p = XGetPixel(img, x, y);
      fputc((p >> 16) & 255, fp); fputc((p >> 8) & 255, fp); fputc(p & 255, fp);
    }
  fclose(fp);
  XDestroyImage(img);
}

static int on_err(Display *d, XErrorEvent *e) { (void)d; fprintf(stderr, "X error %d\n", e->error_code); return 0; }

int main(int argc, char **argv)
{
  Display *d = XOpenDisplay(NULL);
  Window w = 0;
  char line[256], cmd[32], arg[200], path[1024];
  FILE *sc;
  int tries;
  if (!d || argc < 3) return 2;
  XSetErrorHandler(on_err);
  for (tries = 0; tries < 100 && !w; tries++) {
    w = find_win(d, DefaultRootWindow(d), "XPP Ver");
    if (!w) usleep(100000);
  }
  if (!w) { fprintf(stderr, "no xppaut window\n"); return 1; }
  sc = fopen(argv[1], "r");
  if (!sc) return 2;
  while (fgets(line, sizeof line, sc)) {
    if (sscanf(line, "%31s %199s", cmd, arg) < 1) continue;
    if (!strcmp(cmd, "key")) send_key(d, w, arg);
    else if (!strcmp(cmd, "sleep")) usleep(atoi(arg) * 1000);
    else if (!strcmp(cmd, "shot")) {
      snprintf(path, sizeof path, "%s/%s.ppm", argv[2], arg);
      shot(d, w, path);
    }
  }
  XCloseDisplay(d);
  return 0;
}
