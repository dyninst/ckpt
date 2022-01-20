#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

enum {
	MAX_CALLBACKS = 1000
};

static fn_t on_preckpt[MAX_CALLBACKS];
static void *on_preckpt_arg[MAX_CALLBACKS];
static unsigned num_on_preckpt;
static fn_t on_postckpt[MAX_CALLBACKS];
static void *on_postckpt_arg[MAX_CALLBACKS];
static unsigned num_on_postckpt;
static fn_t on_restart[MAX_CALLBACKS];
static void *on_restart_arg[MAX_CALLBACKS];
static unsigned num_on_restart;

static struct ckptfdtbl *fdtbl;

void
ckpt_on_preckpt(fn_t f, void *arg)
{
	if (num_on_preckpt >= MAX_CALLBACKS) {
		fprintf(stderr, "Warning: too many pre-ckpt callbacks\n");
		return;
	}
	on_preckpt[num_on_preckpt] = f;
	on_preckpt_arg[num_on_preckpt++] = arg;
}

void
ckpt_on_postckpt(fn_t f, void *arg)
{
	if (num_on_postckpt >= MAX_CALLBACKS) {
		fprintf(stderr, "Warning: too many post-ckpt callbacks\n");
		return;
	}
	on_postckpt[num_on_postckpt] = f;
	on_postckpt_arg[num_on_postckpt++] = arg;
}

void
ckpt_on_restart(fn_t f, void *arg)
{
	if (num_on_restart >= MAX_CALLBACKS) {
		fprintf(stderr, "Warning: too many restart callbacks\n");
		return;
	}
	on_restart[num_on_restart] = f;
	on_restart_arg[num_on_restart++] = arg;
}

static unsigned long
ckpt_size(const struct ckpt_header *head,
	  const memregion_t *regions)
{
	unsigned long sz = 0;
	int i;

	sz += sizeof(*head);
	sz += head->num_regions*sizeof(*regions);
	for(i = 0; i < head->num_regions; i++)
		sz += regions[i].len;
	return sz;
}

static int
ckpt_save(int fd,
	  struct ckpt_header *head,
	  memregion_t *regions)
{
	int i, rv;
	unsigned long total = 0;
	struct iovec *iov, *vp;
	int cnt;

	ckpt_init_elfstream(fd, ckpt_size(head, regions));

	cnt = 1+1+head->num_regions;
	iov = xmalloc(cnt*sizeof(struct iovec));
	vp = iov;

	vp->iov_base = head;
	total += (vp->iov_len = sizeof(struct ckpt_header));
	vp++;

	vp->iov_base = regions;
	total += (vp->iov_len = head->num_regions * sizeof(memregion_t));
	vp++;

	for (i = 0; i < head->num_regions; i++) {
		vp->iov_base = (void*)regions[i].addr;
		total += (vp->iov_len = regions[i].len);
		vp++;
	}

	rv = writev(fd, iov, cnt);
	free(iov);
	if(rv != total){
		fprintf(stderr, "cannot write checkpoint image\n");
		return -1;
	}

	ckpt_fini_elfstream(fd);

	return 0;
}

static int
getcmd(char *cmd, int max)
{
	int fd;
	int rv;

	fd = open("/proc/self/cmdline", O_RDONLY);
	if (0 > fd)
		return -1;
	rv = read(fd, cmd, max);
	close(fd);
	if (0 >= rv)
		return -1;
	if (rv >= max)
		cmd[max-1] = '\0';
	else
		cmd[rv] = '\0';
	return 0;
}




/*
 *
 * thread vagaries:
 *
 *   RH 9.1 at cmu
 *   defines get_thread_area and modify_ldt but implements only modify_ldf
 *
 */
void
tls_save(struct ckpt_header *h)
{
	int gs;
#ifdef SYS_get_thread_area
	h->tls.entry_number = 6;  /* magic:
				     see linux/include/asm-i386/segment.h */
	if (0 > syscall(SYS_get_thread_area, &h->tls)) {
		fprintf(stderr, "cannot get tls segment\n");
		exit(1);
	}
	asm("movw %%gs,%w0" : "=q" (gs));
	h->gs = gs&0xffff;
	return;
#endif

#ifdef SYS_modify_ldt
	unsigned long s[2];

	/* Based on Redhat 7.3 libc and kernel.
	   See glibc-2.2.5/linuxthreads/sysdeps/i386/useldt.h
	   and linux-2.4.18-3/arch/i386/kernel/ldt.c
	*/

	if (0 > syscall(SYS_modify_ldt, 0, s, sizeof(s))){
		fprintf(stderr, "cannot read tls segments\n");
		exit(1);
	}
	memset(&h->tls, 0, sizeof(h->tls));
	h->tls.entry_number = 0;
	h->tls.base_addr = ((s[1]&0xff000000)
			    | ((s[1]&0xff)<<16)
			    | (s[0]>>16));
	h->tls.limit = (s[1]&0xf0000) | (s[0]&0x0ffff);
	h->tls.read_exec_only = (0x1&(s[1]>>9)) ^ 1;
	h->tls.contents = 0x11&(s[1]>>10);
	h->tls.seg_not_present = (0x1&(s[1]>>15)) ^ 1;
	h->tls.seg_32bit = 0x1&(s[1]>>22);
	h->tls.limit_in_pages = 0x1&(s[1]>>23);
	    
	asm("movw %%gs,%w0" : "=q" (gs));
	h->gs = gs&0xffff;
	return;
#endif
}

int
ckpt_ckpt(char *name)
{
	struct ckpt_header head;
	memregion_t regions[MAXREGIONS];
	int fd;
	int i;

	fdtbl = ckpt_getfdtbl();

	for (i = 0; i < num_on_preckpt; i++)
		on_preckpt[i](on_preckpt_arg[i]);

	if(name == NULL)
		name = ckpt_ckptname();

	if (0 > getcmd(head.cmd, sizeof(head.cmd))) {
		fprintf(stderr, "cannot read my command\n");
		return -1;
	}

	fd = ckpt_open_stream(name, MODE_SAVE);
	if (0 > fd) {
		fprintf(stderr, "cannot obtain a checkpoint stream\n");
		return -1;
	}

	if (0 > read_self_regions(regions, &head.num_regions)) {
		fprintf(stderr, "cannot read my memory map\n");
		return -1;
	}

	if (0 == setjmp(head.jbuf)) {
		/* Checkpoint */
		if (0 > ckpt_signals()) {
			fprintf(stderr, "cannot save the signal state\n");
			return -1;
		}
		head.brk = (unsigned long) sbrk(0);
		tls_save(&head);

		if (0 > ckpt_save(fd, &head, regions)) {
			fprintf(stderr, "cannot save the ckpt image\n");
			return -1;
		}

		ckpt_close_stream(fd);

		if (!ckpt_shouldcontinue())
			_exit(0); /* do not call atexit functions */

		for (i = 0; i < num_on_postckpt; i++)
			on_postckpt[i](on_postckpt_arg[i]);

		ckpt_freefdtbl(fdtbl);
		fdtbl = NULL;
		unblock_signals();
		restore_timers();

		return 0;
	}		

	/* Restart */
	if (0 > unmap_ifnot_orig(regions, head.num_regions)) {
		fprintf(stderr,
			"cannot purge restart code from address space\n");
		return -1;
	}
	if (0 > restore_mprotect(regions, head.num_regions)) {
		fprintf(stderr, "cannot restore address space protection\n");
	}
	if (0 > restore_signals()) {
		fprintf(stderr, "cannot restore signal disposition\n");
		return -1;
	}

	for (i = num_on_restart-1; i >= 0; i--)
		on_restart[i](on_restart_arg[i]);

	ckpt_restorefdtbl(fdtbl);
	ckpt_freefdtbl(fdtbl);
	fdtbl = NULL;
	return 0;
}

static void
ckpt_banner()
{
	return;
	fprintf(stderr, "libckpt of %s %s\n", __DATE__, __TIME__);
}

static void
unlinklib()
{
	char *p;
	p = getenv("LD_PRELOAD");
	if(p == NULL)
		return;
	if(strstr(p, "tmplibckpt") == NULL)
		return;
	unlink(p);
	unsetenv("LD_PRELOAD");
}

void
ckpt_init()
{
	ckpt_banner();
	unlinklib();
	ckpt_initconfig();
}

static char fbuf[1024];
static char buf[1024];

void
ckpt_restart(char *name)
{
	int ifd, ofd;
	int rv;

	if(name == NULL)
		name = ckpt_ckptname();
	ifd = ckpt_open_stream(name, MODE_RESTORE);
	if(0 > ifd)
		return;
	snprintf(fbuf, sizeof(fbuf), "/tmp/tmprestartXXXXXX");
	ofd = mkstemp(fbuf);
	if(0 > ofd){
		fprintf(stderr, "ckpt i/o error\n");
		return;
	}
	fchmod(ofd, 0777);
	while(1){
		rv = read(ifd, buf, sizeof(buf));
		if(0 > rv && errno == EINTR)
			continue;
		if(0 > rv){
			fprintf(stderr, "cannot read ckpt image: %s\n", strerror(errno));
			return;
		}
		if(0 == rv)
			break;
		if(0 >= xwrite(ofd, buf, rv)){
			fprintf(stderr, "cannot write local ckpt image: %s\n", strerror(errno));
			return;
		}
	}
	ckpt_close_stream(ifd);
	close(ofd);
	execl(fbuf, fbuf, NULL);
	fprintf(stderr, "cannot exec checkpoint image: %s\n", strerror(errno));
}
