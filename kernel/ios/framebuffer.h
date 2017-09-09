#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#define IOSFC_BUILDING_IOSFC
#include <IOSurface/IOSurfaceAPI.h>
#include <CoreGraphics/CoreGraphics.h>

CGContextRef get_display_context();
IOSurfaceRef get_display_surface();
void fb_setup();
void fb_update_status(float progress, CFStringRef text);
void fb_update_status_blinking(float progress, CFStringRef text);
void fb_cleanup();
int get_scale();

#endif
