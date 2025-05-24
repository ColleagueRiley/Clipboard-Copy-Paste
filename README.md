# RGFW Under the Hood: Clipboard Copy/Paste
## Introduction
Reading and writing to the clipboard using low-level APIs can be tricky. There are a bunch of steps required. This tutorial simplifies the process so you can easily read and write to the clipboard using the low-level APIs.

The tutorial is based on RGFW's source code and its usage of the low-level APIs.

Note: the cocoa code is written in Pure-C. 

## Overview

1) Clipboard Paste 
- X11 (init atoms, convert section, get data)
- Win32 (open clipboard, get data, convert data, close clipboard)
- Cocoa (set datatypes, get pasteboard, get data, convert data)


2) Clipboard Copy
- X11 (init atoms, convert section, handle request, send data)
- Win32 (setup global object, convert data, open clipboard, convert string, send data, close clipboard)
- Cocoa (create datatype array, declare types, convert string, send data)

## Clipboard Paste

### X11

You'll need to initialize a few Atoms:

These should be initialized in your main function and declared globally 

```c
Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
Atom SAVE_TARGETS = XInternAtom(display, "SAVE_TARGETS", False);
``````

Declare a handler function for SelectionRequest events:

```c
int XHandleClipboardSelection(Display* display, XEvent* event, char* clipboard, size_t clipboard_len);
```

This function will handle selection targets like TARGETS, MULTIPLE, and default ones like UTF8_STRING or XA_STRING.

After setting up the handler, set your window as the selection owner and inform the clipboard manager to save it:

```c
XSetSelectionOwner(display, CLIPBOARD, window, CurrentTime);
if (XGetSelectionOwner(display, CLIPBOARD) != window) {
    printf("X11 failed to become owner of clipboard selection");
    return -1;
}

XConvertSelection(display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, window, CurrentTime);
```

Your main loop should now listen for SelectionRequest events and handle them via your handler function:

```c
while (1) {
    XEvent event;
    XNextEvent(display, &event);
    if (event.type == SelectionRequest &&
        XHandleClipboardSelection(display, &event, text, sizeof(text)))
            break; // breaks out of the main loop
}
```

After breaking from the loop, ensure the clipboard manager has saved the selection:

```c
if (XGetSelectionOwner(display, CLIPBOARD) == window) {
    XConvertSelection(display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, window, CurrentTime);

    if (QLength(display) || XEventsQueued(display, QueuedAlready) + XEventsQueued(display, QueuedAfterReading)) {
        XEvent event;
        XNextEvent(display, &event);

        if (event.type == SelectionRequest)
            XHandleClipboardSelection(display, &event, text, sizeof(text));
        else if (event.type == SelectionNotify && event.xselection.target == SAVE_TARGETS)
            ; // handled
    }
}
```

This ensures reliable clipboard behavior, even after your app exits.

To handle the clipboard, you must create some Atoms via [`XInternAtom`](https://www.x.org/releases/X11R7.5/doc/man/man3/XInternAtom.3.html).
[X Atoms](https://tronche.com/gui/x/xlib/window-information/properties-and-atoms.html) are used to ask for or send specific data or properties through X11. 

### winapi
First allocate global memory for your data and your utf-8 buffer with [`GlobalAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalalloc)

```c
HANDLE object = GlobalAlloc(GMEM_MOVEABLE, (1 + textLen) * sizeof(WCHAR));
WCHAR*  buffer = (WCHAR*) GlobalLock(object);
```

Next, you can use [`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar) to convert your string to a wide string.

```c
MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, textLen);
```

Now unlock the global object and open the clipboard

```c
GlobalUnlock(object);
OpenClipboard(NULL);
```

To update the clipboard data, you start by clearing what's currently on the clipboard via  [`EmptyClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-emptyclipboard) you can use  [`SetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata) to set the data to the utf8 object.

Finally, close the clipboard with `CloseClipboard`.

```c
EmptyClipboard();
SetClipboardData(CF_UNICODETEXT, object);

CloseClipboard();
```

### cocoa
Start by creating an array of the type of data you want to put on the clipboard and convert it to an NSArray using [`initWithObjects`](https://developer.apple.com/documentation/foundation/nsarray/1460068-initwithobjects).

```c
NSPasteboardType ntypes[] = { dataType };

NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
                    (NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);
```

Use  [`declareTypes`](https://developer.apple.com/documentation/appkit/nspasteboard/1533561-declaretypes) to declare the array as the supported data types.

You can also free the NSArray with [`NSRelease`](https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release).

```c
((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, NULL);
NSRelease(array);
```


You can convert the string to want to copy to an NSString via `stringWithUTF8String` and set the clipboard string to be that NSString using [`setString`](https://developer.apple.com/documentation/appkit/nspasteboard/1528225-setstringhttps://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release).

```c
NSString* nsstr = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);

((bool (*)(id, SEL, id, NSPasteboardType))objc_msgSend) (pasteboard, sel_registerName("setString:forType:"), nsstr, dataType);	
```

## Full examples

### X11
```c
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
```

### Winapi
```c
// compile with:
// gcc win32.c

#include <windows.h>
#include <locale.h>

#include <stdio.h>

int main() {
	// output
	if (OpenClipboard(NULL) == 0)
		return 0;

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == NULL) {
		CloseClipboard();
		return 0;
	}

	wchar_t* wstr = (wchar_t*) GlobalLock(hData);

	setlocale(LC_ALL, "en_US.UTF-8");

	size_t textLen = wcstombs(NULL, wstr, 0);

	if (textLen) {
		char* text = (char*) malloc((textLen * sizeof(char)) + 1);

		wcstombs(text, wstr, (textLen) + 1);
		text[textLen] = '\0';
		
		printf("paste: %s\n", text);
		free(text);
	}

	GlobalUnlock(hData);
	CloseClipboard();


	// input
		
	char text[] = "new text\0";

	HANDLE object = GlobalAlloc(GMEM_MOVEABLE, (sizeof(text) / sizeof(char))  * sizeof(WCHAR));

	WCHAR* buffer = (WCHAR*) GlobalLock(object);
	if (!buffer) {
		GlobalFree(object);
		return 0;
	}

	MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, (sizeof(text) / sizeof(char)));
	
	GlobalUnlock(object);
	if (OpenClipboard(NULL) == 0) {
		GlobalFree(object);
		return 0;
	}

	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, object);
	CloseClipboard();
}
```

### Cocoa
```c
// compile with:
// gcc cocoa.c -framework Foundation -framework AppKit  


#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreVideo/CVDisplayLink.h>
#include <ApplicationServices/ApplicationServices.h>

#ifdef __arm64__
/* ARM just uses objc_msgSend */
#define abi_objc_msgSend_stret objc_msgSend
#define abi_objc_msgSend_fpret objc_msgSend
#else /* __i386__ */
/* x86 just uses abi_objc_msgSend_fpret and (NSColor *)objc_msgSend_id respectively */
#define abi_objc_msgSend_stret objc_msgSend_stret
#define abi_objc_msgSend_fpret objc_msgSend_fpret
#endif

typedef void NSPasteboard;
typedef void NSString;
typedef void NSArray;
typedef void NSApplication;

typedef const char* NSPasteboardType;

typedef unsigned long NSUInteger;
typedef long NSInteger;

#define NSAlloc(nsclass) objc_msgSend_id((id)nsclass, sel_registerName("alloc"))

#define objc_msgSend_bool			((BOOL (*)(id, SEL))objc_msgSend)
#define objc_msgSend_void			((void (*)(id, SEL))objc_msgSend)
#define objc_msgSend_void_id		((void (*)(id, SEL, id))objc_msgSend)
#define objc_msgSend_uint			((NSUInteger (*)(id, SEL))objc_msgSend)
#define objc_msgSend_void_bool		((void (*)(id, SEL, BOOL))objc_msgSend)
#define objc_msgSend_void_int		((void (*)(id, SEL, int))objc_msgSend)
#define objc_msgSend_bool_void		((BOOL (*)(id, SEL))objc_msgSend)
#define objc_msgSend_void_SEL		((void (*)(id, SEL, SEL))objc_msgSend)
#define objc_msgSend_id				((id (*)(id, SEL))objc_msgSend)
#define objc_msgSend_id_id				((id (*)(id, SEL, id))objc_msgSend)
#define objc_msgSend_id_bool			((BOOL (*)(id, SEL, id))objc_msgSend)

#define objc_msgSend_class_char ((id (*)(Class, SEL, char*))objc_msgSend)

void NSRelease(id obj) {
	objc_msgSend_void(obj, sel_registerName("release"));
}

int main() {
	/* input */
	NSPasteboardType const NSPasteboardTypeString = "public.utf8-plain-text";

	NSString* dataType = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), (char*)NSPasteboardTypeString);

	NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 
	
	NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
	
	const char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));

	printf("paste: %s\n", str);
	
	char text[] = "new string\0";

	NSPasteboardType ntypes[] = { dataType };

	NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
						(NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);

	((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, NULL);
	NSRelease(array);
	
	NSString* nsstr = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);

	((bool (*)(id, SEL, id, NSPasteboardType))objc_msgSend) (pasteboard, sel_registerName("setString:forType:"), nsstr, dataType);	
}
```
