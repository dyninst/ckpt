#include "sys.h"
#include "ckpt.h"
#include "ckptimpl.h"

static int
open_ckpt_filestream(char *name, int mode)
{
	int fd;
	if(mode == MODE_SAVE)
		fd = open(name, O_CREAT|O_TRUNC|O_WRONLY, 0700);
	else
		fd = open(name, O_RDONLY);
	if(0 > fd){
		fprintf(stderr, "cannot open checkpoint file %s: %s\n",
			name, strerror(errno));
		return -1;
	}
	return fd;
}

int
ckpt_open_stream(char *name, int mode)
{
	static char file[] = "file://";
	static char cssrv[] = "cssrv://";
	regmatch_t match[3], *mp;
	char *addr, *id, *np;
	regex_t re;
	int len;
	int fd;

	if(!strncmp(name, file, strlen(file))){
		np = name;
		np += strlen(file);
		return open_ckpt_filestream(np, mode);
	}else if(!strncmp(name, cssrv, strlen(cssrv))){
		if(0 != regcomp(&re, "cssrv://\\([^/]\\+\\)/\\(.*\\)", 0))
			fatal("cannot compile regular expressions");
		if(regexec(&re, name, 3, match, 0)){
			fprintf(stderr, "bad cssrv URI: %s\n", name);
			return -1;
		}

		mp = &match[1];
		len = mp->rm_eo - mp->rm_so;
		addr = xmalloc(len+1);
		memcpy(addr, name+mp->rm_so, len);

		mp = &match[2];
		len = mp->rm_eo - mp->rm_so;
		id = xmalloc(len+1);
		memcpy(id, name+mp->rm_so, len);

		if(mode == MODE_SAVE)
			fd = ckpt_remote_put(addr, id);
		else if(mode == MODE_RESTORE)
			fd = ckpt_remote_get(addr, id);
		else
			fd = -1;
		free(addr);
		free(id);
		return fd;
	}else
		return open_ckpt_filestream(name, mode);
}

void
ckpt_close_stream(int fd)
{
	close(fd);
}
