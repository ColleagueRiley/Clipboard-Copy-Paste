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

To handle the clipboard, you must create some Atoms via [`XInternAtom`](https://www.x.org/releases/X11R7.5/doc/man/man3/XInternAtom.3.html).
[X Atoms](https://tronche.com/gui/x/xlib/window-information/properties-and-atoms.html) are used to ask for or send specific data or properties through X11. 

You'll need three atoms, 

1) UTF8_STRING: Atom for a UTF-8 string.
2) CLIPBOARD: Atom for getting clipboard data.
3) XSEL_DATA: Atom to get selection data.

```c
const Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
const Atom CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
const Atom XSEL_DATA = XInternAtom(display, "XSEL_DATA", 0);
```

Now, to get the clipboard data you have to request that the clipboard section be converted to UTF8 using [`XConvertSelection`](https://tronche.com/gui/x/xlib/window-information/XConvertSelection.html).

 use [`XSync`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html) to send the request to the server. 

```c
XConvertSelection(display, CLIPBOARD, UTF8_STRING, XSEL_DATA, window, CurrentTime);
XSync(display, 0);
```

The selection will be converted and sent back to the client as a [`XSelectionNotify`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSelectionEvent.3.html) event. You can get the next event, which should be the `SelectionNotify` event with [`XNextEvent`](https://tronche.com/gui/x/xlib/event-handling/manipulating-event-queue/XNextEvent.html).

```c
XEvent event;
XNextEvent(display, &event);
```

 make sure the event is a `SelectionNotify` event.  use `.selection` to ensure the type is a `CLIPBOARD`. Finally, make sure `.property` is not 0 and can be retrieved.

```c
if (event.type == SelectionNotify && event.xselection.selection == CLIPBOARD && event.xselection.property != 0) {
```

You can get the converted data via [`XGetWindowProperty`](https://www.x.org/releases/X11R7.5/doc/man/man3/XChangeProperty.3.html) using the selection property. 

```c
    int format;
    unsigned long N, size;
    char* data, * s = NULL;
    Atom target;

    XGetWindowProperty(event.xselection.display, event.xselection.requestor,
	    event.xselection.property, 0L, (~0L), 0, AnyPropertyType, &target,
	    &format, &size, &N, (unsigned char**) &data);
```

Make sure the data is in the right format by checking `target`

```c 
    if (target == UTF8_STRING || target == XA_STRING) {
```

The data is stored in `data`, once you're done with it free it with [`XFree`](https://software.cfht.hawaii.edu/man/x11/XFree(3x)).

You can also delete the property via [`XDeleteProperty`](https://tronche.com/gui/x/xlib/window-information/XDeleteProperty.html).

```c
        XFree(data);
    }

    XDeleteProperty(event.xselection.display, event.xselection.requestor, event.xselection.property);
}
```

### winapi

First, open the clipboard [`OpenClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard). 

```c
if (OpenClipboard(NULL) == 0)
	return 0;
```

Get the clipboard data as a Unicode string via [`GetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata)

If the data is NULL, you should close the clipboard using [`CloseClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard)

```c
HANDLE hData = GetClipboardData(CF_UNICODETEXT);
if (hData == NULL) {
	CloseClipboard();
	return 0;
}
```

Next, you need to convert the Unicode data back to utf8.  

Start by locking memory for the utf8 data via [`GlobalLock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globallock).

```
wchar_t* wstr = (wchar_t*) GlobalLock(hData);
```

Use [`setlocale`](https://en.cppreference.com/w/c/locale/setlocale) to ensure the data format is utf8.

Get the size of the UTF-8 version with [`wcstombs`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-wcstombs-l?view=msvc-170).

```C
setlocale(LC_ALL, "en_US.UTF-8");

size_t textLen = wcstombs(NULL, wstr, 0);
```

If the size is valid, convert the data using [`wcstombs`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-wcstombs-l?view=msvc-170).

```c
if (textLen) {
	char* text = (char*) malloc((textLen * sizeof(char)) + 1);

	wcstombs(text, wstr, (textLen) + 1);
	text[textLen] = '\0';

    free(text);
}
```

Make sure to free leftover global data using [`GlobalUnlock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalunlock) and close the clipboard with [`CloseClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard).

```c
GlobalUnlock(hData);
CloseClipboard();
```

### cocoa
Cocoa uses [`NSPasteboardTypeString`](https://developer.apple.com/documentation/appkit/nspasteboardtypestring) for asking for a string data. You'll have to define this yourself if you're not using Objective-C.

```c
NSPasteboardType const NSPasteboardTypeString = "public.utf8-plain-text";
```

Although the is a c-string and Cocoa uses NSStrings, you can convert the c-string to an NSString via [`stringWithUTF8String`](https://developer.apple.com/documentation/foundation/nsstring/1497379-stringwithutf8string).

```c
NSString* dataType = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), (char*)NSPasteboardTypeString);
```

Now we'll use [`generalPasteboard`](https://developer.apple.com/documentation/uikit/uipasteboard/1622106-generalpasteboard) to get the default pasteboard object. 

```c
NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 
```

Then you can get the pasteboard's string data with the `dataType` using [`stringForType`](https://developer.apple.com/documentation/appkit/nspasteboard/1533566-stringfortype).

However, it will give you an NSString, which can be converted with [`UTF8String`](https://developer.apple.com/documentation/foundation/nsstring/1411189-utf8string).

```c
NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
const char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));
```

## Clipboard Copy

### X11

To copy to the clipboard you'll need a few more Atoms. 

1) SAVE_TARGETS: to request a section to convert to (for copying). 
2) TARGETS: To handle one requested target
3) MULTIPLE: When there are multiple request targets
4) ATOM_PAIR: to get the supported data types.
5) CLIPBOARD_MANAGER: to access data from the clipboard manager.


```c
const Atom SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);
const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);
const Atom CLIPBOARD_MANAGER = XInternAtom((Display*) display, "CLIPBOARD_MANAGER", False);
```

We can request a clipboard section. First, set the owner of the section to be a client window via [`XSetSelectionOwner`](https://tronche.com/gui/x/xlib/window-information/XSetSelectionOwner.html). Next request a converted section using [`XConvertSelection`](https://tronche.com/gui/x/xlib/window-information/XConvertSelection.html).
 

```c
XSetSelectionOwner((Display*) display, CLIPBOARD, (Window) window, CurrentTime);

XConvertSelection((Display*) display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, (Window) window, CurrentTime);
```

The rest of the code would exist in an event loop. You can create an external event loop from your main event loop if you wish or add this to your main event loop.

We'll be handling [`SelectionRequest`](https://tronche.com/gui/x/xlib/events/client-communication/selection-request.html) in order to update the clipboard selection to data to hold the string data.

```c
if (event.type == SelectionRequest) {
    const XSelectionRequestEvent* request = &event.xselectionrequest;
```

At the end of the SelectionNotify event, a response will be sent back to the requester. The structure should be created here and modified depending on the request data. 

```c
	XEvent reply = { SelectionNotify };
	reply.xselection.property = 0;
```

The first target we will handle is `TARGETS` when the requestor wants to know which targets are supported.

```c
	if (request->target == TARGETS) {
```

I will create an array of supported targets

```c
        const Atom targets[] = { TARGETS,
								MULTIPLE,
								UTF8_STRING,
								XA_STRING };
```

This array can be passed using [`XChangeProperty`](https://tronche.com/gui/x/xlib/window-information/XChangeProperty.html).

I'll also change the selection property so the requestor knows what property we changed.

```c
		XChangeProperty(display,
			request->requestor,
			request->property,
			4,
			32,
			PropModeReplace,
			(unsigned char*) targets,
			sizeof(targets) / sizeof(targets[0]));

		reply.xselection.property = request->property;
	}
```

Next, I will handle `MULTIPLE` targets.

```c
	if (request->target == MULTIPLE) {
```

We'll start by getting the supported targets via `XGetWindowProperty`

```c
		Atom* targets = NULL;

		Atom actualType = 0;
		int actualFormat = 0;
		unsigned long count = 0, bytesAfter = 0;

		XGetWindowProperty(display, request->requestor, request->property, 0, LONG_MAX, False, ATOM_PAIR, &actualType, &actualFormat, &count, &bytesAfter, (unsigned char **) &targets);
```

Now we'll loop through the supported targets. If the supported targets match one of our supported targets, we can pass the data with `XChangeProperty`.

If the target is not used, the second argument should be set to None, marking it as unused.

```c
		unsigned long i;
		for (i = 0; i < count; i += 2) {
			if (targets[i] == UTF8_STRING || targets[i] == XA_STRING) {
				XChangeProperty((Display*) display,
					request->requestor,
					targets[i + 1],
					targets[i],
					8,
					PropModeReplace,
					(unsigned char*) text,
					sizeof(text));
				XFlush(display);
			} else {
				targets[i + 1] = None;
			}
		}
```

You can pass the final array of supported targets to the requestor using `XChangeProperty`. This tells the requestor which targets to expect for the original list it sent. 

The message will be sent out asap when [`XFlush`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html) is called. 

You can free your copy of the target array with `XFree`.

```c
		XChangeProperty((Display*) display,
			request->requestor,
			request->property,
			ATOM_PAIR,
			32,
			PropModeReplace,
			(unsigned char*) targets,
			count);

		XFlush(display);
		XFree(targets);

		reply.xselection.property = request->property;
	}
```

For the final step of the event, send the selection back to the requestor via [`XSendEvent`](https://tronche.com/gui/x/xlib/event-handling/XSendEvent.html).

Then flush the queue with [`XFlush`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html).

```c
	reply.xselection.display = request->display;
	reply.xselection.requestor = request->requestor;
	reply.xselection.selection = request->selection;
	reply.xselection.target = request->target;
	reply.xselection.time = request->time;

	XSendEvent((Display*) display, request->requestor, False, 0, &reply);
	XFlush(display);
}
```

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

int main(void) {
    Display* display = XOpenDisplay(NULL);
 
    Window window = XCreateSimpleWindow(display, RootWindow(display, DefaultScreen(display)), 10, 10, 200, 200, 1,
                                 BlackPixel(display, DefaultScreen(display)), WhitePixel(display, DefaultScreen(display)));
 
    XSelectInput(display, window, ExposureMask | KeyPressMask); 

	const Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
	const Atom CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
	const Atom XSEL_DATA = XInternAtom(display, "XSEL_DATA", 0);

	const Atom SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);
	const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
	const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
	const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);
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

	XSetSelectionOwner((Display*) display, CLIPBOARD, (Window) window, CurrentTime);

	XConvertSelection((Display*) display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, (Window) window, CurrentTime);
		
	Bool running = True;
	while (running) {
		XNextEvent(display, &event);
		if (event.type == SelectionRequest) {
			const XSelectionRequestEvent* request = &event.xselectionrequest;

			XEvent reply = { SelectionNotify };
			reply.xselection.property = 0;

			if (request->target == TARGETS) {
				const Atom targets[] = { TARGETS,
										MULTIPLE,
										UTF8_STRING,
										XA_STRING };

				XChangeProperty(display,
					request->requestor,
					request->property,
					4,
					32,
					PropModeReplace,
					(unsigned char*) targets,
					sizeof(targets) / sizeof(targets[0]));

				reply.xselection.property = request->property;
			}

			if (request->target == MULTIPLE) {	
				Atom* targets = NULL;

				Atom actualType = 0;
				int actualFormat = 0;
				unsigned long count = 0, bytesAfter = 0;

				XGetWindowProperty(display, request->requestor, request->property, 0, LONG_MAX, False, ATOM_PAIR, &actualType, &actualFormat, &count, &bytesAfter, (unsigned char **) &targets);

				unsigned long i;
				for (i = 0; i < count; i += 2) {
					Bool found = False; 

					if (targets[i] == UTF8_STRING || targets[i] == XA_STRING) {
						XChangeProperty((Display*) display,
							request->requestor,
							targets[i + 1],
							targets[i],
							8,
							PropModeReplace,
							(unsigned char*) text,
							sizeof(text));
						XFlush(display);
						running = False;
					} else {
						targets[i + 1] = None;
					}
				}

				XChangeProperty((Display*) display,
					request->requestor,
					request->property,
					ATOM_PAIR,
					32,
					PropModeReplace,
					(unsigned char*) targets,
					count);

				XFlush(display);
				XFree(targets);

				reply.xselection.property = request->property;
			}

			reply.xselection.display = request->display;
			reply.xselection.requestor = request->requestor;
			reply.xselection.selection = request->selection;
			reply.xselection.target = request->target;
			reply.xselection.time = request->time;

			XSendEvent((Display*) display, request->requestor, False, 0, &reply);
			XFlush(display);
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
