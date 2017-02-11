
#import <Cocoa/Cocoa.h>


void fix_mac_menu(void) {

	@autoreleasepool {
		if (NSApp) {
			NSMenu *menu = [NSApp mainMenu];

			for (NSMenuItem *a in [menu itemArray]) {
				for (NSMenuItem *b in [[a submenu] itemArray]) {
					unsigned m = [b keyEquivalentModifierMask];
					if (m & NSEventModifierFlagCommand)
						[b setKeyEquivalentModifierMask: m | NSEventModifierFlagOption];
				}
			}
		}

/*
			NSMenuItem *item = [menu itemAtIndex: 0];
			NSMenu *application = [item submenu];
			for (NSMenuItem *item in [application itemArray]) {

				if ([item action] == @selector(terminate:)) {
					[item setKeyEquivalentModifierMask: NSEventModifierFlagCommand | NSEventModifierFlagOption];
				}

			}
		} 
*/

	}

}
