# RGFW Under the Hood: Clipboard Copy/Paste
# Introduction
# Overview

1) Clipboard Paste 
- X11 (init atoms, convert section, get data)
- Win32 (open clipboard, get data, convert data, close clipboard)
- Cocoa (set datatypes, get pasteboard, get data, convert data)


2) Clipboard Copy
- X11 (init atoms, convert section, handle request, send data)
- Win32 (setup global object, convert data, open clipboard, convert string, send data, close clipboard)
- Cocoa (create datatype array, declaretypes, convert string, send data)

## Clipboard Paste

### X11

In order to handle the clipboard you need to create some Atoms via [`XInternAtom`](https://www.x.org/releases/X11R7.5/doc/man/man3/XInternAtom.3.html).
[X Atoms](https://tronche.com/gui/x/xlib/window-information/properties-and-atoms.html) are used to ask for or send specific data or properties through X11. 

You'll need three atoms, 

1) UTF8_STRING : Atom for a UTF-8 string.
2) CLIPBOARD : Atom for getting clipboard data.
3) XSEL_DATA : Atom to get selection data.

```c
const Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
const Atom CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
const Atom XSEL_DATA = XInternAtom(display, "XSEL_DATA", 0);
```

Now, to get the clipboard data you have to request that the clipboard section be converted to UTF8 via [`XConvertSelection`](https://tronche.com/gui/x/xlib/window-information/XConvertSelection.html).

Then use [`XSync`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html) to make sure the request is sent to the server. 

```c
XConvertSelection(display, CLIPBOARD, UTF8_STRING, XSEL_DATA, window, CurrentTime);
XSync(display, 0);
```

The selection will be converted and sent back to the client as a [`XSelectionNotify`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSelectionEvent.3.html) event. You can get the next event, which should be the `SelectionNotify` event via [`XNextEvent`](https://tronche.com/gui/x/xlib/event-handling/manipulating-event-queue/XNextEvent.html).

```c
XEvent event;
XNextEvent(display, &event);
```

Then make sure the event is a `SelectionNotify` event. Then use `.selection` to make sure the type is a `CLIPBOARD`. Finally, make sure `.property` is not 0 and can be retrived.

```c
if (event.type == SelectionNotify && event.xselection.selection == CLIPBOARD && event.xselection.property != 0) {
```

Now you can get the convered data via [`XGetWindowProperty`](https://www.x.org/releases/X11R7.5/doc/man/man3/XChangeProperty.3.html) using the selection property. 

```c
    int format;
    unsigned long N, size;
    char* data, * s = NULL;
    Atom target;

    XGetWindowProperty(event.xselection.display, event.xselection.requestor,
	    event.xselection.property, 0L, (~0L), 0, AnyPropertyType, &target,
	    &format, &size, &N, (unsigned char**) &data);
```

Makke sure the data is the right format by checking `target`

```c 
    if (target == UTF8_STRING || target == XA_STRING) {
```

The data is stored in `data`, once you're done with it free it via [`XFree`](https://software.cfht.hawaii.edu/man/x11/XFree(3x)).

Lastly, delete the propety via [`XDeleteProperty`](https://tronche.com/gui/x/xlib/window-information/XDeleteProperty.html).

```c
        XFree(data);
    }

    XDeleteProperty(event.xselection.display, event.xselection.requestor, event.xselection.property);
}
```

### winapi

[`OpenClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard)

Open the clipboard
```c
if (OpenClipboard(NULL) == 0)
	return 0;
```

Get the clipboard data as a Unicode string

[`GetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata)

[`CloseClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard)

```c
HANDLE hData = GetClipboardData(CF_UNICODETEXT);
if (hData == NULL) {
	CloseClipboard();
	return 0;
}
```

[`GlobalLock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globallock)

```
wchar_t* wstr = (wchar_t*) GlobalLock(hData);
```

Get the size of the UTF-8 version

[`setlocale`](https://en.cppreference.com/w/c/locale/setlocale)

[`wcstombs`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-wcstombs-l?view=msvc-170)

```C
setlocale(LC_ALL, "en_US.UTF-8");

size_t textLen = wcstombs(NULL, wstr, 0);
```

convert to the utf-8 version

```c
if (textLen) {
	char* text = (char*) malloc((textLen * sizeof(char)) + 1);

	wcstombs(text, wstr, (textLen) + 1);
	text[textLen] = '\0';

    free(text);
}
```

free leftover data

[`GlobalUnlock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalunlock)

```c
GlobalUnlock(hData);
CloseClipboard();
```

### cocoa

[`NSPasteboardTypeString`](https://developer.apple.com/documentation/appkit/nspasteboardtypestring)

[`stringWithUTF8String`](https://developer.apple.com/documentation/foundation/nsstring/1497379-stringwithutf8string)

```c
NSPasteboardType const NSPasteboardTypeString = "public.utf8-plain-text";
NSString* dataType = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), (char*)NSPasteboardTypeString);
```

[`generalPasteboard`](https://developer.apple.com/documentation/uikit/uipasteboard/1622106-generalpasteboard)

```c
NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 
```

[`stringForType`](https://developer.apple.com/documentation/appkit/nspasteboard/1533566-stringfortype)
[`UTF8String`](https://developer.apple.com/documentation/foundation/nsstring/1411189-utf8string)

```c
NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
const char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));
```

## Clipboard Copy

### X11

```c
const Atom SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);
const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);
const Atom CLIPBOARD_MANAGER = XInternAtom((Display*) display, "CLIPBOARD_MANAGER", False);
```

[`XSetSelectionOwner`](https://tronche.com/gui/x/xlib/window-information/XSetSelectionOwner.html)

```c
XSetSelectionOwner((Display*) display, CLIPBOARD, (Window) window, CurrentTime);

XConvertSelection((Display*) display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, (Window) window, CurrentTime);
```

[`SelectionRequest`](https://tronche.com/gui/x/xlib/events/client-communication/selection-request.html)

```c
if (event.type == SelectionRequest) {
    const XSelectionRequestEvent* request = &event.xselectionrequest;
```

```c
	XEvent reply = { SelectionNotify };
	reply.xselection.property = 0;
```

[`XChangeProperty`](https://tronche.com/gui/x/xlib/window-information/XChangeProperty.html)

```c
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
```

[`XFlush`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html)

```c
	if (request->target == MULTIPLE) {

		Atom* targets = NULL;

		Atom actualType = 0;
		int actualFormat = 0;
		unsigned long count = 0, bytesAfter = 0;

		XGetWindowProperty(display, request->requestor, request->property, 0, LONG_MAX, False, ATOM_PAIR, &actualType, &actualFormat, &count, &bytesAfter, (unsigned char **) &targets);
```

```c
        const Atom formats[] = { UTF8_STRING, XA_STRING };
		unsigned long i, j;
		for (i = 0; i < count; i += 2) {
			Bool found = False;

            for (j = 0; j < 2; j++) {
				if (targets[i] == formats[j]) {
                    found = True;
					break;
			    }
            }
```

```c
			if (j < 2 && found) {
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
```

[`XSendEvent`](https://tronche.com/gui/x/xlib/event-handling/XSendEvent.html)

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

[`GlobalAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalalloc)

```c
HANDLE object = GlobalAlloc(GMEM_MOVEABLE, (1 + textLen) * sizeof(WCHAR));

WCHAR*  buffer = (WCHAR*) GlobalLock(object);
if (!buffer) {
	GlobalFree(object);
	return 0;
}
```

[`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)

```c
MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, textLen);
```

```c
GlobalUnlock(object);
if (!OpenClipboard(NULL)) {
	GlobalFree(object);
	return 0;
}
```

[`EmptyClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-emptyclipboard)
[`SetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata)

```c
EmptyClipboard();
SetClipboardData(CF_UNICODETEXT, object);

CloseClipboard();
```

### cocoa

[`initWithObjects`](https://developer.apple.com/documentation/foundation/nsarray/1460068-initwithobjects)
[`NSAlloc`](https://developer.apple.com/documentation/objectivec/nsobject/1571958-alloc)

```c
NSPasteboardType ntypes[] = { dataType };

NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
                    (NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);
```

[`declareTypes`](https://developer.apple.com/documentation/appkit/nspasteboard/1533561-declaretypes)
[`NSRelease`](https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release)

```c
((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, NULL);
NSRelease(array);
```

[`setString`](https://developer.apple.com/documentation/appkit/nspasteboard/1528225-setstringhttps://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release)

```c
NSString* nsstr = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);

((bool (*)(id, SEL, id, NSPasteboardType))objc_msgSend) (pasteboard, sel_registerName("setString:forType:"), nsstr, dataType);	
```
