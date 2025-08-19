#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>

/* Panic helper */
static void panic(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

/* Global X11 handles */
static Display *dpy;
static Window root;

int main(void) {
    /* Open display */
    printf("Starting window manager...\n");
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        panic("Unable to open X display.");
    }

    /* Get root window */
    root = DefaultRootWindow(dpy);

    /* Grab left mouse button */
    XGrabButton(dpy, Button1, AnyModifier, root,
                False, ButtonPressMask,
                GrabModeSync, GrabModeAsync,
                None, None);

    /* Listen for window management events */
    XSelectInput(dpy, root,
                 SubstructureRedirectMask |
                 SubstructureNotifyMask);

    /* Set root cursor */
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, root, cursor);

    /* Grab all possible key combinations */
    for (int keycode = 8; keycode < 256; keycode++) {
      for (int modifiers = 0; modifiers < (1 << 4); modifiers++) {
        XGrabKey(dpy, keycode, modifiers, root,
             False, GrabModeAsync, GrabModeAsync);
      }
    }

    /* Apply changes */
    XSync(dpy, False);

    /* Event loop */
    XEvent e;
    for (;;) {
        XNextEvent(dpy, &e);

        switch (e.type) {
        case ButtonPress:
            printf("Mouse button %d pressed on root %lu\n",
                   e.xbutton.button, e.xbutton.root);
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            return 0; // Exit on key press

        case KeyPress:
            printf("Keycode %d pressed on root %lu\n",
                   e.xkey.keycode, e.xkey.root);
            break;

        default:
            /* Ignore other events */
            break;
        }

        XSync(dpy, False);
    }

    XCloseDisplay(dpy);
    return 0;
}
