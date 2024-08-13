#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define STEXT_PORT 49401  // Stext server's port
#define BUF_SIZE 1024

char *USERNAME;

// Function Headers
void processClient(int clientSock);
void makeDirectories(char *dirPath);
void storeFile(int clientSock, const char *filename, char *filePathTarget, const char *fileContent);

int main() {
    USERNAME = getenv("USER");
    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen;
    pid_t childPid;

    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(STEXT_PORT);

    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSock, 10) < 0) {
        perror("Listen failed");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    printf("Stext server listening on port %d...\n", STEXT_PORT);

    while (1) {
        addrLen = sizeof(clientAddr);
        clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientSock < 0) {
            perror("Accept failed");
            continue;
        }

        if ((childPid = fork()) == 0) {
            close(serverSock);
            processClient(clientSock);
            close(clientSock);
            exit(0);
        } else if (childPid > 0) {
            close(clientSock);
        } else {
            perror("Fork failed");
            close(clientSock);
            continue;
        }
    }

    close(serverSock);
    return 0;
}

void processClient(int clientSock) {
    char buffer[BUF_SIZE];
    char command[BUF_SIZE], filename[BUF_SIZE], destinationPath[BUF_SIZE];
    char *bigBuffer = NULL;
    char *fileContent = NULL;
    int bytesRead;
    size_t totalSize = 0;

    while (1) {
        // Free and reset the buffers for each new client command
        free(bigBuffer);
        bigBuffer = NULL;
        totalSize = 0;

        while (1) {
            // Receive data from the client
            memset(buffer, 0, BUF_SIZE);
            bytesRead = recv(clientSock, buffer, BUF_SIZE, 0);
            printf("bytes read server: %d: %s\n", bytesRead, buffer);
            if (bytesRead <= 0) {
                break;  // Client disconnected or error occurred
            }

            // Check for EOF marker
            if (bytesRead == 3 && strncmp(buffer, "EOF", 3) == 0) {
                break;  // End of file transmission
            }

            // Reallocate the bigBuffer to accumulate the received data
            char *temp = realloc(bigBuffer, totalSize + bytesRead + 1);
            if (!temp) {
                perror("Memory reallocation failed");
                free(bigBuffer);
                return;
            }
            bigBuffer = temp;

            // Append the received data to bigBuffer
            memcpy(bigBuffer + totalSize, buffer, bytesRead);
            totalSize += bytesRead;
            bigBuffer[totalSize] = '\0';  // Null-terminate the accumulated buffer
        }

        // Parse the command, filename, and destination path from the bigBuffer
        sscanf(bigBuffer, "%s %s %s", command, filename, destinationPath);

        // Extract the file content from the remaining part of bigBuffer
        fileContent = bigBuffer + strlen(command) + strlen(filename) + strlen(destinationPath) + 3;

        // Construct the target file path and store the file content
        char filePathTarget[BUF_SIZE];
        snprintf(filePathTarget, BUF_SIZE, "/home/%s/stext/%s/%s", USERNAME, destinationPath + 7, filename);


        
        if (strcmp(command, "ufile") == 0) {
            storeFile(clientSock, filename, filePathTarget, fileContent);
        }
        // Additional command handling (like dfile, etc.) can be added here
    }

    free(bigBuffer);  // Free the allocated buffer when done
}

void makeDirectories(char *dirPath) {
    char tmp[BUF_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dirPath);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

void storeFile(int clientSock, const char *filename, char *filePathTarget, const char *fileContent) {
    FILE *fp;
    char directory[BUF_SIZE];

    // Extract the directory from the file path
    strncpy(directory, filePathTarget, strrchr(filePathTarget, '/') - filePathTarget);
    directory[strrchr(filePathTarget, '/') - filePathTarget] = '\0';

    printf("directory in stext storeFile: %s\n", directory);

    // Create the directory structure if it doesn't exist
    makeDirectories(directory);

    // Open the target file for writing
    fp = fopen(filePathTarget, "wb");
    if (fp == NULL) {
        perror("Failed to open destination file");
        return;
    }

    // Write the entire file content to the destination file
    size_t fileSize = strlen(fileContent);
    size_t written = fwrite(fileContent, 1, fileSize, fp);

    if (written != fileSize) {
        perror("Failed to write the complete file content");
    } else {
        printf("Stored %s successfully.\n", filename);
    }

    fclose(fp);
}
