#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include "includes/utilities.h"

#define LO "127.0.0.1"
#define DB_NAME "test.db"
#define DEFAULT_PORT 7071
#define MAX_PACK 1024*1024
#define MAX_HTML_RES 512*512
#define MAX_TBL 250
#define MAX_SQL_QUERY 100
#define MAX_END_OF_HTML 500
#define MAX_FILE 2048
#define CODE200 "HTTP/1.1 200 OK\n\
\n"
#define CODE404 "HTTP/1.1 404 Not Found\n\
\n"
#define GET_URL_HEADER "GET "
#define GET_URL_FOOTER " HTTP"
#define TAG_SQL_BEGIN "<SQL "
#define TAG_SQL_END "/>"
#define EMPTY_REQ "./"
#define URL_404 "404.html"
#define EXIT_CMD "quit"

typedef struct {
	int sock_id;
	struct sockaddr_in client;
	socklen_t len;
} ListeningParams;

typedef struct {
	int connId;
} ResponseParams;


void* listening(void*);
void* response(void*);
char* readFile(FILE*);
char* buildHTML(FILE*);
int callback(void*, int, char**, char**);

char* html_tbl;

int main(int argc, char* argv[]) {

	printf("Init ...\n");
	int port = argc>1? atoi(argv[1]) : DEFAULT_PORT;

	struct sockaddr_in myself, client;
	myself.sin_family = AF_INET;
	inet_aton(LO, &myself.sin_addr);
	myself.sin_port = htons(port);
	for (int i=0; i<8; i++) myself.sin_zero[i] = 0;

	int sock_id = socket(AF_INET, SOCK_STREAM, 0);
	if ( sock_id <= 0 ) errore("socket()", -1);

	socklen_t len = sizeof(struct sockaddr);
	if ( bind(sock_id, (struct sockaddr*) &myself, len) == -1)
		errore("bind()", -2);

	printf("Server listening on %s:%d ...\n", LO, port);
	if ( listen(sock_id, 50)) 
		errore("listen()", -3); 

	ListeningParams params = { sock_id, client, len };
	pthread_t thread_id;
	if ( pthread_create(&thread_id, NULL, listening, (void*) &params))
		errore("pthread_create()", -4);

	printf("\n");
	char* cmd = inputStr();
	while ( strcmp(cmd, EXIT_CMD) ) {
		free(cmd);
		cmd = inputStr();
	}
	free(cmd);

	close(sock_id);
	printf("Server closed!\n");
	return 0;
}

void* listening(void* param) {
	ListeningParams* p = (ListeningParams*) param;
	int connId;

	while (1) {
		connId = accept(p->sock_id,
						(struct sockaddr*) &p->client,
						&p->len);
		if (connId == -1) 
			errore("accept()", -5);
		printf("Client has connected! ...\n");

		ResponseParams param = { connId };

		pthread_t thread_id;
		if ( pthread_create(&thread_id, NULL, response, (void*) &connId))
			errore("pthread_create()", -6);
	}
}


void* response(void* param) {
	ResponseParams* p = (ResponseParams*) param;
	int rc, connId = p->connId;

	char buffer[MAX_PACK + 1];
	rc = recv(connId,
				buffer,
				MAX_PACK,
				0);
	if (rc < 0) errore("recv()", -7);

	printf("Client is making a request ...\n");
	buffer[rc] = '\0';


	/* ============ LOOKING FOR FILE ============ */
	char* url = strdup(strstr(buffer, GET_URL_HEADER) + 3);
	*url = '.';
	*strstr(url, GET_URL_FOOTER) = '\0';
	printf("Now looks for file %s\n", url);
	fflush(stdout);

	FILE* fp = fopen(url, "r");
	
	char msg[MAX_PACK + strlen(CODE200) + 1];
	int len = 0;
	if (fp != NULL && strcmp(url, EMPTY_REQ)) {
		sprintf(msg, "%s%s", CODE200, buildHTML(fp));
	} else {
		printf("Oh no! File Not Found!\n");
		FILE* fpNotFound = fopen(URL_404, "r");
		if (fpNotFound != NULL)
			sprintf(msg, "%s%s", CODE404, readFile(fpNotFound));
		else sprintf(msg, "%s", CODE404);
	}
	free(url);
	printf("The response:\n%s\n", msg);

	len = strlen(msg);
	if ( send(connId, msg, len, 0) != strlen(msg) ) 
		errore("send()", -8);
	printf("Answer sent!\n");

	shutdown(connId, SHUT_RDWR);
	printf("Connection closed\n\n");
}

char* buildHTML(FILE* fp) {
	html_tbl = (char*) malloc(sizeof(char) * MAX_TBL);

	/* ============ READING FILE ============*/
	char* buffer = readFile(fp);


	/* ============ SEARCHING FOR TAG ============*/
	char* sqlQuery = (char*) malloc(sizeof(char) * (MAX_SQL_QUERY));
	sqlQuery = strdup(strstr(buffer, TAG_SQL_BEGIN) + 5); // trovo l'inizio query
	*strstr(sqlQuery, TAG_SQL_END) = '\0'; // delimito la query


	/* ============ QUERY ON DB ============*/
	sqlite3 *db;
	sqlite3_stmt *stmt;
	char* err_msg = 0;

	int rc = sqlite3_open(DB_NAME, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_free(err_msg);
		sqlite3_close(db);
		errore("sqlite3_open()", -9);
	}

	strcat(html_tbl, "<table>");
	rc = sqlite3_exec(db, sqlQuery, callback, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to select data\n");
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return "404 DB Not Found";
	}
	free(sqlQuery);
	sqlite3_close(db);
	strcat(html_tbl, "</table>");


	/* ============ BUILDING HTML RESPONSE ============*/
	char* html_res = (char*) malloc(sizeof(char) * (MAX_HTML_RES));
	html_res = strdup(buffer);

	*strstr(html_res, TAG_SQL_BEGIN) = '\0';
	strcat(html_res, html_tbl);
	free(html_tbl);

	char* end_of_html = strdup(strstr(buffer, TAG_SQL_END) + 3); 
	strcat(html_res, end_of_html);
	free(end_of_html);
	free(buffer);

	return html_res;  
}

int callback(void* NotUsed, int argc, char* argv[], char* azColName[]) {
	NotUsed = 0;

	/* ============ BUILDING HTML TABLE ROW ============*/
	strcat(html_tbl, "<tr>");
	for (int i=0; i<argc; i++) {
		strcat(html_tbl, "<td>");
		strcat(html_tbl, argv[i]);
		strcat(html_tbl, "</td>");
	}
	strcat(html_tbl, "</tr>");

	return 0;
}

char* readFile(FILE* fp) {
	int len = 0;
	char* buffer = (char*) malloc(sizeof(char) * (MAX_FILE + 1));

	while ( (*(buffer+len) = fgetc(fp)) != EOF )
		len++;
	*(buffer+len) = '\0';
	fclose(fp);

	return buffer;
}
