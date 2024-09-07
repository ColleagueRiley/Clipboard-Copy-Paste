# RGFW Under the Hood: Clipboard I/O
# Introduction
# Overview

## input 

### X11
```c
const Atom UTF8 = XInternAtom(display, "UTF8_STRING", True);
const Atom CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
const Atom XSEL_DATA = XInternAtom(display, "XSEL_DATA", 0);
```

```c
XConvertSelection(display, CLIPBOARD, UTF8, XSEL_DATA, window, CurrentTime);
XSync(display, 0);
```

```c
XEvent event;
XNextEvent(display, &event);

if (event.type == SelectionNotify && event.xselection.selection == CLIPBOARD && event.xselection.property != 0) {
```

```c
    int format;
    unsigned long N, size;
    char* data, * s = NULL;
    Atom target;

    XGetWindowProperty(event.xselection.display, event.xselection.requestor,
	    event.xselection.property, 0L, (~0L), 0, AnyPropertyType, &target,
	    &format, &size, &N, (unsigned char**) &data);
```

```c 
    if (target == UTF8 || target == XA_STRING) {
```

```c
        XFree(data);
    }

    XDeleteProperty(event.xselection.display, event.xselection.requestor, event.xselection.property);
}
```

### winapi

Open the clipboard
```c
if (OpenClipboard(NULL) == 0)
	return (char*) "";
```

Get the clipboard data as a Unicode string
```c
HANDLE hData = GetClipboardData(CF_UNICODETEXT);
if (hData == NULL) {
	CloseClipboard();
	return (char*) "";
}
```
```
wchar_t* wstr = (wchar_t*) GlobalLock(hData);
```

Get the size of the UTF-8 version

```C
setlocale(LC_ALL, "en_US.UTF-8");

size_t textLen = wcstombs(NULL, wstr, 0);
```

convert to the utf-8 version

```c
if (textLen) {
	char* text = (char*) RGFW_MALLOC((textLen * sizeof(char)) + 1);

	wcstombs(text, wstr, (textLen) + 1);
	text[textLen] = '\0';

    free(text);
}
```

free leftover data

```c
GlobalUnlock(hData);
CloseClipboard();
```

### cocoa

```c
NSPasteboardType const NSPasteboardTypeString = "public.utf8-plain-text";

NSString* datatype = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), NSPasteboardTypeString1);
```

```c
NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 

NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));
```

## output

### X11

```c
const Atom SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);
const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);
const Atom CLIPBOARD_MANAGER = XInternAtom((Display*) display, "CLIPBOARD_MANAGER", False);
```

```c
XSetSelectionOwner((Display*) display, CLIPBOARD, (Window) window, CurrentTime);

XConvertSelection((Display*) display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, (Window) window, CurrentTime);
```

```c
if (event.type == SelectionRequest) {
	const XSelectionRequestEvent* request = &event.xselectionrequest;

	XEvent reply = { SelectionNotify };
	reply.xselection.property = 0;
```

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

```c
HANDLE object = GlobalAlloc(GMEM_MOVEABLE, (1 + textLen) * sizeof(WCHAR));

WCHA*  buffer = (WCHAR*) GlobalLock(object);
if (!buffer) {
	GlobalFree(object);
	return;
}
```

```c
MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, textLen);
```

```c
GlobalUnlock(object);
if (!OpenClipboard(window)) {
	GlobalFree(object);
	return;
}

EmptyClipboard();
SetClipboardData(CF_UNICODETEXT, object);
CloseClipboard();
```

### cocoa

```c
NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 

NSString* datatype = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), NSPasteboardTypeString1);
```

```c
NSPasteboardType ntypes[] = { dataType, NULL };

NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
					(NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);
```

```c
((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, owner);
NSRelease(array);
```

```c
NSPasteBoard_setString(pasteboard, text, NSPasteboardTypeString);
```
