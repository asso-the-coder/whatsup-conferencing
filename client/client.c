#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Prototypes
Client_action determine_action(char* action);
int login_session (char* login_string);
int send_message(int socket_fd, Message *msg);

int main(){

    // Prepare for user inputs 
    char user_input[MAX_USER_INPUT];
    char action_str[MAX_USER_INPUT];
    Client_action triggered_action;

    while(true){

        // Obtain and parse user input
        if (fgets(user_input, sizeof(user_input), stdin) == NULL){
            perror("User input");
            return -1;
        }
        sscanf(user_input, "%s", action_str);
        triggered_action = determine_action(action_str);
        
        switch(triggered_action){
            case LOGIN_SESSION:
                if (login_session(user_input) < 0){
                    printf("Login failed. Please try again.\n");
                    continue;
                };
                break;
            case LOGOUT_SESSION:

                break;
            case JOIN_SESSION:

                break;
            case LEAVE_SESSION:

                break;
            case CREATE_SESSION:

                break;
            case LIST_SESSIONS:

                break;
            case QUIT_CLIENT:

                break;
            
            default:
                printf("Please enter a valid command.\n");
                continue;
                break;
        }
    }
    
    return 0;
}

Client_action determine_action(char* action){

    if (strcmp(action, "/login") == 0){
        return LOGIN_SESSION;
    } else if (strcmp(action, "/logout") == 0){
        return LOGOUT_SESSION;
    } else if (strcmp(action, "/joinsession") == 0){
        return JOIN_SESSION;
    } else if (strcmp(action, "/leavesession") == 0){
        return LEAVE_SESSION;
    } else if (strcmp(action, "/createsession") == 0){
        return CREATE_SESSION;
    } else if (strcmp(action, "/list") == 0){
        return LIST_SESSIONS;
    } else if (strcmp(action, "/quit") == 0){
        return QUIT_CLIENT;
    } else {
        return -1;
    }

    return -1;

}

int login_session(char* login_string){

    // Parse login string
    char action[MAX_USER_INPUT];
    unsigned char id[MAX_USER_INPUT];
    unsigned char password[MAX_USER_INPUT];
    char server_ip[MAX_USER_INPUT];
    char server_port[MAX_USER_INPUT];
    sscanf(login_string, "%s %s %s %s %s", action, id, password, server_ip, server_port);
    
    // Create TCP comms socket file descriptor
    int comms_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (comms_socket < 0){
        perror("Client-side comms socket");
        return -1;
    }

    // Create and populate address object for IP + port
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET, 
        .sin_port = htons(atoi(server_port))
    };
    if (inet_pton(AF_INET, server_ip, &(server_addr.sin_addr)) < 1) {
        perror("Client-side comms socket");
        return -1;
    }

    // Connect to server
    if (connect(comms_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Client-side comms socket");
        close(comms_socket);
        return -1;
    }
    
    // Set up login message (strictly a TEXT conferencing app so string functions are fine)
    Message login_msg;
    login_msg.type = LOGIN;
    login_msg.size = strlen((char *) password); // Casting so strcpy cross-compatibility (overkill)
    strcpy((char *) login_msg.source, (char *) id);
    strcpy((char *) login_msg.data, (char *) password);

    // Send login message to server
    send_message(comms_socket, &login_msg);

    return comms_socket;
}


int send_message(int socket_fd, Message *msg){
    /*
    Generic client-side message formatter and sender
    */

    char buffer_tx[MAX_MSG_LEN];

    // Format message for sending (using colons specifically is arbitrary)
    int msg_len = snprintf(buffer_tx, MAX_MSG_LEN, "%u:%u:%s:%s:", \
    msg->type, msg->size, msg->source, msg->data);

    if (msg_len < 0){
        perror("TX Message");
        return -1;
    }

    // Send
    int bytes_send = send(socket_fd, buffer_tx, msg_len, 0);
    if (bytes_send < 0) {
        perror("Client-side comms socket");
        close(socket_fd);
        return -1;
    }

    return bytes_send;
}

// we want to have a thread always receiving messages from server (interrupt style?)