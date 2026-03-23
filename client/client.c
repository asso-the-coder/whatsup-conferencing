#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Prototypes
Client_action determine_action(char *action);
int login_session(char *login_string);
int logout_session(int socket_fd, const char *client_id);
int join_session(int socket_fd, const char *client_id, char *user_input);
int leave_session(int socket_fd, const char *client_id, const char *session_id);
int create_session(int socket_fd, const char *client_id, char *user_input);
int list_sessions(int socket_fd, const char *client_id);
int send_text_message(int socket_fd, const char *client_id, const char *session_id, char *user_input);

int send_message(int socket_fd, Message *msg);
int recv_message(int socket_fd, Message *msg);
int send_all(int socket_fd, const void *buffer, size_t length);
int recv_all(int socket_fd, void *buffer, size_t length);

void *receive_messages(void *arg);
void trim_newline(char *str);

int switch_session_local(char *user_input, char current_session[MAX_NAME],
                         char joined_sessions[MAX_SESSIONS][MAX_NAME], int joined_count);
int session_exists_local(const char *session_id,
                         char joined_sessions[MAX_SESSIONS][MAX_NAME], int joined_count);
void remove_local_session(const char *session_id,
                          char joined_sessions[MAX_SESSIONS][MAX_NAME], int *joined_count);

typedef struct {
    int socket_fd;
    bool *logged_in;
    char *current_session;
    char *client_id;
    char (*joined_sessions)[MAX_NAME];
    int *joined_count;
} Client_thread_args;

int main() {

    char user_input[MAX_USER_INPUT];
    char action_str[MAX_USER_INPUT];
    Client_action triggered_action;

    int comms_socket = -1;
    bool logged_in = false;
    char current_session[MAX_NAME] = {0};
    char client_id[MAX_NAME] = {0};

    char joined_sessions[MAX_SESSIONS][MAX_NAME];
    int joined_count = 0;
    memset(joined_sessions, 0, sizeof(joined_sessions));

    pthread_t rx_thread;
    bool rx_thread_started = false;
    Client_thread_args thread_args;

    while (true) {

        if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
            perror("User input");
            return -1;
        }

        trim_newline(user_input);

        if (strlen(user_input) == 0) {
            continue;
        }

        if (user_input[0] == '/') {
            sscanf(user_input, "%s", action_str);
            triggered_action = determine_action(action_str);
        } else {
            triggered_action = SEND_TEXT;
        }

        switch (triggered_action) {
            case LOGIN_SESSION:
                if (logged_in) {
                    printf("Already logged in.\n");
                    continue;
                }

                comms_socket = login_session(user_input);
                if (comms_socket < 0) {
                    printf("Login failed. Please try again.\n");
                    continue;
                }

                {
                    Message response;
                    if (recv_message(comms_socket, &response) < 0) {
                        printf("Login failed. Server did not respond properly.\n");
                        close(comms_socket);
                        comms_socket = -1;
                        continue;
                    }

                    if (response.type == LO_ACK) {
                        sscanf(user_input, "%*s %19s", client_id);
                        logged_in = true;
                        current_session[0] = '\0';
                        joined_count = 0;
                        memset(joined_sessions, 0, sizeof(joined_sessions));

                        thread_args.socket_fd = comms_socket;
                        thread_args.logged_in = &logged_in;
                        thread_args.current_session = current_session;
                        thread_args.client_id = client_id;
                        thread_args.joined_sessions = joined_sessions;
                        thread_args.joined_count = &joined_count;

                        if (pthread_create(&rx_thread, NULL, receive_messages, &thread_args) != 0) {
                            perror("pthread_create");
                            close(comms_socket);
                            comms_socket = -1;
                            logged_in = false;
                            continue;
                        }
                        rx_thread_started = true;

                        printf("Login successful.\n");
                    } else if (response.type == LO_NAK) {
                        printf("Login denied: %s\n", response.data);
                        close(comms_socket);
                        comms_socket = -1;
                    } else {
                        printf("Unexpected server response during login.\n");
                        close(comms_socket);
                        comms_socket = -1;
                    }
                }
                break;

            case LOGOUT_SESSION:
                if (!logged_in) {
                    printf("You are not logged in.\n");
                    continue;
                }

                logout_session(comms_socket, client_id);

                if (rx_thread_started) {
                    pthread_join(rx_thread, NULL);
                    rx_thread_started = false;
                }

                close(comms_socket);
                comms_socket = -1;
                logged_in = false;
                current_session[0] = '\0';
                client_id[0] = '\0';
                joined_count = 0;
                memset(joined_sessions, 0, sizeof(joined_sessions));

                printf("Logged out.\n");
                break;

            case JOIN_SESSION:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                join_session(comms_socket, client_id, user_input);
                break;

            case LEAVE_SESSION:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                if (strlen(current_session) == 0) {
                    printf("No active session selected.\n");
                    continue;
                }
                leave_session(comms_socket, client_id, current_session);
                break;

            case CREATE_SESSION:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                create_session(comms_socket, client_id, user_input);
                break;

            case LIST_SESSIONS:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                list_sessions(comms_socket, client_id);
                break;

            case SWITCH_SESSION:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                switch_session_local(user_input, current_session, joined_sessions, joined_count);
                break;

            case QUIT_CLIENT:
                if (logged_in) {
                    logout_session(comms_socket, client_id);

                    if (rx_thread_started) {
                        pthread_join(rx_thread, NULL);
                        rx_thread_started = false;
                    }

                    close(comms_socket);
                }
                return 0;

            case SEND_TEXT:
                if (!logged_in) {
                    printf("Please log in first.\n");
                    continue;
                }
                if (strlen(current_session) == 0) {
                    printf("Select a session first with /switchsession or join/create one.\n");
                    continue;
                }
                send_text_message(comms_socket, client_id, current_session, user_input);
                break;

            default:
                printf("Please enter a valid command.\n");
                break;
        }
    }

    return 0;
}

Client_action determine_action(char *action) {

    if (strcmp(action, "/login") == 0) {
        return LOGIN_SESSION;
    } else if (strcmp(action, "/logout") == 0) {
        return LOGOUT_SESSION;
    } else if (strcmp(action, "/joinsession") == 0) {
        return JOIN_SESSION;
    } else if (strcmp(action, "/leavesession") == 0) {
        return LEAVE_SESSION;
    } else if (strcmp(action, "/createsession") == 0) {
        return CREATE_SESSION;
    } else if (strcmp(action, "/list") == 0) {
        return LIST_SESSIONS;
    } else if (strcmp(action, "/quit") == 0) {
        return QUIT_CLIENT;
    } else if (strcmp(action, "/switchsession") == 0) {
        return SWITCH_SESSION;
    } else {
        return INVALID_ACTION;
    }
}

int login_session(char *login_string) {

    char action[MAX_USER_INPUT];
    unsigned char id[MAX_USER_INPUT];
    unsigned char password[MAX_USER_INPUT];
    char server_ip[MAX_USER_INPUT];
    char server_port[MAX_USER_INPUT];

    if (sscanf(login_string, "%s %s %s %s %s", action, id, password, server_ip, server_port) != 5) {
        printf("Usage: /login <client ID> <password> <server-IP> <server-port>\n");
        return -1;
    }

    int comms_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (comms_socket < 0) {
        perror("Client-side comms socket");
        return -1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(server_port))
    };

    if (inet_pton(AF_INET, server_ip, &(server_addr.sin_addr)) < 1) {
        perror("Client-side inet_pton");
        close(comms_socket);
        return -1;
    }

    if (connect(comms_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Client-side connect");
        close(comms_socket);
        return -1;
    }

    Message login_msg;
    memset(&login_msg, 0, sizeof(login_msg));
    login_msg.type = LOGIN;
    login_msg.size = strlen((char *)password);
    strncpy((char *)login_msg.source, (char *)id, MAX_NAME - 1);
    strncpy((char *)login_msg.data, (char *)password, MAX_DATA - 1);

    if (send_message(comms_socket, &login_msg) < 0) {
        close(comms_socket);
        return -1;
    }

    return comms_socket;
}

int logout_session(int socket_fd, const char *client_id) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = EXIT;
    msg.size = 0;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    return send_message(socket_fd, &msg);
}

int join_session(int socket_fd, const char *client_id, char *user_input) {
    char action[MAX_USER_INPUT];
    char session_id[MAX_NAME];

    if (sscanf(user_input, "%s %19s", action, session_id) != 2) {
        printf("Usage: /joinsession <session ID>\n");
        return -1;
    }

    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = JOIN;
    msg.size = strlen(session_id);
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    strncpy((char *)msg.data, session_id, MAX_DATA - 1);

    return send_message(socket_fd, &msg);
}

int leave_session(int socket_fd, const char *client_id, const char *session_id) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = LEAVE_SESS;
    msg.size = strlen(session_id);
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    strncpy((char *)msg.data, session_id, MAX_DATA - 1);
    return send_message(socket_fd, &msg);
}

int create_session(int socket_fd, const char *client_id, char *user_input) {
    char action[MAX_USER_INPUT];
    char session_id[MAX_NAME];

    if (sscanf(user_input, "%s %19s", action, session_id) != 2) {
        printf("Usage: /createsession <session ID>\n");
        return -1;
    }

    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = NEW_SESS;
    msg.size = strlen(session_id);
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    strncpy((char *)msg.data, session_id, MAX_DATA - 1);

    return send_message(socket_fd, &msg);
}

int list_sessions(int socket_fd, const char *client_id) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = QUERY;
    msg.size = 0;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    return send_message(socket_fd, &msg);
}

int send_text_message(int socket_fd, const char *client_id, const char *session_id, char *user_input) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MESSAGE;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);

    snprintf((char *)msg.data, MAX_DATA, "%s|%s", session_id, user_input);
    msg.size = strlen((char *)msg.data);

    return send_message(socket_fd, &msg);
}

void *receive_messages(void *arg) {
    Client_thread_args *args = (Client_thread_args *)arg;
    Message msg;

    while (*(args->logged_in)) {
        if (recv_message(args->socket_fd, &msg) < 0) {
            printf("\nDisconnected from server.\n");
            *(args->logged_in) = false;
            args->current_session[0] = '\0';
            *(args->joined_count) = 0;
            return NULL;
        }

        switch (msg.type) {
            case JN_ACK:
                if (!session_exists_local((char *)msg.data, args->joined_sessions, *(args->joined_count))) {
                    if (*(args->joined_count) < MAX_SESSIONS) {
                        strncpy(args->joined_sessions[*(args->joined_count)], (char *)msg.data, MAX_NAME - 1);
                        args->joined_sessions[*(args->joined_count)][MAX_NAME - 1] = '\0';
                        (*(args->joined_count))++;
                    }
                }
                strncpy(args->current_session, (char *)msg.data, MAX_NAME - 1);
                args->current_session[MAX_NAME - 1] = '\0';
                printf("\nJoined session: %s\n", args->current_session);
                break;

            case NS_ACK:
                if (!session_exists_local((char *)msg.data, args->joined_sessions, *(args->joined_count))) {
                    if (*(args->joined_count) < MAX_SESSIONS) {
                        strncpy(args->joined_sessions[*(args->joined_count)], (char *)msg.data, MAX_NAME - 1);
                        args->joined_sessions[*(args->joined_count)][MAX_NAME - 1] = '\0';
                        (*(args->joined_count))++;
                    }
                }
                strncpy(args->current_session, (char *)msg.data, MAX_NAME - 1);
                args->current_session[MAX_NAME - 1] = '\0';
                printf("\nCreated and joined session: %s\n", args->current_session);
                break;

            case JN_NAK:
                printf("\nRequest failed: %s\n", msg.data);
                break;

            case QU_ACK:
                printf("\n%s\n", msg.data);
                break;

            case MESSAGE: {
                char session_id[MAX_NAME] = {0};
                char text[MAX_DATA] = {0};

                if (sscanf((char *)msg.data, "%19[^|]|%139[^\n]", session_id, text) == 2) {
                    printf("\n[%s][%s]: %s\n", session_id, msg.source, text);
                } else {
                    printf("\n[%s]: %s\n", msg.source, msg.data);
                }
                break;
            }

            case EXIT:
                printf("\n%s\n", msg.data);
                *(args->logged_in) = false;
                args->current_session[0] = '\0';
                *(args->joined_count) = 0;
                return NULL;

            case LO_NAK:
                printf("\nServer error: %s\n", msg.data);
                break;

            default:
                printf("\nReceived packet type %u\n", msg.type);
                break;
        }

        fflush(stdout);
    }

    return NULL;
}

int send_message(int socket_fd, Message *msg) {
    return send_all(socket_fd, msg, sizeof(Message));
}

int recv_message(int socket_fd, Message *msg) {
    return recv_all(socket_fd, msg, sizeof(Message));
}

int send_all(int socket_fd, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const char *buf = (const char *)buffer;

    while (total_sent < length) {
        ssize_t bytes_sent = send(socket_fd, buf + total_sent, length - total_sent, 0);
        if (bytes_sent <= 0) {
            return -1;
        }
        total_sent += bytes_sent;
    }

    return (int)total_sent;
}

int recv_all(int socket_fd, void *buffer, size_t length) {
    size_t total_received = 0;
    char *buf = (char *)buffer;

    while (total_received < length) {
        ssize_t bytes_received = recv(socket_fd, buf + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return -1;
        }
        total_received += bytes_received;
    }

    return (int)total_received;
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

int switch_session_local(char *user_input, char current_session[MAX_NAME],
                         char joined_sessions[MAX_SESSIONS][MAX_NAME], int joined_count) {
    char action[MAX_USER_INPUT];
    char session_id[MAX_NAME];

    if (sscanf(user_input, "%s %19s", action, session_id) != 2) {
        printf("Usage: /switchsession <session ID>\n");
        return -1;
    }

    if (!session_exists_local(session_id, joined_sessions, joined_count)) {
        printf("You have not joined session %s.\n", session_id);
        return -1;
    }

    strncpy(current_session, session_id, MAX_NAME - 1);
    current_session[MAX_NAME - 1] = '\0';
    printf("Active session set to: %s\n", current_session);
    return 0;
}

int session_exists_local(const char *session_id,
                         char joined_sessions[MAX_SESSIONS][MAX_NAME], int joined_count) {
    for (int i = 0; i < joined_count; i++) {
        if (strcmp(joined_sessions[i], session_id) == 0) {
            return 1;
        }
    }
    return 0;
}

void remove_local_session(const char *session_id,
                          char joined_sessions[MAX_SESSIONS][MAX_NAME], int *joined_count) {
    for (int i = 0; i < *joined_count; i++) {
        if (strcmp(joined_sessions[i], session_id) == 0) {
            for (int j = i; j < *joined_count - 1; j++) {
                strcpy(joined_sessions[j], joined_sessions[j + 1]);
            }
            joined_sessions[*joined_count - 1][0] = '\0';
            (*joined_count)--;
            return;
        }
    }
}