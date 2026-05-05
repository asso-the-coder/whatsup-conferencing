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

int server_port;

int main(int argc, char** argv){

    if (argc != 2){
        fprintf(stderr, "Usage: %s <server_port_number>\n", argv[0]);
        return 1;
    }
    server_port = atoi(argv[1]);

    // Select skeleton setup
    //setup_select();

    // One socket for listening to incoming clients
    int listen_socket = init_listener_socket();

    // Main server loop
    while (true){

        accept_clients(listen_socket);

    }
    
        

    /*
    // Communication socket with client
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int comm_socket;
    */

    return 0;
}

        