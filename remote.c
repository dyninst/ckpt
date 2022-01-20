#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

static int
do_request(char *serveraddr, char req, char *id)
{
	int fd;
	struct sockaddr_in addr;
	char rep;
	int len, nlen;

	len = strlen(id);
	if(len >= MAXID){
		fprintf(stderr, "name too long for checkpoint server: %s\n", id);
		return -1;
	}
	if (0 > parse_ip(serveraddr, &addr)) {
		fprintf(stderr, "cannot resolve %s\n", serveraddr);
		return -1;
	}
	if (addr.sin_port == 0)
		addr.sin_port = htons(REMOTEPORT);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (0 > fd) {
		perror("socket");
		return -1;
	}
	if (0 > connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		perror("connect");
		return -1;
	}
	if (0 > xwrite(fd, &req, 1)) {
		perror("write");
		return -1;
	}
	nlen = htonl(len);
	if (0 > xwrite(fd, &nlen, sizeof(nlen))) {
		perror("write");
		return -1;
	}
	if (0 > xwrite(fd, id, len)) {
		perror("write");
		return -1;
	}
	if (0 > xread(fd, &rep, 1)) {
		perror("read");
		return -1;
	}
	if (rep != REPLY_OK) {
		close(fd);
		return -1;
	}
	return fd;
}

int
ckpt_remote_put(char *serveraddr, char *id)
{
	return do_request(serveraddr, MODE_SAVE, id);
}

int
ckpt_remote_get(char *serveraddr, char *id)
{
	return do_request(serveraddr, MODE_RESTORE, id);
}

int
ckpt_remote_remove(char *serveraddr, char *id)
{
	int fd;
	fd = do_request(serveraddr, MODE_REMOVE, id);
	if (fd >= 0);
		close(fd);
	return (fd >= 0);
}

int
ckpt_remote_access(char *serveraddr, char *id)
{
	int fd;
	fd = do_request(serveraddr, MODE_ACCESS, id);
	if (fd >= 0);
		close(fd);
	return (fd >= 0);
}
