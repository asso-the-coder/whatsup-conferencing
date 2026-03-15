#include "server.h"
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

int main(int argc, char** argv){

    if (argc != 2){
        fprintf(stderr, "Usage: %s <server_port_number>\n", argv[0]);
        return 1;
    }
    int server_port = atoi(argv[1]);

    // Create TCP listen socket (file descriptor and address object for IP + port)
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0){
        perror("Listen socket");
        return -1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET, 
        .sin_addr.s_addr = INADDR_ANY, 
        .sin_port = htons(server_port)
    };

    // Bind fd to port
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


    // Communication socket with client
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int comm_socket;
    
    while (true){
        
    }





    
    /*
        // Accept new client connection
        client_len = sizeof(client_addr);
        comm_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &client_len);
    
        if (comm_socket > 0){
            perror("Client socket");
            continue;
        }

        // Receive data from client
        int bytes = recv(comm_socket, )
        */




    return 0;
}