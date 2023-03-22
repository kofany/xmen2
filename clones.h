#ifndef __CLONES_H
#define __CLONES_H

struct clone_t {
	int fd, connected, pp, cnt, hash, read_last, rejoins;
	char *nick, *address, *read_buf, *rejoin_buf;
#ifdef _USE_POLL
	struct pollfd *pfd;
#endif
	xmen *next, *parent, 
		*ping, *pingparent, 
		*h_next, *h_parent;
};

#define random_reason() xreasons[xrand(lxreasons)].string
#define random_ident() random_nick(0)
#define random_realname() xrealnames[xrand(lxrealnames)].string

// do jednego klona
#define xsend(d, c...)	{ xsendint = snprintf(buf, XBSIZE, c); write(d, buf, xsendint); }

extern xmen *root, *tail;
extern xmen *h_clone[XHASH_CLONE];
extern char *hreasons[];
extern int xall, xsendint;

void add_rejoin(xmen *, char *);

xmen *new_clone(void);
void kill_clone(xmen *, int);

xmen *is_clone(char *);

int put_clone(xmen *, char *, int);

#endif
