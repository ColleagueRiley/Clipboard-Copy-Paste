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

First, open the clipboard [`OpenClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard). 

```c
if (OpenClipboard(NULL) == 0)
	return 0;
```

Get the clipboard data as a Unicode string via [`GetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata)

If the data is NULL, you should close the clipboard via [`CloseClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard)

```c
HANDLE hData = GetClipboardData(CF_UNICODETEXT);
if (hData == NULL) {
	CloseClipboard();
	return 0;
}
```

Next you need to convert the unicode data back to utf-8.  

Start by locking memory for the utf-8 data via [`GlobalLock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globallock).

```
wchar_t* wstr = (wchar_t*) GlobalLock(hData);
```

Use [`setlocale`](https://en.cppreference.com/w/c/locale/setlocale) to ensure the data format is utf-8.

Get the size of the UTF-8 version via [`wcstombs`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-wcstombs-l?view=msvc-170).

```C
setlocale(LC_ALL, "en_US.UTF-8");

size_t textLen = wcstombs(NULL, wstr, 0);
```

If the size is valid, conver tthe data via [`wcstombs`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-wcstombs-l?view=msvc-170).

```c
if (textLen) {
	char* text = (char*) malloc((textLen * sizeof(char)) + 1);

	wcstombs(text, wstr, (textLen) + 1);
	text[textLen] = '\0';

    free(text);
}
```

Then free leftover global data using   [`GlobalUnlock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalunlock) and close the clipboard with [`CloseClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard).

```c
GlobalUnlock(hData);
CloseClipboard();
```

### cocoa
Cocoa uses [`NSPasteboardTypeString`](https://developer.apple.com/documentation/appkit/nspasteboardtypestring) for asking for a string data. You'll have to define this yourself if you're using Objective-C.

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

Then you can get the pasteboard's string data using `dataType` via [`stringForType`](https://developer.apple.com/documentation/appkit/nspasteboard/1533566-stringfortype).

However it will give you an NSString, which can be convereted via [`UTF8String`](https://developer.apple.com/documentation/foundation/nsstring/1411189-utf8string).

```c
NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
const char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));
```

## Clipboard Copy

### X11

To copy to the clipboard you'll need a few more Atoms. 

1) SAVE_TARGETS: is used to request a section to convert to (for copying). 
2) TARGETS: To handle one requested target
3) MULTIPLE: When there are multiple request targets
4) ATOM_PAIR: Is used to get the supported data types.
5) CLIPBOARD_MANAGER: is used to access data from the clipboard manager.


```c
const Atom SAVE_TARGETS = XInternAtom((Display*) display, "SAVE_TARGETS", False);
const Atom TARGETS = XInternAtom((Display*) display, "TARGETS", False);
const Atom MULTIPLE = XInternAtom((Display*) display, "MULTIPLE", False);
const Atom ATOM_PAIR = XInternAtom((Display*) display, "ATOM_PAIR", False);
const Atom CLIPBOARD_MANAGER = XInternAtom((Display*) display, "CLIPBOARD_MANAGER", False);
```

Now, we can request a clipboard section. First set the owner of the section to be client window via [`XSetSelectionOwner`](https://tronche.com/gui/x/xlib/window-information/XSetSelectionOwner.html). Next request a converted section via [`XConvertSelection`](https://tronche.com/gui/x/xlib/window-information/XConvertSelection.html).
 

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

At the end of the SelectionNotify event, a response will be sent back to the requester. The structure should be created here and it will be modifed depending on the request data. 

```c
	XEvent reply = { SelectionNotify };
	reply.xselection.property = 0;
```

The first target we will handle is `TARGETS`, this is when the requestor wants to know what targets are supported.

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

I'll also change the selection property so that way the requestor knows what property we changed.

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

Next I will handle `MULTIPLE` targets.

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

Now we'll loop trhough the supported targets. If the supported targets matches one of our supported targets, we can pass the data via `XChangeProperty`.

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

Now, you can pass the final array of supported targets back to the requestor via `XChangeProperty`. This tells the requestor which targets to expect for the original list it sent. 

The message will be sent out asap when [`XFlush`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html) is called. 

Then you can free your copy of the target array with `XFree`.

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

Now for the final step of the event, sending selection even back to the requestor via [`XSendEvent`](https://tronche.com/gui/x/xlib/event-handling/XSendEvent.html).

Then send out the event via [`XFlush`](https://www.x.org/releases/X11R7.5/doc/man/man3/XSync.3.html).

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
Firt allocate global memory for your data and your utf-8 buffer via [`GlobalAlloc`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalalloc)

```c
HANDLE object = GlobalAlloc(GMEM_MOVEABLE, (1 + textLen) * sizeof(WCHAR));
WCHAR*  buffer = (WCHAR*) GlobalLock(object);
```

Next you can use [`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar) to convert your string to a wide string.

```c
MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, textLen);
```

Now unlock the global object and open the clipboard

```c
GlobalUnlock(object);
OpenClipboard(NULL);
```

Now to update the clipboard data, you start by clearing what's currently on the clipboard via  [`EmptyClipboard`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-emptyclipboard) then you can use  [`SetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata) to set the data to the utf8 object.

Finally, close the clipboard via `CloseClipboard`.

```c
EmptyClipboard();
SetClipboardData(CF_UNICODETEXT, object);

CloseClipboard();
```

### cocoa
First create an array of the type of data you want to put on the clipboard, then convert it to a NSArray via [`initWithObjects`](https://developer.apple.com/documentation/foundation/nsarray/1460068-initwithobjects).

```c
NSPasteboardType ntypes[] = { dataType };

NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
                    (NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);
```

Then use  [`declareTypes`](https://developer.apple.com/documentation/appkit/nspasteboard/1533561-declaretypes) to declare the array as the supported data types.

Now you can also free the NSArray via [`NSRelease`](https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release).

```c
((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, NULL);
NSRelease(array);
```


Now convert the string to want to copy to an NSString via `stringWithUTF8String` and set the clipboard string to be that NSString using [`setString`](https://developer.apple.com/documentation/appkit/nspasteboard/1528225-setstringhttps://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release).

```c
NSString* nsstr = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);

((bool (*)(id, SEL, id, NSPasteboardType))objc_msgSend) (pasteboard, sel_registerName("setString:forType:"), nsstr, dataType);	
```
