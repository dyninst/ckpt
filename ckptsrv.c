#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

static char *ckptdir = "/tmp";

struct req {
	char req;
	char id[MAXID];
};

static int
reuseaddr(int sd)
{
	int optval = 1;
	if (0 > setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
			   &optval, sizeof(optval)))
		return -1;
	return 0;
}

static void
mkfilename(char *buf, char *id)
{
	sprintf(buf, "%s/ckpt%s", ckptdir, id);
}

static int
suck(int from, int to)
{
	int rv;
	static char buf[32000];

	while (1) {
		rv = read(from, buf, sizeof(buf));
		if (0 == rv)
			return 0;
		if (0 > rv)
			return -1;
		if (0 > xwrite(to, buf, rv))
			return -1;
	}
}

static void
repok(int fd)
{
	char rep;
	rep = REPLY_OK;
	xwrite(fd, &rep, 1);
}

static void
repfail(int fd)
{
	char rep;
	rep = REPLY_FAIL;
	xwrite(fd, &rep, 1);
}

static void
save(int sd, struct req *req)
{
	int fd;
	char buf[32000];
	
	mkfilename(buf, req->id);
	fd = open(buf, O_CREAT|O_TRUNC|O_WRONLY, 0600);
	if (0 > fd) {
		fprintf(stderr, "cannot open %s for saving\n", buf);
		repfail(sd);
		close(sd);
		return;
	}
	repok(sd);
	suck(sd, fd);
	close(fd);
	close(sd);
}

static void
restore(int sd, struct req *req)
{
	int fd;
	char buf[32000];
	
	mkfilename(buf, req->id);
	fd = open(buf, O_RDONLY);
	if (0 > fd) {
		repfail(sd);
		close(sd);
		return;
	}
	repok(sd);
	suck(fd, sd);
	close(fd);
	close(sd);
}

static void
acc(int sd, struct req *req)
{
	char buf[1024];

	mkfilename(buf, req->id);
	if (0 > access(buf, R_OK))
		repfail(sd);
	else
		repok(sd);
	close(sd);
}

static void
rem(int sd, struct req *req)
{
	char buf[1024];

	mkfilename(buf, req->id);
	if (0 > unlink(buf))
		repfail(sd);
	else
		repok(sd);
	close(sd);
}

static void
do_server(int fd, struct req *req)
{
	switch (req->req) {
	case MODE_SAVE:
		save(fd, req);
		break;
	case MODE_RESTORE:
		restore(fd, req);
		break;
	case MODE_ACCESS:
		acc(fd, req);
		break;
	case MODE_REMOVE:
		rem(fd, req);
		break;
	default:
		repfail(fd);
		close(fd);
		break;
	}
}

static int
server(int fd)
{
	struct req req;
	int len;
	memset(&req, 0, sizeof(req));
	if (0 >= xread(fd, &req.req, 1)) {
		fprintf(stderr, "server read failed\n");
		close(fd);
		return 0;
	}
	if (0 >= xread(fd, &len, sizeof(len))) {
		fprintf(stderr, "server read failed\n");
		close(fd);
		return 0;
	}
	len = ntohl(len);
	if (len >= MAXID) {
		fprintf(stderr, "id too long\n");
		close(fd);
		return 0;
	}

	if (0 >= xread(fd, &req.id, len)) {
		fprintf(stderr, "server read failed\n");
		close(fd);
		return 0;
	}


	do_server(fd, &req);
	return 0;
}

static void
sigchld(int sig)
{
	int rv;
	do
		rv = waitpid(0, NULL, WNOHANG);
	while (rv > 0);
}

static void
usage()
{
	fprintf(stdout, "Usage: cssrv [-p port] [-d ckptdir]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in saddr, caddr;
	int s, c, rv;
	socklen_t len;
	fd_set fds;
	pid_t pid;
	struct sigaction sa;
	unsigned short port = 0;

	opterr = 0;
	optind = 0;
	while (EOF != (c = getopt(argc, argv, "p:d:")))
		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			ckptdir = optarg;
			break;
		case '?':
			fprintf(stdout, "Unknown option `-%c'.\n", optopt);
			usage();
			break;
		}

	bzero(&sa, sizeof(sa));
	sa.sa_handler = sigchld;
	if (0 > sigaction(SIGCHLD, &sa, NULL)) {
		fprintf(stderr, "cannot install sighandler\n");
		exit(1);
	}

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	if (port)
		saddr.sin_port = htons(port);
	else
		saddr.sin_port = htons(REMOTEPORT);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (0 > s) {
		perror("socket");
		exit(1);
	}
	reuseaddr(s);
	if (0 > bind(s, (struct sockaddr *) &saddr, sizeof(saddr))) {
		perror("bind");
		exit(1);
	}
	if (0 > listen(s, 1)) {
		perror("listen");
		exit(1);
	}

	while (1) {
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		rv = select(s+1, &fds, NULL, NULL, NULL);
		if (0 > rv && errno == EINTR)
			continue;
		if (0 > rv) {
			perror("select");
			exit(1);
		}
		c = accept(s, (struct sockaddr *) &caddr, &len);
		if (0 > c) {
			perror("accept");
			exit(1);
		}
		pid = fork();
		if (0 > pid) {
			perror("fork");
			exit(1);
		}
		if (!pid) {
			len = sizeof(caddr);
			close(s);
			exit(server(c));
		}
		close(c);
	}

	exit(0);
}
