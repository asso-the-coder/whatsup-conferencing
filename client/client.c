#include "client.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Prototypes
Client_action determine_action(char *action);

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
                //login_session(user_input);
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

Client_action determine_action(char *action){

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

/*
Login case
// Maximum five inputs in case of login
    sscanf(control_input, "%s %s %s %s %s", action, id, password, server_ip, server_port);
char action[MAX_USER_INPUT];
    char id[MAX_USER_INPUT];
    char password[MAX_USER_INPUT];
    char server_ip[MAX_USER_INPUT];
    char server_port[MAX_USER_INPUT];
*/