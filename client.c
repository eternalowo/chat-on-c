#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#define DEFAULT_PORT 2002
#define N 16384 
#define MAX_USERNAME_LENGTH 64

HANDLE mutex;

char username[MAX_USERNAME_LENGTH] = "\0";

DWORD WINAPI recieve_handling(LPVOID client_socket_voidp) {

	setlocale(LC_ALL, "Russian");

	int bytes_read = 0;
	char response[N] = "\0";

	SOCKET client_socket = *((SOCKET*)client_socket_voidp);
	while (1) {
		bytes_read = recv(client_socket, response, N, 0);
		WaitForSingleObject(mutex, INFINITE);
		if (bytes_read == SOCKET_ERROR) {
			printf("Failed to get request from client.\n");
			WSACleanup();
			return -1;
		}
		else if (strcmp(response, "Server is full. Try to connect again later") == 0) {
			printf("Server is full. Try to connect again later");
			ReleaseMutex(mutex);
			break;
		}
		printf("%s", response);
		ReleaseMutex(mutex);
		memset(response, 0, N);
	}
	return 1;
}

DWORD WINAPI send_handling(LPVOID client_socket_voidp) {

	setlocale(LC_ALL, "Russian");

	int bytes_sent = 0;
	char request[N] = "\0";

	SOCKET client_socket = *((SOCKET*)client_socket_voidp);

	while (1) {
		while (_getch() != '\r');
		WaitForSingleObject(mutex, INFINITE);
		fgets(request, N, stdin);
		ReleaseMutex(mutex);

		if (request[0] == '\n' || request[0] == '\0') {
			continue;
		}

		bytes_sent = send(client_socket, request, strlen(request) + 1, 0);
		if (bytes_sent == SOCKET_ERROR) {
			printf("Failed to send message.\n");
			WSACleanup();
			return -1;
		}
		if (strcmp(request, "/quit") == 0) {
			break;
		}
	}
	return 1;
}

int main() {

	system("chcp 1251 > NUL");
	setlocale(LC_ALL, "Russian");

	WSADATA WSAData;
	SOCKET client_socket;
	struct sockaddr_in server_address;

	int bytes_sent;
	char ip_input[16] = "\0";
	char port_input[6] = "\0";
	char* endptr;
	int port = DEFAULT_PORT;

	mutex = CreateMutex(NULL, FALSE, NULL);
	if (mutex == NULL) {
		printf("Failed to create mutex.\n");
		WSACleanup();
		return 1;
	}

	printf("Введите IP-адрес сервера: ");
	fgets(ip_input, 16, stdin);

	printf("Введите порт сервера: ");
	fgets(port_input, 6, stdin);
	port_input[strcspn(port_input, "\n")] = '\0';

	port = strtol(port_input, &endptr, 10);

	if (!(port >= 1024 && port <= 65535)) {
		printf("Incorrect port");
		return 1;
	}

	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) {
		printf("Unable to initalize Winsock\n");
		return 1;
	}

	client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET) {
		printf("Unable to initialize socket\n");
		WSACleanup();
		return 1;
	}

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = inet_addr(ip_input);

	WaitForSingleObject(mutex, INFINITE);
	if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
		printf("Unable to connect to server\n");
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	printf("Введите Ваш никнейм: ");
	fgets(username, MAX_USERNAME_LENGTH, stdin);

	size_t length = strlen(username);
	if (length > 0 && username[length - 1] == '\n') {
		username[length - 1] = '\0';
	}

	bytes_sent = send(client_socket, username, length + 1, 0);

	if (bytes_sent == SOCKET_ERROR) {
		printf("Failed to send username.\n");
		WSACleanup();
		return 1;
	}
	ReleaseMutex(mutex);

	printf("Подключение к чату прошло успешно!\nДля разрыва соединения введите \"/quit\"\nПеред вводом каждого сообщения необходимо нажимать Enter\n");
	DWORD thread_id;
	HANDLE thread1 = CreateThread(NULL, NULL, send_handling, &client_socket, NULL, &thread_id);
	HANDLE thread2 = CreateThread(NULL, NULL, recieve_handling, &client_socket, NULL, &thread_id);
	WaitForSingleObject(thread1, INFINITE);
	WaitForSingleObject(thread2, INFINITE);
	closesocket(client_socket);
	WSACleanup();

	return 0;
}