// compile with:
// gcc x11.c -lX11

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <X11/Xatom.h>

Atom UTF8_STRING, SAVE_TARGETS;

int XHandleClipboardSelection(Display* display, XEvent* event, char* clipboard, size_t clipboard_len) {
    int ret = 0;

    const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
	const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
	const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);

    const XSelectionRequestEvent* request = &event->xselectionrequest;
    const Atom formats[] = { UTF8_STRING, XA_STRING };
    const int formatCount = sizeof(formats) / sizeof(formats[0]);

    if (request->target == TARGETS) {
        const Atom targets[] = { TARGETS, MULTIPLE, UTF8_STRING, XA_STRING };

        XChangeProperty(display, request->requestor, request->property,
                        XA_ATOM, 32, PropModeReplace, (unsigned char*)targets, sizeof(targets) / sizeof(Atom));
    }  else if (request->target == MULTIPLE) {
		Atom* targets = NULL;

		Atom actualType = 0;
		int actualFormat = 0;
		unsigned long count = 0, bytesAfter = 0;

		XGetWindowProperty(display, request->requestor, request->property, 0, LONG_MAX,
							False, ATOM_PAIR, &actualType, &actualFormat, &count, &bytesAfter, (unsigned char**) &targets);

		unsigned long i;
		for (i = 0; i < (unsigned int)count; i += 2) {
			if (targets[i] == UTF8_STRING || targets[i] == XA_STRING) {
				XChangeProperty(display, request->requestor, targets[i + 1], targets[i],
					8, PropModeReplace, (const unsigned char *)clipboard, clipboard_len);
                ret = 1;    
            }
			else
				targets[i + 1] = None;
		}

		XChangeProperty(display,
			request->requestor, request->property, ATOM_PAIR, 32,
			PropModeReplace, (unsigned char*)targets, count);

		XFlush(display);
		XFree(targets);        
	} else if (request->target == SAVE_TARGETS)
        XChangeProperty(display, request->requestor, request->property, 0, 32, PropModeReplace, NULL, 0);
    else {
        int i;
        for (i = 0;  i < formatCount;  i++) {
			if (request->target != formats[i])
				continue;
			XChangeProperty(display, request->requestor, request->property, request->target,
								8, PropModeReplace, (unsigned char*)clipboard, clipboard_len);
            ret = 1;
        }	    
    }

    XEvent reply = { SelectionNotify };
    reply.xselection.property = request->property;
    reply.xselection.display = request->display;
    reply.xselection.requestor = request->requestor;
    reply.xselection.selection = request->selection;
    reply.xselection.target = request->target;
    reply.xselection.time = request->time;

    XSendEvent(display, request->requestor, False, 0, &reply);

    return ret;
}

int main(void) {
    Display* display = XOpenDisplay(NULL);
 
    Window window = XCreateSimpleWindow(display, RootWindow(display, DefaultScreen(display)), 10, 10, 200, 200, 1,
                                 BlackPixel(display, DefaultScreen(display)), WhitePixel(display, DefaultScreen(display)));
 
    XSelectInput(display, window, ExposureMask | KeyPressMask); 

	UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
	SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);

	const Atom CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
	const Atom XSEL_DATA = XInternAtom(display, "XSEL_DATA", 0);
	const Atom CLIPBOARD_MANAGER = XInternAtom((Display*) display, "CLIPBOARD_MANAGER", False);

	// input
	XConvertSelection(display, CLIPBOARD, UTF8_STRING, XSEL_DATA, window, CurrentTime);
	XSync(display, 0);

	XEvent event;
	XNextEvent(display, &event);

	if (event.type == SelectionNotify && event.xselection.selection == CLIPBOARD && event.xselection.property != 0) {

		int format;
		unsigned long N, size;
		char* data, * s = NULL;
		Atom target;

		XGetWindowProperty(event.xselection.display, event.xselection.requestor,
			event.xselection.property, 0L, (~0L), 0, AnyPropertyType, &target,
			&format, &size, &N, (unsigned char**) &data);

		if (target == UTF8_STRING || target == XA_STRING) {
			printf("paste: %s\n", data);
			XFree(data);
		}

		XDeleteProperty(event.xselection.display, event.xselection.requestor, event.xselection.property);
	}

	// output
	char text[] = "new string\0";

	XSetSelectionOwner(display, CLIPBOARD, window, CurrentTime);
	if (XGetSelectionOwner(display, CLIPBOARD) != window) {
    	printf("X11 failed to become owner of clipboard selection");
		return -1;
	}
		
	while (1) {
		XNextEvent(display, &event);
		if (event.type == SelectionRequest &&
            XHandleClipboardSelection(display, &event, text, sizeof(text) / sizeof(char)))
                break;
    }

	if (XGetSelectionOwner(display, CLIPBOARD) == window) {
        XConvertSelection(display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, window, CurrentTime);
        XEvent event;
        XPending(display);

        if (QLength(display) || XEventsQueued(display, QueuedAlready) + XEventsQueued(display, QueuedAfterReading)) {
            XNextEvent(display, &event);

            switch (event.type) {
                case SelectionRequest:
                    XHandleClipboardSelection(display, &event, text, sizeof(text) / sizeof(char));
                    break;
                case SelectionNotify:
                    if (event.xselection.target == SAVE_TARGETS)
                        break;
                default: break;
            }
        }
    }
    XCloseDisplay(display);
 }
