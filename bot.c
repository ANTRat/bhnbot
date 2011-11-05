/*
** showip.c -- show IP addresses for a host given on the command line
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

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
int cmd_hi(int s, char* cmd_token);

int main(int argc, char *argv[])
{
    char* serv = "irc.gamesurge.net";
    char* port = "6667";
    char* nick = "btc-bot";
    struct addrinfo hints, *res;
    int status;

    //if (argc != 2) {
    //    fprintf(stderr,"usage: showip hostname\n");
    //    return 1;
    //}

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
    
    while(1){
        memset(buff, 0, max_len);
        if((status = recv(s, buff, max_len, 0)) > 0) {
            printf("status: %d\n", status);
            // per line loop
            for( line_token = strtok_r(buff, "\r\n", &line_buff) ; line_token != NULL ; line_token = strtok_r(NULL, "\r\n", &line_buff) ) {
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
                        if(strncmp("#BHNGAMING", strtoupper(cmd_token), strlen("#BHNGAMING")) == 0){
                            cmd_token = strtok_r(NULL, " ", &cmd_buff);
                            if(strncmp(":!DICKS", strtoupper(cmd_token), strlen(":!DICKS")) == 0){
                                cmd_hi(s, "List of Dicks:");
                                cmd_hi(s, " You");
                                cmd_hi(s, "-- Done");
                            }
                        }
                    } else if(tkn_indx == 1 && strncmp("001", strtoupper(cmd_token), strlen("001")) == 0) {
                        char* join_cmd = "JOIN #bhngaming\r\n";
                        send(s, join_cmd, strlen(join_cmd), 0);
                    }
                    printf("CMD[%d]:        '%s'\n", tkn_indx, cmd_token);
                }
            }
            

//            printf("status: %d\n", status);
//            token = strtok(buff, delim);
//            while(token != NULL)
//            {
//                printf("LOG[%d]: %s\n", status, token);
//                token = strtok(NULL, delim);
//            }

            //char* join_line = "JOIN #bhngaming\r\n";
            //send(s, join_line, strlen(join_line), 0);
            printf("thats it!\n");
        } else {
            break;
        }
    }

    //printf("IP addresses for %s:\n\n", argv[1]);

    //for(p = res;p != NULL; p = p->ai_next) {
    //    void *addr;
    //    char *ipver;

    //    // get the pointer to the address itself,
    //    // different fields in IPv4 and IPv6:
    //    if (p->ai_family == AF_INET) { // IPv4
    //        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
    //        addr = &(ipv4->sin_addr);
    //        ipver = "IPv4";
    //    } else { // IPv6
    //        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    //        addr = &(ipv6->sin6_addr);
    //        ipver = "IPv6";
    //    }

    //    // convert the IP to a string and print it:
    //    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    //    printf("  %s: %s\n", ipver, ipstr);
    //}

    freeaddrinfo(res); // free the linked list

    return 0;
}

int sendident(int s, char* nick, char* user, char* host){
    int status = 0;
    char* nick_templ = "NICK %s\r\n";
    char* user_templ = "USER %s %s bla :Bitcoin Bot\r\n";

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

int cmd_hi(int s, char* cmd_token) {
    int status = 0;
    char* pong_msg = malloc(sizeof(char) * 4096);
    memset(pong_msg, 0,  4096);

    sprintf(pong_msg, "PRIVMSG #bhngaming :%s\r\n", cmd_token);
    send(s, pong_msg, strlen(pong_msg), 0);

    free(pong_msg);
    return status;
}
