//smain
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"

#define BUF_SIZE 1024

#define MAX_PATH_LEN 1024
#define MAX_FILENAME_LEN 256
#define MAX_FILE_COUNT 1024

char *USERNAME;
char *STXT_IP = "127.0.0.1";
int STXT_PORT = SMAIN_PORT + 1;
char *SPDF_IP = "127.0.0.1";
int SPDF_PORT = SMAIN_PORT + 2;

// Function Headers
void prclient(int clientSock);
void makeDirectories(char *dirPath);
void storeFile(int clientSock, const char *filename, char *filePathTarget,
               const char *fileContent);
void removeFile(const char *filePath);
char *readFileContent(const char *filePath);
int createConnectedSocket(char *receiverIp, int receiverPort);
void sendBytes(int connectedSock, const char *toSend);
void sendEOFMarker(int connectedSock);
char *receiveBytes(int connectedSock);
int createTarFileOfFileType(const char *filetype, const char *homePath,
                            const char *outputTarFilename);
char *readBinFileContent(const char *tarFilePath, size_t *tarFileSize);
void sendBytesBin(int connectedSock, const char *tarContent,
                  size_t tarFileSize);
char *receiveBytesBin(int connectedSock, size_t *receivedFileSize);
char *retrieveFiles(const char *directory, const char *extension,
                    size_t bufferSize);
char *retrieveAndCombineFileLists(char *cFiles, char *pdfFiles, char *txtFiles);

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

	if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
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
		clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
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
    // Receive data from the client
    memset(buffer, 0, BUF_SIZE);
    bytesRead = recv(clientSock, buffer, BUF_SIZE, 0);
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

	// char *command = receiveBytes(clientSock);
	// char bigBuffer[BUF_SIZE];
	sscanf(bigBuffer, "%s %s %s", command, secondArg, destinationPath);

	if (strcmp(command, "display") == 0) {
		char smainPath[MAX_PATH_LEN];
		snprintf(smainPath, sizeof(smainPath), "/home/%s/smain%s", USERNAME,
		         secondArg + 7);
		char *cFiles = retrieveFiles(smainPath, ".c", BUF_SIZE * 2);
		int socket = createConnectedSocket(SPDF_IP, SPDF_PORT);
		sendBytes(socket, bigBuffer);
		char *pdfFiles = receiveBytes(socket);
		socket = createConnectedSocket(STXT_IP, STXT_PORT);
		sendBytes(socket, bigBuffer);
		char *txtFiles = receiveBytes(socket);
		char *combinedFileList =
		    retrieveAndCombineFileLists(cFiles, pdfFiles, txtFiles);
		sendBytes(clientSock, combinedFileList);
	} else if (strstr(secondArg, ".c")) {
		// Construct the target file path and store the file content
		char filePathTarget[BUF_SIZE];
		snprintf(filePathTarget, BUF_SIZE, "/home/%s/smain/%s/%s", USERNAME,
		         destinationPath + 7, secondArg);
		if (strcmp(command, "ufile") == 0) {
			// Extract the file content from the remaining part of bigBuffer
			fileContent = bigBuffer + strlen(command) + strlen(secondArg) +
			              strlen(destinationPath) + 3;
			storeFile(clientSock, secondArg, filePathTarget, fileContent);
		} else if (strcmp(command, "rmfile") == 0) {
			// second arg has complete path
			removeFile(secondArg);
		} else if (strcmp(command, "dfile") == 0) {
			char fullPath[BUF_SIZE];
			snprintf(fullPath, sizeof(fullPath), "/home/%s%s", USERNAME,
			         secondArg + 1);
			char *fileContent = readFileContent(fullPath);
			sendBytes(clientSock, fileContent);
		} else if (strcmp(command, "dtar") == 0) {
			char fullPath[BUF_SIZE];
			snprintf(fullPath, sizeof(fullPath), "/home/%s/smain", USERNAME);
			if (createTarFileOfFileType(".c", fullPath, "cfiles.tar") == 0) {
				char fullPathWithTar[1024];
				snprintf(fullPathWithTar, sizeof(fullPathWithTar), "%s/cfiles.tar",
				         fullPath);

				size_t tarFileSize;
				char *tarContent = readBinFileContent(fullPathWithTar, &tarFileSize);

				printf("tar content in smain: %s\n", tarContent);

				if (tarContent) {
					sendBytesBin(clientSock, tarContent, tarFileSize);
					free(tarContent);  // Free the allocated memory after sending
					removeFile("~/smain/cfiles.tar");
				}
			}
		}
	} else if (strstr(secondArg, ".txt")) {
		int socket = createConnectedSocket(STXT_IP, STXT_PORT);
		sendBytes(socket, bigBuffer);
		if (strcmp(command, "dfile") == 0) {
			char *receivedBytes = receiveBytes(socket);
			sendBytes(clientSock, receivedBytes);
		} else if (strcmp(command, "dtar") == 0) {
			size_t tarFileSize;
			char *tarContent = receiveBytesBin(socket, &tarFileSize);

			if (tarContent) {
				sendBytesBin(clientSock, tarContent, tarFileSize);
				free(tarContent);
			}
		}
	} else if (strstr(secondArg, ".pdf")) {
		int socket = createConnectedSocket(SPDF_IP, SPDF_PORT);

		sendBytes(socket, bigBuffer);

		if (strcmp(command, "dfile") == 0) {
			size_t pdfFileSize;
			char *pdfContent = receiveBytesBin(socket, &pdfFileSize);
			if (pdfContent) {
				sendBytesBin(clientSock, pdfContent, pdfFileSize);
				free(pdfContent);
			}
		} else if (strcmp(command, "dtar") == 0) {
			size_t tarFileSize;
			char *tarContent = receiveBytesBin(socket, &tarFileSize);

			if (tarContent) {
				sendBytesBin(clientSock, tarContent, tarFileSize);
				free(tarContent);
			}
		}
	}
}

free(bigBuffer);
}

void makeDirectories(char *dirPath) {
	char tmp[BUF_SIZE];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", dirPath);
	len = strlen(tmp);
	if (tmp[len - 1] == '/') tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			mkdir(tmp, 0777);
			*p = '/';
		}
	}
	mkdir(tmp, 0777);
}

void storeFile(int clientSock, const char *filename, char *filePathTarget,
               const char *fileContent) {
	FILE *fp;
	char directory[BUF_SIZE];

// Extract the directory from the file path
	strncpy(directory, filePathTarget,
	        strrchr(filePathTarget, '/') - filePathTarget);
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

void removeFile(const char *filePath) {
	char fullPath[1024];

// Convert the provided path to full path ~/smain
	snprintf(fullPath, sizeof(fullPath), "/home/%s%s", USERNAME, filePath + 1);

// Attempt to remove the file
	if (remove(fullPath) == 0) {
		printf("File %s deleted successfully.\n", fullPath);
	} else {
		perror("Error deleting the file");
	}
}
char *readFileContent(const char *filePath) {
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
	char *content = (char *)malloc(fileSize + 1);
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
int createConnectedSocket(char *receiverIp, int receiverPort) {
	int receiverSock;
	struct sockaddr_in receiverAddr;

	receiverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (receiverSock < 0) {
		perror("Socket creation failed");
		return -1;
	}

	receiverAddr.sin_family = AF_INET;
	receiverAddr.sin_addr.s_addr = inet_addr(receiverIp);
	receiverAddr.sin_port = htons(receiverPort);

	if (connect(receiverSock, (struct sockaddr *)&receiverAddr,
	            sizeof(receiverAddr)) < 0) {
		perror("Connect failed");
		close(receiverSock);
		return -1;
	}

	return receiverSock;
}
void sendBytes(int connectedSock, const char *toSend) {
	int bytesToSend;

// Send the combined buffer to the server
	bytesToSend = strlen(toSend);
	send(connectedSock, toSend, bytesToSend, 0);
	sendEOFMarker(connectedSock);

	printf("Bytes Sent.\n");
}

void sendEOFMarker(int connectedSock) {
	char buffer[BUF_SIZE];

// Sleep for a short time to ensure all previous data is sent
	usleep(100000);

// Copy the EOF marker to the buffer
	strcpy(buffer, "EOF");

// Send the EOF marker
	send(connectedSock, buffer, strlen(buffer), 0);
}
char *receiveBytes(int connectedSock) {
	char buffer[BUF_SIZE];
	char *bigBuffer = NULL;
	int bytesRead;
	size_t totalSize = 0;

	while (1) {
		memset(buffer, 0, BUF_SIZE);
		bytesRead = recv(connectedSock, buffer, BUF_SIZE, 0);
		if (bytesRead <= 0) {
			break;  // End of transmission or error
		}
        if (bytesRead == 3 && strncmp(buffer, "EOF", 3) == 0) {
            break;  // End of file transmission
        }

		// Reallocate the bigBuffer to accumulate the received data
		char *temp = realloc(bigBuffer, totalSize + bytesRead + 1);
		if (!temp) {
			perror("Memory reallocation failed");
			free(bigBuffer);
			return NULL;
		}
		bigBuffer = temp;

		// Append the received data to bigBuffer
		memcpy(bigBuffer + totalSize, buffer, bytesRead);
		totalSize += bytesRead;
		bigBuffer[totalSize] = '\0';  // Null-terminate the accumulated buffer
	}

	return bigBuffer;  // Return the accumulated buffer
}
int createTarFileOfFileType(const char *filetype, const char *homePath,
                            const char *outputTarFilename) {
	char command[1024];

// Construct the find command to search for files with the given filetype and
// create a tar file in homePath
	snprintf(command, sizeof(command),
	         "find %s -type f -name '*%s' -print0 | xargs -0 tar -cvf %s/%s",
	         homePath, filetype, homePath, outputTarFilename);

// Execute the command
	int result = system(command);

// Return 0 on success, or -1 on failure
	return (result == 0) ? 0 : -1;
}
char *readBinFileContent(const char *tarFilePath, size_t *tarFileSize) {
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
void sendBytesBin(int connectedSock, const char *tarContent,
                  size_t tarFileSize) {
	size_t totalSent = 0;
	int bytesSent;

	while (totalSent < tarFileSize) {
		bytesSent =
		    send(connectedSock, tarContent + totalSent, tarFileSize - totalSent, 0);
		if (bytesSent < 0) {
			perror("Failed to send tar file");
			break;
		}
		totalSent += bytesSent;
	}

	sendEOFMarker(
	    connectedSock);
}
char *receiveBytesBin(int connectedSock, size_t *receivedFileSize) {
	char buffer[BUF_SIZE];
	char *tarBuffer = NULL;
	size_t totalSize = 0;
	int bytesRead;

// Loop to receive the tar file in chunks
	while (1) {
		memset(buffer, 0, BUF_SIZE);
		bytesRead = recv(connectedSock, buffer, BUF_SIZE, 0);
		if (bytesRead <= 0) {
			break;  // End of transmission or error
		}

		// Allocate memory for the full file content
		char *tempBuffer = realloc(tarBuffer, totalSize + bytesRead);
		if (!tempBuffer) {
			perror("Memory reallocation failed");
			free(tarBuffer);
			return NULL;
		}
		tarBuffer = tempBuffer;

		// Copy received data to the tar buffer
		memcpy(tarBuffer + totalSize, buffer, bytesRead);
		totalSize += bytesRead;
	}

// Set the final size of the received file
	*receivedFileSize = totalSize;

	return tarBuffer;  // Return the complete tar content
}
char *retrieveFiles(const char *directory, const char *extension,
                    size_t bufferSize) {
	printf("in retrieveFiles\n");
	DIR *dir;
	struct dirent *entry;
	char filePath[MAX_PATH_LEN];
	char *resultBuffer = (char *)malloc(bufferSize);

	if (resultBuffer == NULL) {
		perror("Failed to allocate memory for resultBuffer");
		return NULL;
	}

	resultBuffer[0] = '\0';  // Initialize resultBuffer as an empty string

	if ((dir = opendir(directory)) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type == DT_REG) {  // Regular file
				const char *ext = strrchr(entry->d_name, '.');
				if (ext && strcmp(ext, extension) == 0) {
					// Construct the full file path
					snprintf(filePath, sizeof(filePath), "%s/%s", directory, entry->d_name);

					// Append the file name to the result buffer
					if (strlen(resultBuffer) + strlen(entry->d_name) + 1 < bufferSize) {
						strcat(resultBuffer, entry->d_name);
						strcat(resultBuffer, " ");
					}
				}
			}
		}
		closedir(dir);
		return resultBuffer;
	} else {
		perror("Failed to open directory");
		free(resultBuffer);
		return NULL;
	}
}

char *retrieveAndCombineFileLists(char *cFiles, char *pdfFiles,
                                  char *txtFiles) {
// Allocate memory for the result buffer
	char *resultBuffer = (char *)malloc(MAX_FILE_COUNT * MAX_FILENAME_LEN);
	if (resultBuffer == NULL) {
		perror("Failed to allocate memory");
		return NULL;
	}

// Initialize the result buffer as an empty string
	resultBuffer[0] = '\0';

	if (cFiles && strlen(cFiles) > 0) {
		strcat(resultBuffer, cFiles);
	}

	if (pdfFiles && strlen(pdfFiles) > 0) {
		strcat(resultBuffer, pdfFiles);
	}

	if (txtFiles && strlen(txtFiles) > 0) {
		strcat(resultBuffer, txtFiles);
	}

// Remove the trailing space, if any
	size_t len = strlen(resultBuffer);
	if (len > 0 && resultBuffer[len - 1] == ' ') {
		resultBuffer[len - 1] = '\0';
	}

	return resultBuffer;  // Return the result buffer
}