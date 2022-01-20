#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

extern char libckpt[];
extern unsigned long libckptlen;

enum {
	MAXBUF=1024
};

static void
stashlib(char *buf, unsigned long sz)
{
	int fd, rv;
	
	snprintf(buf, sz, "/tmp/tmplibckptXXXXXX");
	fd = mkstemp(buf);
	if(0 > fd){
		fprintf(stderr, "ckpt I/O error\n");
		exit(1);
	}
	rv = xwrite(fd, libckpt, libckptlen);
	if(0 > rv){
		fprintf(stderr, "ckpt I/O error\n");
		exit(1);
	}
	close(fd);
}

static void
dopreload(struct ckptconfig *cfg)
{
	char s[] = "LD_PRELOAD=";
	char buf[sizeof(s)+sizeof(struct ckptconfig)+1];

	if(cfg->flags&CKPT_NAME){
		snprintf(buf, sizeof(buf), "CKPT_NAME=%s", cfg->name);
		putenv(xstrdup(buf));
	}
	if(cfg->flags&CKPT_CONTINUE){
		snprintf(buf, sizeof(buf), "CKPT_CONTINUE=%d", cfg->continues);
		putenv(xstrdup(buf));
	}
	if(cfg->flags&CKPT_ASYNCSIG){
		snprintf(buf, sizeof(buf), "CKPT_ASYNCSIG=%d", cfg->asyncsig);
		putenv(xstrdup(buf));
	}
	if(cfg->flags&CKPT_MSPERIOD){
		snprintf(buf, sizeof(buf), "CKPT_MSPERIOD=%d", cfg->msperiod);
		putenv(xstrdup(buf));
	}

	snprintf(buf, sizeof(buf), "%s", s);
	stashlib(buf+strlen(s), sizeof(buf)-strlen(s));
	if(0 > putenv(xstrdup(buf))){
		fprintf(stderr, "ckpt error\n");
		exit(1);
	}
}

static void
dohijack(int pid, struct ckptconfig *cfg)
{
	char buf[MAXBUF];

	stashlib(buf, sizeof(buf));
	if(0 > hijack(pid, buf)){
		fprintf(stderr, "cannot hijack %d\n", pid);
		exit(1);
	}

	if(0 > hijack_call(pid, "ckpt_rconfig", cfg, sizeof(struct ckptconfig))){
		fprintf(stderr, "cannot hijack %d\n", pid);
		exit(1);
	}

	unlink(buf);
}


static void
usage()
{
	fprintf(stderr,
		"usage: ckpt [-n filename] [-a signal] [-c] [-z period] <program> <args>\n");
	fprintf(stderr,
		"usage: ckpt [-n filename] [-a signal] [-c] [-z period] -p <pid>\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	int pid = 0;
	struct ckptconfig cfg;
	char *p;
	
	memset(&cfg, 0, sizeof(cfg));
	opterr = 0;
	optind = 0;
	/* `+' means don't reorder argv */
	while (EOF != (c = getopt(argc, argv, "+n:cp:a:z:")))
		switch (c) {
		case 'p':
			pid = atoi(optarg);
			break;
		case 'n':
			if(strlen(optarg) > sizeof(cfg.name)-1){
				fprintf(stderr,
					"CKPT_NAME must be less than %d chars\n",
					sizeof(cfg.name));
				exit(1);
			}
			snprintf(cfg.name, sizeof(cfg.name), "%s", optarg);
			cfg.flags |= CKPT_NAME;
			break;
		case 'c':
			cfg.continues = 1;
			cfg.flags |= CKPT_CONTINUE;
			break;
		case 'a':
			cfg.asyncsig = ckpt_mapsig(optarg);
			if(cfg.asyncsig == -1){
				fprintf(stderr,
					"Bad value for CKPT_ASYNCSIG: %s\n", optarg);
				exit(1);
			}
			cfg.flags |= CKPT_ASYNCSIG;
			break;
		case 'z':
			cfg.msperiod = strtoul(optarg, &p, 0);
			if(*p != 0){
				fprintf(stderr,
					"Bad value for CKPT_MSPERIOD: %s\n", optarg);
				exit(1);
			}
			cfg.flags |= CKPT_MSPERIOD;
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if((argc <= 0 && pid == 0) || (argc > 0 && pid != 0))
		usage();

	if(pid > 0){
		dohijack(pid, &cfg);
		exit(0);
	}else{
		dopreload(&cfg);
		execvp(argv[0], &argv[0]);
		perror("exec");
		exit(1);
	}
}
