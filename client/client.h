#ifndef CLIENT_H
#define CLIENT_H

#define MAX_NAME 20
#define MAX_DATA 140
#define MAX_USER_INPUT 200
#define MAX_SESSIONS 32

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

typedef enum {
    LOGIN_SESSION,
    LOGOUT_SESSION,
    JOIN_SESSION,
    LEAVE_SESSION,
    CREATE_SESSION,
    LIST_SESSIONS,
    QUIT_CLIENT,
    SWITCH_SESSION,
    SEND_TEXT,
    INVALID_ACTION
} Client_action;

#endif