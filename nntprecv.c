#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "conf.h"
#include "dbsubs.h"

#ifdef NNTP_ONLY
extern int  relay(FILE *input);
extern int  sofgets(char *, int, FILE *);
extern void printstat(void);

static int wanthup=0;
static int n_servers=0;
struct in_addr servaddr;

static void chld (int signo)
{
    int old_errno = errno;
    int status;
    pid_t pid;

    pid = wait(&status);
    if (pid > 0)
	log("rc(%u)=%u\n", pid, status);
    --n_servers;
    signal(SIGCHLD, chld);
    errno = old_errno;
}

static void term(int signo)
{
    log("I'm killed, terminating...\n");
    close_base();
    //exit(1);
    signal(signo, SIG_DFL);
    kill(getpid(), signo); /* suicide ;) */
}

static void hup(int signo)
{
    signal(SIGHUP, hup);
    wanthup=1;
}

int nntpsess(int sockfd)
{
    struct sockaddr_in peer_name;
    int peer_name_len = sizeof(peer_name);
    struct hostent *hp;
    char host[512];
    char myhostname[80];
    FILE *input;
    int  r;
    char *p, *perr, *q;
    char str[515];
    int  stream=0;
    time_t reread_time;

    if (getpeername(sockfd, (struct sockaddr *) & peer_name, &peer_name_len) == -1)
    {
	log("can't getpeername: %s\n", strerror(errno));
	return 1;
    }
    hp=gethostbyaddr((char *) &peer_name.sin_addr, sizeof peer_name.sin_addr, AF_INET);
    strncpy (host, hp ? hp->h_name : inet_ntoa(peer_name.sin_addr), sizeof(host));
    host[sizeof(host)-1]='\0';
    if (hp)
	log ("incoming from %s (%s)\n", host, inet_ntoa(peer_name.sin_addr));
    else
	log ("incoming from %s\n", host);

    if (memcmp(&peer_name.sin_addr, &servaddr, sizeof(servaddr)))
    {
	log("access denied\n");
	p = "502 Access denied\r\n";
	write(sockfd, p, strlen(p));
	close(sockfd);
	return 0;
    }

    if (n_servers > MAX_SERVERS)
    {
	log("too many servers\n");
	p = "402 Too many servers, try later\r\n";
	write(sockfd, p, strlen(p));
	close(sockfd);
	return 0;
    }
    input = fdopen(sockfd, "r+");
    if (input == NULL)
    {
	log("can't fdopen socket: %s\n", strerror(errno));
	p = "508 Internal error\r\n";
	write(sockfd, p, strlen(p));
	close(sockfd);
	return 2;
    }
    signal(SIGCHLD, SIG_DFL);
    signal(SIGTERM, term);
    signal(SIGINT,  term);
    signal(SIGHUP,  hup);
    gethost(myhostname, sizeof(myhostname));
    if (open_base(OPEN_RDONLY)) {
	log("can't open base!\n");
	exit(1);
    }
    reread_time=time(NULL);
    atexit(printstat);
    fprintf(input, "200 %s " VERSION "\r\n", myhostname);
#ifdef DEBUG
    printf("nntpsess, fd=%d\n", sockfd);
    printf(">> 200 %s " VERSION "\n", myhostname);
#endif
    for (fflush(input); sofgets(str, sizeof(str), input); fflush(input))
    {
	if (wanthup || time(NULL)-reread_time>REOPEN_TIME) {
	    printstat();
	    close_base();
	    wanthup=0;
	    if (open_base(OPEN_RDONLY)) {
		log("can't open base!\n");
		exit(1);
	    }
	    reread_time=time(NULL);
	}
#ifdef DEBUG
	printf("<< %s", str);
#endif
	for (p=str; *p && !isspace(*p); p++);
	if (*p) *p++='\0';
	for (q=str; *q; q++)
	    *q=tolower(*q);
	if (strcmp(str, "quit")==0) {
	    fputs("205 Goodbye!\r\n", input);
#ifdef DEBUG
	    printf(">> 205 Goodbye!\n");
#endif
	    fclose(input);
	    input=NULL;
	    return 0;
	}
	if (strcmp(str, "ihave")==0) {
	    if (*p=='\0') {
		fputs("435 Bad Message-ID\r\n", input);
#ifdef DEBUG
		printf(">> 435 Bad Message-ID\n");
#endif
		continue;
	    }
	    fputs("335 Use 'takethis', I can't check message-id\r\n", input);
	    fflush(input);
#ifdef DEBUG
	    printf(">> 335 Use 'takethis', I can't check message-id\n");
#endif
dorelay:
	    r=relay(input);
	    if (r==0) { /* transfered ok */
		if (stream) {
			fprintf(input,"239 %s", p);
#ifdef DEBUG
			printf(">> 239 %s", p);
#endif
		} else {
		    fputs("235 Article accepted, thanks!\r\n", input);
#ifdef DEBUG
		    printf(">> 235 Article accepted, thanks!\n");
#endif
		}
	    }
	    else if (r>0) { /* try again later */
		switch (r) {
		    case 1:	perr="Not enough core"; break;
		    case 2:	perr="Can't open SMTP session"; break;
		    case 3:	perr="Can't close SMTP session"; break;
		    case 5:	perr="Bad sendmail retcode"; break;
		    default:	perr="Unknown error"; break;
		}
		log ("%s for %s", perr, p);
		if (stream) {
#if 0 /* no way to tell "try later" after TAKETHIS in stream mode :( */
			fprintf(input, "431 %s", p);
#ifdef DEBUG
			printf(">> 431 %s", p);
#endif
#else /* 400 - not accepting articles */
			fprintf(input, "400 %s\r\n", perr);
#ifdef DEBUG
			printf(">> 400 %s\n", perr);
#endif
#endif
			continue;
		}
		fprintf(input, "436 %s\r\n", perr);
#ifdef DEBUG
		printf(">> 436 %s\n", perr);
#endif
	    }
	    else { /* r<0, reject and not try again */
		switch (r) {
		    case -1:	perr="Line too long"; break;
		    case -2:	perr="Header too large"; break;
		    case -3:	perr="Incorrect message"; break;
		    case -4:	perr="Message too large"; break;
		    case -5:	perr="Connection closed"; break;
		    default:	perr="Unknown error"; break;
		}
		log ("%s for %s", perr, p);
		if (r==-5) break;
		if (stream) {
			fprintf(input, "439 %s", p);
#ifdef DEBUG
			printf(">> 439 %s", p);
#endif
			continue;
		}
		fprintf(input, "437 %s\r\n", perr);
#ifdef DEBUG
		printf(">> 437 %s\n", perr);
#endif
	    }
	    continue;
	}
	if (strcmp(str, "check")==0) {
	    /* always ok */
	    fprintf(input, "238 %s", p);
#ifdef DEBUG
	    printf(">> 238 %s", p);
#endif
	    continue;
	}
	if (strcmp(str, "takethis")==0)
	    goto dorelay;
	for (q=p; *q; q++)
	    *q=tolower(*q);
	if (strcmp(str, "mode")==0 && strncmp(p, "stream", 6)==0) {
	    stream=1;
	    fputs("203 StreamOK.\r\n", input);
#ifdef DEBUG
	    printf(">> 203 StreamOK.\n");
#endif
	    continue;
	}
	log ("unknown command: %s", str);
	fputs("500 Syntax error or bad command\r\n", input);
#ifdef DEBUG
	printf(">> 500 Syntax error or bad command\n");
#endif
    }
#ifdef DEBUG
    printf("Can't fgets socket: %s\n", strerror(errno));
#endif
    fclose(input);
    input=NULL;
    return 1;
}

int nntprecv(void)
{
    int sockfd, new_sockfd;
    int client_addr_len;
    struct sockaddr_in serv_addr, client_addr;
    struct hostent *remote;
    char *servname=NNTP_SERVER;
    int  opt=1;
    int  r;
    struct passwd *pwd;

    if (!isdigit(servname[0]) || \
        (servaddr.s_addr=inet_addr(servname))==INADDR_NONE)
    {
	if ((remote=gethostbyname(servname)) == NULL)
	{   log("Unknown or illegal hostname %s\n", servname);
	    return 1;
	}
	if (remote->h_addr_list[0]==NULL)
	{   log("Can't gethostbyname for %s\n", servname);
	    return 1;
	}
	memcpy(&servaddr, remote->h_addr_list[0], sizeof(servaddr));
    }

    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
	log ("socket: %s\n", strerror(errno));
	return 1;
    }
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR,
		  (char *) &opt, sizeof opt) == -1)
    {
	log ("setsockopt (SO_REUSEADDR): %s\n", strerror(errno));
	close(sockfd);
	return 1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    serv_addr.sin_port = htons (IPORT);

    if (bind (sockfd, (struct sockaddr *) & serv_addr, sizeof (serv_addr)) != 0)
    {
	log ("bind: %s\n", strerror(errno));
	close(sockfd);
	return 1;
    }

    if ((pwd = getpwnam(USER)) == NULL)
	log("getpwnam(%s): %s\n", USER, strerror (errno));
    else if (setgid(pwd->pw_gid) == -1)
	log("setgid(%ld): %s\n", pwd->pw_gid, strerror (errno));
    else if (setuid(pwd->pw_uid) == -1)
	log("setuid(%ld): %s\n", pwd->pw_uid, strerror (errno));
    else
	goto setuidok;

    close(sockfd);
    return 1;

setuidok:

    listen (sockfd, 5);

#ifndef DEBUG
    signal(SIGCHLD, chld);
#endif

    for (;;)
    {
	client_addr_len = sizeof (client_addr);
	if ((new_sockfd = accept (sockfd, (struct sockaddr *) & client_addr,
			&client_addr_len)) == -1)
	{
	    if (errno != EINVAL && errno != EINTR)
		log ("accept: %s\n", strerror(errno));
	    continue;
	}

	++n_servers;
#ifdef DEBUG
	log("started server\n");
	r=nntpsess(new_sockfd);
	log("server retcode %d\n", r);
	n_servers--;
#else
forkagain:
	if ((r=fork())<0)
	{
	    if (errno==EINTR) goto forkagain;
	    log("can't fork: %s\n", strerror(errno));
	    exit(1);
	}
	if (r==0)
	{
	    close(sockfd);
	    r=nntpsess(new_sockfd);
	    exit(r);
	}
	log("started server [%d]\n", r);
	close(new_sockfd);
#endif
    }
}
#endif

