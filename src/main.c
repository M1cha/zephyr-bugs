#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#ifdef __ZEPHYR__
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define zsock_socket socket
#define zsock_bind bind
#define zsock_send send
#define zsock_poll poll
#define zsock_recv recv
#define zsock_connect connect

#define ZSOCK_POLLIN POLLIN
#define ZSOCK_MSG_DONTWAIT MSG_DONTWAIT
#define INADDR_LOOPBACK_INIT { .s_addr = htonl(INADDR_LOOPBACK), }

#define zsock_pollfd pollfd
#endif

static int server(struct sockaddr_in *const out_addr) {
	int ret;

	const int fd = zsock_socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		return -fd;
	}

	struct in_addr ipv4_loopback = INADDR_LOOPBACK_INIT;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr = ipv4_loopback,
		.sin_port = htons(10000),
	};
	*out_addr = addr;

	ret = zsock_bind(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (ret < 0) {
		perror("bind");
		return ret;
	}

	return fd;
}

static int client(struct sockaddr_in *const addr) {
	int ret;

	const int fd = zsock_socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		return -fd;
	}

	ret = zsock_connect(fd, (struct sockaddr *) addr, sizeof(*addr));
	if (ret < 0) {
		perror("connect");
		return ret;
	}

	return fd;
}

int main(void) {
	int ret;
	struct sockaddr_in address;

	const int server_fd = server(&address);
	if (server_fd < 0) {
		return server_fd;
	}

	fprintf(stderr, "port: %u\n", ntohs(address.sin_port));

	const int client_fd = client(&address);
	if (client_fd < 0) {
		return client_fd;
	}

	uint8_t data = 0x42;
	ret = zsock_send(client_fd, &data, sizeof(data), 0);
	if (ret != sizeof(data)) {
		perror("send");
		return ret;
	}

	for (size_t i=0; i<3; i+=1) {
		struct zsock_pollfd pollfd = {
			.fd = server_fd,
			.events = ZSOCK_POLLIN,
		};

		ret = zsock_poll(&pollfd, 1, 0);
		if (ret < 0) {
			perror("poll");
		}
		else if (ret == 0) {
			printf("no data\n");
		}
		else if (ret == 1) {
			printf("poll found data\n");
			break;
		}
		else {
			fprintf(stderr, "unexpected poll result: %d\n", ret);
		}

		sleep(1);
	}

	ssize_t nbytes = zsock_recv(server_fd, &data, sizeof(data), ZSOCK_MSG_DONTWAIT);
	if (nbytes < 0) {
		perror("recv");
	}
	else {
		fprintf(stderr, "recv: nbytes=%zd data=0x%02X\n", nbytes, data);
	}


	return 0;
}
