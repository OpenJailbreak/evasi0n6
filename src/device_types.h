#ifndef __DEVICE_TYPES_H
#define __DEVICE_TYPES_H

struct device_type_t {
	const char* productType;
	const char* displayName;
};

static struct device_type_t device_types[] = {
	// iPhones
	{"iPhone1,1", "iPhone"},
	{"iPhone1,2", "iPhone 3G"},	
	{"iPhone2,1", "iPhone 3GS"},	
	{"iPhone3,1", "iPhone 4 (GSM)"},
	{"iPhone3,2", "iPhone 4 (GSM) R2"},
	{"iPhone3,3", "iPhone 4 (CDMA)"},
	{"iPhone4,1", "iPhone 4S"},
	{"iPhone5,1", "iPhone 5 (GSM)"},
	{"iPhone5,2", "iPhone 5 (Global)"},
	// iPods
	{"iPod1,1", "iPod Touch"},
	{"iPod2,1", "iPod touch (2G)"},
	{"iPod3,1", "iPod Touch (3G)"},
	{"iPod4,1", "iPod Touch (4G)"},
	{"iPod5,1", "iPod Touch (5G)"},
	// iPads
	{"iPad1,1", "iPad"},
	{"iPad2,1", "iPad 2 (Wi-Fi)"},
	{"iPad2,2", "iPad 2 (GSM)"},
	{"iPad2,3", "iPad 2 (CDMA)"},
	{"iPad2,4", "iPad 2 (Wi-Fi) R2"},
	{"iPad3,1", "iPad 3 (Wi-Fi)"},
	{"iPad3,2", "iPad 3 (CDMA)"},
	{"iPad3,3", "iPad 3 (GSM)"},
	{"iPad3,4", "iPad 4 (WiFi)"},
	{"iPad3,5", "iPad 4 (GSM)"},
	{"iPad3,6", "iPad 4 (Global)"},
	// iPad minis
	{"iPad2,5", "iPad mini (Wi-Fi)"},
	{"iPad2,6", "iPad mini (GSM)"},
	{"iPad2,7", "iPad mini (Global)"},
	// AppleTV
	{"AppleTV2,1", "AppleTV (2G)"},
	{"AppleTV3,1", "AppleTV (3G)"},	
	{NULL, NULL}
};

#endif
