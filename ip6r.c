#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define MAX_PACKET_SIZE 2000 // includes ZEP header
#define ZEP_DEFAULT_PORT   17754

//#define IPV6
#ifdef IPV6
static const char *bcast = "ff05::1000";
//#define JOIN
//static const char *bcast = "::1";
int itype = AF_INET6;
#else
static const char *bcast = "0.0.0.0";
int itype = AF_INET;
#endif
static int zep_port = ZEP_DEFAULT_PORT;
extern uint8_t CPD_MAC[8], BPD_MAC[8];

struct zep_handles {
	int rx_handle;
	int tx_handle;
};

static int open_udp(const char *group, int port)
{
	int fd = socket(itype, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return 1;
	}


	zep_port = port;

	if (group == NULL) {

#ifdef IPV6
		u_int on = 1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on))< 0)
		{
			perror("setsockopt loop");
			return 1;
		}
		u_int ttl = 2;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl))< 0)
		{
			perror("setsockopt ttl");
			return 1;
		}
#endif
	} else {

	u_int buffer_size = 65536;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*) &buffer_size, sizeof(buffer_size))<0) {
		perror("setsockopt rcvbuf");
	}
	// allow multiple sockets to use the same PORT number
	//
	u_int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))< 0)
	{
		perror("setsockopt reuse");
		return 1;
	}
#ifdef JOIN
	// JOIN MEMBERSHIP
#ifdef IPV6
	struct ipv6_mreq mreq;
	memset(&mreq, 0, sizeof mreq);
	mreq.ipv6mr_interface = 0;
	inet_pton(AF_INET6, bcast, &mreq.ipv6mr_multiaddr);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof mreq)<0)
#else
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(bcast);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq)<0)
#endif
	{
		perror("setsockopt join");
		return 1;
	}
#endif

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
	}
	return fd;
}

struct zep_handles * phy_zep_init()
{
	struct zep_handles *zep = malloc(sizeof(struct zep_handles));

	if (zep == NULL) return zep;

	zep->rx_handle = open_udp(bcast, ZEP_DEFAULT_PORT);
	zep->tx_handle = open_udp(NULL, ZEP_DEFAULT_PORT);
	if (zep->rx_handle == 0 || zep->tx_handle == 0) {
		free(zep);
		return NULL;
	}
	return zep;
}
int phy_poll_rx(struct zep_handles *zep)
{
	fd_set rfds;
	struct timeval to;
	int rc;

	memset(&to, 0, sizeof(to));
	FD_ZERO(&rfds);
	FD_SET(zep->rx_handle, &rfds);

	rc = select(zep->rx_handle + 1, &rfds, NULL, NULL, &to);

	return rc;
}
int phy_rx_get(struct zep_handles *zep)
{

	struct mbuf *sdu;
	uint8_t *ptr, *dest;
	int bytes;
	socklen_t addrlen;

	struct sockaddr_in6 address;
	memset(&address, 0, sizeof address);
	address.sin6_family = AF_INET6;
	address.sin6_addr = in6addr_any;
	address.sin6_port = htons(ZEP_DEFAULT_PORT);
	addrlen = sizeof address;

	uint8_t buffer[MAX_PACKET_SIZE];
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	bytes = recvfrom(zep->rx_handle, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&address, &addrlen);
	if (bytes < 0)
	{
		if (errno == EAGAIN) return 0;
		perror("recvfrom");
		return 0;
	}

	return bytes;
}
main()
{

	int cnt = 0;
    struct zep_handles *zep  = phy_zep_init();
    while (1) {
		int x = phy_rx_get(zep);
		if (x) {
			++cnt;
			printf("Recv %d bytes\n", x);
		}


    }

    /* if we implement the signal handler to cleanly exit, clean up here */
    /* dgs_close(fd_serial); */

    return 0;


}
