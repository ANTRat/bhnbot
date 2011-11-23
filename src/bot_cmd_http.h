#include "config.h"

#ifndef _botcmd_youtube_h
#define _botcmd_youtube_h

void cmd_http_init();
void cmd_http_cleanup();
int cmd_http(int s, int https, char* line, char* token);

#ifdef HAVE_LIBSQLITE3
void cmd_http_lastlinks(int s);
#endif


#endif
