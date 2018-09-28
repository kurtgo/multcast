#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef IPV6
const char *bcast = "ff15::1234"; //"ff12::1234"
int itype = AF_INET6;
#else
const char *bcast = "239.0.0.1";
int itype = AF_INET;
#endif

const int port = 17754;

int poll_rx(int zep_fd) {
	fd_set rfds;
	struct timeval to;
	int rc;

	memset(&to, 0, sizeof(to));
	to.tv_usec = 10;
	FD_ZERO(&rfds);
	FD_SET(zep_fd, &rfds);

	rc = select(zep_fd + 1, &rfds, NULL, NULL, &to);
	if (FD_ISSET(zep_fd, &rfds)) {
		return 1;
	}

	return rc;
}
main() {
// OPEN
	int fd = socket(itype, SOCK_DGRAM, 0);

// BIND
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		perror("setsockopt");
		return 1;
	}

#ifdef IPV6
	struct sockaddr_in6 address;
	memset(&address, 0, sizeof address);
	address.sin6_family = AF_INET6;
	address.sin6_addr = in6addr_any;
	address.sin6_port = htons(port);
#else
	struct sockaddr_in address;
	memset(&address, 0, sizeof address);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
	address.sin_port = htons(port);
#endif
	if (bind(fd, (struct sockaddr*) &address, sizeof address) < 0) {
		perror("bind");
		return 1;
	}

// JOIN MEMBERSHIP
#ifdef IPV6
	struct ipv6_mreq group;
	memset(&group, 0, sizeof group);
	group.ipv6mr_interface = 0;
	inet_pton(AF_INET6, bcast, &group.ipv6mr_multiaddr);
	setsockopt(fd, IP
			PROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof group);
#else
    struct ip_mreqn mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(bcast);
    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    mreq.imr_ifindex = 0;
	setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
#endif

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &one, sizeof(one)) < 0) {
		perror("setsockopt");
		return 1;
	}

	do {
		if (poll_rx(fd) > 0) {
			// READ
			char buffer[1280];
			read(fd, buffer, sizeof buffer);
			printf("Recv: %s\n", buffer);
		}
		{
// ADDRESS

#ifdef IPV6
			struct sockaddr_in6 address = { AF_INET6, htons(port) };
			inet_pton(AF_INET6, bcast, &address.sin6_addr);
#else
			struct sockaddr_in address;
			memset(&address, 0, sizeof address);
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = inet_addr(bcast); // differs from sender
			address.sin_port = htons(port);
#endif

// SEND TO
			char buffer[128];
			strcpy(buffer, "hello world!");
			sendto(fd, buffer, sizeof buffer, 0, (struct sockaddr*) &address,
					sizeof address);
		}
		sleep(10);
	} while (1);
}

