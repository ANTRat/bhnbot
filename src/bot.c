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
#include "bot_cmd_echo.h"
#include "bot_cmd_http.h"

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

int sendident(int s, char* nick, char* user, char* host);
int pong(int s, char* cmd_token);

// signal handling from: http://www.gnu.org/s/hello/manual/libc/Handler-Returns.html
volatile sig_atomic_t running = 1;

void handle_sigint(int sig) {
    running = 0;
    signal(sig, handle_sigint);
}

int main( int argc __attribute__((unused)), char *argv[] __attribute__((unused)) )
{
    char* serv = IRC_SERVER;
    char* port = IRC_PORT;
    char* nick = IRC_NICK;
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

    if ((status = getaddrinfo(serv, port, &hints, &res)) != 0) {
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
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof timeout) < 0) {
        printf("setsockopt: SO_RCVTIMEO failed\n");
        return 2;
    }
    if (setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout) < 0) {
        printf("setsockopt: SO_SNDTIMEO failed\n");
        return 2;
    }

    if(connect(s, res->ai_addr, res->ai_addrlen) == -1)
    {
        fprintf(stderr, "connect broke");
        return 2;
    }
    
    sendident(s, nick, nick, serv);

    int max_len = 4096;
    char* buff = malloc(sizeof(char) * max_len);
    char* line_token;
    char* line_buff;
    char* cmd_token;
    char* cmd_buff;
    
    while(running){
        memset(buff, 0, max_len);
        if((status = recv(s, buff, max_len, 0)) > 0) {
            printf("status: %d\n", status);
            // per line loop
            for( line_token = strtok_r(buff, "\r\n", &line_buff) ; line_token != NULL ; line_token = strtok_r(NULL, "\r\n", &line_buff) ) {
                char *line = strdup(line_token);
                char *line2 = strdup(line_token);

                printf("LIN[%d]: '%s'\n", status, line_token);
                int tkn_indx;
                for(    tkn_indx = 0 , cmd_token = strtok_r(line_token, " ", &cmd_buff) ;
                        cmd_token != NULL;
                        tkn_indx++, cmd_token = strtok_r(NULL, " ", &cmd_buff) ) {

                    if(tkn_indx == 0 && strncmp("PING", strtoupper(cmd_token), strlen("PING")) == 0) {
                        cmd_token = strtok_r(NULL, " ", &cmd_buff);
                        pong(s, cmd_token);
                        break;
                    }

                    //cmd_token = strtok_r(NULL, " ", &cmd_buff);

                    if(tkn_indx == 1 && strncmp("PRIVMSG", strtoupper(cmd_token), strlen("PRIVMESG")) == 0) {
                        cmd_token = strtok_r(NULL, " ", &cmd_buff);
                        if(strncmp(IRC_CHANNEL, strtoupper(cmd_token), strlen(IRC_CHANNEL)) == 0){
                            cmd_token = strtok_r(NULL, " ", &cmd_buff);

                            char* cmd = malloc(sizeof(char) * strlen(cmd_token) + 1);
                            strcpy(cmd, cmd_token);

                            if( strstr(line, ":!help") != NULL ){
                                cmd_echo(s, "Available Commands:");
                                cmd_echo(s, "   !lastlinks          Prints the last 5 links entered");
                                cmd_echo(s, "   !title      <text>  Searches titles for <text>");
                            }
#ifdef HAVE_LIBCURL
#ifdef HAVE_LIBSQLITE3
                            else if( strstr(line, ":!lastlinks") != NULL ){
                                cmd_http_lastlinks(s);
                            }
                            else if( strstr(line, ":!title ") != NULL ){
                                char* search_term = strstr(line, ":!title ") + strlen(":!title ");
                                cmd_http_title_search(s, search_term);
                            }
#endif
#ifdef STUMBLEUPON_FILTER
                            else if( strstr(line, "http://www.stumbleupon.com/su/") != NULL ) {
                                char* http_indx = strstr(line, "http://www.stumbleupon.com/su/");
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
                            else if( strstr(line, "http://") != NULL ) {
                                char* http_indx = strstr(line, "http://");
                                char* spc_loc;
                                for(spc_loc = http_indx; *spc_loc != '\0'; spc_loc++) {
                                    if( !isgraph(*spc_loc) ) {
                                        break;
                                    }
                                }
                                *spc_loc = '\0';
                                cmd_http(s, 0, line2, http_indx);
                            }
                            else if( strstr(line, "https://") != NULL ) {
                                char* http_indx = strstr(line, "https://");
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
                    } else if(tkn_indx == 1 && strncmp("001", strtoupper(cmd_token), strlen("001")) == 0) {
                        char* join_cmd = malloc(sizeof(char) * 4096);
                        sprintf(join_cmd, "JOIN %s\r\n", IRC_CHANNEL);
                        send(s, join_cmd, strlen(join_cmd), 0);
                        free(join_cmd);
                    }
                    printf("CMD[%d]:        '%s'\n", tkn_indx, cmd_token);
                }

                free(line);
                free(line2);
            }

            printf("thats it!\n");
        }
    }

    free(buff);

    freeaddrinfo(res); // free the linked list

#ifdef HAVE_LIBCURL
    cmd_http_cleanup();
#endif

    return EXIT_SUCCESS;
}

int sendident(int s, char* nick, char* user, char* host){
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

