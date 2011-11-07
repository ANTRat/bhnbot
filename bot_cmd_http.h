#ifndef _botcmd_youtube_h
#define _botcmd_youtube_h

void cmd_http_init();
void cmd_http_cleanup();
int cmd_http(int s, int https, char* line, char* token);

#endif
