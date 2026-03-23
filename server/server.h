#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <time.h>

#define MAX_NAME 20
#define MAX_DATA 140
#define MAX_CLIENTS 32
#define MAX_SESSIONS 32
#define INACTIVITY_LIMIT 60

typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} Message;

typedef enum {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
} Message_type;

typedef struct {
    char id[MAX_NAME];
    char password[MAX_NAME];
} User_record;

typedef struct {
    int active;
    int socket_fd;
    char id[MAX_NAME];
    char sessions[MAX_SESSIONS][MAX_NAME];
    int session_count;
    char ip[INET_ADDRSTRLEN];
    int port;
    time_t last_activity;
} Client_record;

typedef struct {
    int active;
    char session_id[MAX_NAME];
} Session_record;

#endif