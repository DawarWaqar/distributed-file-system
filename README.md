# Distributed File System

A distributed file system developed using socket programming in C.

## About the Project

A distributed file system implemented using socket programming in C for a course project. The system comprises three servers: Smain, Spdf, and Stext, each responsible for managing different file types (.c, .pdf, .txt). Clients interact exclusively with the Smain server to upload, download, and manage files, unaware of the distribution of files across the different servers. The project demonstrates fundamental concepts of distributed systems, client-server communication, and socket programming.

## Quick Start Tutorial

### Clone the repository

```
git clone https://github.com/DawarWaqar/distributed-file-system.git
```

```
cd distributed-file-system
```

### Run the servers

```
gcc -o Smain Smain.c && ./Smain
```

```
gcc -o Stext Stext.c && ./Stext
```

```
gcc -o Spdf Spdf.c && ./Spdf
```

### Run the client

```
gcc -o client24s client24s.c && ./client24s
```

## Client Commands

### Upload file
```
ufile filename destination_path 
```

#### Examples:

- Should store(upload) sample.c in 
the specified folder on the Smain server.

    ```
    ufile sample.c ~/smain/folder1/folder2 
    ```

- Smain transfers 
sample.txt to the Stext server and the Stext server in turn stores sample.txt
in ~Stext/folder1/folder2 //User assumes sample.txt is stored in Smain.

    ```
    ufile sample.txt ~/smain/folder1/folder2
    ```


### Download file

Transfers (downloads) filename from the relevant server to the PWD of the client. If it's a .c file then Smain -> Client, if .txt then Stext -> Smain -> Client and if .pdf then Spdf -> Smain -> Client.

```
dfile filename 
```


### Remove file
```
rmfile filename
```
### Tar files
Creates a tar file of the specified file type and transfers (downloads) the tar file from 
Smain to the PWD of the client

```
dtar filetype 
```

#### Example 
```
dtar .pdf
```

### Display filenames
Smain obtains the list of all .pdf and .txt files (if any) from the 
corresponding directories in Spdf and Stxt. Combines with local .c files and then sends to the client.

```
display pathname
```