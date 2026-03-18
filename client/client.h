#define MAX_NAME 20 // Twitter OG username limit  
#define MAX_DATA 140 // Twitter OG character limit
#define MAX_USER_INPUT 200 // Random but at least greater than max data
#define MAX_MSG_LEN 168 // Max data + max name + 2*(int) for type and size

// Standardizing messages
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
    QUIT_CLIENT
} Client_action;
