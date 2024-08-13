#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SMAIN_PORT 49400
#define BUF_SIZE 1024

int validateInput(const char *command, const char *filename, const char *destinationPath);


int main() {
    int clientSock;
    struct sockaddr_in serverAddr;
    char buffer[BUF_SIZE];
    char command[BUF_SIZE*2], filename[BUF_SIZE], destinationPath[BUF_SIZE];
    FILE *fp;
    int bytesRead;

    clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(SMAIN_PORT);

    if (connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect failed");
        close(clientSock);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Enter command: ");
        fgets(command, BUF_SIZE, stdin);
        sscanf(command, "%s %s %s", buffer, filename, destinationPath);

        if (validateInput(buffer, filename, destinationPath) != 0) {
            continue;
        }

        printf("out of if\n");

        // Open the file to read its content
        fp = fopen(filename, "rb");
        if (fp == NULL) {
            perror("File open failed");
            continue;
        }

        // Send the command first
        // send(clientSock, command, strlen(command), 0);

        // Append a space to the command before adding file content
        strcat(command, " ");

        // Wait for server response
        // memset(buffer, 0, BUF_SIZE);
        // if (recv(clientSock, buffer, BUF_SIZE, 0) > 0) {
            // if (strstr(buffer, "Ready to receive file content.") != NULL) {
                printf("innnnnn\n");
                // Now send the file content
                int i=0;
                while ((bytesRead = fread(buffer, sizeof(char), BUF_SIZE, fp)) > 0) {
                    printf("bytes read:  %d\n",bytesRead);
                    if (i==0){
                         // Append the file content (buffer) to the command
                        strncat(command, buffer, bytesRead);
                        send(clientSock, command, strlen(command), 0);  // Send the command with the initial part of the file content

                    }else{
                        send(clientSock, buffer, bytesRead, 0);

                    }
                    i = i+1;
                   
                }


                // Finally, send EOF marker as a separate message
                memset(buffer, 0, BUF_SIZE);
                usleep(100000);
                strcpy(buffer, "EOF");
                send(clientSock, buffer, strlen(buffer), 0);


                printf("File %s sent successfully.\n", filename);
            // } else {
            //     // If the server response is not positive, print an error message
            //     printf("Server error: %s\n", buffer);
            // }
        // } else {
        //     perror("Failed to receive server response.");
        // }

        fclose(fp);
    }

    close(clientSock);
    return 0;
}



int validateInput(const char *command, const char *filename, const char *destinationPath) {
    // Validate command
    if (!(strcmp(command, "ufile") == 0 || strcmp(command, "dfile") == 0 || strcmp(command, "rmfile") == 0 || strcmp(command, "dtar") == 0 || strcmp(command, "display") == 0)) {
        printf("Error: Invalid command.\n");
        return -1;
    }
    
    // Validate filename extension
    if (!(strstr(filename, ".c") || strstr(filename, ".pdf") || strstr(filename, ".txt"))) {
        printf("Error: Invalid file type. Only .c, .pdf, and .txt files are allowed.\n");
        return -1;
    }

    // Validate and convert destination path
    if (strncmp(destinationPath, "~/smain", 7) != 0) {
        printf("Error: Destination path must start with '~/smain'.\n");
        return -1;
    } 

    return 0; // Input is valid
}