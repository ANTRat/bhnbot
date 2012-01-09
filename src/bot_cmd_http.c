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
#include <jansson.h>
#include "bot_conf.h"
#include "bot_cmd_echo.h"

extern botconf_t* conf;

#ifdef HAVE_LIBSQLITE3
#include <sqlite3.h>

sqlite3* db;
sqlite3_stmt* ins_stmt;
sqlite3_stmt* srch_stmt;
sqlite3_stmt* last_srch_stmt;

// full text search
sqlite3_stmt* fts_title_ins_stmt;
sqlite3_stmt* fts_title_srch_stmt;

#endif

#ifdef ENABLE_SHORTURLS
char* json_resp;
size_t cmd_http_longurlcallback(char *ptr, size_t size, size_t nmemb, void *userdata __attribute__((unused)) ) {
    strncpy(json_resp, ptr, size * nmemb);
    return size * nmemb;
}

char* cmd_http_shortenurl(char* longurl){
    json_resp = malloc(sizeof(char) * 4096);
    memset(json_resp, 0,  4096);

    char* pong_msg = malloc(sizeof(char) * 4096);
    memset(pong_msg, 0,  4096);

    char* api_url = malloc(sizeof(char) * 4096);
    memset(api_url, 0,  4096);
    sprintf(api_url, "https://www.googleapis.com/urlshortener/v1/url?key=%s", conf->google_api_key);

    long resp;
    char* url;
    struct curl_slist* urlshortener_header = NULL;

    urlshortener_header = curl_slist_append(urlshortener_header, "Content-Type: application/json");

    sprintf(pong_msg, "{\"longUrl\": \"%s\"}", longurl);

    CURL* c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, api_url);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, urlshortener_header);
    curl_easy_setopt(c, CURLOPT_POST, 1);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, pong_msg);
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &cmd_http_longurlcallback);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(c, CURLOPT_VERBOSE, 1);
    curl_easy_perform(c);

    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &resp);
    curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &url);

    curl_easy_cleanup(c);

    curl_slist_free_all(urlshortener_header);

    free(pong_msg);
    free(api_url);

    char* shorturl = NULL;

    json_error_t error;
    json_t* root;
    json_t* id;

    root = json_loads(json_resp, 0, &error);
    if( root )
    {
        id = json_object_get(root, "id");
        if( json_is_string(id) )
        {
            shorturl = strdup(json_string_value(id));
            if( conf->debug >= 2) printf("Short URL: %s\n", shorturl);
        } else {
            fprintf(stderr, "Id is not a string");
        }

        json_decref(root);

    } else {
        fprintf(stderr, "Error parsing json: %d: %s\n", error.line, error.text);
    }

    free(json_resp);
    return shorturl;
}
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
        sprintf(pong_msg, "PRIVMSG %s :[ OFN :: %s <%s> %s :: %s ]\r\n",
            conf->channel,
            prev_created,
            prev_nick,
            prev_line,
            prev_title
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
        curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
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

        // full text search
        rc = sqlite3_step(fts_title_ins_stmt);
        if( rc != SQLITE_DONE ) {
            fprintf(stderr, "sqlite3_step() failed: %i\n", rc);
        }
        sqlite3_reset(fts_title_ins_stmt);
#endif

        if( strlen(title) > 0 ) {
#ifdef ENABLE_SHORTURLS
            char* shorturl;
            shorturl = cmd_http_shortenurl(url);
            sprintf(pong_msg, "PRIVMSG %s :[ %s :: %s ]\r\n", conf->channel, title, shorturl );
            free(shorturl);
#endif
#ifndef ENABLE_SHORTURLS
            sprintf(pong_msg, "PRIVMSG %s :[ %s ]\r\n", conf->channel, title );
#endif
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

#ifdef HAVE_LIBSQLITE3
void cmd_http_lastlinks(int s) {
    char* pong_msg = malloc(sizeof(char) * 4096);

    int rc;
    const unsigned char* prev_nick;
    const unsigned char* prev_title;
    const unsigned char* prev_line;
    const unsigned char* prev_created;
    int prev_id;
    while((rc = sqlite3_step(last_srch_stmt)) == SQLITE_ROW) {
        memset(pong_msg, 0,  4096);

        prev_nick = sqlite3_column_text(last_srch_stmt, 0);
        prev_title = sqlite3_column_text(last_srch_stmt, 3);
        prev_line = sqlite3_column_text(last_srch_stmt, 4);
        prev_created = sqlite3_column_text(last_srch_stmt, 5);
        prev_id = sqlite3_column_int(last_srch_stmt, 6);

        sprintf(pong_msg, "PRIVMSG %s :[ Link #%i :: %s <%s> %s :: %s ]\r\n",
            conf->channel,
            prev_id,
            prev_created,
            prev_nick,
            prev_line,
            prev_title
        );

        if( strlen(pong_msg) > 0 ) {
            send(s, pong_msg, strlen(pong_msg), 0);
        }
    }
    sqlite3_reset(last_srch_stmt);
    free(pong_msg);
}

void cmd_http_title_search(int s, char* search_term) {
    char* pong_msg = malloc(sizeof(char) * 4096);

    int rc;
    const unsigned char* prev_nick;
    const unsigned char* prev_title;
    const unsigned char* prev_line;
    const unsigned char* prev_created;
    int prev_id;

    rc = sqlite3_bind_text(fts_title_srch_stmt, 1, search_term, -1, SQLITE_TRANSIENT);

    while((rc = sqlite3_step(fts_title_srch_stmt)) == SQLITE_ROW) {
        memset(pong_msg, 0,  4096);

        prev_nick = sqlite3_column_text(fts_title_srch_stmt, 0);
        prev_title = sqlite3_column_text(fts_title_srch_stmt, 3);
        prev_line = sqlite3_column_text(fts_title_srch_stmt, 4);
        prev_created = sqlite3_column_text(fts_title_srch_stmt, 5);
        prev_id = sqlite3_column_int(fts_title_srch_stmt, 6);

        sprintf(pong_msg, "PRIVMSG %s :[ Link #%i :: %s <%s> %s :: %s ]\r\n",
            conf->channel,
            prev_id,
            prev_created,
            prev_nick,
            prev_line,
            prev_title
        );

        if( strlen(pong_msg) > 0 ) {
            send(s, pong_msg, strlen(pong_msg), 0);
        }
    }
    sqlite3_reset(fts_title_srch_stmt);
    free(pong_msg);
}
#endif

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
        exit(1);
        return;
    }
    // full text search
    rc = sqlite3_exec(db, "drop table http_titles;", NULL, 0, &zErrMsg);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    rc = sqlite3_exec(db, "create virtual table http_titles using fts3 ( title TEXT, id INTEGER, tokenize=porter, order=desc );", NULL, 0, &zErrMsg);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    rc = sqlite3_exec(db, "insert into http_titles (title, id) select title, id from http_urls order by created;", NULL, 0, &zErrMsg);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    char* fts_title_ins_stmt_sql = "insert into http_titles (title, id) select title, max(id) from http_urls;";
    rc = sqlite3_prepare_v2( db, fts_title_ins_stmt_sql, strlen(fts_title_ins_stmt_sql), &fts_title_ins_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare fts_title_ins_stmt failed: %i\n", rc);
        return;
    }
    char* fts_title_srch_stmt_sql = "select b.nick, b.url, b.resp, b.title, b.line, datetime(b.created, 'localtime'), b.id from http_titles a inner join http_urls b on a.id = b.id where a.title match ? order by created desc limit 3;";
    rc = sqlite3_prepare_v2( db, fts_title_srch_stmt_sql, strlen(fts_title_srch_stmt_sql), &fts_title_srch_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare fts_title_srch_stmt failed: %i\n", rc);
        return;
    }
// sqlite3_stmt* fts_title_ins_stmt;
// sqlite3_stmt* fts_title_srch_stmt;
// select b.nick, b.url, b.resp, b.title, b.line, datetime(b.created, 'localtime'), b.id from http_titles a inner join http_urls b on a.id = b.id where a.title match ?;


    char* ins_stmt_sql = "insert into http_urls (nick, url, resp, title, line) values (?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2( db, ins_stmt_sql, strlen(ins_stmt_sql), &ins_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare ins_stmt failed: %i\n", rc);
        return;
    }

    char* srch_stmt_sql = "select nick, url, resp, title, line, datetime(created, 'localtime') from http_urls where url = ? limit 1;";
    rc = sqlite3_prepare_v2( db, srch_stmt_sql, strlen(srch_stmt_sql), &srch_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare srch_stmt failed: %i\n", rc);
        return;
    }

    char* last_srch_stmt_sql = "select nick, url, resp, title, line, datetime(created, 'localtime'), id from http_urls order by created desc limit 3;";
    rc = sqlite3_prepare_v2( db, last_srch_stmt_sql, strlen(last_srch_stmt_sql), &last_srch_stmt, NULL);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "prepare last_srch_stmt failed: %i\n", rc);
        return;
    }
#endif

}

void cmd_http_cleanup() {
    curl_global_cleanup();

#ifdef HAVE_LIBSQLITE3
    sqlite3_finalize(fts_title_ins_stmt);
    sqlite3_finalize(fts_title_srch_stmt);
    sqlite3_finalize(ins_stmt);
    sqlite3_finalize(srch_stmt);
    sqlite3_finalize(last_srch_stmt);
    sqlite3_close(db);
#endif

}
#endif
