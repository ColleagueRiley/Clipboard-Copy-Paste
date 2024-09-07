int main() {
	/* input */
	NSPasteboardType const NSPasteboardTypeString = "public.utf8-plain-text";

	NSString* datatype = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), NSPasteboardTypeString1);

	NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 
	
	NSString* clip = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), dataType);
	
	char* str = ((const char* (*)(id, SEL)) objc_msgSend) (clip, sel_registerName("UTF8String"));

	printf("paste: %s\n", str);
	
	char text[] = "new string\0";

	NSPasteboard* pasteboard = objc_msgSend_id((id)objc_getClass("NSPasteboard"), sel_registerName("generalPasteboard")); 

	NSString* datatype = objc_msgSend_class_char(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), NSPasteboardTypeString1);

	NSPasteboardType ntypes[] = { dataType, NULL };

	NSArray* array = ((id (*)(id, SEL, void*, NSUInteger))objc_msgSend)
						(NSAlloc(objc_getClass("NSArray")), sel_registerName("initWithObjects:count:"), ntypes, 1);

	((NSInteger(*)(id, SEL, id, void*))objc_msgSend) (pasteboard, sel_registerName("declareTypes:owner:"), array, owner);
	NSRelease(array);
	
	NSString* nsstr = ((id(*)(id, SEL, const char*))objc_msgSend)(pasteboard, sel_registerName("stringForType:"), text);
	((bool (*)(id, SEL, id, NSPasteboardType))objc_msgSend) (pasteboard, sel_registerName("setString:forType:"), nsstr, dataType);	
}
