#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "config.h"


int cmd_echo(int s, char* message) {
    int status = 0;
    char* pong_msg = malloc(sizeof(char) * 4096);
    memset(pong_msg, 0,  4096);

    sprintf(pong_msg, "PRIVMSG %s :%s\r\n", IRC_CHANNEL, message);
    send(s, pong_msg, strlen(pong_msg), 0);

    free(pong_msg);
    return status;
}
