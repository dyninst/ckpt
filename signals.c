#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

/* FIXME: State not saved includes:
   sigaltstack, ITIMER_PROF, ITIMER_REALPROF.  */

/* The signal state is stored here */
static sigset_t blocked;	            /* Blocked signals */
static sigset_t pending;                    /* Pending signals */
static struct sigaction action[NSIG];       /* Signal actions */
static unsigned alarm_cnt;                  /* Remaining alarm() time */
static struct timeval time_ckpt;            /* Time of ckpt */
static struct itimerval timer_real;         /* ITIMER_REAL */
static struct itimerval timer_virt;         /* ITIMER_VIRTUAL */

int
ckpt_signals()
{
	sigset_t blkall;
	struct itimerval timer_zero;
	int i;

	/* Save current mask and block all signals */
	sigfillset(&blkall);
	sigemptyset(&blocked);
	sigprocmask(SIG_SETMASK, &blkall, &blocked);

	/* Save the timers state */
	memset(&timer_zero, 0, sizeof(timer_zero));
	gettimeofday(&time_ckpt, NULL);
	syscall(SYS_setitimer, ITIMER_REAL, &timer_zero, &timer_real);
	syscall(SYS_setitimer, ITIMER_VIRTUAL, &timer_zero, &timer_virt);
#if 0
	/* Don't save both itimer and alarm state. */
	/* Can we do both if we do them in the right order? */
	alarm_cnt = alarm(0); 
	fprintf(stderr, "alarm_cnt=%d\n", alarm_cnt);
#else
	alarm_cnt = 0;
#endif

	/* Save the handler for each signal */
	for (i = 0; i < NSIG; i++)
		syscall(SYS_sigaction, i, NULL, &action[i]);

	/* Save the pending signals */
	sigemptyset(&pending);
	sigpending(&pending);

	return 0;
}

static int
restore_timer(int which, struct itimerval *value)
{
	/* FIXME: We ignore usec-level timing */
	if (0 > setitimer(which, value, NULL)) {
		fprintf(stderr, "cannot to set itimer %d\n", which);
		perror("setitimer");
		return -1;
	}
	return 0;
}

void
unblock_signals()
{
	sigprocmask(SIG_SETMASK, &blocked, NULL);
}

void
restore_timers()
{
	restore_timer(ITIMER_REAL, &timer_real);
	restore_timer(ITIMER_VIRTUAL, &timer_virt);
}

int
restore_signals()
{
	sigset_t blkall;
	int i;
	int mypid;

	/* Block all signals */
	sigfillset(&blkall);
	sigprocmask(SIG_SETMASK, &blkall, NULL);

	/* FIXME: It might be wise to clear any pending
           signals (unrelated to the ckpt) that this
           process has received, although we're not
           expecting any. */
	
	/* Restore the handlers */
	for (i = 0; i < NSIG; i++)
		syscall(SYS_sigaction, i, &action[i], NULL);

	/* Restore the timers */
	restore_timer(ITIMER_REAL, &timer_real);
	restore_timer(ITIMER_VIRTUAL, &timer_virt);
	if (alarm_cnt > 0)
		alarm(alarm_cnt);
		
	/* Post pending sigs */
	mypid = getpid();
	for (i = 0; i < NSIG; i++)
		if (sigismember(&pending, i))
			kill(mypid, i);

	unblock_signals();
	
	return 0;
}

static void
asynchandler(int signum)
{
	ckpt_ckpt(ckpt_ckptname());
}

void
ckpt_async(int sig)
{
	if(SIG_ERR == signal(sig, asynchandler))
		fatal("cannot install checkpoint signal handler\n");
}

void
ckpt_cancelasync(int sig)
{
	if(SIG_ERR == signal(sig, SIG_DFL))
		fatal("cannot clear checkpoint signal handler\n");
}

void
ckpt_periodic(unsigned long msperiod)
{
	struct itimerval itv;
	struct timeval tv;

	if(msperiod == 0){
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if(SIG_ERR == signal(SIGALRM, asynchandler))
			fatal("cannot clear periodic checkpoint handler\n");
	}else{
		tv.tv_sec = msperiod/1000;
		tv.tv_usec = (msperiod%1000)*1000;
		if(SIG_ERR == signal(SIGALRM, asynchandler))
			fatal("cannot set periodic checkpoint handler\n");
	}

	itv.it_interval = tv;
	itv.it_value = tv;
	if(0 > setitimer(ITIMER_REAL, &itv, NULL))
		fatal("cannot set periodic checkpoints");
}
