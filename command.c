/* Copyright (c) 2002 Maciej Freudenheim <fahren@bochnia.pl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOTg LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef __FreeBSD__
  #include <sys/param.h>
  #include <sys/sysctl.h>
  #include <net/ethernet.h>
  #include <net/if_var.h>
  #include <net/if_dl.h>
  #include <net/if_types.h>
  #include <net/route.h>
#endif
#include <netinet/in.h>
#ifdef _USE_POLL
  #include <sys/poll.h>
#endif
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "defs.h"
#include "main.h"
#include "clones.h"
#include "irc.h"
#include "command.h"
#include "parse.h"
#include "action.h"

xaddress xconnect;
vhost *lastvhost;
xfriend *friendroot;
struct timeval tv_ping, tv_log;
struct tm *tm_log;
double cping;
char nick_buf[MAX_NICKLEN+1];
int def_takemode, xlastjoin, xrejointimer, xrejoindelay;

const char	xalphaf[] = "`{}[]\\_^|ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
		xalpha[] = "`{}[]\\_^|ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-012345678",
		xcrapf[] = "`{}[]\\_^|",
		xcrap[] = "`{}[]\\_^|-",
		xident[] = "abcdefghijklmnopqrstuvwxyz";
const float	lxalpf = sizeof(xalphaf) - 1.0,
		lxalp = sizeof(xalpha) - 1.0,
		lxcrapf = sizeof(xcrapf) - 1.0,
		lxcrap = sizeof(xcrap) - 1.0,
		lxident = sizeof(xident) - 1.0;

void parse_input(void)
{
	static char cmdline[MAX_CMDLEN];
//	static int cmdline_last = 0;
	char *cmd = cmdline, *w;
	int i;
	xmen *p;

/*
	i = read(0, cmdline+cmdline_last, MAX_CMDLEN-cmdline_last-1);
	if (i == -1) {
		err_printf("parse_input()->read(): (%d)%s\n", i, strerror(errno));
		cmdline_last = 0;
		return;
	} else if (i == 0) {
		i++; // jak jest EOF (koniec bufora) to nie utnie ostatniej literki
	} else if ((i+cmdline_last) < MAX_CMDLEN && cmdline[i+cmdline_last-1] != '\n') {
		cmdline_last += i;
		return;
	}
	
	cmdline[i+cmdline_last-1] = 0;
	cmdline_last = 0;
*/

	fgets(cmdline, MAX_CMDLEN, stdin);
	cmdline[strlen(cmdline)-1] = 0;
	
	if (*cmd == '/' || *cmd == '.')
		cmd++;

	if ((w = newsplit(&cmd)) == 0)
		return;
	
	if (!x_strncasecmp(w, "load", 4)) {
		struct hostent *host;
	
		if (xconnect.bncserver == 0 && xconnect.vhost == 0) {
			err_printf("Vhost list is empty, try .vhost scan.\n");
			return;
		}
		
		w = newsplit(&cmd);
		
		if (!w || sscanf(w, "%d", &i) < 1) {
			help_printf("Usage: load <number of clones> <ircserver> [ircport]\n");
			return;
		}
#ifdef _USE_POLL
		if ((xall + xconnect.connecting + i) >= POLL_ARRAYSIZE) {
			err_printf("Need bigger fds array to poll().\n");
			return;
		}
#endif
		
		if ((w = newsplit(&cmd))) {
//		if ((w = "127.0.0.1")) {
			if (xconnect.ircserver) {
				if (x_strcasecmp(xconnect.ircserver, w))
					cdel_printf("Due to author lameness, I can load clones only from ONE ircserver,\n    so i'm ignoring '%s' and using %s.\n", w, xconnect.ircserver);
			} else {
				xconnect.ircserver = strdup(w);
				if (xconnect.ircserver == 0) {
					err_printf("strdup(): %s\n", strerror(errno));
					return;
				}
				if (!xconnect.bncserver) {
					if ((host = gethostbyname(w)) == 0) {
						err_printf("gethostbyname(%s): %s\n",
								w, hstrerror(errno));
						free(xconnect.ircserver);
						xconnect.ircserver = 0;
						return;
					}
					memset((char *)&xconnect.addr, 0, sizeof(struct sockaddr_in));
					memcpy(&xconnect.addr.sin_addr, host->h_addr, host->h_length);
					xconnect.addr.sin_family	= host->h_addrtype;
					xconnect.addr.sin_port		= htons(xconnect.ircport);
				}
			}
		} else if (xconnect.ircserver == 0) {
			help_printf("Usage: load <number of clones> <ircserver> [ircport]\n");
			return;
		}

		if (*cmd && (sscanf(cmd, "%d", &xconnect.ircport) == 1))
			xconnect.addr.sin_port = htons(xconnect.ircport);
	
		info_printf("Using ircserver: %s%s%s (%s)\n", COLOR_WHITE, xconnect.ircserver, COLOR_NORMAL,
				xconnect.bncserver? "BNC" : inet_ntoa(xconnect.addr.sin_addr));
		info_printf("Using ircport  : %d\n", xconnect.ircport);
		if (xconnect.bncserver) {
			info_printf("Using bnc      : %s\n", xconnect.bncserver);	
		}
		info_printf("Connect delay  : %d\n", xconnect.delay);

		if (xconnect.log_fd) {
			logit(">> .load %d %s %d\n", i, xconnect.ircserver, xconnect.ircport);
		}
		
		xconnect.connecting += i;
		return;
	} else if (!x_strncasecmp(w, "bnc", 3)) {
		if (*cmd) {
			int new = 0;
			struct hostent *host;
			
			w = newsplit(&cmd);
			if (!x_strcasecmp(w, "del") || !x_strcasecmp(w, "d")) {
				if (!(xconnect.bncserver)) {
					err_printf("First you need to set up bnc.\n");
					return;
				}
				free(xconnect.bncserver);
				free(xconnect.bncpass);
				xconnect.bncserver = xconnect.bncpass = 0;
//				free(xconnect.ircserver);
//				xconnect.ircserver = 0;
//				xconnect.ircport = DEF_IRCPORT;
				info_printf("Changed loading mode to direct connect, launching vhost scan.\n");
				if (xconnect.log_fd) {
					logit(">> .bnc del\n");	
				}
				cinfo_printf("Found %d vhosts.\n", get_vhosts());
				return;
			} else if (sscanf(cmd, "%d %2s", &i, buf) != 2) {
				help_printf("Usage: bnc [[d]el] | <host port password>\n");
				help_printf("Note: Recommended BNC: 2.4.x. DO NOT use bnc 2.8.x (it's broken)\n");
				return;
			}
			
			if (!(xconnect.bncpass))
				new = 1;
		
			if (!(xconnect.bncserver) || x_strcasecmp(xconnect.bncserver, w)) {
				if ((host = gethostbyname(w)) == 0) {
					err_printf("gethostbyname(%s): %s\n", w, hstrerror(errno));
					return;
				}
				free(xconnect.bncserver);
				xconnect.bncserver = strdup(w);
				if (xconnect.bncserver == 0) {
					err_printf("strdup(): %s\n", strerror(errno));
					return;
				}
				memset((char *)&xconnect.bncaddr, 0, sizeof(struct sockaddr_in));
				memcpy(&xconnect.bncaddr.sin_addr, host->h_addr, host->h_length);
				xconnect.bncaddr.sin_family        = host->h_addrtype;
				xconnect.bncaddr.sin_port          = htons(xconnect.ircport);
			}
			
			xconnect.bncport = atoi(newsplit(&cmd));
			xconnect.bncaddr.sin_port = htons(xconnect.bncport);
			free(xconnect.bncpass);
			xconnect.bncpass = strdup(cmd);
			if (xconnect.bncpass == 0) {
				err_printf("strdup(): %s\n", strerror(errno));
				free(xconnect.bncserver);
				xconnect.bncserver = 0;
				return;
			}
	
			if (xconnect.log_fd) {
				logit(".bnc %s <...> <...>\n", xconnect.bncserver);
			}
			
			if (new) {
				info_printf("Changed loading mode to bnc. Cleaning vhost list.\n");
				del_vhost_all();
		//		free(xconnect.ircserver);
		//		xconnect.ircserver = 0;
		//		xconnect.ircport = DEF_IRCPORT;
			} else {
				info_printf("Changed bnc settings to %s:%d (%s)\n",
						xconnect.bncserver, xconnect.bncport, xconnect.bncpass);
			}
		} else {
			if (xconnect.bncserver) {
				info_printf("Current bnc settings:\n");
				info_printf(" host      : %s%s%s (%s)\n",
						COLOR_WHITE, xconnect.bncserver, COLOR_NORMAL,
						inet_ntoa(xconnect.bncaddr.sin_addr));
				info_printf(" port      : %d\n", xconnect.bncport);
				info_printf(" password  : %s\n", xconnect.bncpass);
			} else {
				help_printf("Usage: bnc [[d]el] | <host port password>\n");
				help_printf("Note: Recommended BNC: 2.4.x. DO NOT use bnc 2.8.x (it's broken)\n");
			}
		}
	} else if (!x_strncasecmp(w, "vh", 2)) { // vhost
		vhost *v;
		char *from = 0, *to = 0, ipaddr[16], *hname = 0;
		int dots = 0, ifrom, ito, total = 0;
		struct sockaddr_in iaddr, *addr = 0;
		struct hostent *host = 0;
		
		if ((w = newsplit(&cmd)) == 0) {
			help_printf("Usage: vhost <[a]dd | [d]el | [l]ist | [s]can> [vhosts]\n");
			return;
		} else if (!x_strcasecmp(w, "scan") || !x_strcasecmp(w, "s")) {
			if (xconnect.bncserver) {
				err_printf("Can't scan vhosts for bnc.\n");
				return;
			}
			cinfo_printf("Found %d vhosts.\n", get_vhosts());
			if (xconnect.vhost == 0) {
				err_printf("Vhost list is empty.\n");
			}
			return;
		} else if (!x_strcasecmp(w, "list") || !x_strcasecmp(w, "l")) {
			if (!xconnect.vhost) {
				err_printf("Vhost list is empty.\n");
				return;
			}
			for (i = 0, v = xconnect.vhost; v; v = v->next, i++) {
				if (xconnect.bncserver) {
					info_printf("%s%3d%s: %s\n", COLOR_WHITE, i+1, COLOR_NORMAL, v->name);
				} else {
					info_printf("%s%3d%s: %s (%s)\n", COLOR_WHITE, i+1, COLOR_NORMAL,
						v->name, inet_ntoa(v->addr->sin_addr));
				}
			}
			return;
		} else if (!x_strcasecmp(w, "del") || !x_strcasecmp(w, "d")) {
			if (!*cmd) {
				err_printf("You have to specify number/vhosts to delete.\n");
			} else {
				int ii;
				if (xconnect.vhost->next == 0 && !(xconnect.bncserver)) {
					err_printf("You can't delete last vhost.\n");
					return;
				}
				if (sscanf(cmd, "%d", &ii) < 1) {
					while ((w = newsplit(&cmd)))
						if ((v = find_vhost_by_name(w)))
							del_vhost(v, 1);
						else {
							err_printf("Couldn't find vhost '%s'\n", w);
						}
				} else {
					ii--;
					for (i = 0, v = xconnect.vhost; v && i < ii; v = v->next, i++)
						;
					if (v) {
						del_vhost(v, 1);
					} else {
						err_printf("Wrong vhost number #%d\n", ii+1);
					}
				}
			}
			return;
		} else if (!x_strcasecmp(w, "add") || !x_strcasecmp(w, "a")) {
			if (!*cmd) {
				err_printf("You have to specify vhosts to add.\n");
				return;
			}
			w = 0;
		} else
			del_vhost_all();
		
		while (1) {
			if (w == 0 && (w = newsplit(&cmd)) == 0)
				break;
			if (isdigit(w[strlen(w)-1])) { // ip
				from = w;
				dots = 0;
				while (*from) {
					if (*from == '.')
						dots++;
					else if (*from == '-' && dots != 3)
						break;
					else if (*from != '-' && !isdigit(*from)) {
						dots = 0;
						break;
					}
					from++;
				}
				if (dots != 3) {
					err_printf("Invalid ip: %s\n", w);
					w = 0; continue;
				}
				from = strrchr(w, '.');
				*from++ = 0;
				if ((to = strchr(from, '-'))) {
					*to++ = 0;
					ifrom = atoi(from);
					ito = atoi(to);
				} else
					ifrom = ito = atoi(from);
			} else { // hostname
				from = 0;
				ifrom = ito = 0;
			}
				
			iaddr.sin_family = AF_INET;
			ito++;
			while (ifrom < ito) {
				dots = 0;
				if (from == 0) { // hostname
					ifrom++;
					hname = w;
					if (!(xconnect.bncserver)) {
						if ((host = gethostbyname(w)) == 0) {
							err_printf("gethostbyname(%s): %s\n",
									w, hstrerror(errno));
							continue;
						}
						hname = host->h_name;
				 		memcpy(&iaddr.sin_addr, host->h_addr, host->h_length);
					}
				} else { // ip
					snprintf(ipaddr, 16, "%s.%d", w, ifrom++);
					hname = ipaddr;
					if (!(xconnect.bncserver)) {
						if (!inet_aton(ipaddr, &iaddr.sin_addr)) {
							err_printf("inet_aton(%s): %s\n",
								ipaddr, strerror(errno));
							continue;
						}
						host = gethostbyaddr((char *)&iaddr.sin_addr, sizeof(struct in_addr), AF_INET);
						if (host)
							hname = host->h_name;
					}
				}

				for (v = xconnect.vhost; v; v = v->next)
					if (!x_strcasecmp(v->name, hname)) {
						dots = 1;
						break;
					}
				
				if (dots == 1) {
					cdel_printf("%s is already in vhosts list.\n", hname);
					continue;
				}
				
				hname = strdup(hname);
				if (hname == 0) {
					err_printf("strdup(): %s\n", strerror(errno));
					return;
				}
				
				if (xconnect.bncserver) {
					if (add_vhost(hname, 0)) {
						cadd_printf("Added %s to vhost list.\n", hname);
						total++;
					} else
						free(hname);
					continue;
				}
				
				dots = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (dots == -1) {
					err_printf("socket(): %s\n", strerror(errno));
					free(hname);
					return;
				}
				
				addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
				if (addr == 0) {
					err_printf("malloc(): %s\n", strerror(errno));
					close(dots); free(hname);
					return;
				}
				memset((char *)addr, 0, sizeof(struct sockaddr_in));
				memcpy(&addr->sin_addr, &iaddr.sin_addr, sizeof(iaddr.sin_addr));
				addr->sin_family = AF_INET;
				if (bind(dots, (struct sockaddr *)addr, sizeof(struct sockaddr)) == 0) {
					if (add_vhost(hname, addr)) {
						cadd_printf("Added %s to vhost list.\n", hname);
						total++;
					} else {
						close(dots);
						free(hname); free(addr);
						return;
					}
				} else {
					cdel_printf("Couldn't bind to %s.\n", hname);
					free(hname); free(addr);
				}
				close(dots);
			}
			w = 0;
		}
		if (total) {
			cinfo_printf("Total of %d vhosts added.\n", total);
		} else if (!(xconnect.vhost) && !(xconnect.bncserver)) {
			info_printf("No vhost added, launching vhost scan.\n");
			cinfo_printf("Found %d vhosts.\n", get_vhosts());
		} else {
			info_printf("No vhost added.\n");
		}
	} else if (!x_strncasecmp(w, "flush", 5)) {
		if (xall == xcnt && !xconnect.connecting) {
			err_printf("There are no clones to flush.\n");
			return;
		}
		if (xconnect.log_fd) {
			logit(">> .flush\n");
		}
		if (xall != xcnt) {
			xmen *t;
			for (p = root; p; p = t) {
				t = p->next;
				if (p->connected != 3)
					kill_clone(p, 1);
			}
		}
		delete_connect_all();
		return;
	} else if (!x_strncasecmp(w, "disco", 5)) {
		xmen *t;
		int ii;
		
		info_printf("Disconnecting ");
		if (sscanf(cmd, "%d", &ii) == 1) {
			newsplit(&cmd);
			printf("%d clones.\n", ii);
			if (xconnect.log_fd) {
				logit(">> .disco %d\n", ii);
			}
		} else {
			ii = xcnt;
			if (xconnect.log_fd) {
				logit(">> .disco\n");
			}
			printf("all clones.\n");	
		}
		for (i = 0, p = tail; p && i < ii; p = t) {
			t = p->parent;
			if (p->connected != 3) // interesuja nas tylko polaczone z ircem.
				continue;
			if (*cmd)  // sprecyzowalismy reason :/
				xsend(p->fd, "QUIT :%s\n", cmd);
			kill_clone(p, 0);
			i++;
		}
		return;
	} else if (!x_strncasecmp(w, "log", 3)) {
		if (xconnect.log_fd) {
			if (*cmd && (!x_strncasecmp(cmd, "del", 3) || !x_strncasecmp(cmd, "clo", 3))) {
				time_t current = time(0);
				logit("-- Log closed %s", ctime(&current));
				close(xconnect.log_fd);
				xconnect.log_fd = 0;
				cdel_printf("Closed logfile '%s'.\n", xconnect.log_file);
				free(xconnect.log_file);
				xconnect.log_file = 0;
			} else {
				info_printf("Logging is on, logfile is: %s\n", xconnect.log_file);
			}
		} else if (*cmd && (w = newsplit(&cmd))) {
			i = open(w, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR);
			if (i == -1) {
				err_printf("open(): %s\n", strerror(errno));
			} else {
				time_t current = time(0);
				xconnect.log_fd = i;
				xconnect.log_file = strdup(w);
				logit("++ Log opened %s", ctime(&current));
				logit("++ %s version: %s\n", xinfo[0].string, xinfo[1].string);
				info_printf("Started logging to '%s'.\n", w);
			}
		} else {
			help_printf("Usage: %s <logfile | del>\n", w);	
		}
	} else if (!x_strncasecmp(w, "stat", 4) || !x_strcasecmp(w, "s") || \
			!x_strncasecmp(w, "info", 4) || !x_strcasecmp(w, "i")) {
		xchan *ch;
		xclone *c;
		xnick *n;
		int all = 0;
	//	int temp;
		
		if (*cmd && (!x_strcasecmp(cmd, "all") || !x_strcasecmp(cmd, "a")))
			all = 1;
		
		printf("\n%s%s v%s%s%s (c) %s <%s%s%s>%s\n\n", COLOR_CYAN, xinfo[0].string, COLOR_LCYAN,
				xinfo[1].string, COLOR_CYAN, xinfo[2].string, COLOR_LCYAN,
				xinfo[3].string, COLOR_CYAN, COLOR_NORMAL);
		if (xconnect.ircserver) {
			info_printf("Using ircserver    : %s%s%s (%s)\n",
				COLOR_WHITE, xconnect.ircserver, COLOR_NORMAL, xconnect.bncserver? "BNC" : inet_ntoa(xconnect.addr.sin_addr));
			info_printf("Using ircport      : %d\n", xconnect.ircport);
		}
		info_printf("Total of %s%d%s clones loaded (%d on IRC, %d connecting, %d delayed).\n",
				COLOR_WHITE, xall + xconnect.connecting, COLOR_NORMAL,
				xcnt, xall - xcnt, xconnect.connecting);
		if (xconnect.bncserver) {
			info_printf("Using bnc          : %s\n", xconnect.bncserver);	
		}
		info_printf("Connect delay      : %d\n", xconnect.delay);
		info_printf("Rejoin delay       : %d\n", xrejoindelay);
		info_printf("Debug level        : %d\n", debuglvl);
		info_printf("Default clone mode : %s\n", (def_takemode == 0)? "deop" :
					(def_takemode == 1)? "kick" : "close");
		info_printf("Logfile            : %s\n", xconnect.log_fd? xconnect.log_file : "not logging");
		if (all && xall) {
			info_printf("Clones -> ");
			for (p = root; p; p = p->next)
				printf("%s%s%s(%s%d%s), ", COLOR_BLUE, p->nick, COLOR_NORMAL,
						COLOR_WHITE, p->pp, COLOR_NORMAL);
			write(1, "\n", 1);
		}
		for (ch = chanroot; ch; ch = ch->next) {
			if (ch->inactive) {
				info_printf("chan name     : %s%s%s (keeping friends)\n",
						COLOR_WHITE, ch->name, COLOR_NORMAL);
				continue;
			}
			info_printf("chan name     : %s%s%s ", COLOR_WHITE, ch->name, COLOR_NORMAL);
			if (ch->invite)
				fputs("(+i) ", stdout);
			if (ch->limit)
				printf("(+l %d) ", ch->limit);
			if (ch->key)
				printf("(+k %s)", ch->key);
			write(1, "\n", 1);
			if (all)
				info_printf("  topic       : %s\n", ch->topic? ch->topic : "NOT SET");
			info_printf("  clones      : %d (%d ops)\n", ch->clones, ch->clonops);
			info_printf("  users       : %d (%d ops)\n", ch->users, ch->ops);
			if (ch->pun_mode != def_takemode) {
				info_printf("  clone mode  : %s\n", (ch->pun_mode == 0)? "deop" :
							(ch->pun_mode == 1)? "kick" : "close");
			}
			info_printf("  [bIe] modes : %d\n", ch->modes);
			if (all) {
#ifdef _XMEN_DEBUG
				info_printf("  channel cnt : %d\n", ch->cnt);
				info_printf("  Clones -> ");
				for (c = ch->clone; c; c = c->next)
					printf("%s%s%s%s%s(%s%d%s), ", COLOR_LCYAN, c->op? "@" : "", COLOR_BLUE, c->xmen->nick, COLOR_NORMAL, COLOR_WHITE, c->cnt, COLOR_NORMAL);
#else
				info_printf("  Clones -> ");
				for (c = ch->clone; c; c = c->next)
					printf("%s%s%s%s%s, ", COLOR_LCYAN, c->op? "@" : "", COLOR_BLUE, c->xmen->nick, COLOR_NORMAL);
#endif
				write(1, "\n", 1);
				info_printf("  Users  -> ");
				for (i = 0; i < XHASH_SIZE; i++)
					for (n = ch->nick[i]; n; n = n->next)
						printf("%s%s%s%s%s, ", COLOR_WHITE, n->op? "@" : "", n->friend? COLOR_RED : COLOR_NORMAL, n->nick, COLOR_NORMAL);
				write(1, "\n", 1);
#ifdef _XMEN_DEBUG
				info_printf("  Toop   -> (%d) ", ch->toops);
				for (c = ch->c_toop; c; c = c->t_next)
					printf("%s%s%s, ", COLOR_CYAN, c->xmen->nick, COLOR_NORMAL);
				for (n = ch->n_toop; n; n = n->t_next)
					printf("%s%s%s, ", COLOR_CYAN, n->nick, COLOR_NORMAL);
				write(1, "\n", 1);
				info_printf("  Topun  -> (%d) ", ch->topuns);
				for (n = ch->topun; n; n = n->t_next)
					printf("%s%s%s, ", COLOR_CYAN, n->nick, COLOR_NORMAL);
				write(1, "\n", 1);
#endif
/*				info_printf("  Clone's HASH -> ");
				for (i = 0; i < XHASH_CLONE; i++) {
					for (temp = 0, c = ch->h_clone[i]; c; c = c->h_next, temp++)
						;
					printf("%s%d%s(%s%d%s) ", COLOR_CYAN, i+1, COLOR_NORMAL, COLOR_WHITE, temp, COLOR_NORMAL);
				}
				write(1, "\n", 1);
				info_printf("  User's HASH -> ");
				for (i = 0; i < XHASH_SIZE; i++) {
					for (temp = 0, n = ch->nick[i]; n; n = n->next, temp++)
						;
					printf("%s%d%s(%s%d%s) ", COLOR_CYAN, i+1, COLOR_NORMAL, COLOR_WHITE, temp, COLOR_NORMAL);
				}
				write(1, "\n", 1);
*/
			}
		}
		return;
	} else if (!x_strcasecmp(w, "kick") || !x_strcasecmp(w, "deop")) {
		int mode = (*w == 'k')? 1 : 0;
		if (*cmd) {
			xchan *ch;
			if (!x_strncasecmp(cmd, "-a", 2)) {
				for (ch = chanroot; ch; ch = ch->next)
					if (!(ch->inactive)) {
						ch->pun_mode = mode;
						info_printf("Changed %s's clone mode to %s.\n", ch->name, w);
					}
			} else if ((ch = find_channel(cmd)) == 0) {
				err_printf("No such channel: '%s'\n", cmd);
			} else {
				ch->pun_mode = mode;
				info_printf("Changed %s's clone mode to %s.\n", cmd, w);
			}
		} else {
			def_takemode = mode;
			info_printf("Changed default clone mode to %s.\n", w);
		}
	} else if (!x_strncasecmp(w, "quote", 5) || !x_strncasecmp(w, "raw", 3)) {
		if (*cmd) {
			if (!xcnt) {
				err_printf("Load some clones first.\n");
				return;
			}
			if (xconnect.log_fd) {
				logit(">> .raw %s\n", cmd);
			}
			if (*cmd == '-') {
				w = newsplit(&cmd) + 1;
				if (!*cmd) {
					help_printf("Usage: raw [-clonename] <command>\n");
					return;
				}
				xsendint = snprintf(buf, XBSIZE, "%s\n", cmd);
				for (p = root; p; p = p->next)
					if (p->connected == 3 && !x_strcasecmp(p->nick, w))
						break;
				if (p == 0) {
					err_printf("No such clone '%s'.\n", w);
				} else
					put_clone(p, buf, xsendint);
			} else {
				xsendint = snprintf(buf, XBSIZE, "%s\n", cmd);
				for (p = root; p; p = p->next)
					if (p->connected == 3)
						put_clone(p, buf, xsendint);
			}
		} else {
			help_printf("Usage: %s [-clonename] <command>\n", w);
		}
		return;
	} else if (!x_strncasecmp(w, "nick", 4)) {
		int total = 0;
		if ((w = newsplit(&cmd)) == 0) {
			help_printf("Usage: nick <-rand | -crap | -set ##? | -file fname> <nick list>\n");
			return;
		} else if (xcnt == 0) {
			err_printf("Load some clones first.\n");
			return;
		}
		if (xconnect.log_fd) {
			logit(">> .nick %s\n", cmd);
		}
		if (!x_strncasecmp(w, "-r", 2) || !x_strncasecmp(w, "-c", 2)) {
			i = (*(w+1) == 'c')? 2 : 1;
			for (p = root; p; p = p->next)
				if (p->connected == 3) {
					xsend(p->fd, "NICK %s\n", random_nick(i));
					p->pp += 4;
				}
			return;
		} else if (!x_strncasecmp(w, "-s", 2)) {
			char nick[MAX_NICKLEN+1];
			char *n;
			int h;
			if ((w = newsplit(&cmd)) == 0) {
				help_printf("Usage: nick -set <nickmask>\n");
				help_printf("       Where nickmask can contain:\n");
				help_printf("         # - nick counter\n");
				help_printf("         ? - random char\n");
				return;
			}
			for (i = h = 0, n = nick; *w && i < MAX_NICKLEN; i++, w++)
				if (*w != '#' || !h++)
					*n++ = *w;
			*n = 0;
			for (i = 0, p = root; p; p = p->next)
				if (p->connected == 3) {
					xsend(p->fd, "NICK %s\n", make_nick(nick, h, ++i));
					p->pp += 4;
				}
			return;
		} else if (!x_strncasecmp(w, "-f", 2)) {
			FILE *fp;
			if ((w = newsplit(&cmd)) == 0) {
				help_printf("Usage: nick -file <filename with nicks>\n");
				return;
			} else if ((fp = fopen(w, "r")) == 0) {
				err_printf("Can't open '%s' for reading: %s\n", w, strerror(errno));
				return;
			}
			p = root;
			while (p) {
				if (p->connected != 3) continue;
				if (!fgets(cmdline, MAX_CMDLEN, fp)) break;
				cmdline[strlen(cmdline)-1] = 0;
				if (!is_clone(buf)) {
					xsend(p->fd, "NICK %s\n", cmdline);
					total++;
					p->pp += 4;
					p = p->next;
				}
			}
			fclose(fp);
			info_printf("Trying to take %d nicks.\n", total);
			return;
		}
		p = root;
		while (p) {
			if (p->connected != 3) continue;
			if (w == 0 && (w = newsplit(&cmd)) == 0)
				break;
			if (!is_clone(w)) { // zaden klon nie trzyma tego nicka
				xsend(p->fd, "NICK %s\n", w);
				total++;
				p->pp += 4;
				p = p->next;
			}
			w = 0;
		}
		info_printf("Trying to take %d nicks.\n", total);
	} else if (!x_strcasecmp(w, "j") || !x_strncasecmp(w, "join", 4)) {
		if (*cmd) {
			int ii;
			xchan *ch;
			
			if (!xcnt) {
				err_printf("Try to load some clones first.\n");
				return;
			}

			if (xconnect.log_fd) {
				logit(">> .join %s\n", cmd);
			}
			
			if (sscanf(cmd, "%d", &ii) < 1)
				ii = xcnt;
			else
				newsplit(&cmd);
			
			if ((w = newsplit(&cmd)) == 0 || !channame(w)) {
				err_printf("Bad channel name: '%s' :/\n", w);
				return;
			}
			
			ch = find_channel(w);
			xsendint = snprintf(buf, XBSIZE, "JOIN %s %s\n", w, ch? ch->key : cmd); // haha :/
			
			xlastjoin = ii;
			for (p = root, i = 0; p && i < ii; p = p->next)
				if (p->connected == 3) {
					write(p->fd, buf, xsendint);
					p->pp += 3;
					i++;
				}
		} else {
			help_printf("Usage: %s [number] <channel> [key]\n", w);
		}
		return;
	} else if (!x_strcasecmp(w, "p") || !x_strncasecmp(w, "part", 4) || \
			!x_strncasecmp(w, "npart", 5) || !x_strcasecmp(w, "l") || \
			!x_strncasecmp(w, "leave", 5) || !x_strncasecmp(w, "nleave", 6) || \
			!x_strncasecmp(w, "cycle", 5)) {
		if (*cmd) {
			int ii = -1, cycle = (*w == 'c')? 1 : 0, nokick = (*w == 'n')? 1 : 0;
			xchan *ch = 0;
			xclone *c;
			
			if (xconnect.log_fd) {
				logit(">> .%s %s\n", w, cmd);
			}
			
			if (sscanf(cmd, "%d", &ii) == 1)
				newsplit(&cmd);

			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s' :/\n", w);
				return;
			}
			
			if (ii == -1)
				ii = ch->clones;
			
			for (i = 0, c = ch->clone; c && i < ii; c = c->next, i++) {
				if (cycle) {
					if (c->op && !nokick) {
						xsend(c->xmen->fd, "KICK %s %s :%s\nJOIN %s %s\n",
						     w, c->xmen->nick, (*cmd)? cmd : random_reason(), w, ch->key? ch->key : "");
						c->xmen->pp += 7;
					} else {
						xsend(c->xmen->fd, "PART %s :%s\nJOIN %s %s\n",
							w, (*cmd)? cmd : random_reason(), w, ch->key? ch->key : "");
						c->xmen->pp += 8;
					}
				} else {
					if (c->op && !nokick) {
						xsend(c->xmen->fd, "KICK %s %s :%s\n", w, c->xmen->nick,
								(*cmd)? cmd : random_reason());
						c->xmen->pp += 4;
					} else {
						xsend(c->xmen->fd, "PART %s :%s\n", w, (*cmd)? cmd : random_reason());
						c->xmen->pp += 5;
					}
				}
			}
			
			if (!cycle && i == ch->clones)
				del_channel(ch);

		} else {
			help_printf("Usage: %s [number] <channel> [reason]\n", w);
		}
	} else if (!x_strncasecmp(w, "clean", 5)) {
		if (channame(cmd)) {
			xclone *c;
			xmask *m, *t_m;
			xchan *ch;
			int mod_lim, tot, cont;
			char *mode_arg[5], mode_type[5];
			
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			} else if (!(ch->done)) {
				err_printf("There are no opped clones on %s.\n", w);
				return;
			}

			if (xconnect.log_fd) {
				logit(">> .clean %s\n", ch->name);
			}
			
			for (c = ch->clone; c && ch->modes; c = c->next) {
				if (!(c->op) || c->xmen->pp > 9)
					continue;
				if (c->xmen->pp < 3)
					mod_lim = 5;
				else if (c->xmen->pp < 6)
					mod_lim = 4;
				else
					mod_lim = 3;
				
				tot = cont = xsendint = 0;
				for (m = ch->ban; m && mod_lim; m = t_m, mod_lim--) {
					t_m = m->next;
					mode_arg[tot++] = strdup(m->mask);
					del_mode(ch, 'b', m);
				}
				switch (tot) {
					case 5:
						xsend(c->xmen->fd, "MODE %s -bb %s %s\nMODE %s -bbb %s %s %s\n",
								ch->name, mode_arg[0], mode_arg[1],
								ch->name, mode_arg[2], mode_arg[3], mode_arg[4]);
						cont = 1;
						break;
					case 4:
						xsend(c->xmen->fd, "MODE %s -b %s\nMODE %s -bbb %s %s %s\n",
								ch->name, mode_arg[0], ch->name,
								mode_arg[1], mode_arg[2], mode_arg[3]);
						cont = 1;
						break;
					case 3:
						xsend(c->xmen->fd, "MODE %s -bbb %s %s %s\n",
								ch->name, mode_arg[0], mode_arg[1], mode_arg[2]);
						cont = 1;
						break;
					case 2:
						if (c->xmen->pp > 2) {
							xsend(c->xmen->fd, "MODE %s -bb %s %s\n",
									ch->name, mode_arg[0], mode_arg[1]);
							cont = 1;
						} else
							xsendint += snprintf(buf, XBSIZE, "MODE %s -bb %s %s\n",
									ch->name, mode_arg[0], mode_arg[1]);
						break;
					case 1:
						if (c->xmen->pp > 5) {
							xsend(c->xmen->fd, "MODE %s -b %s\n",
									ch->name, mode_arg[0]);
							cont = 1;
						} else
							xsendint += snprintf(buf, XBSIZE, "MODE %s -b %s\n",
									ch->name, mode_arg[0]);
						break;
				}
				if (tot) {
					c->xmen->pp += ((tot>3? 2 : 1) + (3 * tot));
					while (tot)
						free(mode_arg[--tot]);
					if (cont)
						continue;
				}
				for (m = ch->inv; m && mod_lim; m = t_m, mod_lim--) {
					t_m = m->next;
					mode_arg[tot] = strdup(m->mask);
					mode_type[tot++] = 'I';
					del_mode(ch, 'I', m);
				}
				for (m = ch->exc; m && mod_lim; m = t_m, mod_lim--) {
					t_m = m->next;
					mode_arg[tot] = strdup(m->mask);
					mode_type[tot++] = 'e';
					del_mode(ch, 'e', m);
				}
				switch (tot) {
					case 5:
						xsendint += snprintf(buf+xsendint, XBSIZE-xsendint,
								"MODE %s -%c%c %s %s\nMODE %s -%c%c%c %s %s %s\n",
								ch->name, mode_type[0], mode_type[1],
									mode_arg[0], mode_arg[1],
								ch->name, mode_type[2], mode_type[3], mode_type[4],
									mode_arg[2], mode_arg[3], mode_arg[4]);
						break;
					case 4:
						xsendint += snprintf(buf+xsendint, XBSIZE-xsendint,
								"MODE %s -%c %s\nMODE %s -%c%c%c %s %s %s\n",
								ch->name, mode_type[0], mode_arg[0],
								ch->name, mode_type[1], mode_type[2], mode_type[3],
									mode_arg[1], mode_arg[2], mode_arg[3]);

						break;
					case 3:
						xsendint += snprintf(buf+xsendint, XBSIZE-xsendint,
								"MODE %s -%c%c%c %s %s %s\n",
								ch->name, mode_type[0], mode_type[1], mode_type[2],
								mode_arg[0], mode_arg[1], mode_arg[2]);
						break;
					case 2:
						xsendint += snprintf(buf+xsendint, XBSIZE-xsendint,
								"MODE %s -%c%c %s %s\n",
								ch->name, mode_type[0], mode_type[1],
								mode_arg[0], mode_arg[1]);
						break;
					case 1:
						xsendint += snprintf(buf+xsendint, XBSIZE-xsendint,
								"MODE %s -%c %s\n", ch->name,
								mode_type[0], mode_arg[0]);
						break;
				}
				if (tot) {
					c->xmen->pp += ((tot>3? 2 : 1) + (3 * tot));
					while (tot)
						free(mode_arg[--tot]);
				}
				if (xsendint)
					write(c->xmen->fd, buf, xsendint);
			}
		} else {
			help_printf("Usage: clean <channel>\n");
		}
	} else if (!x_strncasecmp(w, "crap", 4)) {
		if (channame(cmd)) {
			xchan *ch;
			xclone *c;
			int max_mod;
			char *crap[] = {
				"*!*@*1*.*", "*!*@*.*1*", "*!*@*2*.*", "*!*@*.*2*",
				"*!*@*3*.*", "*!*@*.*3*", "*!*@*4*.*", "*!*@*.*4*",
				"*!*@*5*.*", "*!*@*.*5*", "*!*@*6*.*", "*!*@*.*7*",
				"*!*@*8*.*", "*!*@*.*8*", "*!*@*9*.*", "*!*@*.*9*",
				"*!*@*0*.*", "*!*@*.*0*", "*!*@*1*:*", "*!*@*:*1*",
				"*!*@*2*:*", "*!*@*:*2*", "*!*@*3*:*", "*!*@*:*3*",
				"*!*@*4*:*", "*!*@*:*4*", "*!*@*5*:*", "*!*@*:*5*",
				"*!*@*6*:*", "*!*@*:*7*", "*!*@*8*:*", "*!*@*:*8*",
				"*!*@*9*:*", "*!*@*:*9*", "*!*@*0*:*", "*!*@*:*0*",
			};
			
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			} else if (!(ch->done)) {
				err_printf("There are no opped clones on %s.\n", w);
				return;
			}

			if (xconnect.log_fd) {
				logit(">> .crap %s\n", ch->name);
			}
			
			max_mod = 33 - ch->modes;
			for (i = 0, c = ch->clone; c && i < max_mod; c = c->next) {
				if (!(c->op) || c->xmen->pp > 9)
					continue;
				if (c->xmen->pp < 3) {
					xsend(c->xmen->fd, "MODE %s +bb %s %s\nMODE %s +bbb %s %s %s\n",
							ch->name, crap[i], crap[i+1],
							ch->name, crap[i+2], crap[i+3], crap[i+4]);
					c->xmen->pp += 17;
					i += 5;
				} else if (c->xmen->pp < 6) {
					xsend(c->xmen->fd, "MODE %s +b %s\nMODE %s +bbb %s %s %s\n",
							ch->name, crap[i], ch->name,
							crap[i+1], crap[i+2], crap[i+3]);
					c->xmen->pp += 14;
					i += 4;
				} else {
					xsend(c->xmen->fd, "MODE %s +bbb %s %s %s\n",
							ch->name, crap[i], crap[i+1], crap[i+2]);
					c->xmen->pp += 10;
					i += 3;
				}
			}
		} else {
			help_printf("Usage: crap <channel>\n");
		}
	} else if (!x_strncasecmp(w, "say", 3) || !x_strncasecmp(w, "top", 3)) {
		xchan *ch;
		xclone *c;
		int top = (*w == 't')? 1 : 0;
		if (channame(cmd)) {
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			}
		} else if ((ch = chanroot) == 0) {
			err_printf("You haven't joined any channels.\n");
			return;
		}

		if (xconnect.log_fd) {
			logit(">> .%s %s %s\n", top? "topic" : "say", ch->name, cmd);
		}
		
		if (top) {
			if (!(ch->done)) {
				err_printf("There are no opped clones on %s.\n", ch->name);
				return;
			}
		} else if (!*cmd) { // do topic unset, samo /topic #chan
			help_printf("Usage: say [channel] <message>\n");
			return;
		}
		
		for (c = ch->clone; c; c = c->next)
			if (c->xmen->pp < 10)
				break;
		if (top) {
			xsendint = snprintf(buf, XBSIZE, "TOPIC %s :%s\n", ch->name, cmd);
			c->xmen->pp += 4;
		} else {
			xsendint = snprintf(buf, XBSIZE, "PRIVMSG %s :%s\n", ch->name, cmd);
			c->xmen->pp += (1 + (xsendint/100));
		}
		write(c->xmen->fd, buf, xsendint);
	} else if (!x_strncasecmp(w, "mode", 4)) {
		xchan *ch;
		xclone *c;
		if (channame(cmd)) {
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			}
		} else if ((ch = chanroot) == 0) {
			err_printf("You haven't joined any channels.\n");
			return;
		}
		if (!*cmd) {
			help_printf("Usage: mode [channel] <mode string>\n");
			return;
		} else if (!(ch->done)) {
			err_printf("There are no opped clones on %s.\n", ch->name);
			return;
		}

		if (xconnect.log_fd) {
			logit(">> .mode %s %s\n", ch->name, cmd);
		}
		
		for (c = ch->clone; c; c = c->next)
			if (c->xmen->pp < 10)
				break;

		xsendint = snprintf(buf, XBSIZE, "MODE %s %s\n", ch->name, cmd);
		put_clone(c->xmen, buf, xsendint);
		
		return;
	} else if (!x_strncasecmp(w, "msg", 3) || !x_strncasecmp(w, "mmsg", 4)) {
		int mass = (*(w+1) == 'm') ? 1 : 0;
		
		if ((w = newsplit(&cmd)) == 0 || !*cmd) {
			help_printf("Usage: msg <target> <message>\n");
			return;
		}
		
		if (xconnect.log_fd) {
			logit(">> .%s%s %s\n", mass? "m" : "", w, cmd);
		}
		
		xsendint = snprintf(buf, XBSIZE, "PRIVMSG %s :%s\n", w, cmd);
		if (mass) {
			for (p = root; p; p = p->next) {
				write(p->fd, buf, xsendint);
				p->pp += (1 + (xsendint/100));
			}
		} else {
			for (p = root; p; p = p->next)
				if (p->pp < 10)
					break;
			write(p->fd, buf, xsendint);
			p->pp += (1 + (xsendint/100));
		}
	} else if (!x_strncasecmp(w, "inv", 3)) {
		xchan *ch;
		xclone *c;
		if (channame(cmd)) {
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			} else if (!ch->done) {
				err_printf("There are no opped clones on %s.\n", ch->name);
				return;
			}
			if (!*cmd) {
				help_printf("Usage: invite [channel] <nicks>\n");
			}

			if (xconnect.log_fd) {
				logit(">> .invite %s %s\n", w, ch->name);
			}
			
			while ((w = newsplit(&cmd)))
				if ((c = get_clone(ch))) {
					xsend(c->xmen->fd, "INVITE %s %s\n", w, ch->name);
					c->xmen->pp += 4;	
				}
		} else {
			char *tcmd, *tcmd_free;
			if (!*cmd) {
				help_printf("Usage: invite [channel] <nicks>\n");
			}
			for (ch = chanroot; ch; ch = ch->next) {
				if (!(ch->done)) continue;
				tcmd = tcmd_free = strdup(cmd);
				while ((w = newsplit(&tcmd)))
					if ((c = get_clone(ch))) {
						xsend(c->xmen->fd, "INVITE %s %s\n", w, ch->name);
						c->xmen->pp += 4;
					}
				free(tcmd_free);
			}		
		}
	} else if (!x_strncasecmp(w, "open", 4)) {
		if (*cmd) {
			int all = 0;
			xchan *ch;

			if (xconnect.log_fd) {
				logit(">> .open %s\n", cmd);
			}
			
			if (!x_strncasecmp(cmd, "-a", 2))
				all = 1;
			else if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0 || ch->inactive) {
				err_printf("No such channel: '%s'\n", w);
				return;
			}
			for (ch = (all? chanroot : ch); ch; ch = ch->next) {
				if (ch->inactive) continue;
				if (ch->pun_mode == 2)
					ch->pun_mode = (def_takemode == 2? DEF_TAKEMODE : def_takemode);
				info_printf("Changed %s's clone mode to %s.\n", ch->name, (ch->pun_mode == 1)? "kick" : "deop");
				if (ch->clonops && (ch->limit < 0 || ch->invite || ch->key)) {
					xclone *c;
					if ((c = get_clone(ch))) {
						xsendint = snprintf(buf, XBSIZE, "MODE %s -ikl %s\n",
							ch->name, ch->key? ch->key : "");
						write(c->xmen->fd, buf, xsendint);
						c->xmen->pp += 10;
					}
				}
				if (!all) break;
			}
		} else {
			help_printf("Usage: open <channel | -all>\n");
		}
	} else if (!x_strncasecmp(w, "close", 5)) {
		
		if (xconnect.log_fd) {
			logit(">> .close %s\n", cmd);
		}
		
		if (*cmd) {
			int all = 0;
			xchan *ch;
			if (!x_strncasecmp(cmd, "-a", 2))
				all = 1;
			else if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0 || ch->inactive) {
				err_printf("No such channel: '%s'\n", w);
				return;
			}
			for (ch = (all? chanroot : ch); ch; ch = ch->next) {
				if (ch->inactive) continue;
				ch->pun_mode = 2;
				info_printf("Changed %s's clone mode to close.\n", ch->name);
				if (ch->done == 2) {
					xnick *n;
					if (ch->limit > -1) {
						xclone *c;
						if ((c = get_clone(ch))) {
							xsend(c->xmen->fd, "MODE %s +l -69\n", ch->name);
							c->xmen->pp += 4;
						}
					}
					for (i = 0; i < XHASH_SIZE; i++)
						for (n = ch->nick[i]; n; n = n->next)
							if (!(n->friend) && !(n->topun) && !(n->sentact_pun))
								add_topun(ch, n);
					chan_action(ch);
				}
				if (!all) break;
			}
		} else {
			def_takemode = 2;
			info_printf("Changed default clone mode to close.\n");
		}
	} else if (!x_strncasecmp(w, "mkick", 5)) {
		if (*cmd) {
			xchan *ch;
			xnick *n;
			if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
				err_printf("No such channel: '%s'\n", w);
				return;
			} else if (!(ch->done)) {
				err_printf("There are no opped clones on %s.\n", w);
				return;
			}

			if (xconnect.log_fd) {
				logit(">> .mkick %s\n", ch->name);
			}
			
			if (ch->pun_mode == 0) // if deop then zmieniamy na kick
				ch->pun_mode = 1;
			if (ch->limit > -1) {
				xclone *c;
				if ((c = get_clone(ch))) {
					xsend(c->xmen->fd, "MODE %s +l -69\n", ch->name);
					c->xmen->pp += 4;
				}
			}
			for (i = 0; i < XHASH_SIZE; i++)
				for (n = ch->nick[i]; n; n = n->next)
					if (!(n->friend) && !(n->topun) && !(n->sentact_pun))
						add_topun(ch, n);

			chan_action(ch);	
		} else {
			help_printf("Usage: mkick <channel>\n");
		}
	} else if (!x_strncasecmp(w, "op", 2) || !x_strncasecmp(w, "dop", 3)) {
		if (*cmd) {
			int op = 0;
			xchan *ch;
			xnick *n;
			
			if (!x_strncasecmp(w, "op", 2))
				op = 1;

			if (xconnect.log_fd) {
				logit(">> .%s %s\n", w, cmd);
			}
			
			if (channame(cmd)) {
				if ((w = newsplit(&cmd)) == 0 || (ch = find_channel(w)) == 0) {
					err_printf("No such channel: '%s' :/\n", w);
					return;
				} else if (!ch->clonops) {
					err_printf("There are no opped clones on %s :/\n", w);
					return;
				}
				if (!x_strncasecmp(cmd, "-all", 4)) {
					for (i = 0; i < XHASH_SIZE; i++)
						for (n = ch->nick[i]; n; n = n->next)
							if (op) {
								if (!n->op && !n->toop && !n->sentact_op)
									add_toop_nick(ch, n);
							} else {
								if (n->op && !n->topun && !n->sentact_pun)
									add_topun(ch, n);
							}
				} else {
					while ((w = newsplit(&cmd))) {
						if ((n = find_nick_slow(ch, w))) {
							if (op) {
								if (!n->op && !n->toop && !n->sentact_op)
									add_toop_nick(ch, n);
							} else {
								if (n->op && !n->topun && !n->sentact_pun)
									add_topun(ch, n);
							}
						} else {
							err_printf("No such nick on %s: %s :/\n", ch->name, w);
						}
					}
				}
				if (!op && ch->pun_mode) {
					i = ch->pun_mode;
					ch->pun_mode = 0;
					chan_action(ch);
					ch->pun_mode = i;
				} else
					chan_action(ch);
			} else {
				for (ch = chanroot; ch; ch = ch->next) {
					if (!ch->clonops) continue;
					if (!x_strncasecmp(cmd, "-all", 4)) {
						for (i = 0; i < XHASH_SIZE; i++)
							for (n = ch->nick[i]; n; n = n->next)
								if (op) {
									if (!n->op && !n->toop && !n->sentact_op)
										add_toop_nick(ch, n);
								} else {
									if (n->op && !n->topun && !n->sentact_pun)
										add_topun(ch, n);
								}
					} else {
						char *tcmd, *tcmd_free;
						tcmd = tcmd_free = strdup(cmd);
						while ((w = newsplit(&tcmd))) {
							if ((n = find_nick_slow(ch, w))) {
								if (op) {
									if (!n->op && !n->toop && !n->sentact_op)
										add_toop_nick(ch, n);
								} else {
									if (n->op && !n->topun && !n->sentact_pun)
										add_topun(ch, n);
								}
							}
						}
						free(tcmd_free);
					}
					if (!op && ch->pun_mode) { // deopujemy a sa ustawione kicki
						i = ch->pun_mode;
						ch->pun_mode = 0;
						chan_action(ch);
						ch->pun_mode = i;
					} else
						chan_action(ch);
				}
			}
		} else {
			help_printf("Usage: %s [channel] <nicks || -all>\n", w);
		}
	} else if (!x_strncasecmp(w, "friend", 6) || !x_strcasecmp(w, "f")) {
		xchan *ch = 0;
		xfriend *f;
		if ((w = newsplit(&cmd)) == 0) {
			help_printf("Usage: friend <[a]dd | [d]el | [l]ist> [-all] [channel | number/masks]\n");
			return;
		}

		if (xconnect.log_fd) {
			logit(">> .friend %s %s\n", w, cmd);
		}
		
		if (!x_strcasecmp(w, "list") || !x_strcasecmp(w, "l")) {
			if (*cmd) {
				if ((w = newsplit(&cmd)) == 0 || (ch = find_channel_friend(w)) == 0) {
					err_printf("No such channel: '%s'\n", w);	
					return;
				} else if (!(ch->friend)) {
					err_printf("There are no friends on channel %s.\n", ch->name);
					return;
				}
				for (i = 0, f = ch->friend; f; f = f->next, i++) {
					info_printf("%s%2d%s: %s\n", COLOR_WHITE, i+1, COLOR_NORMAL, f->mask);
				}
			} else {
				info_printf("Global friends:%s", friendroot? "\n" : " (no friends)\n");
				for (i = 0, f = friendroot; f; f = f->next, i++) {
					info_printf("  %s%2d%s: %s\n", COLOR_WHITE, i+1, COLOR_NORMAL, f->mask);
				}
				if (chanroot)
					info_printf("Channel friends:\n");
				for (ch = chanroot; ch; ch = ch->next) {
					info_printf(" chan name:  %s%s", ch->name, ch->friend? "\n" : " (no friends)\n");
					for (i = 0, f = ch->friend; f; f = f->next, i++) {
						info_printf("   %s%2d%s: %s\n", COLOR_WHITE, i+1, COLOR_NORMAL, f->mask);
					}
				}
			}
			return;
		} else if (!x_strcasecmp(w, "del") || !x_strcasecmp(w, "d")) {
			int ii, all = 0;

			if (!x_strncasecmp(cmd, "-all", 4)) {
				all = 1;
				newsplit(&cmd);
			}
			
			if (channame(cmd)) {
				if ((w = newsplit(&cmd)) == 0 || (ch = find_channel_friend(w)) == 0) {
					err_printf("No such channel: '%s'\n", w);
					return;
				}
			}

			if (all) {
				xfriend *t_f;
				for (f  = (ch? ch->friend : friendroot); f; f = t_f) {
					t_f = f->next;
					del_friend(ch, f);
				}
				return;
			}
			
			if (!*cmd) {
				err_printf("You have to specify number/masks to delete.\n");
				return;
			}

			if (sscanf(cmd, "%d", &ii) < 1) {
				while ((w = newsplit(&cmd))) {
					i = 0;
					if (!strchr(w, '!') && !strchr(w, '@')) {
						xchan *chan;
						xnick *n;
						for (chan = chanroot; chan; chan = chan->next)
							if (!(chan->inactive) && (n = find_nick_slow(chan, w))) {
								i = snprintf(buf, XBSIZE, "*!%s", n->address);
								break;
							}
						if (!i) i = snprintf(buf, XBSIZE, "%s!*@*", w);
					}
					if ((f = find_friend(ch, (i? buf : w)))) // global -> ch = 0
						del_friend(ch, f);
					else {
						err_printf("Couldn't find friendmask '%s'\n", w);
					}
				}	
			} else {
				ii--;
				for (i = 0, f = (ch? ch->friend : friendroot); f && i < ii; f = f->next, i++)
					;
				if (f) {
					del_friend(ch, f);
				} else {
					err_printf("Wrong friend number #%d\n", ii+1);
				}
			}
			return;
		} else if (!x_strcasecmp(w, "add") || !x_strcasecmp(w, "a")) {
			int all = 0;
			xnick *n;
			
			if (!x_strncasecmp(cmd, "-all", 4)) {
				all = 1;
				newsplit(&cmd);
			}
			
			if (all || channame(cmd)) {
				if ((w = newsplit(&cmd)) == 0 || (ch = find_channel_friend(w)) == 0) {
					err_printf("No such channel: '%s'\n", w);
					return;
				}
			}

			if (all) {
				for (i = 0; i < XHASH_SIZE; i++)
					for (n = ch->nick[i]; n; n = n->next)
						if (!(n->friend)) {
							snprintf(buf, XBSIZE, "*!%s", n->address);
							if (!find_friend(0, buf))
								add_friend(0, buf);
						}
				return;
			}
			
			if (!*cmd) {
				err_printf("You have to specify friendmasks to add.\n");
				return;
			}
			
			while ((w = newsplit(&cmd))) {
				i = 0;
				if (!strchr(w, '!')) {
					if (!strchr(w, '@')) { // po prostu 'nick'
						xchan *chan;
						for (chan = chanroot; chan; chan = chan->next)
							if (!(chan->inactive) && (n = find_nick_slow(chan, w))) {
								i = snprintf(buf, XBSIZE, "*!%s", n->address);
								break;
							}
						if (!i) i = snprintf(buf, XBSIZE, "%s!*@*", w);
					} else // user@host
						i = snprintf(buf, XBSIZE, "*!%s", w);
				} else if (!strchr(w, '@')) { // nick!cos ?
						err_printf("Bad mask: '%s'\n", w);
						continue;
					}
				if (find_friend(ch, (i? buf : w))) {
					err_printf("There is already added mask '%s' in %s friendlist.\n",
							(i? buf : w), ch? ch->name : "global");
				} else
					add_friend(ch, (i? buf : w));
			}
			return;
		}
		help_printf("Usage: friend <[a]dd | [d]el | [l]ist> [-all] [channel | number/masks]\n");
	} else if (!x_strncasecmp(w, "ping", 4) || !x_strncasecmp(w, "cping", 5)) {
		if (!xcnt) {
			err_printf("No clones connected :/\n");
			return;
		} else if (xall != xcnt || xconnect.connecting) {
			err_printf("Wait for all clones to connect.\n");
			return;
		} else if (cping || xpingreplies) {
			err_printf("Wait for lag-check to finish.\n");
			return;
		}
		
		if (xconnect.log_fd) {
			logit(">> .ping\n");
		}
		
		info_printf("Checking lags...\n");
		
		gettimeofday(&tv_ping, 0);
		cping = (0.000001 * tv_ping.tv_usec + tv_ping.tv_sec);
		
		ping = 0;
		xsendint = snprintf(buf, XBSIZE, "PING :%s\n", xconnect.ircserver);
		for (p = root; p; p = p->next)
			if (p->connected == 3) {
				write(p->fd, buf, xsendint);
				p->pp += 2;
			}
		return;
	} else if (!x_strncasecmp(w, "sort", 4) || !x_strncasecmp(w, "first", 5)) {
		int ii;
		if (sscanf(cmd, "%d", &ii) == 1) {
			newsplit(&cmd);
			if (*cmd) {
				xchan *ch;
				xclone *c;
				if ((ch = find_channel(cmd)) == 0) {
					err_printf("No such channel: '%s' :/\n", cmd);
					return;
				}
				for (c = ch->clone, i = 0; c && i < ii; c = c->next, i++)
					printf("%s%s%s ", COLOR_BLUE, c->xmen->nick, COLOR_NORMAL);
				if (i > 0)
					write(1, "\n", 1);
			} else {
				for (p = root, i = 0; p && i < ii; p = p->next, i++)
					printf("%s%s%s ", COLOR_BLUE, p->nick, COLOR_NORMAL);
				if (i > 0)
					write(1, "\n", 1);
			}
		} else {
			help_printf("Usage: %s <number> [channel]\n", w);
		}
		return;
	} else if (!x_strncasecmp(w, "rej", 3)) {
		if (sscanf(cmd, "%d", &i) == 1) {
			if (i < 2) i = 2;
			if (xrejointimer && (xrejointimer -= (xrejoindelay - i)) < 2)
				xrejointimer = 2;
			info_printf("Changed rejoin delay from %d to %d.\n", xrejoindelay, i);
			xrejoindelay = i;
		} else if (!x_strncasecmp(cmd, "kill", 4) || !x_strncasecmp(cmd, "del", 3) || !x_strncasecmp(cmd, "rem", 3)) {
			if (!xcnt) {
				err_printf("There are no loaded clones.\n");
				return;
			}
			for (p = root; p; p = p->next)
				if (p->rejoin_buf) {
					free(p->rejoin_buf);
					p->rejoin_buf = 0;
					p->rejoins = 0;
				}
			info_printf("Removed rejoins from all clones.\n");
		} else
			info_printf("Current rejoin delay is set to %d.\n", xrejoindelay);
	} else if (!x_strncasecmp(w, "del", 3)) {
		if (sscanf(cmd, "%d", &i) == 1) {
			if (i < 1) i = 1;
			if (xconnect.timer && (xconnect.timer -= (xconnect.delay - i)) < 1)
				xconnect.timer = 1;
			info_printf("Changed delay from %d to %d.\n", xconnect.delay, i);
			xconnect.delay = i;
		} else
			info_printf("Current delay is set to %d.\n", xconnect.delay);
	} else if (!x_strncasecmp(w, "debug", 5) || !x_strncasecmp(w, "show", 4)) {
		if (sscanf(cmd, "%d", &i) == 1) {
			info_printf("Changed %s level from %d to %d.\n", w, debuglvl, i);
			debuglvl = i;
		} else
			info_printf("Current %s level is %d.\n", w, debuglvl);
	} else if (!x_strncasecmp(w, "exit", 4) || !x_strncasecmp(w, "bye", 3) \
			|| !x_strncasecmp(w, "quit", 4) || !x_strcasecmp(w, "q")) {
		
		if (xconnect.log_fd) {
			logit(">> .%s\n", w);
		}
		
		xmen_exit(0);	
	} else if (!x_strncasecmp(w, "greet", 4)) {
		info_printf("Thanks to the following people for ideas / help with testing:\n");
		info_printf("  %s    <%s>\n", xinfo[4].string, xinfo[5].string);
		info_printf("  %s      <%s>\n", xinfo[6].string, xinfo[7].string);
	} else if (!x_strncasecmp(w, "help", 4)) {
		help_printf("General commands : help, greet, debug, quit.\n");
		help_printf("Load commands    : load, bnc, vh, delay, flush, disco.\n");
		help_printf("Clone commands   : info, kick, deop, close, open, friend, ping,\n");
		help_printf("                   rejoin, first.\n");
		help_printf("IRC commands     : nick, join, part, cycle, op, dop, mkick, invite,\n");
		help_printf("                   clean, crap, mode, say, topic, msg, mmsg, raw.\n")
	} else {
		err_printf("Unknown command '%s'.\a\n", w);
	}
			
	return;
}

void delete_connect_all(void)
{
	if (xconnect.connecting > 0) {
		if (xconnect.log_fd) {
			logit("<< Canceled loading of %d clones.\n", xconnect.connecting);
		}
		cdel_printf("Canceled loading of %d clones.\n", xconnect.connecting);
		if (xall && xall == xcnt) {
			if (xconnect.log_fd) {
				logit("<< All %d clones has been connected to %s.\n", xall, xconnect.ircserver);
			}
			cinfo_printf("All %d clones has been connected to %s.\n", xall, xconnect.ircserver);
			if (xconnect.ident_file)
				set_ident(xconnect.ident_org, 0);
		}
	}
	xconnect.connecting = 0;
	
	if (xcnt == 0) {
		free(xconnect.ircserver);
		xconnect.ircserver = 0;
		xconnect.ircport = DEF_IRCPORT;
	}
}

void do_connect(void)
{
	xmen *p;

	if (xconnect.vhost == 0) {
		err_printf("do_connect(): No vhosts left.\n");
		delete_connect_all();
		return;
	}
	
	cadd_printf("Connecting #%d (%d left)\n", xall + 1, xconnect.connecting - 1);
	
	if (--xconnect.connecting > 0) {
		xconnect.timer = xconnect.delay;
		if (xconnect.delay > 0 && debuglvl > 0)
			info_printf("Delaying next connect()..\n");
	}

	if ((p = new_clone()) == 0) {
		delete_connect_all();
		return;
	}

	if (bind(p->fd, next_vhost(), sizeof(struct sockaddr)) == -1) {
		err_printf("do_connect()->bind(%s): %s\n", lastvhost->name, strerror(errno));
		kill_clone(p, 0);
		del_vhost(lastvhost, 1);
		if (xconnect.vhost == 0)
			delete_connect_all();
		else // probujemy jeszcze raz, z innym vhostem
			xconnect.connecting++;
		return;
	}

	if (xconnect.ident_file)
		set_ident(random_ident(), xconnect.ident_oidentd2);

	if (connect(p->fd, (struct sockaddr *)&xconnect.addr, sizeof(xconnect.addr)) == -1) {
		if (errno == EINPROGRESS) { // wszystko ok
			p->connected = 1;
#ifdef _USE_POLL
			SET_WRITE_EVENT(p->pfd);
#endif
		} else {
			err_printf("do_connect()->connect(): %s\n", strerror(errno));
			kill_clone(p, 1);
			del_vhost(lastvhost, 1); // pewnie to wina vhosta :>
			if (xconnect.vhost == 0)
				delete_connect_all();
			else
				xconnect.connecting++;
			
		}
	} else { // lol, od razu sie polaczyl, that's impossible !
#ifdef _XMEN_DEBUG
		fprintf(fdebug, "+ Clone %s connect()'ed, sending NICK + USER (xcnt=%d, xall=%d, xcon=%d)\n",
				p->nick, xcnt, xall, xconnect.connecting);
#endif
		xdebug(5, "Clone %s connect()'ed, sending NICK + USER\n", p->nick);
		p->connected = 2;
#ifdef _USE_POLL
		SET_READ_EVENT(p->pfd); // write'a nie trzeba usuwac
#endif
		xsend(p->fd, "NICK %s\n", p->nick);
		xsend(p->fd, "USER %s - - :%s\n",
				random_ident(), random_realname());
	}
}

void do_connect_bnc(void)
{
	xmen *p;

	cadd_printf("Connecting #%d (%d left)\n", xall + 1, xconnect.connecting - 1);
	
	if (--xconnect.connecting > 0) {
		xconnect.timer = xconnect.delay;
		if (xconnect.delay > 0 && debuglvl > 0)
			info_printf("Delaying next connect()..\n");
	}

	if ((p = new_clone()) == 0) {
		delete_connect_all();
		return;
	}

	if (connect(p->fd, (struct sockaddr *)&xconnect.bncaddr, sizeof(xconnect.bncaddr)) == -1) {
		if (errno == EINPROGRESS) { // wszystko ok
			p->connected = 1;
#ifdef _USE_POLL
			SET_WRITE_EVENT(p->pfd);
#endif
		} else {
			err_printf("do_connect_bnc()->connect(): %s\n", strerror(errno));
			kill_clone(p, 1);
			delete_connect_all();
		}
	} else { // lol, od razu sie polaczyl, that's impossible !
#ifdef _XMEN_DEBUG
		fprintf(fdebug, "+ Clone %s connect()'ed, sending NICK + USER + BNC (xcnt=%d, xall=%d, xcon=%d)\n",
				p->nick, xcnt, xall, xconnect.connecting);
#endif
		xdebug(5, "Clone %s connect()'ed, sending NICK + PASS + USER\n", p->nick);
		p->connected = 2;
#ifdef _USE_POLL
		SET_READ_EVENT(p->pfd); // write'a nie trzeba usuwac
#endif
		xsend(p->fd, "NICK %s\n", p->nick);
		xsend(p->fd, "PASS %s\n", xconnect.bncpass);
		xsend(p->fd, "USER %s - - :%s\n",
				random_ident(), random_realname());
		if (xconnect.vhost) {
			xsend(p->fd, "VIP %s\n", next_bnc());
		}
		xsend(p->fd, "CONN %s %d\n", xconnect.ircserver, xconnect.ircport);
	}
}

vhost *add_vhost(char *name, struct sockaddr_in *addr)
// name i addr jest malloc()'owane
{
	vhost *p;	
	
	if (xconnect.vhost == 0)
		p =  xconnect.vhost = (vhost *)malloc(sizeof(vhost));
	else
		p = xconnect.vhosttail->next = (vhost *)malloc(sizeof(vhost));
	
	if (p == 0) {
		err_printf("add_vhost()->malloc(): %s\n", strerror(errno));
		return 0;
	}

	memset(p, 0, sizeof(vhost));
	
	p->parent = xconnect.vhosttail;
	xconnect.vhosttail = p;
	
	p->name = name;
	p->addr = addr;
	
	return p;
}

vhost *find_vhost_by_name(char *name)
{
	vhost *v;
	for (v = xconnect.vhost; v; v = v->next)
		if (!x_strcasecmp(v->name, name))
			return v;
	return 0;
}

vhost *find_vhost_by_fd(int fd)
{
	vhost *v;
	struct sockaddr addr;
	struct sockaddr_in *sin;
	int s = sizeof(struct sockaddr);

	if (getsockname(fd, &addr, &s) == -1) {
		err_printf("getsockname(): %s\n", strerror(errno));
		return 0;
	}
	
	sin = (struct sockaddr_in *)&addr;

	for (v = xconnect.vhost; v; v = v->next)
		if (v->addr->sin_addr.s_addr == sin->sin_addr.s_addr)
			return v;

	return 0;
}

char *next_bnc(void)
{
	if (lastvhost == 0 || lastvhost->next == 0)
		lastvhost = xconnect.vhost;
	else
		lastvhost = lastvhost->next;
	
	return lastvhost->name;
}

struct sockaddr *next_vhost(void)
{
	if (lastvhost == 0 || lastvhost->next == 0)
		lastvhost = xconnect.vhost;
	else
		lastvhost = lastvhost->next;
	
	return (struct sockaddr *)lastvhost->addr;
}

void del_vhost_all(void)
{
	vhost *v, *t;
	for (v = xconnect.vhost; v; v = t) {
		t = v->next;
		del_vhost(v, 0);
	}
}

void del_vhost(vhost *v, int print)
{
	if (v == 0)		// niepotrzebne zabezpieczenie
		return;
	else if (v == lastvhost)
		lastvhost = v->next;

	if (v == xconnect.vhosttail)
		xconnect.vhosttail = v->parent;
	else if (v->next)
		v->next->parent = v->parent;
	
	if (v == xconnect.vhost)
		xconnect.vhost = v->next;
	else
		v->parent->next = v->next;

	if (print)
		cdel_printf("Deleted vhost %s from list.\n", v->name);
	
	free(v->name);
	free(v->addr);
	free(v);

	if (xconnect.vhost == 0) {
		if (print)
			err_printf("del_vhost(): No vhosts left.\n");
		if (!xconnect.bncserver)
			delete_connect_all();
	}
}

#ifdef __FreeBSD__

/* ten kod zosta³ prawie ca³y wyciêty z fbsd'owego ifconfig.c
 *
 * Copyright (c) 1983, 1993
 * The Regents of the University of California.  All rights reserved.
 */ 

#define ROUNDUP(a) \
        ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)))
int get_vhosts(void)
{
	struct if_msghdr *ifm, *nextifm;
	struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
	struct rt_addrinfo info;
	struct sockaddr *sa;
	char   *buf, *lim, *next, *cp, *cplim, name[32];
	size_t needed;
	int    mib[6], flags, addrcount, total = 0, i;
	
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;     /* address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		err_printf("get_vhosts()->sysctl(): %s\n", strerror(errno));
		return -1;		
	}
	if ((buf = malloc(needed)) == 0) {
		err_printf("get_vhosts()->malloc(): %s\n", strerror(errno));
		return -1;
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		err_printf("get_vhosts()->sysctl(): %s\n", strerror(errno));
		free(buf); return -1;
	}
		
	del_vhost_all();
	
	lim = buf + needed;
	next = buf;
	
	while (next < lim) {
		ifm = (struct if_msghdr *)next;

		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
		} else {
			err_printf("get_vhosts(): ifm->ifm_type != RTM_ITINFO\n");
			free(buf); return -1;
		}
	
		next += ifm->ifm_msglen;
		ifam = 0;
		addrcount = 0;

		while (next < lim) {
			nextifm = (struct if_msghdr *)next;

			if (nextifm->ifm_type != RTM_NEWADDR)
				break;

			if (ifam == 0)
				ifam = (struct ifa_msghdr *)nextifm;
			
			addrcount++;
			next += nextifm->ifm_msglen;
		}

		strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
		name[sdl->sdl_nlen] = 0;
		
		if ((flags & IFF_UP) == 0 || !x_strncasecmp(name, "lo", 2))
			continue;

		while (addrcount > 0) {
			cp = (char *)(ifam + 1);
			cplim = ifam->ifam_msglen + (char *)ifam;
			info.rti_addrs = ifam->ifam_addrs;
			memset(info.rti_info, 0, sizeof(info.rti_info));
			for (i = 0; i < RTAX_MAX && (cp < cplim); i++) {
				if ((info.rti_addrs & (1 << i)) == 0)
					continue;
				info.rti_info[i] = sa = (struct sockaddr *)cp;
				ADVANCE(cp, info.rti_info[i]->sa_len);	
			}

			if (get_vhost_add(((struct sockaddr_in *)info.rti_info[RTAX_IFA])->sin_addr))
				total++;
			
			addrcount--;
			ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
		}
		
	}
	free(buf);
	
	if (!total) {
		struct sockaddr_in *sin;
		sin = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
		if (sin == 0) {
			err_printf("get_vhosts()->malloc(): %s\n", strerror(errno));
			return -1;
		}
		memset((char *)sin, 0, sizeof(struct sockaddr_in));
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		buf = strdup("INADDR_ANY");
		if (buf == 0) {
			err_printf("get_vhosts()->strdup(): %s\n", strerror(errno));
			return -1;
		}
		if (!add_vhost(buf, sin)) {
			free(buf); free(sin);
			return -1;
		}
	}
	
	return total;
}
#else
int get_vhosts(void)
{
	struct ifconf ifc;
	struct ifreq *ifp, *end;
	int s, numreqs = 30, total = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		err_printf("get_vhosts()->socket(): %s\n", strerror(errno));
		return -1;
	}

	ifc.ifc_buf = 0;
	while (1) {
		ifc.ifc_len = sizeof(struct ifreq) * numreqs;
		ifc.ifc_buf = (char *)realloc(ifc.ifc_buf, ifc.ifc_len);
		if (ifc.ifc_buf == 0) {
			err_printf("get_vhosts()->realloc(): %s\n", strerror(errno));
			return -1;
		}
		if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
			err_printf("get_vhosts()->ioctl(): %s\n", strerror(errno));
			ifc.ifc_len = 0;
		}
		if (ifc.ifc_len == sizeof(struct ifreq) * numreqs) {
			numreqs += 10;
			continue;
		}
		break;
	}
	
	del_vhost_all();

	ifp = ifc.ifc_req;
	end = (struct ifreq *)(ifc.ifc_buf + ifc.ifc_len);

	while (ifp < end) {
		if (ioctl(s, SIOCGIFFLAGS, ifp) > -1 && \
				x_strncmp(ifp->ifr_name, "lo", 2) && (ifp->ifr_flags & IFF_UP) && \
					get_vhost_add(((struct sockaddr_in *)&ifp->ifr_addr)->sin_addr))
						total++;
		ifp++;
	}
	close(s);
	
	if (!total) {
		struct sockaddr_in *sin;
		char *buf;
		sin = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
		if (sin == 0) {
			err_printf("get_vhosts()->malloc(): %s\n", strerror(errno));
			return -1;
		}
		memset((char *)sin, 0, sizeof(struct sockaddr_in));
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		buf = strdup("INADDR_ANY");
		if (buf == 0) {
			err_printf("get_vhosts()->strdup(): %s\n", strerror(errno));
			return -1;
		}
		if (!add_vhost(buf, sin)) {
			free(buf); free(sin);
			return -1;
		}
	}
	
	return total;
}
#endif
int get_vhost_add(struct in_addr ia) {
	struct hostent *host;
	struct sockaddr_in *addr;
	int s;
	char *hname;

	// resolwujemy
	host = gethostbyaddr((char *)&ia, sizeof(struct in_addr), AF_INET);
	if (host == 0) {
		//hname = inet_ntoa(ifpaddr->sin_addr);
		return 0; // nie resolwuje sie, pewnie z +r wejdzie
	}
	hname = host->h_name;
	if (find_vhost_by_name(hname))
		return 0; // juz dodany

	hname = strdup(hname);
	if (hname == 0) {
		err_printf("get_vhost_add()->strdup(): %s\n", strerror(errno));
		return 0;
	}
	
	addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (addr == 0) {
		err_printf("get_vhosts()->malloc(): %s\n", strerror(errno));
		free(hname);
		return 0;
	}

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) {
		err_printf("get_vhost_add()->socket(): %s\n", strerror(errno));
		free(hname); free(addr);
		return 0;
	}

	memset((char *)addr, 0, sizeof(struct sockaddr_in));
	memcpy(&addr->sin_addr, &ia, sizeof(ia));
	addr->sin_family = AF_INET;

	if (!bind(s, (struct sockaddr *)addr, sizeof(struct sockaddr))) {
		close(s);
		if (add_vhost(hname, addr))
			return 1;
		else {
			free(hname); free(addr);
		}
	} else {
		close(s);
		free(hname); free(addr);
	}
	
	return 0;
}

// friendy
xfriend *add_friend(xchan *chan, char *mask)
{
	xfriend *f;
	int i;
	xnick *n;

	f = (xfriend *)malloc(sizeof(xfriend));

	if (f == 0) {
		err_printf("add_friend()->malloc(): %s\n", strerror(errno));
		return 0;
	}
	
	memset(f, 0, sizeof(xfriend));
	
	if ((f->mask = strdup(mask)) == 0) {
		err_printf("add_friend()->strdup(): %s\n", strerror(errno));
		free(f);
		return 0;
	}
	
	if (chan) {
		if (chan->friend)
			chan->friend->parent = f;
		f->next = chan->friend;
		chan->friend = f;
		for (i = 0; i < XHASH_SIZE; i++)
			for (n = chan->nick[i]; n; n = n->next)
				if (mask_match(f->mask, n->nick, n->address)) {
					if (n->topun)
						del_topun(chan, n);
					if (!(n->op) && !(n->toop) && !(n->sentact_op))
						add_toop_nick(chan, n);
					n->friend = f;
				}
		cadd_printf("Added '%s' to %s's friendlist.\n", f->mask, chan->name);
	} else {
		xchan *ch;
		if (friendroot)
			friendroot->parent = f;
		f->next = friendroot;
		friendroot = f;
		for (ch = chanroot; ch; ch = ch->next) {
			if (ch->inactive) continue;
			for (i = 0; i < XHASH_SIZE; i++)
				for (n = ch->nick[i]; n; n = n->next)
					if (mask_match(f->mask, n->nick, n->address)) {
						if (n->topun)
							del_topun(ch, n);
						if (!(n->op) && !(n->toop) && !(n->sentact_op))
							add_toop_nick(ch, n);
						n->friend = f;
					}
		}
		cadd_printf("Added '%s' to global friendlist.\n", f->mask);
	}

	return f;
}

xfriend *is_friend(xchan *ch, xnick *n)
{
	xfriend *p;
	
//	if (n->friend) // wtf ? :>
//		return n->friend;
	
	for (p = ch->friend; p; p = p->next)
		if (mask_match(p->mask, n->nick, n->address)) {
#ifdef _XMEN_DEBUG
			fprintf(fdebug, "* is_friend(): %s (%s) is matching %s on %s\n",
					n->nick, n->address, p->mask, ch->name);
#endif
			if (n->topun)
				del_topun(ch, n);
			if (!(n->op) && !(n->toop) && !(n->sentact_op))
				add_toop_nick(ch, n);
			return (n->friend = p);
		}

	for (p = friendroot; p; p = p->next) // globale
		if (mask_match(p->mask, n->nick, n->address)) {
#ifdef _XMEN_DEBUG
			fprintf(fdebug, "* is_friend(): %s (%s) is matching %s on %s\n",
					n->nick, n->address, p->mask, ch->name);
#endif
			if (n->topun)
				del_topun(ch, n);
			if (!(n->op) && !(n->toop) && !(n->sentact_op))
				add_toop_nick(ch, n);
			return (n->friend = p);
		}
			
	return (n->friend = 0); // profilaktycznie :>
}

xfriend *find_friend(xchan *ch, char *mask)
{
	xfriend *f;
	for (f = (ch? ch->friend : friendroot); f; f = f->next)
		if (!x_strcasecmp(f->mask, mask))
			return f;
	return 0;
}

void del_friend_nick(xchan *ch, xnick *n)
{
	n->friend = 0;	
	if (n->toop) // nie mial opa, byl w kolejce toop
		del_toop_nick(ch, n);
	if ((n->op || ch->pun_mode == 2) && !(n->topun) && !(n->sentact_pun))
		add_topun(ch, n);
}

void del_friend(xchan *chan, xfriend *f) {
	xnick *n;
	int i;
	
	if (f->next)
		f->next->parent = f->parent;
	
	if (chan) {
		if (chan->friend == f)
			chan->friend = f->next;
		else
			f->parent->next = f->next;
		cdel_printf("Removed '%s' from %s's friendlist.\n", f->mask, chan->name);
		if (!(chan->inactive)) {
			for (i = 0; i < XHASH_SIZE; i++)
				for (n = chan->nick[i]; n; n = n->next)
					if (n->friend == f) {
						del_friend_nick(chan, n);
						is_friend(chan, n); // moze matchuje do innej maski?
					}
		} else if (chan->friend == 0) { // czyscimy kanal
			if (chan->next)	
				chan->next->parent = chan->parent;
			if (chanroot == chan)
				chanroot = chan->next;
			else
				chan->parent->next = chan->next;
			free(chan->name); free(chan);
		}
	} else { // global
		xchan *ch;
		
		if (friendroot == f)
			friendroot = f->next;
		else
			f->parent->next = f->next;
		
		for (ch = chanroot; ch; ch = ch->next) {
			if (ch->inactive) continue;
			for (i = 0; i < XHASH_SIZE; i++)
				for (n = ch->nick[i]; n; n = n->next)
					if (n->friend == f) {
						del_friend_nick(ch, n);
						is_friend(ch, n);
					}
		}
		cdel_printf("Removed '%s' from global friendlist.\n", f->mask);
	}
	
	free(f->mask); free(f);
}

int mask_match(char *mask, char *nick, char *address)
{
	char *p, *s;
	
	snprintf(buf, XBSIZE, "%s!%s", nick, address);
	
	s = buf;
	p = mask;

	while (*p) {
		if (!*s) // koniec naszego n!u@h, jesli w mask zostalo cos innego niz '*' to return 0
			while (*p) if (*p++ != '*') return 0;
		if (*p == '*') { // gwiazdeczka..
			while (*p == '*') // przeskakujemy wszystkie gwiazdeczki
				p++;
			if (*p) { // sprawdzamy czy jest cos po gwiazdeczce
				while (*s) // probujemy to znalesc
					if (x_tolower(*s++) == x_tolower(*p)) break;
				if (!*s) // uuuu, nie znalezlismy
					return 0;
				else
					p++;
			} // nic nie ma po gwiazdeczce, czyli obojetnie co dalej to i tak matchuje
		} else if (*p != '?' && x_tolower(*p) != x_tolower(*s))
			return 0;
		else {
			p++; s++;
		}
	}
	
	return 1;
}

// nicki
char *random_nick(int mode)
{
	int l = xrand(7.0) + 2.0;
	char *r = nick_buf;

	switch (mode)
	{
		case 0:	// ident
			l++;
			while (l--)
				*r++ = xident[xrand(lxident)];
			break;
		case 1:	// normal nick
			*r++ = xalphaf[xrand(lxalpf)];
			while (l--)
				*r++ = xalpha[xrand(lxalp)];
			break;
		case 2:	// crap nick
			*r++ = xcrapf[xrand(lxcrapf)];
			while (l--)
				*r++ = xcrap[xrand(lxcrap)];
			
			break;
	}
	*r = 0;
	
	return nick_buf;
}

char *make_nick(char *w, int how, int cnt)
{
	int i;
	
	for (i = 0; *w && i < MAX_NICKLEN; w++)
		if (*w == '#')
			i += snprintf(nick_buf+i, MAX_NICKLEN+1-i, "%.*d", how, cnt);
		else if (*w == '?') {
			if (i == 0)
				nick_buf[i++] = xalphaf[xrand(lxalpf)];
			else
				nick_buf[i++] = xalpha[xrand(lxalp)];
		} else
			nick_buf[i++] = *w;
	
	nick_buf[i] = 0;
	
	return nick_buf;
}
