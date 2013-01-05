#ifndef _bot_conf_h
#define _bot_conf_h

#include "config.h"
#include <jansson.h>

typedef struct {
    const char* server;
    const char* port;
    const char* nick;
    const char* channel;
#ifdef ENABLE_SHORTURLS
    const char* google_api_key;
#endif
    int debug;
    json_t* root;
    json_t* on_connect_send;
} botconf_t;

botconf_t* botconf_load_file(const char *path);
void botconf_free_conf(botconf_t* conf);
void botconf_on_connect_send(const botconf_t* conf, int s);
void botconf_print(botconf_t* conf);


#endif
