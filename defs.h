#ifndef __DEFS_H
#define __DEFS_H

// main.h
#define XBSIZE		1537  	// 3 * 512 + 1
#define XHASH_SIZE      60      // 300 / 5
#define XHASH_CLONE     30      // 150 / 5

#define	COLOR_LEN	9
#define COLOR_BLACK	"[0;30;1m"
#define	COLOR_RED	"[0;31;1m"
#define COLOR_GREEN	"[0;32;1m"
#define COLOR_YELLOW	"[0;33;1m"
#define COLOR_BLUE	"[0;34;1m"
#define COLOR_PURPLE	"[0;35;1m"
#define COLOR_CYAN	"[0;36;2m"
#define COLOR_LCYAN	"[0;36;1m"
#define COLOR_WHITE	"[0;37;1m"
#define COLOR_NORLEN	4
#define COLOR_NORMAL	"[0m"

#define xdebug(l, c...) {			\
	if (l <= debuglvl) {			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "[", 1);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	write(1, "*", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); } }

#define info_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_CYAN, COLOR_LEN);	\
	write(1, "*", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }

#define cdel_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_LCYAN, COLOR_LEN);	\
	write(1, "-", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }

#define cadd_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_BLUE, COLOR_LEN);	\
	write(1, "+", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }

#define cinfo_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_GREEN, COLOR_LEN);	\
	write(1, "*", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }
	
#define help_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_WHITE, COLOR_LEN);	\
	write(1, "?", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }

#define err_printf(c...) { 			\
	write(1, COLOR_BLACK, COLOR_LEN); 	\
	write(1, "[", 1);			\
	write(1, COLOR_RED, COLOR_LEN);		\
	write(1, "!", 1);			\
	write(1, COLOR_BLACK, COLOR_LEN);	\
	write(1, "] ", 2);			\
	write(1, COLOR_NORMAL, COLOR_NORLEN);	\
	printf(c); }

typedef struct clone_t xmen; // clones.h
// irc.h

typedef struct channick_t xnick;
typedef struct chanclone_t xclone;
typedef struct channel_t xchan;
typedef struct chanmask_t xmask;
typedef struct chanmask_t xfriend;

#endif
