#include "server.h"
#include "socket_helpers.h"
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

int init_listener_socket(){
    
    // Create TCP listen socket file descriptor
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0){
        perror("Listen socket");
        return -1;
    }

    // Create and populate address object for IP + port
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET, 
        .sin_addr.s_addr = INADDR_ANY, 
        .sin_port = htons(server_port)
    };

    // Bind file descriptor to port
    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Listen socket");
        return -1;
    }

    // Always listening for clients requesting to connect
    if (listen(listen_socket, MAX_CLIENTS) < 0){
        perror("Listen socket");
        return -1;
    }

    printf("WhatsUp server launched! Listening on port %d...\n", server_port);
    
    return listen_socket;
}

int accept_clients(listen_sckt){

    // Init potential client fields
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Accept new client connection
    client_socket = accept(listen_sckt, (struct sockaddr *)&client_addr, &client_len);

    if (client_socket < 0){
        perror("Client socket");
        close(client_socket);
        return -1;
    }

    // Receive login request from client
    char buffer[MAX_MSG_LEN];
    int bytes = recv(client_socket, (void *) buffer, sizeof(buffer), 0);

    printf("%s", buffer);

    // Check if database match exists and respond accordingly

    // Check if max number of clients hit
    
    // If all checks pass, register this FD into select() master set

    // Successfully registered a client (non-zero for failures)
    return 0;
}