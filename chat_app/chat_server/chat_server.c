#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 54972
#define BUF_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_CHANNELS 10

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    int addr_len;
    char name[32];
    char channel[32];
} client_t;

typedef struct {
    char name[32];
    client_t *clients[MAX_CLIENTS];
    FILE *log_file;
} channel_t;

client_t *clients[MAX_CLIENTS];
channel_t *channels[MAX_CHANNELS];
CRITICAL_SECTION clients_mutex;
CRITICAL_SECTION channels_mutex;

void broadcast_message(char *message, char *channel_name, SOCKET exclude_socket) {
    EnterCriticalSection(&channels_mutex);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && strcmp(channels[i]->name, channel_name) == 0) {
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (channels[i]->clients[j] && channels[i]->clients[j]->socket != exclude_socket) {
                    if (send(channels[i]->clients[j]->socket, message, strlen(message), 0) < 0) {
                        printf("Failed to send message to client.\n");
                    } else {
                        printf("Message sent to client %s: %s\n", channels[i]->clients[j]->name, message);
                    }
                }
            }
            break;
        }
    }

    LeaveCriticalSection(&channels_mutex);
}

DWORD WINAPI handle_client(LPVOID arg) {
    char buffer[BUF_SIZE];
    char message[BUF_SIZE + 32];
    int leave_flag = 0;

    client_t *cli = (client_t *)arg;
    printf("Client connected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));

    // 사용자 이름 요청
    int receive = recv(cli->socket, buffer, 32, 0);
    if (receive <= 0) {
        printf("Failed to receive client name.\n");
        closesocket(cli->socket);
        free(cli);
        return 0;
    }
    buffer[receive] = '\0';
    strcpy(cli->name, buffer);
    printf("Client name: %s\n", cli->name);

    // 채널 요청
    receive = recv(cli->socket, buffer, 32, 0);
    if (receive > 0) {
        buffer[receive] = '\0';
        strcpy(cli->channel, buffer);
        printf("Client channel: %s\n", cli->channel);

        // 클라이언트를 채널에 추가
        EnterCriticalSection(&channels_mutex);
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (channels[i] == NULL) {
                channels[i] = (channel_t *)malloc(sizeof(channel_t));
                strcpy(channels[i]->name, cli->channel);
                memset(channels[i]->clients, 0, sizeof(channels[i]->clients));

                // 채널별 로그 파일 열기
                char log_filename[64];
                sprintf(log_filename, "%s_log.txt", cli->channel);
                channels[i]->log_file = fopen(log_filename, "a");
                if (channels[i]->log_file == NULL) {
                    printf("Could not open log file for channel %s.\n", cli->channel);
                    free(cli);
                    LeaveCriticalSection(&channels_mutex);
                    closesocket(cli->socket);
                    return 0;
                }
            }
            if (strcmp(channels[i]->name, cli->channel) == 0) {
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (channels[i]->clients[j] == NULL) {
                        channels[i]->clients[j] = cli;
                        printf("Client %s added to channel %s\n", cli->name, cli->channel);
                        break;
                    }
                }
                break;
            }
        }
        LeaveCriticalSection(&channels_mutex);
    } else {
        printf("Failed to receive channel name.\n");
        closesocket(cli->socket);
        free(cli);
        return 0;
    }

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        receive = recv(cli->socket, buffer, BUF_SIZE, 0);
        if (receive > 0) {
            buffer[receive] = '\0';
            if (strlen(buffer) > 0) {
                sprintf(message, "%s: %s\n", cli->name, buffer);
                printf("Received message from %s: %s\n", cli->name, buffer);

                // 메시지를 채널별 로그 파일에 기록
                EnterCriticalSection(&channels_mutex);
                for (int i = 0; i < MAX_CHANNELS; i++) {
                    if (channels[i] && strcmp(channels[i]->name, cli->channel) == 0) {
                        fprintf(channels[i]->log_file, "[%s][%s]: %s\n", cli->channel, cli->name, buffer);
                        fflush(channels[i]->log_file);
                        break;
                    }
                }
                LeaveCriticalSection(&channels_mutex);

                broadcast_message(message, cli->channel, cli->socket);
            }
        } else if (receive == 0 || strcmp(buffer, "exit") == 0) {
            sprintf(message, "%s has left the chat.\n", cli->name);
            printf("Client disconnected: %s\n", cli->name);
            broadcast_message(message, cli->channel, cli->socket);
            leave_flag = 1;
        } else {
            printf("Failed to receive message from client.\n");
            leave_flag = 1;
        }

        if (leave_flag) {
            break;
        }
    }

    closesocket(cli->socket);
    EnterCriticalSection(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == cli) {
            clients[i] = NULL;
            break;
        }
    }

    LeaveCriticalSection(&clients_mutex);
    EnterCriticalSection(&channels_mutex);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && strcmp(channels[i]->name, cli->channel) == 0) {
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (channels[i]->clients[j] == cli) {
                    channels[i]->clients[j] = NULL;
                    break;
                }
            }
        }
    }

    LeaveCriticalSection(&channels_mutex);
    free(cli);
    printf("Client handler thread terminated for %s\n", cli->name);
    return 0;
}

void print_server_ip() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        printf("Error getting hostname: %d\n", WSAGetLastError());
        return;
    }

    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        printf("Error getting host by name: %d\n", WSAGetLastError());
        return;
    }

    printf("Server IP addresses: \n");
    for (int i = 0; host->h_addr_list[i] != 0; i++) {
        struct in_addr addr;
        memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
        printf("%s\n", inet_ntoa(addr));
    }
}

int main() {
    WSADATA wsa;
    SOCKET server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(struct sockaddr_in);
    HANDLE thread;
    DWORD thread_id;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    InitializeCriticalSection(&clients_mutex);
    InitializeCriticalSection(&channels_mutex);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Could not create socket. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error code: %d\n", WSAGetLastError());
        return 1;
    }

    if (listen(server_socket, 10) == SOCKET_ERROR) {
        printf("Listen failed with error code: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Server started on port %d\n", PORT);
    print_server_ip();

    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (new_socket == INVALID_SOCKET) {
            printf("Accept failed with error code: %d\n", WSAGetLastError());
            continue;
        }

        EnterCriticalSection(&clients_mutex);

        int client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == NULL) {
                client_index = i;
                break;
            }
        }

        if (client_index == -1) {
            printf("Max clients reached. Rejected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            closesocket(new_socket);
        } else {
            client_t *cli = (client_t *)malloc(sizeof(client_t));
            cli->address = client_addr;
            cli->socket = new_socket;
            cli->addr_len = addr_len;
            clients[client_index] = cli;

            printf("Client accepted: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            thread = CreateThread(NULL, 0, handle_client, (void *)cli, 0, &thread_id);
        }

        LeaveCriticalSection(&clients_mutex);
    }

    closesocket(server_socket);
    DeleteCriticalSection(&clients_mutex);
    DeleteCriticalSection(&channels_mutex);
    WSACleanup();

    EnterCriticalSection(&channels_mutex);
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i] && channels[i]->log_file) {
            fclose(channels[i]->log_file);
        }
    }
    LeaveCriticalSection(&channels_mutex);

    return 0;
}