#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "config.h"

#define BUF_SIZE 1024

int STXT_PORT = SMAIN_PORT + 1;

char *USERNAME;

// Function Headers
void processClient(int connectedSock);
void makeDirectories(char *dirPath);
void storeFile(int connectedSock, const char *filename, char *filePathTarget, const char *fileContent);
void removeFileFromStext(const char *filePath);
char* readFileContent(const char *filePath);
void sendBytes(int connectedSock, const char *toSend);
char *readTarFileContent(const char *tarFilePath, size_t *tarFileSize);
void sendBytesTar(int connectedSock, const char *tarContent, size_t tarFileSize);
int createTarFileOfFileType(const char *filetype, const char *homePath, const char *outputTarFilename);

int main() {
    USERNAME = getenv("USER");
    int serverSock, connectedSock;
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
    serverAddr.sin_port = htons(STXT_PORT);

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

    printf("Stext server listening on port %d...\n", STXT_PORT);

    while (1) {
        addrLen = sizeof(clientAddr);
        connectedSock = accept(serverSock, (struct sockaddr*)&clientAddr, &addrLen);
        if (connectedSock < 0) {
            perror("Accept failed");
            continue;
        }

        if ((childPid = fork()) == 0) {
            close(serverSock);
            processClient(connectedSock);
            close(connectedSock);
            exit(0);
        } else if (childPid > 0) {
            close(connectedSock);
        } else {
            perror("Fork failed");
            close(connectedSock);
            continue;
        }
    }

    close(serverSock);
    return 0;
}

void processClient(int connectedSock) {
    char buffer[BUF_SIZE];
    char command[BUF_SIZE], secondArg[BUF_SIZE], destinationPath[BUF_SIZE];
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
            // Receive data
            memset(buffer, 0, BUF_SIZE);
            bytesRead = recv(connectedSock, buffer, BUF_SIZE, 0);
            if (bytesRead <= 0) {
                break;  // sender disconnected or error occurred
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

        // Parse the command, secondArg, and destination path from the bigBuffer
        sscanf(bigBuffer, "%s %s %s", command, secondArg, destinationPath);

        
        if (strcmp(command, "ufile") == 0) {
            // Construct the target file path and store the file content
            char filePathTarget[BUF_SIZE];
            snprintf(filePathTarget, BUF_SIZE, "/home/%s/stext/%s/%s", USERNAME, destinationPath + 7, secondArg);
            // Extract the file content from the remaining part of bigBuffer
            fileContent = bigBuffer + strlen(command) + strlen(secondArg) + strlen(destinationPath) + 3;
            storeFile(connectedSock, secondArg, filePathTarget, fileContent);
        } else if (strcmp(command, "rmfile") == 0) {
            // Remove the file from Stext
            removeFileFromStext(secondArg);
        } else if (strcmp(command, "dfile") == 0) {
                char fullPath[BUF_SIZE]; 
                snprintf(fullPath, sizeof(fullPath), "/home/%s/stext%s", USERNAME, secondArg + 7);
                char *fileContent = readFileContent(fullPath);
                sendBytes(connectedSock, fileContent);
                

            } else if (strcmp(command, "dtar") == 0) {
                char fullPath[BUF_SIZE];
                snprintf(fullPath, sizeof(fullPath), "/home/%s/stext", USERNAME);
                if (createTarFileOfFileType(".txt", fullPath, "txtFiles.tar") == 0) {
                    char fullPathWithTar[1024];
                    snprintf(fullPathWithTar, sizeof(fullPathWithTar), "%s/txtFiles.tar", fullPath);

                    size_t tarFileSize;
                    char *tarContent = readTarFileContent(fullPathWithTar, &tarFileSize);

                    if (tarContent) {
                        sendBytesTar(connectedSock, tarContent, tarFileSize);
                        free(tarContent);  // Free the allocated memory after sending
                    }
                }
            }
        
        close(connectedSock);
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

void storeFile(int connectedSock, const char *filename, char *filePathTarget, const char *fileContent) {
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

void removeFileFromStext(const char *filePath) {
    char fullPath[1024];

    // Convert the provided path to full path
    snprintf(fullPath, sizeof(fullPath), "/home/%s/stext%s", USERNAME, filePath + 7);

    // Attempt to remove the file
    if (remove(fullPath) == 0) {
        printf("File %s deleted successfully.\n", fullPath);
    } else {
        perror("Error deleting the file");
    }
}
char* readFileContent(const char *filePath) {
    FILE *file = fopen(filePath, "rb");  // Open file in binary mode
    if (file == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    // Move the file pointer to the end and get the file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);  // Move the file pointer back to the beginning

    // Allocate memory to hold the entire file content
    char *content = (char*)malloc(fileSize + 1);
    if (content == NULL) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    // Read the entire file into the allocated memory
    size_t bytesRead = fread(content, 1, fileSize, file);
    if (bytesRead != fileSize) {
        perror("Failed to read the complete file");
        free(content);
        fclose(file);
        return NULL;
    }

    content[fileSize] = '\0';  // Null-terminate the string

    fclose(file);
    return content;  // Return the content of the file
}
void sendBytes(int connectedSock, const char *toSend) {
    int bytesToSend;

    // Send the combined buffer to the server
    bytesToSend = strlen(toSend);
    send(connectedSock, toSend, bytesToSend, 0);


}
// Additional functions for handling tar files
char *readTarFileContent(const char *tarFilePath, size_t *tarFileSize) {
    FILE *file = fopen(tarFilePath, "rb");  // Open file in binary mode
    if (file == NULL) {
        perror("Failed to open tar file");
        return NULL;
    }

    // Move the file pointer to the end and get the file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);  // Move the file pointer back to the beginning

    // Allocate memory to hold the entire tar file content
    char *content = (char *)malloc(size);
    if (content == NULL) {
        perror("Failed to allocate memory for tar file");
        fclose(file);
        return NULL;
    }

    // Read the entire tar file into the allocated memory
    size_t bytesRead = fread(content, 1, size, file);
    if (bytesRead != size) {
        perror("Failed to read the complete tar file");
        free(content);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *tarFileSize = size;
    return content;  // Return the content of the tar file
}

void sendBytesTar(int connectedSock, const char *tarContent, size_t tarFileSize) {
    size_t totalSent = 0;
    int bytesSent;


    while (totalSent < tarFileSize) {
        bytesSent = send(connectedSock, tarContent + totalSent, tarFileSize - totalSent, 0);
        if (bytesSent < 0) {
            perror("Failed to send tar file");
            break;
        }
        totalSent += bytesSent;
    }


}
int createTarFileOfFileType(const char *filetype, const char *homePath, const char *outputTarFilename)
{
    char command[1024];

    // Construct the find command to search for files with the given filetype and create a tar file in homePath
    snprintf(command, sizeof(command), "find %s -type f -name '*%s' -print0 | xargs -0 tar -cvf %s/%s", homePath, filetype, homePath, outputTarFilename);

    // Execute the command
    int result = system(command);

    // Return 0 on success, or -1 on failure
    return (result == 0) ? 0 : -1;
}