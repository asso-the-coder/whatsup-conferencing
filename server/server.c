#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Prototypes
int send_all(int socket_fd, const void *buffer, size_t length);
int recv_all(int socket_fd, void *buffer, size_t length);
int send_message(int socket_fd, Message *msg);
int recv_message(int socket_fd, Message *msg);

int find_user(const char *id);
bool password_matches(const char *id, const char *password);
int find_connected_client_by_id(Client_record clients[], const char *id);
int find_connected_client_by_fd(Client_record clients[], int fd);
int find_session(Session_record sessions[], const char *session_id);
int create_session_record(Session_record sessions[], const char *session_id);
void remove_session_if_empty(Session_record sessions[], Client_record clients[], const char *session_id);

int client_in_session(Client_record clients[], int client_idx, const char *session_id);
int add_client_to_session(Client_record clients[], int client_idx, const char *session_id);
int remove_client_from_session(Client_record clients[], int client_idx, const char *session_id);
void update_client_activity(Client_record clients[], int fd);
void check_inactive_clients(Client_record clients[], Session_record sessions[],
                            fd_set *master_fds, int *fd_max);

void handle_login(Client_record clients[], int fd, struct sockaddr_in *client_addr, Message *msg);
void handle_exit(Client_record clients[], Session_record sessions[], int fd, Message *msg);
void handle_join(Client_record clients[], Session_record sessions[], int fd, Message *msg);
void handle_leave(Client_record clients[], Session_record sessions[], int fd, Message *msg);
void handle_new_session(Client_record clients[], Session_record sessions[], int fd, Message *msg);
void handle_message(Client_record clients[], int fd, Message *msg);
void handle_query(Client_record clients[], Session_record sessions[], int fd, Message *msg);

static User_record user_db[] = {
    {"amirhosein", "1234"},
    {"nick", "1234"},
    {"asser", "1234"},
    {"ali", "1234"},
    {"shahrokh", "1234"}
};

static const int user_db_size = sizeof(user_db) / sizeof(user_db[0]);

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port_number>\n", argv[0]);
        return 1;
    }

    int server_port = atoi(argv[1]);

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        perror("Listen socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_socket);
        return -1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(server_port)
    };

    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind");
        close(listen_socket);
        return -1;
    }

    if (listen(listen_socket, MAX_CLIENTS) < 0) {
        perror("Listen");
        close(listen_socket);
        return -1;
    }

    printf("WhatsUp server launched! Listening on port %d...\n", server_port);

    Client_record clients[MAX_CLIENTS];
    Session_record sessions[MAX_SESSIONS];
    memset(clients, 0, sizeof(clients));
    memset(sessions, 0, sizeof(sessions));

    fd_set master_fds;
    fd_set read_fds;
    FD_ZERO(&master_fds);
    FD_SET(listen_socket, &master_fds);

    int fd_max = listen_socket;

    while (true) {
        read_fds = master_fds;

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(fd_max + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0) {
            perror("select");
            break;
        }

        if (ready == 0) {
            check_inactive_clients(clients, sessions, &master_fds, &fd_max);
            continue;
        }

        for (int fd = 0; fd <= fd_max; fd++) {
            if (!FD_ISSET(fd, &read_fds)) {
                continue;
            }

            if (fd == listen_socket) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int comms_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &client_len);
                if (comms_socket < 0) {
                    perror("accept");
                    continue;
                }

                FD_SET(comms_socket, &master_fds);
                if (comms_socket > fd_max) {
                    fd_max = comms_socket;
                }

                printf("Incoming connection accepted on fd %d.\n", comms_socket);
            } else {
                Message msg;

                if (recv_message(fd, &msg) < 0) {
                    int client_index = find_connected_client_by_fd(clients, fd);
                    if (client_index >= 0) {
                        printf("Client %s disconnected unexpectedly.\n", clients[client_index].id);

                        char old_sessions[MAX_SESSIONS][MAX_NAME];
                        int old_count = clients[client_index].session_count;
                        memset(old_sessions, 0, sizeof(old_sessions));

                        for (int j = 0; j < old_count; j++) {
                            strncpy(old_sessions[j], clients[client_index].sessions[j], MAX_NAME - 1);
                        }

                        clients[client_index].active = 0;
                        clients[client_index].session_count = 0;
                        memset(clients[client_index].sessions, 0, sizeof(clients[client_index].sessions));

                        close(fd);
                        FD_CLR(fd, &master_fds);

                        for (int j = 0; j < old_count; j++) {
                            remove_session_if_empty(sessions, clients, old_sessions[j]);
                        }

                        while (fd_max > 0 && !FD_ISSET(fd_max, &master_fds)) {
                            fd_max--;
                        }
                    } else {
                        close(fd);
                        FD_CLR(fd, &master_fds);
                    }
                    continue;
                }

                update_client_activity(clients, fd);

                switch (msg.type) {
                    case LOGIN: {
                        struct sockaddr_in peer_addr;
                        socklen_t peer_len = sizeof(peer_addr);
                        getpeername(fd, (struct sockaddr *)&peer_addr, &peer_len);
                        handle_login(clients, fd, &peer_addr, &msg);
                        break;
                    }
                    case EXIT:
                        handle_exit(clients, sessions, fd, &msg);
                        close(fd);
                        FD_CLR(fd, &master_fds);
                        while (fd_max > 0 && !FD_ISSET(fd_max, &master_fds)) {
                            fd_max--;
                        }
                        break;
                    case JOIN:
                        handle_join(clients, sessions, fd, &msg);
                        break;
                    case LEAVE_SESS:
                        handle_leave(clients, sessions, fd, &msg);
                        break;
                    case NEW_SESS:
                        handle_new_session(clients, sessions, fd, &msg);
                        break;
                    case MESSAGE:
                        handle_message(clients, fd, &msg);
                        break;
                    case QUERY:
                        handle_query(clients, sessions, fd, &msg);
                        break;
                    default:
                        printf("Unknown packet type %u received.\n", msg.type);
                        break;
                }
            }
        }

        check_inactive_clients(clients, sessions, &master_fds, &fd_max);
    }

    close(listen_socket);
    return 0;
}

void handle_login(Client_record clients[], int fd, struct sockaddr_in *client_addr, Message *msg) {
    Message response;
    memset(&response, 0, sizeof(response));

    if (find_user((char *)msg->source) < 0) {
        response.type = LO_NAK;
        strncpy((char *)response.data, "Unknown client ID", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (!password_matches((char *)msg->source, (char *)msg->data)) {
        response.type = LO_NAK;
        strncpy((char *)response.data, "Incorrect password", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (find_connected_client_by_id(clients, (char *)msg->source) >= 0) {
        response.type = LO_NAK;
        strncpy((char *)response.data, "Client ID already logged in", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].active = 1;
            clients[i].socket_fd = fd;
            strncpy(clients[i].id, (char *)msg->source, MAX_NAME - 1);
            clients[i].session_count = 0;
            memset(clients[i].sessions, 0, sizeof(clients[i].sessions));
            inet_ntop(AF_INET, &(client_addr->sin_addr), clients[i].ip, sizeof(clients[i].ip));
            clients[i].port = ntohs(client_addr->sin_port);
            clients[i].last_activity = time(NULL);

            response.type = LO_ACK;
            response.size = 0;
            send_message(fd, &response);

            printf("Client %s logged in on fd %d.\n", clients[i].id, fd);
            return;
        }
    }

    response.type = LO_NAK;
    strncpy((char *)response.data, "Server client table full", MAX_DATA - 1);
    response.size = strlen((char *)response.data);
    send_message(fd, &response);
}

void handle_exit(Client_record clients[], Session_record sessions[], int fd, Message *msg) {
    (void)msg;

    int idx = find_connected_client_by_fd(clients, fd);
    if (idx < 0) {
        return;
    }

    char old_sessions[MAX_SESSIONS][MAX_NAME];
    int old_count = clients[idx].session_count;
    memset(old_sessions, 0, sizeof(old_sessions));

    for (int j = 0; j < old_count; j++) {
        strncpy(old_sessions[j], clients[idx].sessions[j], MAX_NAME - 1);
    }

    printf("Client %s logged out.\n", clients[idx].id);

    clients[idx].active = 0;
    clients[idx].session_count = 0;
    memset(clients[idx].sessions, 0, sizeof(clients[idx].sessions));

    for (int j = 0; j < old_count; j++) {
        remove_session_if_empty(sessions, clients, old_sessions[j]);
    }
}

void handle_join(Client_record clients[], Session_record sessions[], int fd, Message *msg) {
    Message response;
    memset(&response, 0, sizeof(response));

    int client_idx = find_connected_client_by_fd(clients, fd);
    if (client_idx < 0) {
        return;
    }

    if (find_session(sessions, (char *)msg->data) < 0) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Session does not exist", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (client_in_session(clients, client_idx, (char *)msg->data)) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Already joined that session", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (add_client_to_session(clients, client_idx, (char *)msg->data) < 0) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Session list full for client", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    response.type = JN_ACK;
    strncpy((char *)response.data, (char *)msg->data, MAX_DATA - 1);
    response.size = strlen((char *)response.data);
    send_message(fd, &response);

    printf("Client %s joined session %s.\n", clients[client_idx].id, (char *)msg->data);
}

void handle_leave(Client_record clients[], Session_record sessions[], int fd, Message *msg) {
    int client_idx = find_connected_client_by_fd(clients, fd);
    if (client_idx < 0) {
        return;
    }

    if (!client_in_session(clients, client_idx, (char *)msg->data)) {
        return;
    }

    remove_client_from_session(clients, client_idx, (char *)msg->data);

    printf("Client %s left session %s.\n", clients[client_idx].id, (char *)msg->data);

    remove_session_if_empty(sessions, clients, (char *)msg->data);
}

void handle_new_session(Client_record clients[], Session_record sessions[], int fd, Message *msg) {
    Message response;
    memset(&response, 0, sizeof(response));

    int client_idx = find_connected_client_by_fd(clients, fd);
    if (client_idx < 0) {
        return;
    }

    if (find_session(sessions, (char *)msg->data) >= 0) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Session already exists", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (create_session_record(sessions, (char *)msg->data) < 0) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Server session table full", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    if (add_client_to_session(clients, client_idx, (char *)msg->data) < 0) {
        response.type = JN_NAK;
        strncpy((char *)response.data, "Client session membership full", MAX_DATA - 1);
        response.size = strlen((char *)response.data);
        send_message(fd, &response);
        return;
    }

    response.type = NS_ACK;
    strncpy((char *)response.data, (char *)msg->data, MAX_DATA - 1);
    response.size = strlen((char *)response.data);
    send_message(fd, &response);

    printf("Client %s created session %s.\n", clients[client_idx].id, (char *)msg->data);
}

void handle_message(Client_record clients[], int fd, Message *msg) {
    int sender_idx = find_connected_client_by_fd(clients, fd);
    if (sender_idx < 0) {
        return;
    }

    char session_id[MAX_NAME] = {0};
    char text[MAX_DATA] = {0};

    if (sscanf((char *)msg->data, "%19[^|]|%139[^\n]", session_id, text) != 2) {
        return;
    }

    if (!client_in_session(clients, sender_idx, session_id)) {
        return;
    }

    Message forward_msg;
    memset(&forward_msg, 0, sizeof(forward_msg));
    forward_msg.type = MESSAGE;
    strncpy((char *)forward_msg.source, clients[sender_idx].id, MAX_NAME - 1);
    snprintf((char *)forward_msg.data, MAX_DATA, "%s|%s", session_id, text);
    forward_msg.size = strlen((char *)forward_msg.data);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            continue;
        }

        if (client_in_session(clients, i, session_id)) {
            send_message(clients[i].socket_fd, &forward_msg);
        }
    }
}

void handle_query(Client_record clients[], Session_record sessions[], int fd, Message *msg) {
    (void)msg;

    Message response;
    memset(&response, 0, sizeof(response));
    response.type = QU_ACK;

    char list_buffer[MAX_DATA];
    memset(list_buffer, 0, sizeof(list_buffer));

    strncat(list_buffer, "Online users:\n", sizeof(list_buffer) - strlen(list_buffer) - 1);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            strncat(list_buffer, clients[i].id, sizeof(list_buffer) - strlen(list_buffer) - 1);
            strncat(list_buffer, " [", sizeof(list_buffer) - strlen(list_buffer) - 1);

            for (int j = 0; j < clients[i].session_count; j++) {
                strncat(list_buffer, clients[i].sessions[j], sizeof(list_buffer) - strlen(list_buffer) - 1);
                if (j != clients[i].session_count - 1) {
                    strncat(list_buffer, ",", sizeof(list_buffer) - strlen(list_buffer) - 1);
                }
            }

            strncat(list_buffer, "]\n", sizeof(list_buffer) - strlen(list_buffer) - 1);
        }
    }

    strncat(list_buffer, "Sessions:\n", sizeof(list_buffer) - strlen(list_buffer) - 1);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            strncat(list_buffer, sessions[i].session_id, sizeof(list_buffer) - strlen(list_buffer) - 1);
            strncat(list_buffer, "\n", sizeof(list_buffer) - strlen(list_buffer) - 1);
        }
    }

    strncpy((char *)response.data, list_buffer, MAX_DATA - 1);
    response.size = strlen((char *)response.data);
    send_message(fd, &response);
}

int find_user(const char *id) {
    for (int i = 0; i < user_db_size; i++) {
        if (strcmp(user_db[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

bool password_matches(const char *id, const char *password) {
    int idx = find_user(id);
    if (idx < 0) {
        return false;
    }
    return strcmp(user_db[idx].password, password) == 0;
}

int find_connected_client_by_id(Client_record clients[], const char *id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int find_connected_client_by_fd(Client_record clients[], int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket_fd == fd) {
            return i;
        }
    }
    return -1;
}

int find_session(Session_record sessions[], const char *session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].session_id, session_id) == 0) {
            return i;
        }
    }
    return -1;
}

int create_session_record(Session_record sessions[], const char *session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            sessions[i].active = 1;
            strncpy(sessions[i].session_id, session_id, MAX_NAME - 1);
            sessions[i].session_id[MAX_NAME - 1] = '\0';
            return i;
        }
    }
    return -1;
}

void remove_session_if_empty(Session_record sessions[], Client_record clients[], const char *session_id) {
    if (strlen(session_id) == 0) {
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            continue;
        }

        for (int j = 0; j < clients[i].session_count; j++) {
            if (strcmp(clients[i].sessions[j], session_id) == 0) {
                return;
            }
        }
    }

    int session_idx = find_session(sessions, session_id);
    if (session_idx >= 0) {
        sessions[session_idx].active = 0;
        sessions[session_idx].session_id[0] = '\0';
        printf("Session %s deleted because it became empty.\n", session_id);
    }
}

int client_in_session(Client_record clients[], int client_idx, const char *session_id) {
    for (int i = 0; i < clients[client_idx].session_count; i++) {
        if (strcmp(clients[client_idx].sessions[i], session_id) == 0) {
            return 1;
        }
    }
    return 0;
}

int add_client_to_session(Client_record clients[], int client_idx, const char *session_id) {
    if (clients[client_idx].session_count >= MAX_SESSIONS) {
        return -1;
    }

    strncpy(clients[client_idx].sessions[clients[client_idx].session_count], session_id, MAX_NAME - 1);
    clients[client_idx].sessions[clients[client_idx].session_count][MAX_NAME - 1] = '\0';
    clients[client_idx].session_count++;
    return 0;
}

int remove_client_from_session(Client_record clients[], int client_idx, const char *session_id) {
    for (int i = 0; i < clients[client_idx].session_count; i++) {
        if (strcmp(clients[client_idx].sessions[i], session_id) == 0) {
            for (int j = i; j < clients[client_idx].session_count - 1; j++) {
                strcpy(clients[client_idx].sessions[j], clients[client_idx].sessions[j + 1]);
            }
            clients[client_idx].sessions[clients[client_idx].session_count - 1][0] = '\0';
            clients[client_idx].session_count--;
            return 0;
        }
    }
    return -1;
}

void update_client_activity(Client_record clients[], int fd) {
    int idx = find_connected_client_by_fd(clients, fd);
    if (idx >= 0) {
        clients[idx].last_activity = time(NULL);
    }
}

void check_inactive_clients(Client_record clients[], Session_record sessions[],
                            fd_set *master_fds, int *fd_max) {
    time_t now = time(NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            continue;
        }

        if ((now - clients[i].last_activity) >= INACTIVITY_LIMIT) {
            Message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = EXIT;
            strncpy((char *)msg.data, "Disconnected by server due to inactivity.", MAX_DATA - 1);
            msg.size = strlen((char *)msg.data);

            send_message(clients[i].socket_fd, &msg);

            char old_sessions[MAX_SESSIONS][MAX_NAME];
            int old_count = clients[i].session_count;
            memset(old_sessions, 0, sizeof(old_sessions));

            for (int j = 0; j < old_count; j++) {
                strncpy(old_sessions[j], clients[i].sessions[j], MAX_NAME - 1);
            }

            printf("Client %s disconnected due to inactivity.\n", clients[i].id);

            close(clients[i].socket_fd);
            FD_CLR(clients[i].socket_fd, master_fds);

            clients[i].active = 0;
            clients[i].session_count = 0;
            memset(clients[i].sessions, 0, sizeof(clients[i].sessions));

            for (int j = 0; j < old_count; j++) {
                remove_session_if_empty(sessions, clients, old_sessions[j]);
            }

            while (*fd_max > 0 && !FD_ISSET(*fd_max, master_fds)) {
                (*fd_max)--;
            }
        }
    }
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