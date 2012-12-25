#include "config.h"

#ifdef HAVE_LIBCURL
#ifdef HAVE_LIBPCRE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <jansson.h>
#include <pcre.h>
#include "bot_conf.h"
#include "bot_cmd_echo.h"

extern botconf_t* conf;

char* title = "";

size_t cmd_imgur_writecallback(char *ptr, size_t size, size_t nmemb, void *userdata __attribute__((unused)) ) {
    size_t i = 0;
    for(i = 0; i < (size * nmemb); i++) {
        title[i] = ptr[i];
        if( isprint(ptr[i]) ) {
            printf("%c", ptr[i]);
        } else {
            title[i] = ' ';
        }
    }
    printf("\n");
    return size * nmemb;
}

size_t cmd_imgur_writecallback_title(char *ptr, size_t size, size_t nmemb, void *userdata __attribute__((unused)) ) {
    size_t title_start_loc = 0;
    size_t title_end_loc = 0;
    size_t i = 0;
    for(i = 0; i < (size * nmemb); i++) {
        if (strncmp("<title>", ptr + i, strlen("<title>")) == 0) {
            title_start_loc = i + strlen("<title>");
        } else
        if (strncmp("</title>", ptr + i, strlen("</title>")) == 0) {
            title_end_loc = i;
            break;
        } else
        if (strncmp("<TITLE>", ptr + i, strlen("<TITLE>")) == 0) {
            title_start_loc = i + strlen("<TITLE>");
        } else
        if (strncmp("</TITLE>", ptr + i, strlen("</TITLE>")) == 0) {
            title_end_loc = i;
            break;
        }
    }

    for(i = title_start_loc; i < title_end_loc; i++){
        if( isgraph(ptr[i]) ) {
            break;
        }
    }

    title_start_loc = i;

    for(i = title_end_loc - 1; i > title_start_loc; i--) {
        if( isgraph(ptr[i]) ) {
            break;
        }
    }

    title_end_loc = i + 1;
    
    // if we found the title then lets copy it!
    // make sure that the locs are valid posisions
    // also, a 4k title is insane
    if(title_start_loc != 0 && title_end_loc != 0 && (title_end_loc - title_start_loc < 4096) ) {
        // we should have alredy done this, but ~welp~
        memset(title, 0,  4096);
        memcpy(title, ptr + title_start_loc, title_end_loc - title_start_loc);
        char *p;
        for (p = title; *p != '\0'; p++) {
            if( !isprint(*p) ) {
                *p = ' ';
            }
        }
    }
    return size * nmemb;
}

char* imgur_re_image = "https?://(i[.])?imgur.com/(\\w{4,})";

char* cmd_imgur_get_imageid(char* url) {
    char* id = malloc(256 * sizeof(char));
    memset(id, 0,  256);
    const char* error;
    int erroffset;
    pcre* re;
    int OVECCOUNT = 100*3;
    int ovector[OVECCOUNT];

    re = pcre_compile(imgur_re_image, 0, &error, &erroffset, 0);
    if( !re ) {
        printf("pcre_compile failed (offset: %d), %s\n", erroffset, error);
        return NULL;
    }

    int rc = pcre_exec(re, 0, url, strlen(url), 0, 0, ovector, OVECCOUNT);
    if( rc < 0 ) {
        switch( rc ) {
            case PCRE_ERROR_NOMATCH:
                printf("String didn't match\n");
                break;
            default:
                printf("Error while matching: %d\n", rc);
                break;
        }
        free(re);
        return NULL;
    }
    free(re);

    int i;
    int olen;
    printf("rc: %d\n", rc);
    for (i = 0; i < rc; i++) {
        switch( i ) {
            case 2:
                olen = ovector[2*i+1] - ovector[2*i];
                strncpy(id, url + ovector[2*i], olen);
                id[olen] = '\0';

                switch( id[olen-1] ) {
                    case 's':
                    case 'b':
                    case 't':
                    case 'm':
                    case 'l':
                    case 'h':
                        id[olen-1] = '\0';
                        break;
                }
                return id;
                break;
        }
    }
    return id;
}


int cmd_imgur(int s, char* id) {
    if( id == NULL)
        return -1;
    int status = 0;
    int found = 0;
    char* pong_msg = malloc(sizeof(char) * 4096);
    memset(pong_msg, 0,  4096);

    char* auth_header = "Authorization: Client-ID 9a8fad3535ee041";

    char* api_url = "https://api.imgur.com/3/image/%s.json";
    char* token = malloc(sizeof(char) * 4096);
    memset(token, 0,  4096);

    sprintf(token, api_url, id);

    if( !found ) {
        title = malloc(sizeof(char) * 4096);
        memset(title, 0,  4096);

        CURL* c = curl_easy_init();
        struct curl_slist *headers=NULL;

        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(c, CURLOPT_URL, token );
        //curl_easy_setopt(c, CURLOPT_URL, token + strlen("https://"));
        curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(c, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10);
        //curl_easy_setopt(c, CURLOPT_USERAGENT, "curl/7.21.3 (x86_64-pc-linux-gnu) libcurl/7.21.3 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &cmd_imgur_writecallback);
        curl_easy_setopt(c, CURLOPT_VERBOSE, 1);
        curl_easy_perform(c);


        long resp;
        char* url;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &resp);
        curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &url);

        if( resp != 200 && resp != 404 )
            return -1;

        json_error_t error;
        json_t* image_j = json_loads(title, 0, &error);
        json_t* data = json_object_get(image_j, "data");
        json_t* success = json_object_get(image_j, "success");
        

        if( json_is_true(success) ) {
            json_t* data_title = json_object_get(data, "title");
            json_t* data_link = json_object_get(data, "link");
            json_t* data_width = json_object_get(data, "width");
            json_t* data_height = json_object_get(data, "height");
            json_t* data_size = json_object_get(data, "size");
            json_t* data_views = json_object_get(data, "views");
            const char* img_title = json_string_value(data_title);
            const char* img_link = json_string_value(data_link);
            json_int_t img_width = json_integer_value(data_width);
            json_int_t img_height = json_integer_value(data_height);
            json_int_t img_size = json_integer_value(data_size);
            json_int_t img_views = json_integer_value(data_views);

            sprintf(pong_msg, "[ %c%d%s%c: %c%d%s%c size: %" JSON_INTEGER_FORMAT "x%" JSON_INTEGER_FORMAT " bytes: %" JSON_INTEGER_FORMAT " views: %" JSON_INTEGER_FORMAT " link: %c%d%s%c ]\r\n",
                Color, LightGreen, "Title", Reset,
                Color, Orange, img_title, Reset,
                img_width,
                img_height,
                img_size,
                img_views,
                Color, DarkRed, img_link, Reset
            );
        } else {
            json_t* data_error = json_object_get(data, "error");
            const char* img_error = json_string_value(data_error);
            sprintf(pong_msg, "[ %s  ]\r\n", img_error);
        }

        free(image_j);
        free(title);
        curl_slist_free_all(headers);
        curl_easy_cleanup(c);
    }
    cmd_echo(s, pong_msg);

    free(pong_msg);
    return status;
}

void cmd_imgur_init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void cmd_imgur_cleanup() {
    curl_global_cleanup();
}
#endif
#endif
