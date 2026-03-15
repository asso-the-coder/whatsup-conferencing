#define MAX_NAME 20 // Twitter OG username limit  
#define MAX_DATA 140 // Twitter OG character limit
#define MAX_CLIENTS 10 // Small circles last longer

// Standardizing messages
typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} Message;



