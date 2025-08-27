#CustomWM
A Custom Window Manager Using the X11 Framework

Process Breakdown:
1. Creating a Window - Done
2. Exiting the Window (i.e. Testing keypress) - Done
3. Launching Execs - Done
4. Creating tabs - Done
5. Iterating between tabs - Done
6. Drawing Graphs/Clock/TabName on the Root Window - Done
7. Auto creating Windows - HWStats, Chrome(With Cronometer),Chrome - Done
8. Controller Support - Use case removed, use thread safe Xinit to pass a secondary thread with a callback

# CustomWM

**A Custom Window Manager Using the X11 Framework**

## Overview

**CustomWM** is a lightweight, customizable window manager built atop the X11 framework. It's crafted in C and showcases features such as:

- Creating and managing windows  
- Handling keypress events (e.g., exit shortcuts)  
- Launching external applications (`execs`)  
- Tabbed window organization  
- Drawing widgets (graphs, clock, tab names) on the root window  
- Automatic window creation for tools like HWStats and Chrome (with Cronometer)  
- Thread-safe Xinit support (for future controller/thread integrations)

---

## Features

- **Window Lifecycle Management**: Create and manage X11 windows seamlessly.  
- **Interactive Controls**: Respond to keyboard events such as window closing and navigation.  
- **Application Launching**: Spawn external applications directly through the WM.  
- **Tabbed View Support**: Group windows into tabs and easily switch between them.  
- **On-Screen Visuals**: Render graphs, clocks, and tab labels on the root window.  
- **Auto-Spawn Windows**: Automatically open utility windows (e.g., HWStats, Chrome with Cronometer).  
- **Multithreading Ready**: Uses thread-safe `XInitThreads` to support background routines or controllers.

## Quick Start

```bash
git clone https://github.com/AsbestosLampshade/CustomWM.git
cd CustomWM
make
./customwm
