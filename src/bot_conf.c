#include "config.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <jansson.h>
#include "bot_conf.h"



botconf_t* botconf_load_file(const char *path) {
    json_error_t error;

    botconf_t* conf = malloc(sizeof(botconf_t));

    conf->root = json_load_file(path, 0, &error);
    if( !conf->root ) {
        fprintf(stderr, "botconf_load_file: Unable to parse JSON in '%s'\n", path);
        return NULL;
    }

    json_t* server = json_object_get(conf->root, "server");
    json_t* port = json_object_get(conf->root, "port");
    json_t* nick = json_object_get(conf->root, "nick");
#ifdef ENABLE_SHORTURLS
    json_t* google_api_key = json_object_get(conf->root, "google_api_key");
#endif
    json_t* debug = json_object_get(conf->root, "debug");
    json_t* on_connect_send = json_object_get(conf->root, "on_connect_send");

    if(    !server
        || !port
        || !nick
#ifdef ENABLE_SHORTURLS
        || !google_api_key
#endif
        || !on_connect_send
        ) {
        fprintf(stderr, "botconf_load_file: Syntax error in '%s'\n", path);
        return NULL;
    }

    if(    !json_is_string(server)
        || !json_is_string(port)
        || !json_is_string(nick)
#ifdef ENABLE_SHORTURLS
        || !json_is_string(google_api_key)
#endif
        || !json_is_array(on_connect_send) ) {
        fprintf(stderr, "botconf_load_file: Syntax error in '%s': invalid type\n", path);
        return NULL;
    }

    if( !debug || !json_is_integer(debug) ) {
        json_decref(debug);
        conf->debug = 0;
    } else {
        conf->debug = json_integer_value(debug);
    }

    conf->server = json_string_value(server);
    conf->port = json_string_value(port);
    conf->nick = json_string_value(nick);
#ifdef ENABLE_SHORTURLS
    conf->google_api_key = json_string_value(google_api_key);
#endif
    conf->on_connect_send = on_connect_send;

    return conf;
}

void botconf_free_conf(botconf_t* conf) {
    json_decref(conf->root);
    free(conf);
}

void botconf_on_connect_send(const botconf_t* conf, int s) {
    json_t* sends = conf->on_connect_send;
    size_t size = json_array_size(sends);

    size_t i;
    for( i = 0; i < size; i++ ) {
        json_t* send_item = json_array_get(sends, i);
        if( json_is_string(send_item) ) {
            char* send_cmd = malloc(sizeof(char) * 4096);
            sprintf(send_cmd, "%s\r\n", json_string_value(send_item));

            if( s ) {
                send(s, send_cmd, strlen(send_cmd), 0);
            } else if ( conf->debug ) {
                printf("  on_connect_send[%d,%d]: %s", s, (int)i, send_cmd);
            }

            free(send_cmd);
        }
    }

}

void botconf_print(botconf_t* conf) {
    if( !conf->debug)
        return;
    printf("From config file:\n  server: %s\n  port: %s\n  nick: %s\n  google_api_key: %s\n  debug: %d\n", 
        conf->server
      , conf->port
      , conf->nick
#ifdef ENABLE_SHORTURLS
      , conf->google_api_key
#endif
#ifndef ENABLE_SHORTURLS
      , ""
#endif
      , conf->debug
    );
    botconf_on_connect_send(conf, 0);
}

