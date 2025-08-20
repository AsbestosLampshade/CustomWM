// wm.c
#define _XOPEN_SOURCE 700
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>

#define AltMask Mod1Mask
#define WinMask Mod4Mask

typedef struct Client {
    Window win;
    struct Client *next;
    struct Client *prev;
    char *name;
} Client;

static Display *dpy;
static Window root, bar;
static Client *clients = NULL;

static void panic(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}


/* global/static Xft objects */
static XftDraw  *xft_draw;
static XftColor xft_white;
static XftFont  *xft_font;

static void init_bar_fonts(int screen) {
    Visual *visual = DefaultVisual(dpy, screen);
    Colormap colormap = DefaultColormap(dpy, screen);

    xft_draw = XftDrawCreate(dpy, bar, visual, colormap);

    if (!XftColorAllocName(dpy, visual, colormap, "white", &xft_white)) {
        fprintf(stderr, "Could not allocate white color\n");
        exit(1);
    }

    xft_font = XftFontOpenName(dpy, screen, "monospace:size=18:bold");
    if (!xft_font) {
        fprintf(stderr, "Could not load font\n");
        exit(1);
    }
}

/* Draw the whole bar: background, left title, right clock */
static void draw_bar(GC gc, int screen, int bar_height, int screen_width) {
    // background
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));
    XFillRectangle(dpy, bar, gc, 0, 0, (unsigned int)screen_width, (unsigned int)bar_height);

    // left: active window name
    const char *title = "Welcome to the Window Manager!";
    if (clients && clients->name) title = clients->name;

    XftDrawStringUtf8(xft_draw, &xft_white, xft_font,
                      10, bar_height - 5,
                      (FcChar8*)title, strlen(title));

    // right: clock HH:MM:SS
    char buf[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof buf, "%H:%M:%S", tm);

    // measure text width for right alignment
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, xft_font, (FcChar8*)buf, strlen(buf), &extents);
    int text_x = screen_width - extents.xOff - 10;

    XftDrawStringUtf8(xft_draw, &xft_white, xft_font,
                      text_x, bar_height - 5,
                      (FcChar8*)buf, strlen(buf));
}


static void focus_window(Window w) {
    if (w && w != None) {
        XRaiseWindow(dpy, w);
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    }
}

/* Get window name (_NET_WM_NAME or WM_NAME) */
char* get_window_name(Display *dpy, Window w) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;
  char *name = NULL;

  Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
  Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);

  // First try _NET_WM_NAME (UTF-8)
  if (XGetWindowProperty(dpy, w, net_wm_name, 0, (~0L), False,
                         utf8_string, &actual_type, &actual_format,
                         &nitems, &bytes_after, &prop) == Success) {
      if (prop) {
          name = strndup((char*)prop, nitems);
          XFree(prop);
          return name;
      }
  }

  // Fallback: WM_NAME (may not be UTF-8)
  XTextProperty text;
  if (XGetWMName(dpy, w, &text) && text.value && text.nitems) {
      name = strndup((char*)text.value, text.nitems);
      XFree(text.value);
      return name;
  }

  // If all fails
  return strdup("Unnamed");
}

static int handle_keypress(XKeyEvent *kev, KeyCode qKey, KeyCode retKey, KeyCode tabKey, int *screen_w, int *screen_h, int bar_h) {
    unsigned int s = kev->state;

    // Shift+Alt+Q → quit
    if (kev->keycode == qKey && (s & (ShiftMask | AltMask)) == (ShiftMask | AltMask)) {
        return -1;
    }

    // Shift+Alt+Enter → launch st
    if (kev->keycode == retKey && (s & (ShiftMask | AltMask)) == (ShiftMask | AltMask)) {
        if (fork() == 0) {
            execlp("st", "st", NULL);
            _exit(1);
        }
        return 1;
    }

    // Alt+Tab → next (forward)
    if (kev->keycode == tabKey && (s & AltMask) && !(s & ShiftMask)) {
        if (clients && clients->prev) {
            Client *cur = clients;
            XUnmapWindow(dpy, cur->win);
            cur = cur->prev ? cur->prev : clients;
            XMapWindow(dpy, cur->win);
            XMoveResizeWindow(dpy, cur->win, 0, bar_h, *screen_w, *screen_h - bar_h);
            clients = cur;
            focus_window(cur->win);
        }
        return 1;
    }

    // Alt+Shift+Tab → previous (backward)
    if (kev->keycode == tabKey && (s & AltMask) && (s & ShiftMask)) {
        if (clients && clients->next) {
            Client *cur = clients;
            XUnmapWindow(dpy, cur->win);
            cur = cur->next ? cur->next : clients;
            XMapWindow(dpy, cur->win);
            XMoveResizeWindow(dpy, cur->win, 0, bar_h, *screen_w, *screen_h - bar_h);
            clients = cur;
            focus_window(cur->win);
        }
        return 1;
    }

    // Not a WM hotkey — allow client to receive it
    XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
    return 0;
}

int main(void) {
    printf("Starting window manager...\n");
    dpy = XOpenDisplay(NULL);
    if (!dpy) panic("Unable to open X display.");

    int screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    int bar_h = 30;
    bar = XCreateSimpleWindow(dpy, root, 0, 0, (unsigned)screen_w, (unsigned)bar_h,
                              0, WhitePixel(dpy, screen), BlackPixel(dpy, screen));
    init_bar_fonts(screen);

    XSelectInput(dpy, bar, ExposureMask);
    XMapWindow(dpy, bar);

    GC gc = XCreateGC(dpy, bar, 0, NULL);
    XSetForeground(dpy, gc, WhitePixel(dpy, screen));

    // Pointer grab for Button1
    XGrabButton(dpy, Button1, AnyModifier, root, False, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);

    // Manage substructure
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

    // Cursor
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, root, cursor);

    // Key grabs
    KeyCode qKey     = XKeysymToKeycode(dpy, XK_q);
    KeyCode retKey   = XKeysymToKeycode(dpy, XK_Return);
    KeyCode tabKey   = XKeysymToKeycode(dpy, XK_Tab);

    XGrabKey(dpy, qKey,   ShiftMask | AltMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, retKey, ShiftMask | AltMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, tabKey, AltMask,            root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, tabKey, ShiftMask | AltMask,root, True, GrabModeAsync, GrabModeAsync);

    XSync(dpy, False);

    // Draw the bar initially
    draw_bar(gc, screen, bar_h, screen_w);
    XFlush(dpy);

    int xfd = ConnectionNumber(dpy);
    int running = 1;
    
    //Pre-setup
    if (fork() == 0) {
      sleep(1); // Give time for the bar to be drawn
      execlp("chromium", "chromium", NULL);
      _exit(1);
    }
    if (fork() == 0) {
      sleep(1); // Give time for the bar to be drawn
      execlp("/home/alfaugus/projects/custom_windows/hw_stats", "hw_stats", NULL);
      _exit(1);
    }
    // Main event loop
    while (running) {
        // Wait for either X events or 1-second timeout
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int r = select(xfd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0) {
            perror("select");
            break;
        }

        // Timeout → refresh clock
        if (r == 0) {
            draw_bar(gc, screen, bar_h, screen_w);
            XFlush(dpy);
            continue;
        }

        // There are X events ready
        while (XPending(dpy)) {
            XEvent e;
            XNextEvent(dpy, &e);

            switch (e.type) {
            case ButtonPress: {
                printf("Mouse button %d on root %lu\n", e.xbutton.button, e.xbutton.root);
                Window w = e.xbutton.subwindow;
                if (w != None) focus_window(w);
                XAllowEvents(dpy, ReplayPointer, CurrentTime);
                break;
            }

            case KeyPress: {
                int act = handle_keypress(&e.xkey, qKey, retKey, tabKey, &screen_w, &screen_h, bar_h);
                if (act < 0) running = 0;
                if (act != 0) {
                    draw_bar(gc, screen, bar_h, screen_w);
                    XFlush(dpy);
                }
                break;
            }

            case MapRequest: {
                Window w = e.xmaprequest.window;
                printf("MapRequest → %lu\n", w);
                XMapWindow(dpy, w);
                XMoveResizeWindow(dpy, w, 0, bar_h, screen_w, screen_h - bar_h);

                Client *c = malloc(sizeof(Client));
                if (!c) panic("malloc");
                c->win = w;
                c->name = get_window_name(dpy, c->win);
                if (!clients) {
                    c->next = c->prev = c;
                    clients = c;
                } else {
                    c->next = clients;
                    c->prev = clients->prev;
                    clients->prev->next = c;
                    clients->prev = c;
                    clients = c; // jump to newly created
                }
                focus_window(c->win);
                draw_bar(gc, screen, bar_h, screen_w);
                break;
            }

            case Expose:
                if (e.xexpose.window == bar && e.xexpose.count == 0) {
                    draw_bar(gc, screen, bar_h, screen_w);
                }
                break;

            default:
                break;
            }
        }
        XSync(dpy, False);
    }

    XCloseDisplay(dpy);
    return 0;
}
