#include "sys.h"

int
xread(int sd, void *buf, size_t len)
{
	char *p = (char *)buf;
	size_t nrecv = 0;
	ssize_t rv;
	
	while (nrecv < len) {
		rv = read(sd, p, len - nrecv);
		if (0 > rv && errno == EINTR)
			continue;
		if (0 > rv)
			return -1;
		if (0 == rv)
			return 0;
		nrecv += rv;
		p += rv;
	}
	return nrecv;
}

int
xwrite(int sd, const void *buf, size_t len)
{
	char *p = (char *)buf;
	size_t nsent = 0;
	ssize_t rv;
	
	while (nsent < len) {
		rv = write(sd, p, len - nsent);
		if (0 > rv && (errno == EINTR || errno == EAGAIN))
			continue;
		if (0 > rv)
			return -1;
		nsent += rv;
		p += rv;
	}
	return nsent;
}

void
call_if_present(char *name, char *lib)
{
	void *h;
	void (*f)();
	h = dlopen(lib, RTLD_NOW);
	if (!h)
		return;
	f = dlsym(h, name);
	if (!f)
		return;
	f();
}

void
fatal(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

void *
xmalloc(size_t size)
{
	void *p;
	p = malloc(size);
	if (!p)
		fatal("out of memory");
	bzero(p, size);
	return p;
}

char *
xstrdup(char *p)
{
	char *q;
	q = strdup(p);
	if(!q)
		fatal("out of memory");
	return q;
}

static struct {
	char *name;
	int num;
} map[] = {
	{ "0",          0         },
	{ "SIGHUP",     SIGHUP    },
	{ "SIGINT",     SIGINT    },
	{ "SIGQUIT",    SIGQUIT   },
	{ "SIGILL",     SIGILL    },
	{ "SIGTRAP",    SIGTRAP   },
	{ "SIGABRT",    SIGABRT   },
	{ "SIGIOT",     SIGIOT    },
	{ "SIGBUS",     SIGBUS    },
	{ "SIGFPE",     SIGFPE    },
	{ "SIGKILL",    SIGKILL   },
	{ "SIGUSR1",    SIGUSR1   },
	{ "SIGSEGV",    SIGSEGV   },
	{ "SIGUSR2",    SIGUSR2   },
	{ "SIGPIPE",    SIGPIPE   },
	{ "SIGALRM",    SIGALRM   },
	{ "SIGTERM",    SIGTERM   },
	{ "SIGSTKFLT",  SIGSTKFLT },
	{ "SIGCLD",     SIGCLD    },
	{ "SIGCHLD",    SIGCHLD   },
	{ "SIGCONT",    SIGCONT   },
	{ "SIGSTOP",    SIGSTOP   },
	{ "SIGTSTP",    SIGTSTP   },
	{ "SIGTTIN",    SIGTTIN   },
	{ "SIGTTOU",    SIGTTOU   },
	{ "SIGURG",     SIGURG    },
	{ "SIGXCPU",    SIGXCPU   },
	{ "SIGXFSZ",    SIGXFSZ   },
	{ "SIGVTALRM",  SIGVTALRM },
	{ "SIGPROF",    SIGPROF   },
	{ "SIGWINCH",   SIGWINCH  },
	{ "SIGPOLL",    SIGPOLL   },
	{ "SIGIO",      SIGIO     },
	{ "SIGPWR",     SIGPWR    },
	{ "SIGSYS",     SIGSYS    },
	{ NULL, 0 }
};

int
ckpt_mapsig(char *s)
{
	int i;
	i = 0;
	while(map[i].name) {
		if (!strcmp(map[i].name, s))
			return map[i].num;
		i++;
	}
	return -1;
}
