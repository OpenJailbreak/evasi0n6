#ifndef __BUILD_SUPPORTED_H
#define __BUILD_SUPPORTED_H

struct build_supported_t {
	const char* buildVersion;
        int supported;
};

static struct build_supported_t build_supported[] = {
        // 6.0
        {"10A403", 1},
        {"10A405", 1},
        {"10A406", 1},
        {"10A407", 1},

        // 6.0.1
        {"10A523", 1},
        {"10A525", 1},
        {"10A8426", 1},

        // 6.0.2
        {"10A550", 1},
        {"10A551", 1},
        {"10A8500", 1},

        // 6.1
        {"10B141", 1},
        {"10B142", 1},
        {"10B143", 1},
        {"10B144", 1},

        // 6.1.1 4S
	{"10B145", 1},

        // 6.1.2
        {"10B146", 1},
        {"10B147", 1},

	// 6.1.1 beta
	{"10B311", 1},

        {NULL, 0}
};

#endif
