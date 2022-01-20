#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

enum {
	SELFOWNED = ~0
};

static struct tcpsock*
inode_to_tcp(unsigned long inode)
{
	struct tcpsock *sk;
	FILE *fp;
	char buf[1024];
	int state;
	unsigned lp, pp;
	unsigned long lh, ph;
	unsigned long i;
	int n = 0;

	fp = fopen("/proc/net/tcp", "r");
	if (!fp) {
		perror("open /proc/net/tcp");
		assert(0);
	}

	sk = NULL;
	while (fgets(buf, sizeof(buf), fp)) {
		if (!n++)
			continue; /* skip initial line of column labels */

		sscanf(buf, "%*s %lx:%x %lx:%x %x %*s %*s %*s %*s %*s %lu",
		       &lh, &lp, &ph, &pp, &state, &i);
		if (i != inode)
			continue;

		/* found it */
		sk = xmalloc(sizeof(struct tcpsock));

		/* unlike udp, the kernel prints tcp ports in host order;
		   see /usr/src/linux/net/ipv4/tcp_ipv4.c:^get_tcp_sock */
		lp = htons(lp);
		pp = htons(pp);

		sk->addr.sin_addr.s_addr = lh;
		sk->addr.sin_port = lp;
		sk->peer_addr.sin_addr.s_addr = ph;
		sk->peer_addr.sin_port = pp;
		sk->state = state;
		break;
	}
	fclose(fp);
	return sk;
}

static struct udpsock *
inode_to_udp(unsigned long inode)
{
	FILE *fp;
	struct udpsock *sk;
	char buf[1024];
	int n = 0;
	int state;
	unsigned lp, pp;
	unsigned long lh, ph, i;

	fp = fopen("/proc/net/udp", "r");
	if (!fp) {
		perror("open /proc/net/udp");
		assert(0);
	}

	sk = NULL;
	while (fgets(buf, sizeof(buf), fp)) {
		if (!n++)
			continue; /* skip initial line of column labels */
		sscanf(buf, "%*s %lx:%x %lx:%x %x %*s %*s %*s %*s %*s %lu",
		       &lh, &lp, &ph, &pp, &state, &i);
		if (i != inode)
			continue;

		/* found it */
		sk = xmalloc(sizeof(struct udpsock));
		sk->addr.sin_addr.s_addr = lh;
		sk->addr.sin_port = lp;
		sk->peer_addr.sin_addr.s_addr = ph;
		sk->peer_addr.sin_port = pp;
		sk->state = state;
		break;
	}
	fclose(fp);
	return sk;
}

static struct unixsock *
inode_to_unix(unsigned long inode)
{
	struct unixsock *sk;
	FILE *fp;
	char buf[1024];
	char pth[1024];
	int n = 0;
	int rv, flags, type, state;
	unsigned long i;

	fp = fopen("/proc/net/unix", "r");
	if (!fp) {
		perror("open /proc/net/unix");
		assert(0);
	}

	sk = NULL;
	while (fgets(buf, sizeof(buf), fp)) {
		if (!n++)
			continue; /* skip initial line of column labels */
		rv = sscanf(buf, "%*s %*s %*s %x %x %x %lu %s",
			    &flags, &type, &state, &i, pth);
		if (i != inode)
			continue;

		/* found it */
		sk = xmalloc(sizeof(struct unixsock));
		sk->flags = flags;
		sk->type = type;
		sk->state = state;
		if (rv == 5)
			sk->path = xstrdup(pth);
		break;
	}
	fclose(fp);
	return sk;
}

static void
type_socket(struct ckptfd *fdp, unsigned long inode)
{
	struct tcpsock *tcp;
	struct udpsock *udp;
	struct unixsock *un;

	if ((tcp = inode_to_tcp(inode))) {
		fdp->type = CKPT_FD_TCPSOCK;
		fdp->u.tcp = tcp;
	} else if ((udp = inode_to_udp(inode))) {
		fdp->type = CKPT_FD_UDPSOCK;
		fdp->u.udp = udp;
	} else if ((un = inode_to_unix(inode))) {
		fdp->type = CKPT_FD_UNIXSOCK;
		fdp->u.un = un;
	} else
		fdp->type = CKPT_FD_UNKNOWN;
}

static void
type_pipe(struct ckptfd *fdp, unsigned long inode)
{
	struct pipe *p;

	fdp->type = CKPT_FD_PIPE;
	fdp->u.pip = p = xmalloc(sizeof(struct pipe));
	p->inode = inode;
}

static void
type_dev(struct ckptfd *fdp)
{
	fdp->type = CKPT_FD_DEV;
}

static void
type_filename(struct ckptfd *fdp, char *filename)
{
	struct regular *r;
	char buf[1024];
	struct stat st;
	static int st_to_flags[] = {
		0, 0,
		O_WRONLY, O_WRONLY,
		O_RDONLY, O_RDONLY,
		O_RDWR, O_RDWR
	};

	fdp->type = CKPT_FD_REGULAR;
	fdp->u.reg = r = xmalloc(sizeof(struct regular));
	r->path = xstrdup(filename);
	sprintf(buf, "/proc/self/fd/%d", fdp->fd);
	if (0 > lstat(buf, &st)) {
		perror("stat");
		assert(0);
	}
	r->mode = st_to_flags[(st.st_mode & 0700) >> 6];
	r->offset = lseek(fdp->fd, 0, SEEK_CUR);
	if (r->offset == (off_t)-1) {
		perror("lseek");
		assert(0);
	}
}

static void
type_file(struct ckptfd *fdp, char *name)
{
	unsigned long inode;
	if (1 == sscanf(name, "socket:[%lu]", &inode)) {
		type_socket(fdp, inode);
		return;
	}
	if (1 == sscanf(name, "pipe:[%lu]", &inode)) {
		type_pipe(fdp, inode);
		return;
	}
	if (!strncmp(name, "/dev", 4)) {
		type_dev(fdp);
		return;
	}
	type_filename(fdp, name);
}

static void
free_fd(struct ckptfd *fdp)
{
	switch (fdp->type) {
	case CKPT_FD_REGULAR:
		free(fdp->u.reg->path);
		free(fdp->u.reg);
		break;
	case CKPT_FD_DEV:
		break;
	case CKPT_FD_PIPE:
		free(fdp->u.pip);
		break;
	case CKPT_FD_TCPSOCK:
		free(fdp->u.tcp);
		break;
	case CKPT_FD_UDPSOCK:
		free(fdp->u.udp);
		break;
	case CKPT_FD_UNIXSOCK:
		if(fdp->u.un->path)
			free(fdp->u.un->path);
		free(fdp->u.un);
		break;
	case CKPT_FD_UNKNOWN:
		break;
	default:
		assert(0);
	}
}

static void
defaultpolicy(struct ckptfdtbl *tbl)
{
	int i;
	struct ckptfd *fdp;

	for(i = 0, fdp = tbl->fds; i < tbl->nfd; i++, fdp++){
		switch (fdp->type) {
		case CKPT_FD_REGULAR:
			fdp->treatment = CKPT_FD_RESTORE;
			break;
		case CKPT_FD_DEV:
		case CKPT_FD_PIPE:
		case CKPT_FD_TCPSOCK:
		case CKPT_FD_UDPSOCK:
		case CKPT_FD_UNIXSOCK:
		case CKPT_FD_UNKNOWN:
			fdp->treatment = CKPT_FD_IGNORE;
			break;
		default:
			assert(0);
		}
	}
}

struct ckptfdtbl *
ckpt_getfdtbl()
{
	struct ckptfdtbl *tbl;
	struct ckptfd *fdp;
	DIR *dir;
	struct dirent *e;
	char s[1024];
	char buf[1024];
	int rv;

	tbl = xmalloc(sizeof(struct ckptfdtbl));
	snprintf(s, sizeof(s), "/proc/self/fd");
	dir = opendir(s);
	if (!dir) {
		fprintf(stderr, "cannot open %s\n", s);
		return NULL;
	}

	/* count fds held open */
	while((e = readdir(dir))){
		/* don't count the fd for the dir */
		if(atoi(e->d_name) == dirfd(dir))
			continue;
		if(strcmp(e->d_name, ".") == 0)
			continue;
		if(strcmp(e->d_name, "..") == 0)
			continue;
		tbl->nfd++;
	}

	rewinddir(dir);
	tbl->fds = xmalloc(tbl->nfd*sizeof(struct ckptfd));

	fdp = tbl->fds;
	while ((e = readdir(dir))) {

		if(atoi(e->d_name) == dirfd(dir))
			continue;
		if(strcmp(e->d_name, ".") == 0)
			continue;
		if(strcmp(e->d_name, "..") == 0)
			continue;

		snprintf(s, sizeof(s), "/proc/self/fd/%s", e->d_name);
		rv = readlink(s, buf, sizeof(buf));
		if (0 > rv)
			continue;
		if (rv >= sizeof(buf)) {
			fprintf(stderr, "buffer overflow\n");
			assert(0);
		}
		buf[rv] = '\0';

		fdp->fd = atoi(e->d_name);

		errno = 0;
		fdp->flags = fcntl(fdp->fd, F_GETFL);
		if (errno) {
			perror("fcntl(F_GETFL)");
			assert(0);
		}
#if 0
		fdp->owner = fcntl(fdp->fd, F_GETOWN);
		if (errno) {
			perror("fcntl(F_GETOWN)");
			assert(0);
		}
		if (fdp->owner == getpid())
			fdp->owner = SELFOWNED;
		fdp->sig = fcntl(fdp->fd, F_GETSIG);
		if (errno) {
			perror("fcntl(F_GETSIG)");
			assert(0);
		}
#endif
		type_file(fdp, buf);
		
		fdp++;
	}
	closedir(dir);

	defaultpolicy(tbl);

	return tbl;
}

void
ckpt_freefdtbl(struct ckptfdtbl *tbl)
{
	int i;
	for (i = 0; i < tbl->nfd; i++)
		free_fd(&tbl->fds[i]);
	free(tbl);
}

static void
restore_regular(struct ckptfd *fdp)
{
	int fd, rv;
	struct regular *r;

	r = fdp->u.reg;
	fd = open(r->path, r->mode);
	if (0 > fd) {
		perror("open");
		assert(0);
	}
	if ((off_t)-1 == lseek(fd, r->offset, SEEK_SET)) {
		perror("lseek");
		assert(0);
	}
	
	rv = fcntl(fd, F_SETFL, fdp->flags);
	if (0 > rv) {
		perror("fcntl(F_SETFL)");
		assert(0);
	}

#if 0
	rv = fcntl(fd, F_SETOWN,
		   fdp->owner == SELF ? getpid() : fdp->owner);
	if (0 > rv) {
		perror("fcntl(F_SETOWNER)");
		assert(0);
	}
	rv = fcntl(fd, F_SETSIG, fdp->sig);
	if (0 > rv) {
		perror("fcntl(F_SETSIG)");
		assert(0);
	}
#endif

	if (fd != fdp->fd) {
		rv = dup2(fd, fdp->fd);
		if (0 > rv) {
			perror("dup2");
			assert(0);
		}
		close(fd);
	}
}

void
ckpt_restorefdtbl(struct ckptfdtbl *tbl)
{
	int i;
	struct ckptfd *fdp;

	for(i = 0, fdp = tbl->fds; i < tbl->nfd; i++, fdp++){
		if(fdp->treatment == CKPT_FD_IGNORE)
			continue;

		switch (fdp->type) {
		case CKPT_FD_REGULAR:
			restore_regular(fdp);
			break;
		case CKPT_FD_DEV:
		case CKPT_FD_PIPE:
		case CKPT_FD_TCPSOCK:
		case CKPT_FD_UDPSOCK:
		case CKPT_FD_UNIXSOCK:
		case CKPT_FD_UNKNOWN:
			fprintf(stderr,
				"no built-in support for restoring non-regular file descriptors\n");
			continue;
		default:
			assert(0);
		}
	}
}
