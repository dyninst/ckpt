#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#define __USE_GNU    /* defines F_GETSIG */
#include <fcntl.h>
#undef  __USE_GNU
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <syscall.h>
#include <link.h>
#include <elf.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <linux/unistd.h>
#include <asm/ldt.h>
#include <asm/page.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>
#include <dirent.h>

#ifndef  SYS_get_thread_area
#ifndef  SYS_modify_ldt
#define CKPT_NOTLS
#endif
#endif
