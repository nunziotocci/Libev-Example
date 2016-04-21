#if defined _WIN32
	#define WIN32_LEAN_AND_MEAN

	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>

	#define ssize_t SSIZE_T
	#include <io.h>

	#define EV_CONFIG_H "ev_config.h.win32"
	#include "ev.c"
#else
	#include <wchar.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <netdb.h>

	#include <unistd.h>
	#include <errno.h>

	#define SOCKET int
	#define SOCKET_ERROR -1
	#define INVALID_SOCKET -1
	#include <ev.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>


#if defined _WIN32
	#pragma comment(lib, "Ws2_32.lib")
#endif

EV_P;
ev_io server_io;

typedef struct {
	ev_io io;
	SOCKET int_sock;
} client;

void client_cb(EV_P, ev_io *w, int revents) {
	client *_client = (client *)w;
	char buf1[1001] = {0};

	printf("reading from client...\n");

	int ret = recv(_client->int_sock, buf1, 1000, 0);
	if (ret == SOCKET_ERROR) {
		printf("recv failed, error: %d\n", errno);
		abort();
	}

	printf("read from client, ret: %d\n", ret);

	char *buf = "HTTP 200 OK\r\n\r\n";
	send(_client->int_sock, buf, strlen(buf), 0);
	printf("wrote >%s<\n", buf);
	send(_client->int_sock, buf1, strlen(buf1), 0);
	printf("wrote >%s<\n", buf1);

	ev_io_stop(EV_A, w);
	#if defined _WIN32
	_close(w->fd);
	#else
	close(_client->int_sock);
	#endif
	free(_client);
}

int setnonblock(SOCKET int_sock) {
	#if defined _WIN32
	u_long mode = 1;
	return ioctlsocket(int_sock, FIONBIO, &mode);
	#else
	int flags;
	flags = fcntl(int_sock, F_GETFL);
	flags |= O_NONBLOCK;
	return fcntl(int_sock, F_SETFL, flags);
	#endif
}

SOCKET int_sock;
void server_cb(EV_P, ev_io *w, int revents) {
	SOCKET int_client_sock;
	struct sockaddr_in client_address;
	socklen_t int_client_len = sizeof(client_address);
	client *client_io = NULL;

	while (1) {
		errno = 0;
		int_client_sock = accept(int_sock, (struct sockaddr *)&client_address, &int_client_len);
		if (int_client_sock == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
				printf("accept() failed errno: %i (%s)\012", errno, strerror(errno));
				abort();
			} else {
				errno = 0;
			}

			break;
		}

		printf("client accepted\n");

		errno = 0;
		client_io = malloc(sizeof(client));
		if (client_io == NULL) {
			printf("out of memory\n");
			abort();
		}
		client_io->int_sock = int_client_sock;
		setnonblock(int_client_sock);
		#if defined _WIN32
		int fd = _open_osfhandle(int_client_sock, 0);
		ev_io_init(&client_io->io, client_cb, fd, EV_READ);
		#else
		ev_io_init(&client_io->io, client_cb, int_client_sock, EV_READ);
		#endif
		ev_io_start(EV_A, &client_io->io);
	}
}

int main() {
	#if defined _WIN32
	WORD w_version_requested;
	WSADATA wsa_data;
	int err;
	w_version_requested = MAKEWORD(2, 2);

	err = WSAStartup(w_version_requested, &wsa_data);
	if (err != 0) {
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	#endif

	EV_A = ev_default_loop(0);
	if (EV_A == NULL) {
		printf("ev_default_loop failed!\n");
		goto error;
	}
	
	struct addrinfo hints;
	struct addrinfo *res;
	char bol_reuseaddr = 1;

	// Get address info
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int int_status = getaddrinfo(NULL, "8080", &hints, &res);
	if (int_status != 0) {
		wprintf(L"getaddrinfo failed: %d (%s)\n", int_status, gai_strerror(int_status));
		goto error;
	}

	// Get socket to bind
	int_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (int_sock == INVALID_SOCKET) {
		printf("Failed to create socket: %s (%d)\n", strerror(errno), errno);
		goto error;
	}

	if (setsockopt(int_sock, SOL_SOCKET, SO_REUSEADDR, &bol_reuseaddr, sizeof(int)) == -1) {
		printf("setsockopt failed: %s (%d)\n", strerror(errno), errno);
		goto error;
	}

	if (bind(int_sock, res->ai_addr, res->ai_addrlen) == -1) {
		printf("bind failed: %s (%d)\n", strerror(errno), errno);
		goto error;
	}

	freeaddrinfo(res);

	if (listen(int_sock, 10) == -1) {
		printf("listen failed: %s (%d)\n", strerror(errno), errno);
		goto error;
	}
	
	int int_ret;
	if ((int_ret = setnonblock(int_sock)) != 0) {
		printf("setnonblock failed: %d\n", int_ret);
		goto error;
	}

	#if defined _WIN32
	int fd = _open_osfhandle(int_sock, 0);
	ev_io_init(&server_io, server_cb, fd, EV_READ);
	#else
	ev_io_init(&server_io, server_cb, int_sock, EV_READ);
	#endif
	ev_io_start(EV_A, &server_io);

	ev_run(EV_A, 0);

	#if defined _WIN32
	_close(fd);
	#else
	close(int_sock);
	#endif

	#if defined _WIN32
	WSACleanup();
	#endif
	return 0;
error:
	#if defined _WIN32
	WSACleanup();
	#endif
	return 1;
}
