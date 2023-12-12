// https://youtu.be/dQyXuFWylm4
// sudo apt install libjson-c-dev
// gcc test.c -o test -lpthread -lsqlite3 -ljson-c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <json-c/json.h>
#include "includes/utilities.h"

#define LO "127.0.0.1"
#define DEFAULT_PORT 6060
#define DB_NAME "test.db"
#define ALL_QUERY "all"
#define MAX_PACK 1024*1024
#define MAX_QUERY 1024
#define MAX_JSON 1024
#define MAX_KEY 5
#define CODE200 "HTTP/1.1 200 OK\n\
\n"
#define CODE404 "HTTP/1.1 404 Not Found\n\
\n"
#define GET_URL_HEADER "GET "
#define GET_URL_FOOTER " HTTP"
#define PROMPT "$>"
#define EXIT_CMD "quit"

typedef struct {
	int id;
	char* name;
	int age;
} User;

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
char* queryDB(char*, char*);
int callback(void*, int, char**, char**);

User users[50];
int index_user = 0;


int main(int argc, char* argv[]) {

	printf("Init ...\n");
	int port = argc>1? atoi(argv[1]) : DEFAULT_PORT;

	struct sockaddr_in myself, client;
	myself.sin_family = AF_INET;
	inet_aton(LO, &myself.sin_addr);
	myself.sin_port = htons(port);
	for (int i=0; i<8; i++) myself.sin_zero[i] = 0;

	int sock_id = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_id < 0) errore("socket()", -1);

	socklen_t len = sizeof(struct sockaddr);
	if (bind(sock_id, (struct sockaddr*) &myself, len) == -1)
		errore("bind()", -2);

	printf("Server listening on %s:%d ...\n", LO, port);
	if (listen(sock_id, 50))
		errore("listening()", -3);

	ListeningParams params = { sock_id, client, len };
	pthread_t thread_id;
	if ( pthread_create(&thread_id, NULL, listening, (void*) &params) )
		errore("pthread_create()", -4);

	printf("%s\n", PROMPT);
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

void* listening(void* params) {
	ListeningParams* p = (ListeningParams*) params;

	int connId;
	while (1) {
		connId = accept(p->sock_id,
						(struct sockaddr*) &p->client,
						&p->len);
		if (connId == -1) 
			errore("accept()", -5);
		printf("Client has connected!\n");

		ResponseParams params = { connId };
		pthread_t thread_id;
		if ( pthread_create(&thread_id, NULL, response, (void*) &connId) )
			errore("pthread_create()", -6);
	}
}

void* response(void* params) {
	ResponseParams* p = (ResponseParams*) params;

	int rc, len, connId = p->connId;
	char buffer[MAX_PACK + 1];
	rc = recv(connId,
				buffer,
				MAX_PACK + 1,
				0);
	if (rc < 0)
		errore("recv()", -7);
	buffer[rc] = '\0';
	printf("Client is making a request ...\n");

	char* url = strdup(strstr(buffer, GET_URL_HEADER) + 5);
	*strstr(url, GET_URL_FOOTER) = '\0';

	char** data = (char**) malloc(sizeof(char**) * MAX_PACK/2);
	data = split(url, "?");
	char* tbl_name = data[0];
	char* query = data[1];

	printf("[tbl_name:%s] and [query:%s]\n", tbl_name, query);

	char res[MAX_PACK + 1];
	if (query != NULL) {
		char* json = queryDB(tbl_name, query);
		sprintf(res, "%s%s", CODE200, json);
	} else sprintf(res, "Without query");
	len = strlen(res);
	if ( send(connId, res, len, 0) != strlen(res) )
		errore("send()", -8);

	shutdown(connId, SHUT_RDWR);
	printf("Connection closed\n\n");
}

char* queryDB(char* tbl_name, char* query) {
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

	char* sqlQuery = (char*) malloc(sizeof(char*) * MAX_QUERY);
	if (strcmp(query, ALL_QUERY)) {
		sprintf(sqlQuery, "SELECT * FROM %s WHERE %s", tbl_name, query);
	} else {
		sprintf(sqlQuery, "SELECT * FROM %s", tbl_name);
	}
	rc = sqlite3_exec(db, sqlQuery, callback, 0, &err_msg);	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to select data");
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return "404 Not Found";
	}
	free(sqlQuery);
	sqlite3_close(db);

	// creating a json obj
	json_object *jobj = json_object_new_object();
	// creating users array
	json_object *jarray_users = json_object_new_array();

	for (int i=0; i<index_user; i++) {
		// creating user obj
		json_object *jobj_user = json_object_new_object();
		json_object *j_id = json_object_new_int(users[i].id);
		json_object *j_name = json_object_new_string(users[i].name);
		json_object *j_age = json_object_new_int(users[i].age);
		json_object_object_add(jobj_user, "id", j_id);
		json_object_object_add(jobj_user, "name", j_name);
		json_object_object_add(jobj_user, "age", j_age);


		json_object_array_add(jarray_users, jobj_user);
	}
	index_user = 0;

	// adding to json obj
	json_object_object_add(jobj, "Users", jarray_users);

	printf("%s\n", json_object_to_json_string(jobj));	

	return strdup(json_object_to_json_string(jobj));
}

int callback(void* NotUsed, int argc, char* argv[], char* azColName[]) {
	NotUsed = 0;

	User tmp = { atoi(argv[0]), strdup(argv[1]), atoi(argv[2]) };
	users[index_user] = tmp;
	index_user++;

	return 0;
}