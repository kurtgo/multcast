#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/net.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/net.h>
#include <asm-generic/unistd.h>
#include <errno.h>
#include <sys/select.h>

#define IPV6
#ifdef IPV6
const char *bcast = "ff12::1234"; //"ff12::1234"
int itype = AF_INET6;
#else
const char *bcast = "239.0.0.1";
int itype = AF_INET;
#endif

const int port = 17754;

#ifndef NEW
int os_direct_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	return syscall(SYS_socketcall, SYS_SETSOCKOPT, &fd);
}

int os_direct_socket(int domain, int type, int protocol)
{
	int s = syscall(SYS_socketcall, SYS_SOCKET, &domain);
	return s;
}

int os_direct_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return syscall(SYS_socketcall, SYS_BIND, &fd);
}
ssize_t os_direct_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t alen)
{
	return syscall(SYS_socketcall, SYS_SENDTO, &fd);
}
ssize_t os_direct_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t * alen)
{
	return syscall(SYS_socketcall, SYS_RECVFROM, &fd);
}
ssize_t os_direct_recv(int fd, void *buf, size_t len, int flags)
{
	return os_direct_recvfrom(fd, buf, len, flags, 0, 0);
}

#else

int os_direct_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	return syscall(__NR_setsockopt, fd, level, optname, optval, optlen, 0);
}

int os_direct_socket(int domain, int type, int protocol)
{
	int s = syscall(SYS_socketcall, __NR_socket, domain, type, protocol, 0, 0, 0);
	if (s<0 && (errno==EINVAL || errno==EPROTONOSUPPORT)
	    && (type&(SOCK_CLOEXEC|SOCK_NONBLOCK))) {
		s = syscall(SYS_socketcall,socket, domain,
			type & ~(SOCK_CLOEXEC|SOCK_NONBLOCK),
			protocol, 0, 0, 0);
		if (s < 0) return s;
		if (type & SOCK_CLOEXEC)
			syscall(SYS_fcntl, s, F_SETFD, FD_CLOEXEC);
		if (type & SOCK_NONBLOCK)
			syscall(SYS_fcntl, s, F_SETFL, O_NONBLOCK);
	}
	return s;
}

int os_direct_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return syscall(__NR_bind, fd, addr, len, 0, 0, 0);
}
ssize_t os_direct_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t alen)
{
	return syscall(__NR_sendto, fd, buf, len, flags, addr, alen);
}
ssize_t os_direct_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t * alen)
{
	return syscall(__NR_recvfrom, fd, buf, len, flags, addr, alen);
}
ssize_t os_direct_recv(int fd, void *buf, size_t len, int flags)
{
	return os_direct_recvfrom(fd, buf, len, flags, 0, 0);
}
#endif
int os_direct_open_udp(const char *group, int port)
{
	struct sockaddr_in6 zep_address;

	int fd = os_direct_socket(AF_INET6, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return 1;
	}

	// allow multiple sockets to use the same PORT number
	//
	u_int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0)
	{
		perror("Reusing ADDR failed");
		return 1;
	}

	// JOIN MEMBERSHIP
	struct ipv6_mreq groupl;
	groupl.ipv6mr_interface = 0;
	inet_pton(AF_INET6, "ff12::1234", &groupl.ipv6mr_multiaddr);
	if (os_direct_setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &groupl, sizeof groupl) < 0 ) {
		perror("membership");
		return 1;
	}

	// set up destination address
	//

	memset(&zep_address, 0, sizeof zep_address);
	zep_address.sin6_family  = AF_INET6;
	zep_address.sin6_port = htons(port);


	// bind to receive address
	//
	if (os_direct_bind(fd, (struct sockaddr*) &zep_address, sizeof(zep_address)) < 0)
	{
		perror("bind1");
		return 1;
	}


	return fd;
}
int os_direct_send(int fd, void *buf,  int size, int flags, const char *address, int port)
{
	struct sockaddr_in6 zep_address;

	// set up destination address
	//

	memset(&zep_address, 0, sizeof zep_address);
	zep_address.sin6_family  = AF_INET6;
	zep_address.sin6_port = htons(port);
	inet_pton(AF_INET6, address, &zep_address.sin6_addr);

	os_direct_sendto(fd, buf, size, flags, (struct sockaddr *)&zep_address, sizeof(zep_address));
}



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
	int fd2 = os_direct_open_udp(bcast, port);

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
		perror("bind2");
		return 1;
	}

// JOIN MEMBERSHIP
#ifdef IPV6
	struct ipv6_mreq group;
	memset(&group, 0, sizeof group);
	group.ipv6mr_interface = 0;
	inet_pton(AF_INET6, bcast, &group.ipv6mr_multiaddr);
	setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof group);
#else
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(bcast);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
#endif

	while (1) {

		// READ
		struct sockaddr_in6 s;
		socklen_t l = sizeof(s);
		int bytes;

		char buffer[1280];
		memset(&s, 0, sizeof s);;
		bytes = recvfrom(fd, buffer, sizeof buffer, 0, (struct sockaddr*)&s, &l);
		printf("Recv: %d bytes\n", bytes);
		os_direct_send(fd2, buffer, sizeof buffer, 0, bcast, port);
	}
}

