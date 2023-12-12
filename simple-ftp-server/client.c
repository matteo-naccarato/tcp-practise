#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "includes/utilities.h"

#define MAX_PACK 1024*1024
#define MAX_SEND 1024

// client
#define SEND_USER "USER,"
#define SEND_PASS "PASS,"

// server
#define READY "220 Ready"
#define USER_OK_NEED_PASS "331 Username okay, need password"
#define USER_OR_PASS_WRONG "430 Invalid username or password"
#define GREETING "230 Greeting"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("USAGE: %s IP PORT FILE_TO_RETRIEVE\n", argv[0]);
        return -1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    char* file_name = argv[3];

    int sock_id = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_id < 0)
        error("socket()", -2);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    inet_aton(ip, &server.sin_addr);
    server.sin_port = htons(port);
    for (int i=0; i<8; i++) server.sin_zero[i] = 0;

    // connessione al server
    socklen_t len = sizeof(struct sockaddr);
    if ( connect(sock_id, (struct sockaddr*) &server, len) )
        error("connect()", -3);

    
    char* check[] = { READY, USER_OK_NEED_PASS };
    int check_length = 2;
    char* cmd[] = { SEND_USER, SEND_PASS };
    char* input_req[] = { "username", "password" };

    char buffer[MAX_PACK + 1];
    char* msg = (char*) malloc(sizeof(char) * MAX_SEND);
    bool berror = false;
    for (int i = 0; i < check_length && !berror; i++) {
        int rc = recv(sock_id, buffer, MAX_PACK + 1, 0);
        if (rc < 0)
            error("recv()", -4);
        buffer[rc] = '\0';
        printf("[FTP-SERVER]\t'%s'\n\n", buffer);

        if (!strcmp(buffer, check[i])) {
            printf("insert %s: ", input_req[i]);
            char* input = inputStr();
            sprintf(msg, "%s %s", cmd[i], input);
            free(input);
            if ( send(sock_id, msg, strlen(msg), 0) != strlen(msg) )
                error("send()", -6);
            printf("[CLIENT]\t'%s'\n", msg);
            free(msg);

        } else berror = true;
    }
    if (!berror) {
        int rc = recv(sock_id, buffer, MAX_PACK + 1, 0);
        if (rc < 0)
            error("recv()", -4);
        buffer[rc] = '\0';
        printf("\n[FTP-SERVER]\t'%s'\n", buffer);

        if (!strcmp(buffer, GREETING)) {
            printf("Logged in!\n\n");


            /* SEND FILENAME */
            if ( send(sock_id, file_name, strlen(file_name), 0) != strlen(file_name) )
                error("send()", -6);
            printf("[CLIENT]\tSent filename\n");

            /* RECEIVE FILE CONTENT */
            while (rc = recv(sock_id, buffer, MAX_PACK + 1, 0) > 0) {
                if (rc < 0)
                    error("recv()", -4);
                printf("[FTP-SERVER]\t'%s'\n\n", buffer);
            }
        }
    } else {
        printf("Something went wrong!\n\n");
    }

    close(sock_id);
    return 0;
}