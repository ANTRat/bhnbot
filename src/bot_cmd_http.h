#include "config.h"

#ifndef _bot_cmd_http_h
#define _bot_cmd_http_h

void cmd_http_init();
void cmd_http_cleanup();
int cmd_http(int s, int https, char* line, char* token);

#ifdef HAVE_LIBSQLITE3
void cmd_http_lastlinks(int s);
void cmd_http_title_search(int s, char* search_term);
#endif


#endif
