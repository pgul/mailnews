/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1992-1995 by Stanislav Voronyi, Kharkov, Ukraine
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
#ifdef SEND_BY_SMTP
#include "mnewssmtp.h"
#endif

#define TEMPNAME	"lister.tmp"
#define BADNAME		"lister.bad"

#ifndef lint
static char sccsid[] = "@(#)lister.c 4.0 04/02/94";
#endif

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *cut25(), *subsplit(), *adate(), *malloc();
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

void unmime_hdr(char * header);

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
#define fwrite(a,b,c,d)	Fwrite(a,b,c,d)
#define ftell(a)	Ftell(a)
#define fb	&Fb
inline int Fseek(void **a,long o,int c)	{*a=(c==SEEK_SET?base+o:c==SEEK_END?(base+mmsize)-o:*a+o);return 0;}
inline int Fread(void *w,int h,int c,void **a)	{(void)memcpy(w,*a,h*c);return c;}
inline long Ftell(void **a) {return (*a-base);}
inline int Fwrite(void *w,int h,int c,void **a) {return c;}
inline int close_base(void) {free(base);return 0;}
inline int retry_base(void) {return 0;}
#include "libs/lower_str.c"
#include "libs/get_uparam.c"
#include "getuser.c"
#include "libs/ext.c"
#undef fseek
#undef fread
#undef ftell
#undef fwrite
#undef fb
#endif

#ifndef _NFILE
#ifdef BSD
#define _NFILE FOPEN_MAX
#else
#define _NFILE 20
#endif
#endif
/* #define DEBUG */

char ArtList[1024];
#ifdef NNTP_ONLY
char TmpName[1024]="";
#endif

#define SKIP_FIELD(p)  while(*p && *p!=' ' && *p!='\t' ) p++
#define SKIP_SPACES(p) while(ISspace(*p)) p++


char pname[80];
int find = 0, imm = 0;
static char sender[80];
static char host[80];
static char lhead[2048];
#ifndef SEND_BY_SMTP
static char *av[10];
#endif

int main(argc, argv)
int argc;
char **argv;
{
    FILE *alist=NULL, *helpf, *badlist=NULL;
    char sortcmd[200];
    char iline[5120], iline2[5120];
    char filename[200], badname[200];
    char addressee[160], group[160];
    char *m_adr, *m_grp, *m_number, *m_size, *m_from, *m_subject;
    register char *p;
    FILE *outf=NULL;
    int status=0, r;
    int size, limit=64*1024, lang=DEFLANG, cnt = 0;
    register c;
    char *sub;
#ifdef SEND_BY_SMTP
    int h=0, nsmtp=0;
    char * users[2];
    struct mailnewsreq req;
#else
    int pid;
    int pfd[2];
#endif

#ifdef OS2
    extern int _fmode_bin;
    _fmode_bin=1;
#endif

    signal(SIGCHLD, SIG_DFL);	/* shamanstvo */
    strncpy(pname,argv[0],sizeof(pname));
    gethost(host, sizeof(host));
    if(open_base(OPEN_RDONLY)){
	log("Can't open data base\n");
	exit(1);
    }

    if (argc > 1) {
	if (strcmp(argv[1], "-i") == 0) {
	    imm = 1;
	    sprintf(ArtList, "%s/%s", WORKDIR, INDEXLIST);
#ifdef DEBUG
	    printf("Start with -i\n");
#endif
	} else if (strcmp(argv[1], "-f") == 0) {
	    imm = find = 1;
	    sprintf(ArtList, "%s/%s", WORKDIR, FINDLIST);
#ifdef DEBUG
	    printf("Start with -f\n");
#endif
	}
    } else
	sprintf(ArtList, "%s/%s", WORKDIR, ARTLIST);
    size = 0;

	/*
	 * Lock run file
	 */
    lockrun(WORKDIR);

    /*
	 * Sort the list
	 */
    if (access(ArtList, R_OK) != 0)
	goto out;
#ifdef NNTP_ONLY
    sprintf(TmpName, "%s/%s.%u", WORKDIR, ARTLIST, getpid());
    if (rename(ArtList, TmpName))
	goto out;
    sleep(1); /* waiting for feeder reopen artlist */
    sprintf(sortcmd, "%s %s", SORT, TmpName);
#else
    sprintf(sortcmd, "%s %s", SORT, ArtList);
#endif

#ifdef DEBUG
    printf("Call: %s\n", sortcmd);
#endif
    alist = popen(sortcmd, "r");
    if (alist == NULL) {
#ifdef DEBUG
	printf("Error in calling of sort\n");
#endif
	status = 72;
	goto out;
    }

    snprintf(sender, sizeof(sender), "%s@%s", find ? FINDSENDER : NEWSSENDER, host);
    /*
     * make the header of the message
     */
    snprintf(iline, sizeof(iline), "Sender: %s\n", sender);
    strncpy(lhead, iline, sizeof(lhead));
#ifdef USE_XCLASS
    sprintf(iline, "X-Class: slow\n");
    strncat(lhead, iline, sizeof(lhead));
#endif
#ifdef USE_PRECEDENCE
    sprintf(iline, "Precedence: %s\n", PRECEDENCE);
    strncat(lhead, iline, sizeof(lhead));
#endif
    snprintf(iline, sizeof(iline), "From: News Mailing Service <%s@%s>\n", NEWSUSER, host);
    strncat(lhead, iline, sizeof(lhead));
    /* gul */
    snprintf(iline, sizeof(iline), "Reply-To: %s@%s\n", NEWSUSER, host);
    strncat(lhead, iline, sizeof(lhead));
    snprintf(iline, sizeof(iline), "Errors-To: %s\n", sender);
    strncat(lhead, iline, sizeof(lhead));
    sprintf(iline, "Date: %%s\n");
    strncat(lhead, iline, sizeof(lhead));
    sprintf(iline, "To: %%s\n");
    strncat(lhead, iline, sizeof(lhead));
    sprintf(iline, "Subject: List of new USENET articles\n\n");
    strncat(lhead, iline, sizeof(lhead));

    /*
     * Process the list
     */
    addressee[0] = '\0';
    group[0] = '\0';
    while (fgets(iline, sizeof iline, alist) != NULL) {
#ifdef DEBUG
	printf("iline: %s\n", iline);
#endif
		/*
		 * Parse the line
		 */
	strcpy(iline2, iline);
	p = iline2;
	m_adr = p;
	SKIP_FIELD(p);
	if (*p)
	    *p++ = '\0';
	SKIP_SPACES(p);
	m_grp = p;
	SKIP_FIELD(p);
	if (*p)
	    *p++ = '\0';
	SKIP_SPACES(p);
	m_number = p;
	SKIP_FIELD(p);
	if (*p)
	    *p++ = '\0';
	SKIP_SPACES(p);
	m_size = p;
	SKIP_FIELD(p);
	if (*p)
	    *p++ = '\0';
	SKIP_SPACES(p);
	m_from = p;
	SKIP_FIELD(p);
	if (*p)
	    *p++ = '\0';
	SKIP_SPACES(p);
	m_subject = p;
	if ((p = index(p, '\n')) != NULL)
	    *p = '\0';
	p = m_subject;
	while ((p = index(p, '\t')) != NULL)
	    *p = ' ';
	if (!*m_adr || !*m_grp || !*m_number)
	    continue;
	unmime_hdr(m_subject);

	/*
		 * If we have a new addressee, call a mailer
		 */
	if (strcmp(m_adr, addressee) != 0 || (size > limit && limit > 0)) {
	    if (addressee[0] != '\0') {
#ifdef DEBUG
		printf("-----END OF FILE------\n");
#else
#ifdef SEND_BY_SMTP
		h=dup(fileno(outf));
		fclose(outf);
		if (closesmtpdata(h)) {
		    log("smtp-server returned bad status\n");
		    closesmtpsession(h);
		    h = 0;
		    status=72;
		}
		else {
		    status=0;
		    if (nsmtp++>10) {
			closesmtpsession(h);
			h=nsmtp=0;
		    }
		}
#else
		fclose(outf);
		close(pfd[1]);
#if defined(_POSIX_SOURCE) || defined(ISC)
		waitpid(pid, &status, 0);
#else
		wait(&status);
#endif
#endif
		if (status != 0)
		    log("mailer returned status %d\n", status);
#endif
	    }
	    if (strcmp(m_adr, addressee)) {
		if (cnt > 0)
			log("sent %d articles to %s\n",
				    cnt, addressee);
		cnt = 0;
		strcpy(addressee, m_adr);

		if(get_uparam(addressee)){
			limit=64*1024;
			lang=DEFLANG;
		}
		else{
			limit=ui_entry.limit*1024;
			lang=ui_entry.lang;
		}
	    }
	    size = 0;
	    
	    group[0] = '\0';
#ifdef DEBUG
	    printf("\n-----MAIL TO %s-----\n", addressee);
	    outf = stdout;
#else
#ifdef SEND_BY_SMTP
	    req.Iam=sender;
	    req.mailhub=MAILHUB;
	    req.port=SMTPPORT;
#ifdef MAILHUB2
	    req.mailhub2=MAILHUB2;
	    req.port2=SMTPPORT2;
#endif
	    req.users=users;
	    users[0]=addressee;
	    users[1]=NULL;
	    if ((h=opensmtpsession(&req,h))<0) {
		log("cannot make smtp-connection\n");
		if (h==-1) {
		    h = 0;
		    status = 72;
		    goto out_err;
		}
		h = 0;
		addressee[0]='\0';
		if (badlist==NULL) {
			sprintf(badname, "%s/%s", WORKDIR, BADNAME);
			badlist = fopen(badname, "a");
		}
		if (badlist)
			fputs(iline, badlist);
		else
			log("Cannot open %s\n", badname);
		continue;
	    }
	    else status=0;
	    outf = fdopen(h, "w");
#else
	    if (pipe(pfd) == -1) {
		log("cannot make a pipe\n");
		status = 72;
		goto out_err;
	    }
	  tryagain:
	    if ((pid = fork()) == 0)
	    {
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
		if (!imm)
		    av[c++] = MFLAG_QUEUE;
#endif
#ifdef MFLAG
		av[c++] = MFLAG;
#endif
		av[c++] = addressee;
		av[c++] = (char *) NULL;
#ifdef DEBUG
		execv("/bin/echo", av);
		exit(1);
#endif
		execv(MAILER, av);
		exit(1);
	    }
	    if (pid == -1)
	    {
		sleep(30);
		goto tryagain;
	    }
	    close(pfd[0]);
	    outf = fdopen(pfd[1], "w");
#endif /* SEND_BY_SMTP */
	    if (outf == NULL) {
		log("fdopen failed\n");
		status = 1;
		goto out_err;
	    }
#endif
        /*
	 * Print the header of the message
	 */
	    size += fprintf(outf, lhead, adate(), addressee);

	    /*
	     * Copy the help file into the begginig of message
	     */
	    if (!(ui_entry.flag & UF_NLHLP))
		p = ext[lang];
		sprintf(filename, HELP_NOTIFY, p);
		if ((helpf = fopen(filename, "r")) != NULL) {
		    while ((c = getc(helpf)) != EOF) {
			if (c == '$') {
			    c = getc(helpf);
			    if (c == EOF)
				break;
			    if (c == '$')
				size += fprintf(outf, "%s@%s", NEWSUSER, host);
			    continue;
			}
			putc(c, outf);
			size++;
		    }
		    putc('\n', outf);
		    size++;
		    fclose(helpf);
		}
	    }
		/*
		 * If we have a new group, write the command GROUP
		 */
	if (strcmp(m_grp, group) != 0) {
		strcpy(group, m_grp);
		size += fprintf(outf, "\nGROUP %s\n", group);
	}
		/*
		 * Write an article id
		 */
	cnt++;
	sub = m_subject;
	if (ui_entry.flag&UF_FMT) {
	    size += fprintf(outf, "-ART %5s %4s %-60s\n",
			    m_number, m_size, m_from);
	    while (sub != NULL)
		size += fprintf(outf, ">\t\t\t\t %s\n", subsplit(&sub, 45));
	    size += fprintf(outf, ">\n");
	} else {
	    size += fprintf(outf, "-ART %5s %4s %-25s  %s\n",
		    m_number, m_size, cut25(m_from), subsplit(&sub, 35));
	    while (sub != NULL)
		size += fprintf(outf, ">\t\t\t\t\t   %s\n", subsplit(&sub, 35));
	}
    }
    if (addressee[0] != '\0') {
#ifdef DEBUG
	printf("-----END OF FILE------\n");
#else
	if (cnt > 0)
		log("sent %d articles to %s\n",
			cnt, addressee);
#ifdef SEND_BY_SMTP
	h=dup(fileno(outf));
	fclose(outf);
	if (closesmtpdata(h)) {
	    log("smtp-server returned bad status\n");
	    closesmtpsession(h);
	    h = 0;
	    status=72;
	}
	else status=0;
#else
	fclose(outf);
	close(pfd[1]);
#if defined(_POSIX_SOURCE) || defined(ISC)
	waitpid(pid, &status, 0);
#else
	wait(&status);
#endif
	if (status != 0)
	    log("mailer returned status %d\n", status);
#endif
#endif
    }
    if ((r=pclose(alist))==0) {
#ifndef DEBUG
#ifdef NNTP_ONLY
        if (unlink(TmpName))
		log("Can't unlink %s: %s!\n", TmpName, strerror(errno));
#else
        if (unlink(ArtList))
		log("Can't unlink %s: %s!\n", ArtList, strerror(errno));
#endif
#else
        printf("End of alist\n");
#endif
    }
    else
	log("sort retcode %d!\n", r);
    close_base();
    if (badlist) fclose(badlist);
	/*
	 * Unlock run file
	 */
  out:
#ifdef DEBUG
    printf("Exit status: %d\n", status);
#endif
#ifdef SEND_BY_SMTP
    if (h)
	closesmtpsession(h);
#endif
#ifdef NNTP_ONLY
    /* append TmpName to ArtList */
    if (TmpName[0] && access(TmpName, R_OK) == 0) {
	FILE *ft;
	char str[1024];
	alist=fopen(ArtList, "a");
	if (alist==NULL)
		lexit(status);
	ft=fopen(TmpName, "r");
	if (ft==NULL) {
		fclose(alist);
		lexit(status);
	}
	while (fgets(str, sizeof(str), ft)) {
		fputs(str, alist);
		fflush(alist);
	}
	fclose(alist);
	fclose(ft);
	unlink(TmpName);
    }
#endif
    lexit(status);
out_err:
	{
		FILE * tf;
		char tempname[256];
		snprintf(tempname, sizeof(tempname), "%s/%s", WORKDIR, TEMPNAME);
		tf = fopen(tempname, "w");
		if (tf==NULL) {
			log("Cannot open %s!\n", tempname);
			goto out;
		}
		do
			if (fputs(iline, tf)==EOF) {
				log("cannot write to tempfile\n");
				fclose(tf);
				unlink(tempname);
				goto out;
			}
		while (fgets(iline, sizeof(iline), alist));
		if (fclose(tf)) {
			log("cannot close tempfile\n");
			unlink(tempname);
			goto out;
		}
		pclose(alist);
#ifdef NNTP_ONLY
		if (unlink(TmpName)) {
#else
		if (unlink(ArtList)) {
#endif
			log("cannot unlink %s\n", ArtList);
			goto out;
		}
#ifdef NNTP_ONLY
		if (rename(tempname, TmpName))
		    log("cannot rename %s to %s\n", tempname, TmpName);
#else
		if (rename(tempname, ArtList))
		    log("cannot rename %s to %s\n", tempname, ArtList);
#endif
		goto out;
	}
}
