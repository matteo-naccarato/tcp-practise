#ifndef __UTILITIES_H
#define __UTILITIES_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_WORDS 20
#define MAX_STRING_LEN 20

void errore(char* s, int n);
char** split(char* s, char* seps);
char* readCSV (FILE* fp, char sep);
char* inputStr();

void errore(char* s, int n) {
	printf("ERROR in %s - %d - %s\n", s, errno, strerror(errno));
	printf("return code: %d\n", n);
	exit(n);
}

char** split(char* s, char* seps) {
	char* temp[MAX_WORDS+1];
	char* s1 = strdup(s);
	char* token = strtok(s1, seps);
	
	int i = 0;
	for(i=0; token != NULL ; i++) { 
   		temp[i] = strdup(token);
   		token = strtok(NULL, seps);
   	}
   	temp[i] = NULL;
   	free(s1);
   	
   	char** ret = (char**) malloc(sizeof(char*) * i);
   	for( i=0; *(ret+i) = temp[i]; i++)  { }
   	return ret;
}

char* readCSV (FILE* fp, char sep) {
	char temp[MAX_STRING_LEN +1];
	int i = 0;
	for(; 	((temp[i] = fgetc(fp)) != sep) &&
			(temp[i] != '\n') &&
			(temp[i] != EOF) ;
			i++) { }
	if (i==0) return NULL;
	temp[i] = '\0';
	return strdup(temp);
}

char* inputStr() {
	char temp[MAX_STRING_LEN +1];
	int i = 0;
	for(; (temp[i] = fgetc(stdin)) != '\n' ; i++) { }
	temp[i] = '\0';
	return strdup(temp);	
}

#endif