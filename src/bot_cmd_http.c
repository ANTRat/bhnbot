#include "config.h"

#ifdef HAVE_LIBCURL

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

#ifdef HAVE_LIBSQLITE3
#include <sqlite3.h>

sqlite3* db;
sqlite3_stmt* ins_stmt;
sqlite3_stmt* srch_stmt;

#endif

char* title;

size_t cmd_http_writecallback(char *ptr, size_t size, size_t nmemb, void *userdata __attribute__((unused)) ) {
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

int cmd_http(int s, int https, char* line, char* token) {
    int status = 0;
    int found = 0;
    char* pong_msg = malloc(sizeof(char) * 4096);
    memset(pong_msg, 0,  4096);
#ifdef HAVE_LIBSQLITE3
    int rc;
    const unsigned char* prev_nick;
    const unsigned char* prev_title;
    const unsigned char* prev_line;
    const unsigned char* prev_created;
    sqlite3_bind_text(srch_stmt, 1, token, -1, SQLITE_TRANSIENT);
    while((rc = sqlite3_step(srch_stmt)) == SQLITE_ROW) {
        found++;
        prev_nick = sqlite3_column_text(srch_stmt, 0);
        prev_title = sqlite3_column_text(srch_stmt, 3);
        prev_line = sqlite3_column_text(srch_stmt, 4);
        prev_created = sqlite3_column_text(srch_stmt, 5);
        break;
    }
    if( found ) {
        sprintf(pong_msg, "PRIVMSG %s :[ OFN :: %s <%s> :: %s :: %s ]\r\n",
            IRC_CHANNEL,
            prev_created,
            prev_nick,
            prev_title,
            prev_line
        );
    }
    sqlite3_reset(srch_stmt);
#endif
    if( !found ) {
        title = malloc(sizeof(char) * 4096);
        memset(title, 0,  4096);

        CURL* c = curl_easy_init();
        if( https ) {
            curl_easy_setopt(c, CURLOPT_URL, token + strlen("https://"));
        } else {
            curl_easy_setopt(c, CURLOPT_URL, token + strlen("http://"));
        }
        curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
        curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP);
        curl_easy_setopt(c, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10);
        //curl_easy_setopt(c, CURLOPT_USERAGENT, "curl/7.21.3 (x86_64-pc-linux-gnu) libcurl/7.21.3 OpenSSL/0.9.8o zlib/1.2.3.4 libidn/1.18");
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &cmd_http_writecallback);
        curl_easy_setopt(c, CURLOPT_VERBOSE, 1);
        curl_easy_perform(c);


        long resp;
        char* url;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &resp);
        curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &url);

#ifdef HAVE_LIBSQLITE3
        char* nick = strdup(line + 1);
        char* start_line = strstr(nick, ":") + 1;
        char* end_nick = strstr(nick, "!");
        end_nick[0] = '\0';
        rc = sqlite3_bind_text(ins_stmt, 1, nick, -1, SQLITE_TRANSIENT);
        rc = sqlite3_bind_text(ins_stmt, 2, token, -1, SQLITE_TRANSIENT);
        rc = sqlite3_bind_int(ins_stmt, 3, resp);
        if( strlen(title) > 0 ) {
            rc = sqlite3_bind_text(ins_stmt, 4, title, -1, SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_text(ins_stmt, 4, "", 0, SQLITE_STATIC);
        }
        rc = sqlite3_bind_text(ins_stmt, 5, start_line, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(ins_stmt);
        if( rc != SQLITE_DONE ) {
            fprintf(stderr, "sqlite3_step() failed: %i\n", rc);
        }
        sqlite3_reset(ins_stmt);
        free(nick);
#endif

        switch(resp) {
            case 200:
                if( strlen(title) > 0 ) {
                    sprintf(pong_msg, "PRIVMSG %s :[ %s ]\r\n", IRC_CHANNEL, title );
                }
                break;
            default:
                sprintf(pong_msg, "PRIVMSG %s :[ HttpErr: %li ]\r\n", IRC_CHANNEL, resp );
                break;
        }

        free(title);
        curl_easy_cleanup(c);
    }
    if( strlen(pong_msg) > 0 ) {
        send(s, pong_msg, strlen(pong_msg), 0);
    }
    free(pong_msg);
    return status;
}

void cmd_http_init() {
    curl_global_init(CURL_GLOBAL_ALL);

#ifdef HAVE_LIBSQLITE3
    char* zErrMsg = NULL;
    int rc;
    rc = sqlite3_open("cmd_http_db", &db);
    rc = sqlite3_exec(db, "create table if not exists http_urls (id INTEGER, created INTEGER DEFAULT CURRENT_TIMESTAMP, time TEXT, nick TEXT, url TEXT, resp INTEGER, title TEXT, line TEXT, PRIMARY KEY(id ASC) );", NULL, 0, &zErrMsg);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return;
    }

    char* ins_stmt_sql = "insert into http_urls (nick, url, resp, title, line) values (?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2( db, ins_stmt_sql, strlen(ins_stmt_sql), &ins_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare ins_stmt failed: %i\n", rc);
        return;
    }

    char* srch_stmt_sql = "select nick, url, resp, title, line, datetime(created, 'localtime') from http_urls where url = ?;";
    rc = sqlite3_prepare_v2( db, srch_stmt_sql, strlen(srch_stmt_sql), &srch_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare srch_stmt failed: %i\n", rc);
        return;
    }
#endif

}

void cmd_http_cleanup() {
    curl_global_cleanup();

#ifdef HAVE_LIBSQLITE3
    sqlite3_finalize(ins_stmt);
    sqlite3_finalize(srch_stmt);
    sqlite3_close(db);
#endif

}
#endif
