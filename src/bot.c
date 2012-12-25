#include "config.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "bot_conf.h"
#include "bot_cmd_echo.h"
#include "bot_cmd_http.h"
#include "bot_cmd_imgur.h"

char* strtolower(char* str) {
    char *p;
    for (p = str; *p != '\0'; p++)
        *p = (char)tolower(*p);
    return str;
}

char* strtoupper(char* str) {
    char *p;
    for (p = str; *p != '\0'; p++)
        *p = (char)toupper(*p);
    return str;
}

char *stristr(char *haystack, const char *needle)
{
   if ( !*needle )
   {
      return haystack;
   }
   for ( ; *haystack; ++haystack )
   {
      if ( toupper(*haystack) == toupper(*needle) )
      {
         // Matched starting char -- loop through remaining chars.
         const char *h, *n;
         for ( h = haystack, n = needle; *h && *n; ++h, ++n )
         {
            if ( toupper(*h) != toupper(*n) )
            {
               break;
            }
         }
         if ( !*n ) /* matched all of 'needle' to null termination */
         {
            return haystack; /* return the start of the match */
         }
      }
   }
   return 0;
}

int sendident(int s, const char* nick, const char* user, const char* host);
int pong(int s, char* cmd_token);

// signal handling from: http://www.gnu.org/s/hello/manual/libc/Handler-Returns.html
volatile sig_atomic_t running = 1;

void handle_sigint(int sig) {
    running = 0;
    signal(sig, handle_sigint);
}

botconf_t* conf;

int main( int argc __attribute__((unused)), char *argv[] __attribute__((unused)) )
{
    conf = botconf_load_file("bhnbot.conf");
    if( !conf ) {
        exit(1);
    }
    botconf_print(conf);

    struct timeval timeout;
    struct addrinfo hints, *res;
    int status;

    signal(SIGINT, handle_sigint);

#ifdef HAVE_LIBCURL
    cmd_http_init();
#endif

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(conf->server, conf->port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
    
    int s = socket(res->ai_family, res->ai_socktype, res-> ai_protocol);
    if(s == -1)
    {
        fprintf(stderr, "i broked");
        return 2;
    } 

    // set socket timeout
    memset(&timeout, 0, sizeof timeout);
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof timeout) < 0) {
        fprintf(stderr, "setsockopt: SO_RCVTIMEO failed\n");
        return 2;
    }
    if (setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout) < 0) {
        fprintf(stderr, "setsockopt: SO_SNDTIMEO failed\n");
        return 2;
    }

    if(connect(s, res->ai_addr, res->ai_addrlen) == -1)
    {
        fprintf(stderr, "connect broke");
        return 2;
    }
    
    sendident(s, conf->nick, conf->nick, conf->server);

    int max_len = 4096;
    char* buff = malloc(sizeof(char) * max_len);
    char* line_token;
    char* line_buff;
    char* cmd_token;
    char* cmd_buff;
    char* token_match;
    
    while(running){
        memset(buff, 0, max_len);
        status = recv(s, buff, max_len, 0);
        if( status == -1 && errno == EAGAIN ) {
            continue;
        }
        else if ( status == 0 || status == -1 )
        {
            printf("status: %d, errno: %d\n", status, (int)errno);
            break;
        }
        else if ( status > 0 ) {
            if( conf->debug >= 2 ) printf("status: %d\n", status);
            // per line loop
            for( line_token = strtok_r(buff, "\r\n", &line_buff) ; line_token != NULL ; line_token = strtok_r(NULL, "\r\n", &line_buff) ) {
                char *line = strdup(line_token);
                char *line2 = strdup(line_token);

                if( conf->debug >= 1 ) printf("LIN[%d]: '%s'\n", status, line_token);
                int tkn_indx;
                for(    tkn_indx = 0 , cmd_token = strtok_r(line_token, " ", &cmd_buff) ;
                        cmd_token != NULL;
                        tkn_indx++, cmd_token = strtok_r(NULL, " ", &cmd_buff) ) {

                    if(tkn_indx == 0 && (token_match = stristr(cmd_token, "PING")) != NULL && (token_match - cmd_token) == 0) {
                        cmd_token = strtok_r(NULL, " ", &cmd_buff);
                        pong(s, cmd_token);
                        break;
                    }
                    if(tkn_indx == 0 && (token_match = stristr(cmd_token, "ERROR")) != NULL && (token_match - cmd_token) == 0) {
                        running = 0;
                        fprintf(stderr, "Recieved ERROR, quitting\n");
                    }

                    if(tkn_indx == 1 && strncmp("PRIVMSG", strtoupper(cmd_token), strlen("PRIVMESG")) == 0) {
                        cmd_token = strtok_r(NULL, " ", &cmd_buff);

                        if( *cmd_token == '#' ){
                            conf->channel = cmd_token;

                            cmd_token = strtok_r(NULL, " ", &cmd_buff);

                            char* cmd = malloc(sizeof(char) * strlen(cmd_token) + 1);
                            strcpy(cmd, cmd_token);

                            if( stristr(line, ":!help") != NULL ){
                                cmd_echo(s, "Available Commands:");
                                cmd_echo(s, "   !list           Prints the last 5 links entered");
                                cmd_echo(s, "   !last <text>    Searches titles for <text>");
                            }
#ifdef HAVE_LIBCURL
#ifdef HAVE_LIBSQLITE3
                            else if( stristr(line, ":!list") != NULL ){
                                cmd_http_lastlinks(s);
                            }
                            else if( stristr(line, ":!last ") != NULL ){
                                char* search_term = stristr(line, ":!last ") + strlen(":!last ");
                                cmd_http_title_search(s, search_term);
                            }
#endif
#ifdef ENABLE_IMGURAPI
                            else if( stristr(line, "imgur.com/") != NULL ) {
                                char* imgur_id = cmd_imgur_get_imageid(line);
                                cmd_imgur(s, imgur_id);
                                free(imgur_id);
                            }
#endif
#ifdef ENABLE_STUMBLEUPONFILTER
                            else if( stristr(line, "http://www.stumbleupon.com/su/") != NULL ) {
                                char* http_indx = stristr(line, "http://www.stumbleupon.com/su/");
                                char* spc_loc;
                                http_indx += strlen("http://www.stumbleupon.com/su/");
                                for(spc_loc = http_indx; *spc_loc != '\0'; spc_loc++) {
                                    if( *spc_loc == '/') {
                                        break;
                                    }
                                }
                                if(*spc_loc != '\0') {
                                    http_indx = ++spc_loc;
                                }
                                for(spc_loc = http_indx; *spc_loc != '\0'; spc_loc++) {
                                    if( !isgraph(*spc_loc) ) {
                                        break;
                                    }
                                }
                                *spc_loc = '\0';
                                if( strlen(http_indx) > 0 ) {
                                    http_indx -= strlen("http://");
                                    memcpy(http_indx, "http://", strlen("http://"));
                                    cmd_echo(s, http_indx);
                                    cmd_http(s, 0, line2, http_indx);
                                }
                            }
#endif
                            else if( stristr(line, "http://") != NULL ) {
                                char* http_indx = stristr(line, "http://");
                                char* spc_loc;
                                for(spc_loc = http_indx; *spc_loc != '\0'; spc_loc++) {
                                    if( !isgraph(*spc_loc) ) {
                                        break;
                                    }
                                }
                                *spc_loc = '\0';
                                cmd_http(s, 0, line2, http_indx);
                            }
                            else if( stristr(line, "https://") != NULL ) {
                                char* http_indx = stristr(line, "https://");
                                char* spc_loc;
                                for(spc_loc = http_indx; *spc_loc != '\0'; spc_loc++) {
                                    if( !isgraph(*spc_loc) ) {
                                        break;
                                    }
                                }
                                *spc_loc = '\0';
                                cmd_http(s, 1, line2, http_indx);
                            }
#endif
                            free(cmd);
                        }
                    }
                    // we just connected, do on connect stuff
                    else if( tkn_indx == 1 && strstr(cmd_token, "001") != NULL ) {
                        botconf_on_connect_send(conf, s);
                    }
                    if( conf->debug >= 2 ) printf("CMD[%d]:        '%s'\n", tkn_indx, cmd_token);
                }

                free(line);
                free(line2);
            }

            if( conf->debug >= 3 ) printf("thats it!\n");
        }
    }
    printf("status: %d\n", status);

    free(buff);

    freeaddrinfo(res); // free the linked list

#ifdef HAVE_LIBCURL
    cmd_http_cleanup();
#endif

    return EXIT_SUCCESS;
}

int sendident(int s, const char* nick, const char* user, const char* host){
    int status = 0;
    char* nick_templ = "NICK %s\r\n";
    char* user_templ = "USER %s %s bla :Bright Bot\r\n";

    char* nick_buff = malloc(sizeof(char) * 255);
    char* user_buff = malloc(sizeof(char) * 255);

    snprintf(nick_buff, 255, nick_templ, nick);
    snprintf(user_buff, 255, user_templ, user, host);

    if((status = send(s, nick_buff, strlen(nick_buff), 0)) == -1) {
        return status;
    }
    if((status = send(s, user_buff, strlen(user_buff), 0)) == -1) {
        return status;
    }

    free(nick_buff);
    free(user_buff);
    return status;
}



int pong(int s, char* cmd_token) {
    int status = 0;
    char* pong_msg = malloc(sizeof(char) * 255);
    memset(pong_msg, 0,  255);

    sprintf(pong_msg, "PONG %s\r\n", cmd_token);
    send(s, pong_msg, strlen(pong_msg), 0);

    free(pong_msg);
    return status;
}

