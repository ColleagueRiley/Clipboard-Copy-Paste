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
