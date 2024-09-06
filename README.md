# RGFW Under the Hood: Clipboard I/O
# Introduction
# Overview

```c
char* RGFW_readClipboard(size_t* size) {
		static Atom UTF8 = 0;
		if (UTF8 == 0)
			UTF8 = XInternAtom(RGFW_root->src.display, "UTF8_STRING", True);

		XEvent event;
		int format;
		unsigned long N, sizeN;
		char* data, * s = NULL;
		Atom target;
		Atom CLIPBOARD = 0, XSEL_DATA = 0;

		if (CLIPBOARD == 0) {
			CLIPBOARD = XInternAtom(RGFW_root->src.display, "CLIPBOARD", 0);
			XSEL_DATA = XInternAtom(RGFW_root->src.display, "XSEL_DATA", 0);
		}

		XConvertSelection(RGFW_root->src.display, CLIPBOARD, UTF8, XSEL_DATA, RGFW_root->src.window, CurrentTime);
		XSync(RGFW_root->src.display, 0);
		XNextEvent(RGFW_root->src.display, &event);

		if (event.type != SelectionNotify || event.xselection.selection != CLIPBOARD || event.xselection.property == 0)
			return NULL;

		XGetWindowProperty(event.xselection.display, event.xselection.requestor,
			event.xselection.property, 0L, (~0L), 0, AnyPropertyType, &target,
			&format, &sizeN, &N, (unsigned char**) &data);

		if (target == UTF8 || target == XA_STRING) {
			s = (char*)RGFW_MALLOC(sizeof(char) * sizeN);
			strncpy(s, data, sizeN);
			s[sizeN] = '\0';
			XFree(data);
		}

		XDeleteProperty(event.xselection.display, event.xselection.requestor, event.xselection.property);

		if (s != NULL && size != NULL)
			*size = sizeN;

		return s;
		}

	/*
		almost all of this function is sourced from GLFW
	*/
	void RGFW_writeClipboard(const char* text, u32 textLen) {
		static Atom CLIPBOARD = 0,
			UTF8_STRING = 0,
			SAVE_TARGETS = 0,
			TARGETS = 0,
			MULTIPLE = 0,
			ATOM_PAIR = 0,
			CLIPBOARD_MANAGER = 0;

		if (CLIPBOARD == 0) {
			CLIPBOARD = XInternAtom((Display*) RGFW_root->src.display, "CLIPBOARD", False);
			UTF8_STRING = XInternAtom((Display*) RGFW_root->src.display, "UTF8_STRING", False);
			SAVE_TARGETS = XInternAtom((Display*) RGFW_root->src.display, "SAVE_TARGETS", False);
			TARGETS = XInternAtom((Display*) RGFW_root->src.display, "TARGETS", False);
			MULTIPLE = XInternAtom((Display*) RGFW_root->src.display, "MULTIPLE", False);
			ATOM_PAIR = XInternAtom((Display*) RGFW_root->src.display, "ATOM_PAIR", False);
			CLIPBOARD_MANAGER = XInternAtom((Display*) RGFW_root->src.display, "CLIPBOARD_MANAGER", False);
		}
		
		XSetSelectionOwner((Display*) RGFW_root->src.display, CLIPBOARD, (Window) RGFW_root->src.window, CurrentTime);

		XConvertSelection((Display*) RGFW_root->src.display, CLIPBOARD_MANAGER, SAVE_TARGETS, None, (Window) RGFW_root->src.window, CurrentTime);
		for (;;) {
			XEvent event;

			XNextEvent((Display*) RGFW_root->src.display, &event);
			if (event.type != SelectionRequest) {
				break;
			}

			const XSelectionRequestEvent* request = &event.xselectionrequest;

			XEvent reply = { SelectionNotify };
			reply.xselection.property = 0;

			const Atom formats[] = { UTF8_STRING, XA_STRING };
			const i32 formatCount = sizeof(formats) / sizeof(formats[0]);
		
			if (request->target == TARGETS) {
				const Atom targets[] = { TARGETS,
										MULTIPLE,
										UTF8_STRING,
										XA_STRING };

				XChangeProperty((Display*) RGFW_root->src.display,
					request->requestor,
					request->property,
					4,
					32,
					PropModeReplace,
					(u8*) targets,
					sizeof(targets) / sizeof(targets[0]));

				reply.xselection.property = request->property;
			}

			if (request->target == MULTIPLE) {
				Atom* targets = NULL;

				Atom actualType = 0;
				int actualFormat = 0;
				unsigned long count = 0, bytesAfter = 0;

				XGetWindowProperty((Display*) RGFW_root->src.display, request->requestor, request->property, 0, LONG_MAX, False, ATOM_PAIR, &actualType, &actualFormat, &count, &bytesAfter, (u8**) &targets);

				unsigned long i;
				for (i = 0; i < (u32)count; i += 2) {
					i32 j;

					for (j = 0; j < formatCount; j++) {
						if (targets[i] == formats[j])
							break;
					}

					if (j < formatCount)
					{
						XChangeProperty((Display*) RGFW_root->src.display,
							request->requestor,
							targets[i + 1],
							targets[i],
							8,
							PropModeReplace,
							(u8*) text,
							textLen);
						XFlush(RGFW_root->src.display);
					} else {
						targets[i + 1] = None;
					}
				}

				XChangeProperty((Display*) RGFW_root->src.display,
					request->requestor,
					request->property,
					ATOM_PAIR,
					32,
					PropModeReplace,
					(u8*) targets,
					count);

				XFlush(RGFW_root->src.display);
				XFree(targets);

				reply.xselection.property = request->property;
			}

			reply.xselection.display = request->display;
			reply.xselection.requestor = request->requestor;
			reply.xselection.selection = request->selection;
			reply.xselection.target = request->target;
			reply.xselection.time = request->time;

			XSendEvent((Display*) RGFW_root->src.display, request->requestor, False, 0, &reply);
			XFlush(RGFW_root->src.display);
		}
	}

	char* RGFW_readClipboard(size_t* size) {
		/* Open the clipboard */
		if (OpenClipboard(NULL) == 0)
			return (char*) "";

		/* Get the clipboard data as a Unicode string */
		HANDLE hData = GetClipboardData(CF_UNICODETEXT);
		if (hData == NULL) {
			CloseClipboard();
			return (char*) "";
		}

		wchar_t* wstr = (wchar_t*) GlobalLock(hData);

		char* text;

		{
			setlocale(LC_ALL, "en_US.UTF-8");

			size_t textLen = wcstombs(NULL, wstr, 0);
			if (textLen == 0)
				return (char*) "";

			text = (char*) RGFW_MALLOC((textLen * sizeof(char)) + 1);

			wcstombs(text, wstr, (textLen) +1);

			if (size != NULL)
				*size = textLen + 1;

			text[textLen] = '\0';
		}

		/* Release the clipboard data */
		GlobalUnlock(hData);
		CloseClipboard();

		return text;
	}

	void RGFW_writeClipboard(const char* text, u32 textLen) {
		HANDLE object;
		WCHAR* buffer;

		object = GlobalAlloc(GMEM_MOVEABLE, (1 + textLen) * sizeof(WCHAR));
		if (!object)
			return;

		buffer = (WCHAR*) GlobalLock(object);
		if (!buffer) {
			GlobalFree(object);
			return;
		}

		MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, textLen);
		GlobalUnlock(object);

		if (!OpenClipboard(RGFW_root->src.window)) {
			GlobalFree(object);
			return;
		}

		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, object);
		CloseClipboard();
	}


	char* RGFW_readClipboard(size_t* size) {
		char* clip = (char*)NSPasteboard_stringForType(NSPasteboard_generalPasteboard(), NSPasteboardTypeString);
		
		size_t clip_len = 1;

		if (clip != NULL) {
			clip_len = strlen(clip) + 1; 
		}

		char* str = (char*)RGFW_MALLOC(sizeof(char) * clip_len);
		
		if (clip != NULL) {
			strncpy(str, clip, clip_len);
		}

		str[clip_len] = '\0';
		
		if (size != NULL)
			*size = clip_len;
		return str;
	}

	void RGFW_writeClipboard(const char* text, u32 textLen) {
		RGFW_UNUSED(textLen);

		NSPasteboardType array[] = { NSPasteboardTypeString, NULL };
		NSPasteBoard_declareTypes(NSPasteboard_generalPasteboard(), array, 1, NULL);

		NSPasteBoard_setString(NSPasteboard_generalPasteboard(), text, NSPasteboardTypeString);
	}
```
