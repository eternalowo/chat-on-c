#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#define DEFAULT_PORT 2002
#define N 16384 
#define MAX_CLIENTS 32
#define MAX_USERNAME_LENGTH 64

int clients_count = 0;

SOCKET client_sockets[MAX_CLIENTS];
char client_usernames[MAX_CLIENTS][MAX_USERNAME_LENGTH];

HANDLE mutex;

DWORD WINAPI client_handling(LPVOID client_socket_voidp) {

	setlocale(LC_ALL, "Russian");

	SOCKET client_socket = *((SOCKET*)client_socket_voidp);
	char request[N];
	char response[N];

	while (1) {

		int bytes_read = recv(client_socket, request, N, 0);

		if (bytes_read == SOCKET_ERROR) {
			printf("Failed to get request from client.\n");
			WSACleanup();
			return -1;
		} 

		if (bytes_read > N) {
			request[N - 1] = "\0";
		}
		
		int current_client = 0;
		
		for (int i = 0; i < N; ++i) {
			if (client_sockets[i] == client_socket) {
				break;
			}
			++current_client;
		}

		if (strcmp(request,"/quit\n") == 0) {
			WaitForSingleObject(mutex, INFINITE);
			memset(response, 0, N);
			strcat(response, client_usernames[current_client]);
			strcat(response, " покинул чат.\n");
		
			for (int i = 0; i < clients_count; ++i) {
				if (client_sockets[i] != client_socket && send(client_sockets[i], response, strlen(response), 0) == SOCKET_ERROR) {
					printf("Failed to send message.\n");
					WSACleanup();
					return -2;
				}
			}

			printf(response);

			if (closesocket(client_socket) == SOCKET_ERROR) {
				printf("Failed to close socket.\n");
				WSACleanup();
				return -3;
			}

			for (int i = current_client; i < clients_count - 1; ++i) {
				client_sockets[i] = client_sockets[i + 1];
				strcpy(client_usernames[i], client_usernames[i + 1]);
			}

			client_sockets[clients_count--] = SOCKET_ERROR;
			printf("Current online: %d\n", clients_count);
			ReleaseMutex(mutex);
			break;
		}
		WaitForSingleObject(mutex, INFINITE);

		printf("%s : %s", client_usernames[current_client], request);

		memset(response, 0, N);
		strcat(response, client_usernames[current_client]);
		strcat(response, " : ");
		strcat(response, request);

		for (int i = 0; i < clients_count; ++i) {
			if (client_sockets[i] != client_socket && send(client_sockets[i], response, strlen(response) + 1, 0) == SOCKET_ERROR) {
				printf("Failed to send message.\n");
				WSACleanup();
				return -4;
			}
		}
		ReleaseMutex(mutex);
		memset(response, 0, N);
		memset(request, 0, N);
	}
	return 1;
}

int main() {
	system("chcp 1251 > NUL");
	setlocale(LC_ALL, "Russian");

	WSADATA WSAData;
	SOCKET listen_socket, client_socket;
	struct sockaddr_in server_address, client_address;
	int client_address_size = sizeof(client_address);
	int bytes_read;
	mutex = CreateMutex(NULL, FALSE, NULL);
	if (mutex == NULL) {
		printf("Failed to create mutex.\n");
		WSACleanup();
		return 1;
	}

	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) {
		printf("Unable to initalize Winsock\n");
		return 1;
	}

	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET) {
		printf("Unable to initialize socket\n");
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(DEFAULT_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(listen_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
		printf("Unable to bind address to socket\n");
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	printf("Server started at port: %d\n", DEFAULT_PORT);

	while (1) {

		if (listen(listen_socket, MAX_CLIENTS) == SOCKET_ERROR) {
			printf("Unable to listen socket\n");
			closesocket(listen_socket);
			WSACleanup();
			return 1;
		}

		client_socket = accept(listen_socket, (struct sockaddr*)&client_address, &client_address_size);
		if (client_socket == INVALID_SOCKET) {
			printf("Unable to accept client socket\n");
			closesocket(client_socket);
			WSACleanup();
			return 1;
		}

		printf("New connection : %s\n", inet_ntoa(client_address.sin_addr));
		printf("Users: %d\n", clients_count + 1);

		bytes_read = recv(client_socket, client_usernames[clients_count], MAX_USERNAME_LENGTH, 0);
		if (bytes_read == SOCKET_ERROR) {
			printf("Failed to get request from client.\n");
			closesocket(client_socket);
			WSACleanup();
			return 1;
		}
		if (clients_count >= MAX_CLIENTS){
			printf("Server is full");
			if (send(client_socket, "Server is full. Try to connect again later", strlen("Server is full. Try to connect again later") + 1, 0) == SOCKET_ERROR) {
				printf("Failed to send message.\n");
				WSACleanup();
				return 1;
			}
			continue;
			ReleaseMutex(mutex);
		}
		else {

			client_sockets[clients_count] = client_socket;

			char new_connection_send[N] = "Новый пользователь под ником ";
			strcat(new_connection_send, client_usernames[clients_count++]);
			strcat(new_connection_send, " присоединился (IP: ");
			strcat(new_connection_send, inet_ntoa(client_address.sin_addr));
			strcat(new_connection_send, ")\n");

			for (int i = 0; i < clients_count; ++i) {
				if (client_sockets[i] != client_socket && send(client_sockets[i], new_connection_send, strlen(new_connection_send) + 1, 0) == SOCKET_ERROR) {
					printf("Failed to send message.\n");
					WSACleanup();
					return 1;
				}
			}

			ReleaseMutex(mutex);
			DWORD thread_id;
			CreateThread(NULL, NULL, client_handling, &client_socket, NULL, &thread_id);
		}
	}

	closesocket(client_socket);
	closesocket(listen_socket);
	WSACleanup();

	return 0;
}