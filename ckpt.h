#include <netinet/ip.h>

enum {
	CKPT_MAXNAME=1024,
	CKPT_NAME=(1<<1),
	CKPT_ASYNCSIG=(1<<2),
	CKPT_CONTINUE=(1<<3),
	CKPT_MSPERIOD=(1<<4),
};

struct ckptconfig {
	/* For users */
	char name[CKPT_MAXNAME];
	unsigned int asyncsig;
	unsigned int continues;
	unsigned int msperiod;
	int flags;

	/* For internal use */
//	char tryname[CKPT_MAXNAME];
//	char exename[CKPT_MAXNAME];
//	unsigned long ckptcnt;
};
void ckpt_config(struct ckptconfig *cfg, struct ckptconfig *old);
int ckpt_ckpt(char *name);
void ckpt_restart(char *name);

typedef void (*fn_t)(void *);
void ckpt_on_preckpt(fn_t f, void *arg);
void ckpt_on_postckpt(fn_t f, void *arg);
void ckpt_on_restart(fn_t f, void *arg);

/* fd.c */
enum {
	CKPT_FD_REGULAR=0,
	CKPT_FD_DEV,
	CKPT_FD_PIPE,
	CKPT_FD_TCPSOCK,
	CKPT_FD_UDPSOCK, 
	CKPT_FD_UNIXSOCK,
	CKPT_FD_UNKNOWN,
	CKPT_FD_NUM_TYPES,

	CKPT_FD_RESTORE=0,
	CKPT_FD_IGNORE
} fdtype;

struct tcpsock {
	unsigned long inode;
	int state;
	struct sockaddr_in addr;
	struct sockaddr_in peer_addr;
};

struct udpsock {
	unsigned long inode;
	int state;
	struct sockaddr_in addr;
	struct sockaddr_in peer_addr;
};

struct unixsock {
	unsigned long inode;
	unsigned long peer_inode;
	int flags;
	int type;
	int state;
	char *path;
};

struct pipe {
	unsigned long inode;
};

struct regular {
	char *path;
	int mode;
	off_t offset;
};

struct ckptfdtbl {
	unsigned nfd;           /* number of fds */
	struct ckptfd *fds;     /* array of fds */
};

struct ckptfd {
	int fd;                 /* file descriptor number */
	int treatment;          /* CKPT_FD_RESTORE or CKPT_FD_IGNORE */
	int type;               /* CKPT_FD_REGULAR, ... */
	int flags;		/* F_GETFL flags */
	int owner;		/* F_GETOWN owner */
	int sig;		/* F_GETSIG signal */
	union {
		struct tcpsock *tcp;   /* CKPT_FD_TCPSOCK */
		struct udpsock *udp;   /* CKPT_FD_UDPSOCK */
		struct unixsock *un;   /* CKPT_FD_UNIXSOCK */
		struct pipe *pip;      /* CKPT_FD_PIPE */
		struct regular *reg;   /* CKPT_FD_REGULAR */
	} u;
};

struct ckptfdtbl * ckpt_fds();
