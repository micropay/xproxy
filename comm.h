#ifndef __COMM_H_
#define __COMM_H_

#define MAX_BUFFER_LENGTH  65535

#define DEBUG_LOG( args... ) \
	do{ \
		char msg[MAX_BUFFER_LENGTH] = {0}; \
		snprintf(msg + strlen(msg), MAX_BUFFER_LENGTH - strlen(msg) -1, " [FILE:%s-LINE:%u-FUNC:%s] ", __FILE__, __LINE__, __func__); \
		snprintf(msg + strlen(msg), MAX_BUFFER_LENGTH - strlen(msg) -1, args); \
		printf("%s\n", msg); \
	}while(0)

#endif