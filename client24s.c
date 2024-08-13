#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "config.h"

#define BUF_SIZE 1024

int validateInput(const char *command, const char *filename, const char *destinationPath);


int main()
{
    int clientSock;
    struct sockaddr_in serverAddr;
    char buffer[BUF_SIZE];
    char  command[BUF_SIZE];
    char line[BUF_SIZE * 2], secondArg[BUF_SIZE], thirdArg[BUF_SIZE];
    FILE *fp;
    int bytesRead;
    char filename[BUF_SIZE];

    clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(SMAIN_PORT);

    if (connect(clientSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Connect failed");
        close(clientSock);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        printf("Enter command: ");
        fgets(line, BUF_SIZE, stdin);
        sscanf(line, "%s %s %s",  command, secondArg, thirdArg);


        if (strcmp(command, "ufile") == 0){
            if (validateInput( command, secondArg, thirdArg) != 0){
                continue;
            }
         

            // Open the file to read its content
            fp = fopen(secondArg, "rb");
            if (fp == NULL)
            {
                perror("File open failed");
                continue;
            }


            // Append a space to the command before adding file content
            strcat(line, " ");

            // Now send the file content
            int i = 0;
            while ((bytesRead = fread(buffer, sizeof(char), BUF_SIZE, fp)) > 0)
            {
                printf("bytes read:  %d\n", bytesRead);
                if (i == 0)
                {
                    // Append the file content (buffer) to the line
                    strncat(line, buffer, bytesRead);
                    printf("line: %s\n",line);
                    send(clientSock, line, strlen(line), 0); // Send the line with the initial part of the file content
                }
                else
                {
                    printf("rem buffer: %s\n",buffer);
                    send(clientSock, buffer, bytesRead, 0);
                }
                i = i + 1;
            }



            fclose(fp);
        } else if (strcmp(command, "rmfile") == 0){
            
            // Extract the filename from the path
            strncpy(filename, strrchr(secondArg, '/') + 1, BUF_SIZE);
            if (validateInput( command, filename, secondArg) != 0){
                continue;
            }

            send(clientSock, line, strlen(line), 0);
            


        } else{
            printf("Invalid first argument\n");
            continue;
        }


        // Finally, send EOF marker as a separate message
        memset(buffer, 0, BUF_SIZE);
        usleep(100000);
        strcpy(buffer, "EOF");
        send(clientSock, buffer, strlen(buffer), 0);

        printf("Command sent successfully.\n");

       


        
    }

    close(clientSock);
    return 0;
}

int validateInput(const char *command, const char *filename, const char *destinationPath)
{
    // Validate command
    if (!(strcmp(command, "ufile") == 0 || strcmp(command, "dfile") == 0 || strcmp(command, "rmfile") == 0 || strcmp(command, "dtar") == 0 || strcmp(command, "display") == 0))
    {
        printf("Error: Invalid command.\n");
        return -1;
    }

    // Validate filename extension
    if (!(strstr(filename, ".c") || strstr(filename, ".pdf") || strstr(filename, ".txt")))
    {
        printf("Error: Invalid file type. Only .c, .pdf, and .txt files are allowed.\n");
        return -1;
    }

    // Validate and convert destination path
    if (strncmp(destinationPath, "~/smain", 7) != 0)
    {
        printf("Error: Destination path must start with '~/smain'.\n");
        return -1;
    }

    return 0; // Input is valid
}

