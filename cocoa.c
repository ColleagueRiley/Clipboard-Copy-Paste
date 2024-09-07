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
