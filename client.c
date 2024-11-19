#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "helper.h"
#include <pthread.h>
#include <errno.h>
#include <signal.h>

void *sendAndRecieveFromSS(char *details, char *buffer) {
    char *ipOfSS = strtok(details, ",");
    char *portOfSS = strtok(NULL, ",");
    // Generated with copilot
    int sock;
    struct sockaddr_in ss_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Set up server address
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(atoi(portOfSS));
    if (inet_pton(AF_INET, ipOfSS, &ss_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(sock);
        return NULL;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *) &ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return NULL;
    }

    // Send buffer
    if (send_good(sock, buffer, strlen(buffer)) < 0) {
        perror("Send failed");
        close(sock);
        return NULL;
    }

    // Receive data continuously
    char response[BUFFER_SIZE];
    while (1) {
        ssize_t bytes_received = recv_good(sock, response, sizeof(response) - 1);
        if (bytes_received <= 0) {
            break;
        }
        response[bytes_received] = '\0';
        printf("%s", response);
    }

    fflush(stdin);
    close(sock);
    return NULL;
}

volatile int isPaused = 0; // Global flag for play/pause control

// Thread function to handle user input for play/pause
void *handleUserInput(void *arg) {
    while (1) {
        char input = getchar(); // Wait for user input
        if (input == 'p') { // 'p' for play/pause toggle
            isPaused = !isPaused;
            if (isPaused) {
                printf("Playback paused\n");
            } else {
                printf("Playback resumed\n");
            }
        }
    }
    return NULL;
}

void *streamMusicFromSS(char *details, char *buffer) {
    char *ipOfSS = strtok(details, ",");
    char *portOfSS = strtok(NULL, ",");
    int sock;
    struct sockaddr_in ss_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Set up server address
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(atoi(portOfSS));
    if (inet_pton(AF_INET, ipOfSS, &ss_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(sock);
        return NULL;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *) &ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return NULL;
    }

    // Send buffer to server
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        close(sock);
        return NULL;
    }

    // Open FFplay pipe for streaming
    FILE *ffplayPipe = popen("ffplay -autoexit -", "w");
    if (!ffplayPipe) {
        perror("Failed to open FFplay");
        close(sock);
        return NULL;
    }

    // Start a thread to handle user input for play/pause
    pthread_t inputThread;
    pthread_create(&inputThread, NULL, handleUserInput, NULL);

    // Buffer for receiving data
    char response[BUFFER_SIZE_FOR_MUSIC];
    ssize_t bytes_received;
    int firstTime = 1;

    while (1) {
        // Pause logic: Wait until playback is resumed
        while (isPaused) {
            usleep(100000); // Sleep for 100ms to avoid busy waiting
        }

        bytes_received = recv(sock, response, sizeof(response), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("End of stream\n");
            } else {
                if (firstTime) {
                    printf("File not Found\n");
                }
                perror("Receive failed");
            }
            break;
        }

        // Write data to FFplay
        if (fwrite(response, 1, bytes_received, ffplayPipe) != (size_t) bytes_received) {
            perror("Error writing to FFplay");
            break;
        }

        fflush(ffplayPipe); // Ensure data is flushed to the FFplay process
        firstTime = 0;
    }

    // Clean up
    pclose(ffplayPipe);
    close(sock);
    pthread_cancel(inputThread);     // Cancel the input thread
    pthread_join(inputThread, NULL); // Ensure thread cleanup
    return NULL;
}

int count_words(const char *input) {
    char buffer[100]; // Temporary buffer to avoid modifying the input
    strcpy(buffer, input);

    int count = 0;
    char *token = strtok(buffer, " "); // Tokenize using space as delimiter

    while (token != NULL) {
        count++;
        token = strtok(NULL, " "); // Get the next token
    }

    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <naming_server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int client_socket;
    struct sockaddr_in server_addr;



    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CLIENT_PORT_NS);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // Connect to Naming Server
    if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Naming Server @ %s:%d\n", argv[1], CLIENT_PORT_NS);

    char buffer[BUFFER_SIZE];
    char buffer2[BUFFER_SIZE];

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        bzero(buffer2, BUFFER_SIZE);
        printf(">> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\nExiting...\n");
            break; // Exit loop on EOF (Ctrl+D)
        }

        trim(buffer); // Remove trailing newline or extra spaces
        if (strlen(buffer) == 0)
            continue; // Skip empty input

        int c = count_words(buffer);
        if (c <= 1) {
            printf("Incorrect format\n");
            continue;
        }

        char *path_for_write;
        if (strncmp(buffer, "write", strlen("write")) == 0) {
            char *tmpdup = strdup(buffer);
            path_for_write = strtok(tmpdup, " ");
            path_for_write = strtok(NULL, " ");
            path_for_write = strtok(NULL, " ");
            // free(tmpdup); // Avoid memory leak   RUDRA
        }

        // Send command to Naming Server
        if (send_good(client_socket, buffer, strlen(buffer)) < 0) {
            perror("Naming Server not responding");
            continue;
        }
        // char buffer2[BUFFER_SIZE];
        bzero(buffer2, BUFFER_SIZE);
        if (strcmp(buffer, "ls ~") == 0 || strcmp(buffer, "ls .") == 0) {
            int bytes = 0;
            while ((bytes = recv_good(client_socket, buffer2, BUFFER_SIZE)) > 0) {
                char *end_msg = strstr(buffer2, "@#!@#over@$");
                if (end_msg != NULL) {
                    *end_msg = '\0';       // Terminate string before end message
                    printf("%s", buffer2); // Print remaining data
                    break;                 // Exit loop
                }
                buffer2[bytes] = '\0';
                printf("%s", buffer2);
            }
        } else if (strncmp(buffer, "mkdir", 5) == 0 || strncmp(buffer, "rmdir", 5) == 0 ||
                   strncmp(buffer, "touch", 5) == 0 || strncmp(buffer, "rm", 2) == 0 ||
                   strncmp(buffer, "copy", 4) == 0) {

            int bytes = 0;
            printf("Buffer before %s\n",buffer2);
            if ((bytes = recv_good(client_socket, buffer2, BUFFER_SIZE)) <= 0) {
                perror("Naming Server not responding");
            }
            buffer2[bytes] = '\0';
            printf("Received from NM Directly |%s|\n", buffer2);
            bzero(buffer2,BUFFER_SIZE);
        } else if (strncmp(buffer, "write", strlen("write")) == 0) {

            char write_buff[BUFFER_SIZE] = {0};

            int bytes_received = recv_good(client_socket, write_buff, BUFFER_SIZE);
            if (bytes_received <= 0) {
                perror("Naming Server not responding\n");
            }
            // else {
            //     strcat(write_buff, "\0");
            // }

            fflush(stdin);
            fflush(stdout);
            printf("ns response: %s\n", write_buff);
            if (strncmp(write_buff, "PATH_OK", strlen("PATH_OK")) == 0) {
                char *input_str = (char *) malloc(sizeof(char) * 4096); // to change size of max_write
                int i;
                if (strncmp(write_buff, "PATH_OK_SYNC", strlen("PATH_OK_SYNC")) == 0) {
                    strcpy(input_str, "WRITE_SYNC|");
                    i = 11;
                }
                if (strncmp(write_buff, "PATH_OK_ASYNC", strlen("PATH_OK_ASYNC")) == 0) {
                    strcpy(input_str, "WRITE_ASYNC|");
                    i = 12;
                }
                strcat(input_str, path_for_write);
                strcat(input_str, "|");
                i += strlen(path_for_write) + 1;
                char ch;
                printf("enter data here:");
                for (;;) {
                    ch = fgetc(stdin);
                    if (ch != '\n')
                        input_str[i++] = ch;
                    else
                        break;
                }
                input_str[i] = '\0'; // Null-terminate the input string
                // printf("inputstr: %s\n", input_str);
                char *temp_buffer = strdup(write_buff);
                char *ipOfSS = strtok(write_buff, "|");
                ipOfSS = strtok(NULL, "|");
                char *portOfSS = strtok(NULL, "|");

                // Generated with copilot
                int sock;
                struct sockaddr_in ss_addr;

                // Create socket
                if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    perror("Socket creation failed");
                    return 2;
                }

                // Set up server address
                ss_addr.sin_family = AF_INET;
                ss_addr.sin_port = htons(atoi(portOfSS));
                if (inet_pton(AF_INET, ipOfSS, &ss_addr.sin_addr) <= 0) {
                    perror("Invalid IP address");
                    close(sock);
                    return 2;
                }

                // Connect to server
                if (connect(sock, (struct sockaddr *) &ss_addr, sizeof(ss_addr)) < 0) {
                    perror("Connection failed");
                    close(sock);
                    return 2;
                }

                // Send buffer
                if (send_good(sock, input_str, strlen(input_str)) < 0) {
                    perror("Send failed");
                    close(sock);
                    return 2;
                }

                char *rec_msg = (char *) malloc(sizeof(char) * 4096);
                if (recv_good(sock, rec_msg, BUFFER_SIZE) <= 0) {
                    perror("Storage Server not responding\n");
                }
                printf("Received from SS |%s|\n", rec_msg);
                free(rec_msg);
            } else {
                // TODO error
            }
        } else {
            if (recv_good(client_socket, buffer2, BUFFER_SIZE) <= 0) {
                printf("Naming Server not responding\n");
                continue;
            }
            printf("Got this Other Commands from NM|%s|\n", buffer2);

            if (strstr(buffer2, "not found") != NULL) {
                continue;
            }

            if (strncmp(buffer, "stream", 6) == 0) {
                streamMusicFromSS(buffer2, buffer);
            } else {
                sendAndRecieveFromSS(buffer2, buffer);
            }
        }

        bzero(buffer, BUFFER_SIZE);
    }

    // Close socket before exiting
    close(client_socket);
    return 0;
}
