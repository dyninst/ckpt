#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ckpt.h"

extern int parse_ip(const char *s, struct sockaddr_in *addr);

static void
segv(int sig)
{
	while (1)
		;
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	int i = 0;
	int fd;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = segv;
//	if (0 > sigaction(SIGSEGV, &sa, NULL)) {
//		fprintf(stderr, "cannot install sighandler\n");
//		exit(1);
//	}
	printf("%s: ckpt test program (pid = %d)\n",
	       argv[0], getpid());
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if(0 > fd)
		perror("cannot create socket");
	fd = open("/etc/passwd", O_RDONLY);
	if(0 > fd)
		perror("cannot open /etc/passwd");

	while (1) {
		unsigned j;
		for (j = 0; j < 100000000; j++)
			;
		malloc(100000);
		fprintf(stderr, "%d\n", ++i);
	}
	return 0;
}
