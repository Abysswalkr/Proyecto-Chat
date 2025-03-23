#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <username> <server_ip> <server_port>\n", argv[0]);
        return 1;
    }
    
    printf("Connecting to %s:%s as %s\n", argv[2], argv[3], argv[1]);
    // Implementation will go here
    
    return 0;
}
