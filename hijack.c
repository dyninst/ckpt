#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

/*
 *  The routine hijack(pid,libname) injects LIBNAME into process PID.  It
 *  stops the process; writes code on its stack that, effectively, calls
 *  dlopen(LIBNAME); executes that code; and resumes the process.
 * 
 *  A kink: dlopen actually lives in libdl.so, which programs ordinarily do
 *  not load.  Instead hijack calls _dl_open, a function in libc that
 *  implements the core of dlopen.  _dl_open tries to protect itself from
 *  being called from arbitrary code.  It returns an error if its return
 *  address is not in libc or libdl.  Hijack circumvents this check by pushing
 *  the address of a return instruction in libc ahead of the real return
 *  address.
 */

/* Write NLONG 4 byte words from BUF into PID starting
   at address POS.  Calling process must be attached to PID. */
static int
write_mem(pid_t pid, unsigned long *buf, int nlong, unsigned long pos)
{
	unsigned long *p;
	int i;

	for (p = buf, i = 0; i < nlong; p++, i++)
		if (0 > ptrace(PTRACE_POKEDATA, pid, pos+(i*4), *p))
			return -1;
	return 0;
}

static int
read_mem(pid_t pid, unsigned char *c, int nbyte, unsigned long pos)
{
	unsigned long *p;
	unsigned long *buf;
	unsigned long nlong;
	int i;

	assert((nbyte%4) == 0);
	assert((pos%4) == 0);
	nlong = nbyte/4;
	buf = (unsigned long*)c;
	for (p = buf, i = 0; i < nlong; p++, i++){
		errno = 0;
		*p = ptrace(PTRACE_PEEKTEXT, pid, pos+(i*4), 0);
		if (errno != 0)
			return -1;
	}
	return 0;

}

static char codecall[] = {
	0xbc, 0x0, 0x0, 0x0, 0x0,   /* mov $0, %esp (NEWESP)*/
	0xe8, 0x0, 0x0, 0x0, 0x0,   /* call $0 (FN)*/
                                    /* (RET) */
	0xbc, 0x0, 0x0, 0x0, 0x0,   /* mov $0, %oldesp (OLDESP) */
	0xcc                        /* trap */
};

static char codejmp[] = {
	0xbc, 0x0, 0x0, 0x0, 0x0,   /* mov $0, %esp (NEWESP)*/
	0xe9, 0x0, 0x0, 0x0, 0x0,   /* jmp $0 (FN)*/
                                    /* (RET) */
	0xbc, 0x0, 0x0, 0x0, 0x0,   /* mov $0, %oldesp (OLDESP) */
	0xcc                        /* trap */
};

/* offsets in code for addresses */
enum {
	OFF_NEWESP = 1,
	OFF_FN = 6,
	OFF_RET = 10,
	OFF_OLDESP = 11
};

int
hijack_call(int pid, char *fnname, void *arg, unsigned long n)
{
	struct user_regs_struct regs, oregs;
	unsigned long fnaddr, codeaddr, straddr;
	unsigned long esp;
	unsigned long *p;
	int status, rv;
	struct modulelist *ml;

	if (0 > kill(pid, 0)) {
		fprintf(stderr, "cannot hijack process %d: %s\n",
			pid, strerror(errno));
		return -1;
	}

	/* find ckpt_rputenv in PID */
	ml = rf_parse(pid);
	if (!ml) {
		fprintf(stderr, "cannot parse %d\n", pid);
		return -1;
	}
	if (!ml->is_dynamic) {
		fprintf(stderr, "cannot hijack static programs\n");
		return -1;
	}

	fnaddr = rf_find_function(ml, fnname);
	if (-1UL == fnaddr) {
		fprintf(stderr, "cannot find %s in process %d\n", fnname, pid);
		return -1;
	}
	rf_free_modulelist(ml);

	/* Attach */
	if (0 > ptrace(PTRACE_ATTACH, pid, 0, 0)) {
		fprintf(stderr, "cannot attach to %d\n", pid);
		exit(1);
	}
	waitpid(pid, NULL, 0);
	ptrace(PTRACE_GETREGS, pid, 0, &oregs);
	memcpy(&regs, &oregs, sizeof(regs));

	/* push EIP */
	regs.esp -= 4;
	ptrace(PTRACE_POKEDATA, pid, regs.esp, regs.eip-2);

	/* OLDESP */
	esp = regs.esp;

	/* push argument */
	assert((n%4) == 0);  /* if not, copy argument to 4-byte aligned buffer */
	straddr = esp - n;
	if (0 > write_mem(pid, (unsigned long*)arg, n/4, straddr)) {
		fprintf(stderr, "cannot write argument (%s)\n",
			strerror(errno));
		return -1;
	}

	codeaddr = straddr - sizeof(codecall);

	/* pointer to argument, above code */
	if (0 > write_mem(pid, &straddr, 4, codeaddr-4)){
		fprintf(stderr, "cannot write argument (%s)\n",
			strerror(errno));
		return -1;
	}

	p = (unsigned long*)&codecall[OFF_NEWESP];
	*p = codeaddr - 4; /* stack begins after code and argument */
	p = (unsigned long*)&codecall[OFF_FN];
	*p = fnaddr-(codeaddr+OFF_RET);
	p = (unsigned long*)&codecall[OFF_OLDESP];
	*p = esp;
	if (0 > write_mem(pid, (unsigned long*)&codecall,
			  sizeof(codecall)/sizeof(long), codeaddr)) {
		fprintf(stderr, "cannot write code\n");
		return -1;
	}
	regs.eip = codeaddr;

	ptrace(PTRACE_SETREGS, pid, 0, &regs);

	rv = ptrace(PTRACE_CONT, pid, 0, 0);
	if (0 > rv) {
		perror("PTRACE_CONT");
		return -1;
	}
	while (1) {
		if (0 > waitpid(pid, &status, 0)) {
			perror("waitpid");
			return -1;
		}
		if (WIFSTOPPED(status)) {
			if (WSTOPSIG(status) == SIGTRAP)
				break;
			else if (WSTOPSIG(status) == SIGSEGV
				 || WSTOPSIG(status) == SIGBUS) {
				if (0 > ptrace(PTRACE_CONT, pid, 0,
					       WSTOPSIG(status))) {
					perror("PTRACE_CONT");
					return -1;
				}
			} else if (0 > ptrace(PTRACE_CONT, pid, 0, 0)) {
				/* FIXME: Remember these signals */
				perror("PTRACE_CONT");
				return -1;
			}
		}
	}
		
	if (0 > ptrace(PTRACE_SETREGS, pid, 0, &oregs)) {
		perror("PTRACE_SETREGS");
		return -1;
	}
	rv = ptrace(PTRACE_DETACH, pid, 0, 0);
	if (0 > rv) {
		perror("PTRACE_DETACH");
		return -1;
	}

	return 0;
}

static unsigned char libcbuf[512];

/* Find a return instruction in the libc text */
static unsigned long
findlibcret(int pid, struct modulelist *ml)
{
	struct mm *m;
	int i;
	unsigned char *p, *q;
	unsigned long a, n, o;
	enum {
		RET = 0xc3
	};

	/* Find libc record */
	for (i = 0, m = ml->mm; i < ml->num_mm; i++, m++) {
		if (strstr(m->name, "/libc"))
			break;
	}
	if(i >= ml->num_mm)
		assert(0);

	/* Scan libc code for a ret instruction */
	o = 0;  /* offset past start of libc */
	for(a = m->start; a < m->end; a += sizeof(libcbuf)){
		if(sizeof(libcbuf) > m->end - a)
			n = m->end - a;
		else
			n = sizeof(libcbuf);
		if(0 > read_mem(pid, libcbuf, n, a)){
			perror("readmem");
			assert(0);
		}
		p = libcbuf;
		q = &libcbuf[n];
		while(p < q){
			if(*p == RET)
				return (unsigned long)(m->start+o);
			p++;
			o++;
		}
	}
	assert(0); /* how can there be no ret instruction? */
}

int
hijack(int pid, char *libname)
{
	struct user_regs_struct regs, oregs;
	unsigned long dlopenaddr, codeaddr, libaddr;
	unsigned long xaddr;
	unsigned long esp;
	unsigned long *p;
	int n;
	char *arg;
	int status, rv;
	struct modulelist *ml;

	if (0 > kill(pid, 0)) {
		fprintf(stderr, "cannot hijack process %d: %s\n",
			pid, strerror(errno));
		return -1;
	}

	/* find dlopen in PID's libc */
	ml = rf_parse(pid);
	if (!ml) {
		fprintf(stderr, "cannot parse %d\n", pid);
		return -1;
	}
	if (!ml->is_dynamic) {
		fprintf(stderr, "cannot hijack static programs\n");
		return -1;
	}

	dlopenaddr = rf_find_libc_function(ml, "_dl_open");
	if (-1UL == dlopenaddr) {
		fprintf(stderr, "cannot find _dl_open in process\n");
		return -1;
	}

	/* Attach */
	if (0 > ptrace(PTRACE_ATTACH, pid, 0, 0)) {
		fprintf(stderr, "cannot attach to %d\n", pid);
		exit(1);
	}
	waitpid(pid, NULL, 0);
	ptrace(PTRACE_GETREGS, pid, 0, &oregs);
	memcpy(&regs, &oregs, sizeof(regs));

	xaddr = findlibcret(pid, ml);
	rf_free_modulelist(ml);

	/* push EIP */
	regs.esp -= 4;
	ptrace(PTRACE_POKEDATA, pid, regs.esp, regs.eip-2);

	/* OLDESP */
	esp = regs.esp;

	/* push library name */
	n = strlen(libname)+1;
	n = n/4 + (n%4 ? 1 : 0);
	arg = xmalloc(n*sizeof(unsigned long));
	memcpy(arg, libname, strlen(libname));
	libaddr = esp - n*4;
	if (0 > write_mem(pid, (unsigned long*)arg, n, libaddr)) {
		fprintf(stderr, "cannot write dlopen argument (%s)\n",
			strerror(errno));
		free(arg);
		return -1;
	}
	free(arg);
		
	/* finish code and push it */
	codeaddr = libaddr - sizeof(codejmp);
	p = (unsigned long*)&codejmp[OFF_NEWESP];
	*p = codeaddr - 8; /* stack begins after code and return addresses */
	p = (unsigned long*)&codejmp[OFF_FN];
	*p = dlopenaddr-(codeaddr+OFF_RET);
	p = (unsigned long*)&codejmp[OFF_OLDESP];
	*p = esp;
	if (0 > write_mem(pid, (unsigned long*)&codejmp,
			  sizeof(codejmp)/sizeof(long), codeaddr)) {
		fprintf(stderr, "cannot write code\n");
		return -1;
	}
	regs.eip = codeaddr;

	/* push return addresses */
	ptrace(PTRACE_POKEDATA, pid, codeaddr-4, codeaddr+OFF_RET);
	ptrace(PTRACE_POKEDATA, pid, codeaddr-8, xaddr);

	/* Setup dlopen call; use internal register calling convention */
	regs.eax = libaddr;             /* library name */
	regs.edx = RTLD_NOW;            /* dlopen mode */
	regs.ecx = codeaddr+OFF_RET;    /* caller */
	ptrace(PTRACE_SETREGS, pid, 0, &regs);

	rv = ptrace(PTRACE_CONT, pid, 0, 0);
	if (0 > rv) {
		perror("PTRACE_CONT");
		return -1;
	}
	while (1) {
		if (0 > waitpid(pid, &status, 0)) {
			perror("waitpid");
			return -1;
		}
		if (WIFSTOPPED(status)) {
			if (WSTOPSIG(status) == SIGTRAP)
				break;
			else if (WSTOPSIG(status) == SIGSEGV
				 || WSTOPSIG(status) == SIGBUS) {
				if (0 > ptrace(PTRACE_CONT, pid, 0,
					       WSTOPSIG(status))) {
					perror("PTRACE_CONT");
					return -1;
				}
			} else if (0 > ptrace(PTRACE_CONT, pid, 0, 0)) {
				/* FIXME: Remember these signals */
				perror("PTRACE_CONT");
				return -1;
			}
		}
	}
		
	if (0 > ptrace(PTRACE_SETREGS, pid, 0, &oregs)) {
		perror("PTRACE_SETREGS");
		return -1;
	}
	rv = ptrace(PTRACE_DETACH, pid, 0, 0);
	if (0 > rv) {
		perror("PTRACE_DETACH");
		return -1;
	}

	return 0;
}
