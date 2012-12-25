#include "config.h"

#ifndef _bot_cmd_imgur_h
#define _bot_cmd_imgur_h

void cmd_imgur_init();
void cmd_imgur_cleanup();
char* cmd_imgur_get_imageid(char* url);
int cmd_imgur(int s, char* token);

#endif
