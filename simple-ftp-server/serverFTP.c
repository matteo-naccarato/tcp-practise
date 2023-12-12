// sudo apt-get install libsqlite3-dev
// gcc test.c -o test -lpthread -lsqlite3

// FTP Server [RFC 959, TCP, PORTS 20-21]
// Fase di autenticazione, PORT 21
// https://lh3.googleusercontent.com/proxy/9HJFk2FY2rJP4nfcTrbrWwC_xRy-QM2mnHtL6FVJOdHue_IbFXINjUOlhQsXl_-meiG-BwVm0D7aXBZSwIfW5tn0pAgKnd_hgm1mkoeOGjIP
// https://tools.ietf.org/html/rfc959
// https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sqlite3.h>

#include "includes/utilities.h"

#define PROMPT "$>"
#define EXIT_CMD "quit"

#define LO "127.0.0.1"
#define MAX_CONN 50
#define PORT_DATA 20
#define PORT_CMD 21
#define MAX_PACK 1024*1024
#define MAX_TMP 256
#define MAX_SQL 256

#define DB_NAME "ftp_server.db"
#define COLUMN_USERNAME "username"
#define COLUMN_PASSWORD "password"

// REPLIES
#define ABT_OPEN_DCONN "150 File status okay; about to start data connection"
#define READY "220 Ready"
#define GREETING "230 Greeting"
#define USER_OK_NEED_PASS "331 Username okay, need password"
#define USER_OR_PASS_WRONG "430 Invalid username or password"

typedef struct {
    int sock_id;
    struct sockaddr_in client;
    socklen_t len;
} ListeningParams;

void* listening(void*);
void* response(void*);
void my_send(int connId, char*);
void my_receive(int connId, char buffer[MAX_PACK + 1]);
bool isUserValid(char*);
bool isPassValid(char*);
bool checkDB(char*, char*);


int main(int argc, char* argv[]) {

    printf("[FTP] Init ...\n");
    int port = argc>1? atoi(argv[1]) : PORT_CMD;

    struct sockaddr_in myself, client;
    myself.sin_family = AF_INET;
    inet_aton(LO, &myself.sin_addr);
    myself.sin_port = htons(port);
    for (int i=0; i<8; i++) myself.sin_zero[i] = 0;

    int sock_id = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_id < 0)
        error("socket()", -1);

    socklen_t len = sizeof(struct sockaddr);
    if (bind(sock_id, (struct sockaddr*) &myself, len) == -1)
        error("bind()", -2);

    printf("[FTP] Server listening on %s:%d ...\n", LO, port);
    if (listen(sock_id, MAX_CONN))
        error("listen()", -3);

    ListeningParams params = { sock_id, client, len };
    pthread_t thread_id;
    if ( pthread_create(&thread_id, NULL, listening, (void*) &params) )
        error("pthread_create()", -4);

    printf("[FTP] %s\n\n", PROMPT);
    char* cmd = inputStr();
    while ( strcmp(cmd, EXIT_CMD) ) {
        free(cmd);
        cmd = inputStr();
    }
    free(cmd);

    close(sock_id);
    printf("[FTP] Server closed!\n");
    return 0;
}

void* listening(void* params) {
    ListeningParams* p = (ListeningParams*) params;

    int connId;
    while (1) {
        connId = accept(p->sock_id,
                        (struct sockaddr*) &p->client,
                        &p->len);
        if (connId == -1)
            error("accept()", -5);
        printf("[FTP] Client has connected!\n");

        pthread_t thread_id;
        if ( pthread_create(&thread_id, NULL, response, (void*) &connId))
            error("pthread_create()", -6);
    }
}

void* response(void* params) {
    int connId = *((int*) params);
    int rc, len;
    char buffer[MAX_PACK + 1];

    // invio "220 READY"
    my_send(connId, READY);
    printf("[FTP] sent to\t\t[client]\t'%s'\n\n", READY);

    // attesa USER
    my_receive(connId, buffer);
    printf("[FTP] my_received from\t[client]\t'%s'\n", buffer);

    // risposta USER
    char** tmp = (char**) malloc(sizeof(char) * MAX_TMP);
    tmp = split(buffer, " "); // USER, username
    char* user = tmp[1];
    free(tmp);
    if (isUserValid(user)) {
        free(user);

        my_send(connId, USER_OK_NEED_PASS);
        printf("[FTP] sent to\t\t[client]\t'%s'\n\n", USER_OK_NEED_PASS);

        // attesa PASS
        my_receive(connId, buffer);
        printf("[FTP] my_received from\t[client]\t'%s'\n", buffer);

        char** tmp = (char**) malloc(sizeof(char) * MAX_TMP);
        tmp = split(buffer, " "); // PASS, password
        char* pass = tmp[1];
        free(tmp);
        if (isPassValid(pass)) {
            my_send(connId, GREETING);
            printf("[FTP] sent to\t\t[client]\t'%s'\n\n", GREETING);



            /* READ AND SEND FILE CONTENT */
            my_send(connId, ABT_OPEN_DCONN);

            my_receive(connId, buffer);
            printf("[FTP] my_received from\t[client]\t'%s'\n", buffer);

            char* tmp = (char*) malloc(sizeof(char) * MAX_TMP);
            sprintf(tmp, "%s", buffer);
            FILE* fptr = fopen(tmp, "r");
            if (fptr == NULL)
                error("fopen()", -7);
            printf("[FTP] reading file %s\n", tmp);
            free(tmp);

            while(!feof(fptr)) {
                char filebuffer[MAX_PACK];
                int read = fread(filebuffer, sizeof(char), sizeof(filebuffer), fptr);
                my_send(connId, filebuffer);
            }
            fclose(fptr);
            printf("[FTP] sent file to\t[client]\n");


        } else my_send(connId, USER_OR_PASS_WRONG);
        free(pass);
        
    } else my_send(connId, USER_OR_PASS_WRONG);
    
    shutdown(connId, SHUT_RDWR);
    printf("[FTP] Connection closed!\n\n==============================================\n\n");
}

void my_send(int connId, char* msg) {
    if ( send(connId, msg, strlen(msg), 0) != strlen(msg) )
        error("send()", -11);
}

void my_receive(int connId, char buffer[MAX_PACK + 1]) {
    int rc = recv(connId, buffer, MAX_PACK + 1, 0);
    if (rc < 0)
        error("recv()", -9);
    buffer[rc] = '\0';
}

bool isUserValid(char* user) {
    return checkDB(COLUMN_USERNAME, user);
}

bool isPassValid(char* pass) {
    return checkDB(COLUMN_PASSWORD, pass);
}

bool checkDB(char* column, char* value) {
    bool isValid;

    sqlite3 *db;
    sqlite3_stmt *stmt;
    char* err_msg = 0;

    int rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] Cannot open database: %s\n", sqlite3_errmsg(db));
        error("sqlite3_open()", -11);
    }

    char* sql = (char*) malloc(sizeof(char) * MAX_SQL);
    sprintf(sql, "SELECT * FROM Users WHERE %s = '%s'", column, value);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] Failed to execute statement: %s\n", sqlite3_errmsg(db));
        error("sqlite3_prepare_v2()", -12);
    }
    free(sql);
    
    int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW) {
        isValid = true;
        printf("[DB] User found: %s|%s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1));
    } else {
        isValid = false;
        printf("[DB] User not found!\n\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return isValid;
}