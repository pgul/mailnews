/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1993 by Stanislav Voronyi, Kharkov, Ukraine
 * Some modify for POSIX suport.
 *
 * This code is public-domain thus you can use it, modify it
 * or redistribute it as long as you keep this copyright note unchanged.
 * You aren't allowed to sell it. The author is not responsible for
 * the consequences of use of this software. Modifyed versions
 * should be explicitly marked as such. This code is not subject to
 * any license of AT&T or of the Regents of UC, Berkeley or of DEMOS, Moscow.
 */

/*
 * THIS PROCEDURE IS SYSTEM-DEPENDENT
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "conf.h"

extern int open(),read(),close(),pipe(),fork(),dup(),waitpid();
extern void execl();

/*
 * Get a current host name for replies
 */
void gethost(buf, len)
char buf[];
int len;
{
    int fd[2];
    int l, rlen;
#if defined(_POSIX_SOURCE) || defined(ISC)
    int pid;
#endif
    static char hn[80] = "";
    int istream;
    char *p;
#ifdef UUNAME
    FILE *inf;
#endif
    typedef void (*sighandler_t)(int);
    sighandler_t oldchld;

    if (hn[0] != '\0') {
	strncpy(buf, hn, len);
	return;
    }
    istream = open("/etc/myhostname", 0);
    if (istream >= 0) {
	(void) read(istream, hn, sizeof(hn)-1);
	close(istream);
	goto got;
    }
    oldchld=signal(SIGCHLD, SIG_DFL);
    pipe(fd);
    if (
#if defined(_POSIX_SOURCE) || defined(ISC)
	   (pid = fork())
#else
	   fork()
#endif
	   == 0) {
	/* New proc */
	close(fd[0]);
	close(0);
	close(1);
	close(2);
	open("/dev/null", 0);
	dup(fd[1]);
	dup(fd[1]);
	close(fd[1]);

	execl("/usr/bin/hostname", "hostname", 0);
	execl("/bin/hostname", "hostname", 0);
	execl("/etc/hostname", "hostname", 0);
	exit(1);
    }
    close(fd[1]);
    rlen = read(fd[0], hn, sizeof(hn)-1);
    close(fd[0]);
#if defined(_POSIX_SOURCE) || defined(ISC)
    waitpid(pid, &l, 0);
#else
    wait(&l);
#endif
    if (l != 0 || rlen <= 0) {	/* Error */
	/*
		 * Get a local UUCP hostname
		 */
#ifdef UUNAME
	if ((inf = popen(UUNAME, "r")) != NULL) {
	    fgets(hn, sizeof hn, inf);
	    pclose(inf);
	    if ((p = index(hn, '\n')) != NULL)
		*p = '\0';
	} else
#endif
	    strcpy(hn, "foobar");
	strcat(hn, ".UUCP");
    }
    signal(SIGCHLD, oldchld);
  got:
    hn[sizeof(hn)-1] = '\0';
    if ((p = index(hn, '\n')) != NULL)
	*p = '\0';
    /* Ugh! */
    strncpy(buf, hn, len);
}
