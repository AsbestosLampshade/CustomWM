// wm.c
#define _XOPEN_SOURCE 700
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
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

/* Draw the whole bar: background, left title, right clock */
static void draw_bar(GC gc, int screen, int bar_height, int screen_width) {
    // background
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));
    XFillRectangle(dpy, bar, gc, 0, 0, (unsigned int)screen_width, (unsigned int)bar_height);

    // left: active window name
    const char *title = "Welcome to the Window Manager!";
    if (clients && clients->name) title = clients->name;

    XSetForeground(dpy, gc, WhitePixel(dpy, screen));
    XDrawString(dpy, bar, gc, 10, bar_height - 5, title, (int)strlen(title));

    // right: clock HH:MM:SS
    char buf[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof buf, "%H:%M:%S", tm);

    int text_x = screen_width - 80; // simple right area; no font extents yet
    if (text_x < 10) text_x = 10;
    XDrawString(dpy, bar, gc, text_x, bar_height - 5, buf, (int)strlen(buf));
}

static void focus_window(Window w) {
    if (w && w != None) {
        XRaiseWindow(dpy, w);
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    }
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

    int bar_h = 20;
    bar = XCreateSimpleWindow(dpy, root, 0, 0, (unsigned)screen_w, (unsigned)bar_h,
                              0, WhitePixel(dpy, screen), BlackPixel(dpy, screen));
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
                c->name = "Window"; // TODO: fetch _NET_WM_NAME or WM_NAME
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
