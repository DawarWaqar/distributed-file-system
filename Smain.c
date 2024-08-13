#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define SMAIN_PORT 49400
#define BUF_SIZE 1024

char *USERNAME;
char *STEXT_IP = "127.0.0.1";  // Global variable for Stext server IP
int STEXT_PORT = 49401;        // Global variable for Stext server port

// Function Headers
void prclient(int clientSock);
void makeDirectories(char *dirPath);
void storeFile(int clientSock, const char *filename, char *filePathTarget, const char *fileContent);
int copyFile(const char *srcFilename, const char *destFilename);
void forwardFileToServer(char *serverIp, int serverPort, const char *toSend);

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
    serverAddr.sin_port = htons(SMAIN_PORT);

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

    printf("Smain server listening on port %d...\n", SMAIN_PORT);

    while (1) {
        addrLen = sizeof(clientAddr);
        clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientSock < 0) {
            perror("Accept failed");
            continue;
        }

        if ((childPid = fork()) == 0) {
            close(serverSock);
            prclient(clientSock);
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

void prclient(int clientSock) {
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

        if (strstr(filename, ".c")) {
            // Construct the target file path and store the file content
            char filePathTarget[BUF_SIZE];
            snprintf(filePathTarget, BUF_SIZE, "/home/%s/smain/%s/%s", USERNAME, destinationPath + 7, filename);
            if (strcmp(command, "ufile") == 0) {
                storeFile(clientSock, filename, filePathTarget, fileContent);
            }
            // else other commands like dfile, etc. can be handled here
        } else if (strstr(filename, ".txt")) {
            // Transfer to Stext server using the global variable for Stext IP and port
            forwardFileToServer(STEXT_IP, STEXT_PORT, bigBuffer);
        }
    }

    free(bigBuffer);  // Free the allocated buffer when done
}



void makeDirectories(char *dirPath) {
    char tmp[BUF_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dirPath);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
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


int copyFile(const char *srcFilename, const char *destFilename) {
    FILE *srcFp, *destFp;
    char buffer[BUF_SIZE];
    size_t bytes;

    // Open source file in binary read mode
    srcFp = fopen(srcFilename, "rb");
    if (srcFp == NULL) {
        perror("Failed to open source file");
        return 1;
    }

    // Open destination file in binary write mode
    destFp = fopen(destFilename, "wb");
    if (destFp == NULL) {
        perror("Failed to open destination file");
        fclose(srcFp);
        return 1;
    }

    // Copy content from source to destination
    while ((bytes = fread(buffer, 1, sizeof(buffer), srcFp)) > 0) {
        fwrite(buffer, 1, bytes, destFp);
    }

    // Close both files
    fclose(srcFp);
    fclose(destFp);

    return 0;
}

void forwardFileToServer(char *serverIp, int serverPort, const char *toSend) {
    int serverSock;
    struct sockaddr_in serverAddr;
    int bytesToSend;

    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed");
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIp);
    serverAddr.sin_port = htons(serverPort);

    if (connect(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect failed");
        close(serverSock);
        return;
    }


    
    // Send the combined buffer to the Stext server
    bytesToSend = strlen(toSend);
    send(serverSock, toSend, bytesToSend, 0);

    printf("Command and file content forwarded to Stext.\n");

    close(serverSock);
}
