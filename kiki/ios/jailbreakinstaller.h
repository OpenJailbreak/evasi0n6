//
//  jailbreakinstaller.h
//  utility
//
//  Created by Yiduo Wang on 10/31/12.
//  Copyright (c) 2012 Yiduo Wang. All rights reserved.
//

#ifndef utility_jailbreakinstaller_h
#define utility_jailbreakinstaller_h

void uicache();
void prepare_jailbreak_install();
void delete_dir(const char *path);
int run(char **argv, char **envp);
void disable_ota_updates();
void hide_weather_on_ipads();

#endif
