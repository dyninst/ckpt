#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

static struct ckptconfig ckptconfig;

static void
cmdname(char *buf, unsigned long max)
{
	int fd, rv;

	fd = open("/proc/self/cmdline", O_RDONLY);
	if(0 > fd)
		fatal("cannot open /proc/self/cmdline");
	rv = read(fd, buf, max-1);
	if(0 >= rv)
		fatal("cannot read /proc/self/cmdline");
	close(fd);
}

static void
defaults(struct ckptconfig *cfg)
{
	char *p;
	int m;

	cmdname(cfg->name, sizeof(cfg->name));
	p = strrchr(cfg->name, '/');
	if(p != NULL){
		p++;
		m = strlen(p);
		memmove(cfg->name, p, m);
	}else
		m = strlen(cfg->name);
	snprintf(&cfg->name[m], sizeof(cfg->name)-m, ".ckpt");

	cfg->continues = 0;
	cfg->asyncsig = SIGTSTP;
	cfg->msperiod = 0;
}

static void
readenv(struct ckptconfig *cfg)
{
	char *p;
	char *q;
	int sig;

	p = getenv("CKPT_NAME");
	if(p){
		if(0 == strcmp(p, ""))
			fatal("Empty CKPT_NAME\n");
		if(strlen(p) >= CKPT_MAXNAME)
			fatal("CKPT_NAME too long\n");
		strncpy(cfg->name, p, CKPT_MAXNAME);
	}

	p = getenv("CKPT_ASYNCSIG");
	if(p){
		if(0 == strcmp(p, ""))
			fatal("Empty CKPT_ASYNCSIG\n");
		if(isdigit(p[0])){
			sig = strtol(p, &q, 0);
			if(*q || sig < 0 || sig >= _NSIG)
				fatal("Bad value for CKPT_ASYNCSIG: %s\n", p);
			cfg->asyncsig = sig;
		}else{
			cfg->asyncsig = ckpt_mapsig(p);
			if(cfg->asyncsig == -1)
				fatal("Bad value for CKPT_ASYNCSIG: %s\n", p);
		}
	}
	
	p = getenv("CKPT_CONTINUE");
	if(p){
		if(0 == strcmp(p, ""))
			fatal("Empty CKPT_CONTINUE\n");
		if(0 == strcmp(p, "0"))
			cfg->continues = 0;
		else
			cfg->continues = 1;
	}

//	p = getenv("CKPT_TRYRESTARTNAME");
//	if(p){
//		if(0 == strcmp(p, ""))
//			fatal("Empty CKPT_TRYRESTARTNAME\n");
//		if(strlen(p) >= CKPT_MAXNAME)
//			fatal("CKPT_TRYRESTARTNAME too long\n");
//		strncpy(cfg->tryname, p, CKPT_MAXNAME);
//	}

	p = getenv("CKPT_MSPERIOD");
	if(p){
		if(0 == strcmp(p, ""))
			fatal("Empty CKPT_MSPERIOD\n");
		cfg->msperiod = atoi(p);
	}
}

void
ckpt_initconfig()
{
	defaults(&ckptconfig);
	readenv(&ckptconfig);

//	if(ckptconfig.tryname)
//		ckpt_tryrestart(ckptconfig.tryname);

	if(ckptconfig.asyncsig > 0)
		ckpt_async(ckptconfig.asyncsig);
	if(ckptconfig.msperiod > 0)
		ckpt_periodic(ckptconfig.msperiod);
}

void
ckpt_config(struct ckptconfig *cfg, struct ckptconfig *old)
{
	if(old)
		memcpy(old, &ckpt_config, sizeof(struct ckptconfig));

	if(cfg == NULL)
		return;

	if(cfg->flags&CKPT_NAME)
		memcpy(&ckptconfig.name, cfg->name, sizeof(ckptconfig.name));

	if(cfg->flags&CKPT_ASYNCSIG){
		if(ckptconfig.asyncsig != 0)
			ckpt_cancelasync(ckptconfig.asyncsig);
		ckpt_async(cfg->asyncsig);
		ckptconfig.asyncsig = cfg->asyncsig;
	}

	if(cfg->flags&CKPT_CONTINUE)
		ckptconfig.continues = cfg->continues;

	if(cfg->flags&CKPT_MSPERIOD){
		ckpt_periodic(cfg->msperiod);
		ckptconfig.msperiod = cfg->msperiod;
	}
}

int
ckpt_shouldcontinue()
{
	return ckptconfig.continues;
}

char *
ckpt_ckptname()
{
	return ckptconfig.name;
}

void
ckpt_rconfig(struct ckptconfig *cfg)
{
	ckpt_config(cfg, NULL);
}
