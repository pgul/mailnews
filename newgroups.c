/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1992-1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "conf.h"
#include "dbsubs.h"
#include "nnrp.h"

#ifndef lint
static char sccsid[] = "@(#)newgroups:.c 4.0 04/02/94";
#endif

#ifdef NNTP_ONLY
int sofgets(char *,int, FILE *);
#endif

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *subsplit(), *adate(), *malloc();
extern void free();
extern int wait();
#if defined(_POSIX_SOURCE) || defined(ISC)
extern int waitpid();
#endif

#ifdef MEMMAP
struct base_header	header;
struct string_chain	st_entry;
struct user_rec		ui_entry;
struct domain_rec	dm_entry;
void	*Fb,*base;
char	group[160],wbuff[160];
long	boffset,soffset,uoffset,doffset,goffset,coffset,poffset,mmsize;
int	bserrno;
char 	*bufb,user[160];

#include "libs/flock.c"

int
open_base(param)
int param;
{
char	filename[200];
FILE	*fb;

(void)sprintf(filename,"%s/%s",BASEDIR,SUBSBASE);
if((fb=fopen(filename,"r"))==NULL)
	return 1;
if(lock(fb,F_RDLCK)<0)
	return 1;
if(fseek(fb,0L,SEEK_END))
	return 1;
mmsize=ftell(fb);
if(fseek(fb,0L,SEEK_SET))
	return 1;
if((base=malloc(mmsize))==NULL)	
	return 1;
if(fread(base,mmsize,1,fb)!=1)
	return 1;
fclose(fb);
memcpy((void *)&header,base,sizeof(struct base_header));
Fb=base;
return 0;
}

#define fseek(a,b,c)	Fseek(a,b,c)
#define fread(a,b,c,d)	Fread(a,b,c,d)
#define fb	&Fb
inline int Fseek(void **a,long o,int c)	{*a=(c==SEEK_SET?base+o:c==SEEK_END?(base+mmsize)-o:*a+o);return 0;}
inline int Fread(void *w,int h,int c,void **a)	{(void)memcpy(w,*a,h*c);return c;}
inline int close_base(void) {free(base);return 0;}
inline int retry_base(void) {return 0;}
#include "libs/strins.c"
#include "libs/get_uaddr.c"
#include "libs/get_utree.c"
#include "libs/isdis.c"
#endif

#ifndef _NFILE
#ifdef BSD
#define _NFILE FOPEN_MAX
#else
#define _NFILE 20
#endif
#endif
/* #define DEBUG */

char pname[80];
static char sender[80];
static char host[80];
static char lhead[2048];
static char *av[10];
static FILE *ngf;

static int
_sng(entry,param,name)
long entry;
int *param;
char *name;
{
FILE *outf;
int c, pfd[2], status;
int pid;

bserrno=BS_OK;
if(*param)
    return 0;
if(fseek(fb,entry,SEEK_SET)){
serr:
	bserrno=BS_SYSERR;
	return 1;
}
if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
	goto serr;
if(ui_entry.magic==DE_MAGIC) /* gul */
	return 0; /* There are no users at domain `name` */
if((ui_entry.flag&UF_NONGRP) || (ui_entry.flag&UF_SUSP))
	return 0;

#ifdef MEMMAP
#undef fseek
#undef fread
#undef fb
#endif

    if (pipe(pfd) == -1) {
	fprintf(stderr, "newgroups: cannot make a pipe\n");
	goto serr;
    }

  tryagain:
    if ((pid = fork()) == 0) {
	/* new process */
	close(0);
	dup(pfd[0]);	/* should be 0 :-) */
	for (c = 2; c <= _NFILE; c++)
	    close(c);
	c = 0;
	av[c++] = "send-mail";
	av[c++] = SENDER_FLAG;
	av[c++] = sender;
#ifdef MFLAG_QUEUE
	av[c++] = MFLAG_QUEUE;
#endif
#ifdef MFLAG
	av[c++] = MFLAG;
#endif
	av[c++] = name;
	av[c++] = (char *) NULL;
#ifdef DEBUG
	execv("/bin/echo", av);
	exit(1);
#endif
	execv(MAILER, av);
	exit(1);
    }
    if (pid == -1) {
	sleep(30);
	goto tryagain;
    }
    close(pfd[0]);
    outf = fdopen(pfd[1], "w");
    if (outf == NULL) {
	fprintf(stderr, "newgroups: fdopen failed\n");
	goto serr;
    }
    fprintf(outf, lhead, adate(), name);
    fprintf(outf, "%s\n", mesg(37, ui_entry.lang));
    fseek(ngf, 0L, SEEK_SET);
    while ((c = getc(ngf)) != EOF)
	putc(c, outf);
    fclose(outf);
#if defined(_POSIX_SOURCE) || defined(ISC)
    waitpid(pid, &status, 0);
#else
    wait(&status);
#endif
    if (status != 0){
	fprintf(stderr, "newgroups: mailer returned status %d\n", status);
	bserrno=BS_SYSERR;
    }
return status;
}

void 
send_newgr(file)
char *file;
{
#ifndef NNTP_ONLY
    FILE *ng;
#endif
    FILE *outf;
    char line[200], iline[200];
    register char *p, *q;
    int len;
    char *descr;

    if ((ngf = fopen(file, "r")) == NULL)
	return;
    unlink(file);
#ifdef NNTP_ONLY
    if (connect_nnrp()!=0) {
#else
    if ((ng = fopen(NEWSGROUPS, "r")) == NULL) {
#endif
	fclose(ngf);
	return;
    }
    sprintf(iline, "%s/%s", WORKDIR, TMPNG);
    if ((outf = fopen(iline, "w")) == NULL) {
	fprintf(stderr, "newgroups: Can't create temporary file\n");
#ifndef NNTP_ONLY
	fclose(ng);
#endif
	fclose(ngf);
	return;
    }
    while ((q = fgets(iline, sizeof iline, ngf)) != NULL) {
	if ((p = index(iline, '\n')) != NULL)
	    *p = '\0';
	len = strlen(iline);
#ifdef NNTP_ONLY
	fprintf(fsocket, "XGTITLE %s\r\n", iline);
	fflush(fsocket);
#ifdef DEBUG
	printf(">> XGTITLE %s\n", iline);
#endif
	if (!sofgets(snntp, sizeof(snntp), fsocket) || snntp[0]!='2') {
	    fclose(ngf);
	    return;
	}
#ifdef DEBUG
	printf("<< %s", snntp);
#endif
	if (!sofgets(line, sizeof(line), fsocket)) {
	    fclose(ngf);
	    return;
	}
#ifdef DEBUG
	printf("<< %s", line);
#endif
	if ((p=index(line, '\r')) != NULL)
	    if (strcmp(p, "\r\n") == 0)
		strcpy(p, "\n");
	if (strcmp(line, ".\n"))
	{
		if (!sofgets(snntp, sizeof(snntp), fsocket)) { /* ".\r\n" */
		    fclose(ngf);
		    return;
		}
#ifdef DEBUG
		printf("<< %s", snntp);
#endif
#else
	fseek(ng, 0L, SEEK_SET);
	while ((q = fgets(line, sizeof line, ng)) != NULL) {
	    if ((p = index(line, '\n')) != NULL)
		*p = '\0';
#endif
	    p = line;
	    while (ISspace(*p))	p++;
	    {
		register j;
		for (j = 0; !ISspace(*(p + j)); j++);
		if (strncmp(p, iline, j > len ? j : len) == 0) {
		    for (descr = p + j; ISspace(*descr); descr++);
		    fprintf(outf, "%s\t%s%s\n", iline,
			    len > 31 ? "" : len > 23 ? "\t" : len > 15 ? "\t\t" : len > 7 ? "\t\t\t" : "\t\t\t\t",
			    subsplit(&descr, 38));
		    while (descr != NULL)
			fprintf(outf, ">\t\t\t\t\t%s\n", subsplit(&descr, 38));
		    goto next;
		}
	    }
	}
	fprintf(outf, "%s\n", iline);
      next:
	continue;
    }
    fclose(ngf);
#ifdef NNTP_ONLY
    if (fsocket) {
	fprintf(fsocket, "QUIT\r\n");
	fflush(fsocket);
#ifdef DEBUG
	printf(">> QUIT\n");
#endif
	if (sofgets(snntp, sizeof(snntp), fsocket)) {
#ifdef DEBUG
		printf("<< %s", snntp);
#endif
	}
	fclose(fsocket);
	fsocket=NULL;
	sockfd=-1;
    }
#else
    fclose(ng);
#endif
    fclose(outf);

    snprintf(sender, sizeof(sender), "%s@%s", NEWSSENDER, host);

    /*
     * make the header of the message
     */
    snprintf(iline, sizeof(iline), "Sender: %s\n", sender);
    strncpy(lhead,iline,sizeof(lhead));
#ifdef USE_XCLASS
    sprintf(iline, "X-Class: slow\n");
    strncat(lhead,iline,sizeof(lhead));
#endif
#ifdef USE_PRECEDENCE
    snprintf(iline, sizeof(iline), "Precedence: %s\n", PRECEDENCE);
    strncat(lhead,iline,sizeof(lhead));
#endif
    snprintf(iline, sizeof(iline), "From: %s@%s (News Mailing Service)\n", NEWSUSER, host);
    strncat(lhead,iline,sizeof(lhead));
    /* gul */
    snprintf(iline, sizeof(iline), "Reply-To: %s@%s\n", NEWSUSER, host);
    strncat(lhead,iline,sizeof(lhead));
    snprintf(iline,sizeof(iline),"Errors-To: %s\n", sender);
    strncat(lhead,iline,sizeof(lhead));
    sprintf(iline, "Date: %%s\n");
    strncat(lhead,iline,sizeof(lhead));
    sprintf(iline, "To: %%s\n");
    strncat(lhead,iline,sizeof(lhead));
    sprintf(iline, "Subject: List of new USENET newsgroups\n\n");
    strncat(lhead,iline,sizeof(lhead));

    snprintf(iline, sizeof(iline), "%s/%s", WORKDIR, TMPNG);
    if ((ngf = fopen(iline, "r")) == NULL) {
	fprintf(stderr, "newgroups: Can't read temporary file\n");
	return;
    }
    unlink(iline);

    if(get_utree(header.user,isdis,_sng,0,0,(char *)NULL))
	goto err;
    return;
  err:
    fprintf(stderr, "newgroups: error on send_newgr\n");
    exit(1);
}

int main(argc, argv)
int argc;
char **argv;
{
    char filename[200];

    signal(SIGCHLD, SIG_DFL);	/* shamanstvo */
    strncpy(pname,argv[0],sizeof(pname));
    gethost(host, sizeof(host));
    if(open_base(OPEN_RDONLY)){
	fprintf(stderr,"newgroups: Can't open data base\n");
	exit(1);
    }
    snprintf(filename, sizeof(filename), "%s/%s", WORKDIR, NEWGROUP);
    if (access(filename, R_OK) == 0)
	send_newgr(filename);
return 0;
}
