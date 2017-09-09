#define IOSFC_BUILDING_IOSFC
#include <IOSurface/IOSurfaceAPI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <mach-o/getsect.h>
#include <pthread.h>

#include "framebuffer.h"

int IOMobileFramebufferGetMainDisplay(void**);
int IOMobileFramebufferGetLayerDefaultSurface(void*, int, IOSurfaceRef*);

static IOSurfaceRef surface = NULL;
static CGContextRef context = NULL;
static int text_displayed = 0;
static CGRect last_text_bounds;
static CGRect logo_bounds;
static CFDictionaryRef default_text_attributes = NULL;
static CFDictionaryRef blink_text_attributes = NULL;

static void setup_display_context()
{
    if(!surface)
    {
        void* framebuffer;
        IOMobileFramebufferGetMainDisplay(&framebuffer);

        IOMobileFramebufferGetLayerDefaultSurface(framebuffer, 0, &surface);
    }

    if(!context)
    {
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        context = CGBitmapContextCreate(
            IOSurfaceGetBaseAddress(surface),
            IOSurfaceGetWidth(surface),
            IOSurfaceGetHeight(surface),
            8,
            IOSurfaceGetBytesPerRow(surface),
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

        CGColorSpaceRelease(colorSpace);
    }
}

int get_scale()
{
    static int scale_set = 0;
    static float scale = 0.0f;
    if(scale_set)
        return scale;

    if(!context)
        setup_display_context();

    scale = IOSurfaceGetWidth(surface) > 480 ? 2 : 1;
    scale_set = 1;
    return scale;
}

CGContextRef get_display_context()
{
    if(!context)
        setup_display_context();
    return context;
}

IOSurfaceRef get_display_surface()
{
    if(!surface)
        setup_display_context();
    return surface;
}

void cleanup_display()
{
    CGContextRelease(context);
    IOSurfaceDecrementUseCount(surface);
    context = NULL;
    surface = NULL;
}

static CGLayerRef create_text_layer(CFStringRef text, CFDictionaryRef text_attributes)
{
    if(!context)
        setup_display_context();

    CFAttributedStringRef attrString = CFAttributedStringCreate(kCFAllocatorDefault, text, text_attributes);
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    CGRect bounds = CTLineGetImageBounds(line, context);
    CGLayerRef layer = CGLayerCreateWithContext(context, bounds.size, NULL);
    CGContextRef layer_context = CGLayerGetContext(layer);

    float leading;
    float descent;
    CTLineGetTypographicBounds(line, NULL, &descent, &leading);
    CGContextSetTextPosition(layer_context, leading, descent);
    CTLineDraw(line, layer_context);

    CFRelease(line);
    CFRelease(attrString);

    return layer;
}

static CGSize text_size(CFStringRef text, CFDictionaryRef text_attributes)
{
    CFAttributedStringRef attrString = CFAttributedStringCreate(kCFAllocatorDefault, text, text_attributes);
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    CGRect bounds = CTLineGetImageBounds(line, context);
    CFRelease(line);
    CFRelease(attrString);
    return bounds.size;
}

static CFDictionaryRef create_text_attributes(CFStringRef font_name, float size, float red, float green, float blue, float alpha)
{
    CTFontRef font = CTFontCreateWithName(font_name, size, NULL);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[] = {red, green, blue, alpha};
    CGColorRef color = CGColorCreate(colorSpace, components);
    CFStringRef keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    CFTypeRef values[] = {font, color};

    CFDictionaryRef text_attributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys,
        (const void**)&values, sizeof(keys) / sizeof(keys[0]),
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CGColorRelease(color);
    CGColorSpaceRelease(colorSpace);
    CFRelease(font);

    return text_attributes;
}

static void clear_rect(CGRect rect)
{
    if(!context)
        setup_display_context();

    CGLayerRef layer = CGLayerCreateWithContext(context, rect.size, NULL);
    CGContextRef layerContext = CGLayerGetContext(layer);
    CGContextSetRGBFillColor(layerContext, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(layerContext, CGRectMake(0.0f, 0.0f, rect.size.width, rect.size.height));
    CGContextDrawLayerAtPoint(context, rect.origin, layer);
    CGLayerRelease(layer);
}

static void display_logo()
{
    if(!context)
        setup_display_context();

    unsigned long logo_size;
    void* logo_data = (void*) getsectdata("__DATA", "logo", &logo_size);

    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, logo_data, logo_size, NULL);
    if(!provider)
        return;

    CGImageRef image = CGImageCreateWithPNGDataProvider(provider, NULL, true, kCGRenderingIntentDefault);
    if(!image)
        return;

    CGSize size = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    if(get_scale() == 1)
    {
        size.width /= 2;
        size.height /= 2;
    }

    CGLayerRef layer = CGLayerCreateWithContext(context, size, NULL);
    CGContextDrawImage(CGLayerGetContext(layer), CGRectMake(0.0f, 0.0f, size.width, size.height), image);

    float x = (IOSurfaceGetWidth(surface) - size.width) / 2.0f;
    float y = (IOSurfaceGetHeight(surface) - size.height) / 2.0f + (50.0f * get_scale());

    logo_bounds = CGRectMake(x, y, size.width, size.height);
    CGContextDrawLayerAtPoint(context, logo_bounds.origin, layer);
    CGLayerRelease(layer);
}

static pthread_mutex_t FBMutex = PTHREAD_MUTEX_INITIALIZER;

static void _fb_update_status(float progress, CFStringRef text, CFDictionaryRef text_attributes)
{
    pthread_mutex_lock(&FBMutex);
    if(text_displayed)
        clear_rect(last_text_bounds);

    CGSize size = text_size(text, text_attributes);
    float x = (IOSurfaceGetWidth(surface) - size.width) / 2.0f;
    float y = logo_bounds.origin.y + (10.0f * get_scale()) - size.height;

    CGLayerRef layer = create_text_layer(text, text_attributes);
    CGSize layer_size = CGLayerGetSize(layer);
    last_text_bounds = CGRectMake(x, y, layer_size.width, layer_size.height);
    CGContextDrawLayerAtPoint(context, last_text_bounds.origin, layer);
    CGLayerRelease(layer);

    text_displayed = 1;
    pthread_mutex_unlock(&FBMutex);
}

static pthread_t BlinkThread = NULL;
static pthread_mutex_t BlinkMutex = PTHREAD_MUTEX_INITIALIZER;
static int BlinkThreadActive = 0;
static int BlinkState = 0;
static CFStringRef BlinkText = NULL;
static CFDictionaryRef BlinkA = NULL;
static CFDictionaryRef BlinkB = NULL;
static float BlinkProgress = 0.0f;

static void blink_function(void* arg)
{
    while(1)
    {
        sleep(1);
        pthread_mutex_lock(&BlinkMutex);
        if(!BlinkThreadActive)
        {
            pthread_mutex_unlock(&BlinkMutex);
            return;
        }

        if(BlinkA && BlinkB && BlinkText)
        {
            float display_progress = BlinkProgress;
            CFStringRef display = BlinkText;
            CFDictionaryRef text_attributes;
            if(BlinkState == 0)
            {
                BlinkState = 1;
                text_attributes = BlinkB;
            } else
            {
                BlinkState = 0;
                text_attributes = BlinkA;
            }
            CFRetain(display);
            CFRetain(text_attributes);
            pthread_mutex_unlock(&BlinkMutex);
            _fb_update_status(display_progress, display, text_attributes);
            CFRelease(display);
            CFRelease(text_attributes);
        } else
        {
            pthread_mutex_unlock(&BlinkMutex);
        }
    }
}

void fb_setup()
{
    pthread_mutex_lock(&FBMutex);
    get_display_surface();

    clear_rect(CGRectMake(0, 0, IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface)));
    default_text_attributes = create_text_attributes(CFSTR("Helvetica"), 20.0f * get_scale(), 0.0f, 0.0f, 0.0f, 1.0f);
    blink_text_attributes = create_text_attributes(CFSTR("Helvetica"), 20.0f * get_scale(), 0.5f, 0.5f, 0.5f, 1.0f);

    display_logo();

    pthread_mutex_unlock(&FBMutex);

    if(!BlinkThread)
    {
        pthread_mutex_lock(&BlinkMutex);
        BlinkThreadActive = 1;
        BlinkState = 0;
        pthread_mutex_unlock(&BlinkMutex);
        pthread_create(&BlinkThread, NULL, (void*)blink_function, NULL);
    }
}

void fb_cleanup()
{
    pthread_mutex_lock(&BlinkMutex);
    BlinkThreadActive = 0;
    pthread_mutex_unlock(&BlinkMutex);

    pthread_join(BlinkThread, NULL);
    BlinkThread = NULL;

    pthread_mutex_lock(&FBMutex);
    cleanup_display();
    pthread_mutex_unlock(&FBMutex);
}

void fb_update_status_blinking(float progress, CFStringRef text)
{
    pthread_mutex_lock(&BlinkMutex);
    if(BlinkA)
        CFRelease(BlinkA);

    if(BlinkB)
        CFRelease(BlinkB);

    BlinkA = default_text_attributes;
    BlinkB = blink_text_attributes;
    BlinkText = text;

    CFRetain(BlinkA);
    CFRetain(BlinkB);
    CFRetain(BlinkText);

    BlinkProgress = progress;
    BlinkState = 0;
    pthread_mutex_unlock(&BlinkMutex);

    _fb_update_status(progress, text, default_text_attributes);
}

void fb_update_status(float progress, CFStringRef text)
{
    pthread_mutex_lock(&BlinkMutex);
    if(BlinkA)
        CFRelease(BlinkA);

    if(BlinkB)
        CFRelease(BlinkB);

    if(BlinkText)
        CFRelease(BlinkText);

    BlinkA = NULL;
    BlinkB = NULL;
    pthread_mutex_unlock(&BlinkMutex);

    _fb_update_status(progress, text, default_text_attributes);
}
