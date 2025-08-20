#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

#define AltMask Mod1Mask
#define WinMask Mod4Mask

typedef struct Client {
  Window win;
  struct Client *next;
  struct Client *prev;
  
} Client;

static Client *clients = NULL;

/* Panic helper */
static void panic(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

/* Global X11 handles */
static Display *dpy;
static Window root;

int commandHandling(XEvent e){
  // This function is for command handling logic.

  //Exit
  if (e.xkey.keycode == XKeysymToKeycode(dpy, XK_q))  { //Should be fine as only this specific sequence is grabbed
    printf("Exiting window manager...\n");
    XUngrabPointer(dpy, CurrentTime);
    XUngrabKeyboard(dpy, CurrentTime);
    return -1;// Exit the event loop
  }

  //Terminal
  if (e.xkey.keycode == XKeysymToKeycode(dpy, XK_Return)) { 
    printf("Shift+Enter pressed → launching xterm\n");
    if (fork() == 0) {
      printf("Launching st...\n");
      execlp("st", "st", NULL);
    _exit(1);  // if exec fails
    }
  } 

  //Switching windows Forward
  if (e.xkey.keycode == XKeysymToKeycode(dpy, XK_Tab) && (e.xkey.state & Mod1Mask)&& !(e.xkey.state & ShiftMask)) {
    // Check if there are clients to switch
    printf("Alt+Tab pressed → switching windows forward\n");
    if (clients && clients->prev) {
      printf("Switching to next client\n");
        Client *current = clients;
        XUnmapWindow(dpy, current->win);
        current = current->prev ? current->prev : clients;
        XMapWindow(dpy, current->win);
        clients = current; // Update the global pointer
        XSetInputFocus(dpy, current->win, RevertToPointerRoot, CurrentTime);
    }
  }

  //Switching windows Backward
  if (e.xkey.keycode == XKeysymToKeycode(dpy, XK_Tab) && (e.xkey.state & Mod1Mask) && (e.xkey.state & ShiftMask)) {
    // Check if there are clients to switch
    printf("Alt+Shift+Tab pressed → switching windows backward\n");
    if (clients && clients->next) {
        printf("Switching to previous client\n");
        Client *current = clients;
        XUnmapWindow(dpy, current->win);
        current = current->next ? current->next : clients;
        XMapWindow(dpy, current->win);
        clients = current; // Update the global pointer
        XSetInputFocus(dpy, current->win, RevertToPointerRoot, CurrentTime);
    }
  }

  return 0; // Continue processing other events
}

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

    // grab Shift+Alt+Q
    KeyCode qKeyCode = XKeysymToKeycode(dpy, XStringToKeysym("q"));
    XGrabKey(dpy, qKeyCode, ShiftMask | Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    // grab Shift+Alt+Enter
    KeyCode returnKey = XKeysymToKeycode(dpy, XStringToKeysym("Return"));
    XGrabKey(dpy, returnKey, ShiftMask | Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    KeyCode tabKey = XKeysymToKeycode(dpy, XStringToKeysym("Tab"));
    XGrabKey(dpy, tabKey, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    KeyCode shiftTabKey = XKeysymToKeycode(dpy, XStringToKeysym("Tab"));
    XGrabKey(dpy, shiftTabKey, ShiftMask | Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

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
            Window w = e.xbutton.subwindow;
            if (w != None) {
              XRaiseWindow(dpy, w);
              XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
            }
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            break;

        case KeyPress:
          // Handle key press events
          if(commandHandling(e)==-1) {
            // Exit the event loop if commandHandling returns -1
            printf("Exiting event loop...\n");
            return 0;
          }
          //If any keys are to be shared Please use AllowEvents
          // XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
          printf("Keycode %d pressed on root %lu\n",
                  e.xkey.keycode, e.xkey.root);
          break;
        
        case MapRequest:
          printf("MapRequest received → mapping window %lu\n", e.xmaprequest.window);
          XMapWindow(dpy, e.xmaprequest.window);
          //Window System
          Client *c = malloc(sizeof(Client));
          c->win = e.xmaprequest.window;

          if (!clients) {
              // First client in the list
              c->next = c;
              c->prev = c;
              clients = c;
              XMapWindow(dpy, c->win); // Map the first client immediately
          } else {
              // Add to the circular doubly linked list
              c->next = clients;
              c->prev =clients->prev;
              clients->prev->next = c;
              clients->prev = c;
              //XUnmapWindow(dpy, c->win); // Keep hidden until selected
              XMapWindow(dpy, c->win);//Create & Jump to
              XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
          }
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
