#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "10.0.3.12", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    char *message = "Request for haircut\n";
    send(sock, message, strlen(message), 0);
    printf("Message sent: %s\n", message);

    char buffer[1024] = {0};
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, 1024);
        printf("Response from server: %s\n", buffer);
    
        if (strstr(buffer, "Your haircut is starting") || strstr(buffer, "No space available")) {
            break;
        }
    }
    close(sock);
}
