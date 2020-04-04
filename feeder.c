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
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "dbsubs.h"
#include "conf.h"

#ifndef lint
static char sccsid[] = "@(#) feeder.c 4.0  03/26/94";
#endif

#define ISspace(x) ( x==' ' || x=='\t' )

#ifdef NNTP_ONLY
int sofgets(char *, int, FILE *);
int sofgetc(FILE *);
#define lexit(retcode)	exit(retcode)
#else
#define sofgets(str,size,file)	fgets(str,size,file)
#define sofgetc(file)		getc(file)
#endif

extern char *malloc(), *calloc();
extern int atoi(), wait();
extern void free();
#if defined(_POSIX_SOURCE) || defined(ISC)
extern int waitpid();
#endif

struct grpnode {
    int count;
    char name[1];
};

struct addr {
    struct addr *next;
    struct grpnode *group;
    char name[1];
}
*cnewsers,
*feeds,
*efeeds,
*subscribers;

#ifdef LOGFILE
struct sl {
    struct sl *next;
    int count;
    char name[1];
}

*slc,
*wsl;

void printstat(void)
{
	if (slc==NULL) return;
	for (wsl = slc; wsl != NULL; wsl = wsl->next)
	    log("sent %d articles to %s\n",
		 wsl->count, wsl->name);
	while (slc->next) {
		for (wsl=slc; wsl->next->next != NULL; wsl = wsl->next);
		free(wsl->next);
		wsl->next=NULL;
	}
	free(slc);
	slc=NULL;
}
#else
void printstat(void) {}
#endif

static char inbuf[INBUFSIZE];
char *ibp;

char pname[80];
char sendername[100];

char originator[250];
char subject_line[5120];
int find=0,imm=0;

struct xreflist {
    struct xreflist *next;
    int number;
    char name[1];
}

*XrefList;

char ArtList[100];

#ifdef MEMMAP
struct base_header	header;
struct subs_chain	sb_entry;
struct string_chain	st_entry;
struct user_rec		ui_entry;
struct domain_rec	dm_entry;
struct group_rec	gr_entry;
void	*Fb,*base;
char	group[160],wbuff[160];
long	boffset,soffset,uoffset,doffset,goffset,coffset,poffset,mmsize;
int	bserrno;
char 	*bufb;
FILE	*art_list = NULL, *cart_list = NULL;

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
inline int Fseek(void **a,long o,int c) \
	{	*a=(c==SEEK_SET?base+o:c==SEEK_END?(base+mmsize)-o:*a+o);\
		return 0; }
inline int Fread(void *w,int h,int c,void **a) \
	{	(void)memcpy(w,*a,h*c); return c; }
#include "libs/getgroup.c"
#include "libs/get_uaddr.c"
inline int close_base(void) { free(base); return 0; }
#endif /* MEMMAP */

#ifndef _NFILE
#ifdef BSD
#define _NFILE FOPEN_MAX
#else
#define _NFILE 20
#endif
#endif

/*
 * Collect subscribers for groups
 */
int getsubscribers(p, size)
char *p;
int size;
{
    struct addr *ap;
    char *q, c;
    struct grpnode *grp;

    for (;;) {
	while (ISspace(*p) || *p == ',')
	    p++;
	if (*p == '\0' || *p == '\n')
	    goto end;
	q = p;
	while (*p && *p != '\n' && !ISspace(*p) && *p != ',')
	    p++;
	c = *p;
	*p = '\0';

	if(getgroup(q))
		if(bserrno==BS_ENOGROUP){
notany:
			*p = c;
			continue;}
		else 
			goto err;
	grp = (struct grpnode *) malloc(sizeof(struct grpnode) +
					strlen(q));
	if (grp == NULL)
	    goto err;
	strcpy(grp->name, q);
	grp->count = 0;
	*p = c;

	if (gr_entry.begin == NULL)
	    goto notany;

	boffset = gr_entry.begin;
	while (boffset) {
	    if (fseek(fb, boffset, SEEK_SET))
		goto err;
	    if (fread((char *)&sb_entry, sizeof(struct subs_chain), 1, fb) != 1)
		 goto err;
	    if (fseek(fb, sb_entry.user, SEEK_SET) != 0)
		goto err;
	    if (fread((char *)&ui_entry, sizeof(struct user_rec), 1, fb) != 1)
		 goto err;
	    if (ui_entry.flag & UF_SUSP)
		goto susp;
	    
	    if(get_uaddr(ui_entry.s_user)==NULL)
	    	goto susp;
	    if(bserrno==BS_DISDOMAIN)
		goto susp;
	    ap = (struct addr *) malloc(sizeof(struct addr) +
					ui_entry.size);
	    if (ap == NULL)
		goto err;
	    strcpy(ap->name, user);
	    ap->group = grp;
	    grp->count++;

	    switch (sb_entry.mode) {
	    case SUBS:
	      subs:
		ap->next = subscribers;
		subscribers = ap;
		break;
	    case FEED:
	      feed:
		ap->next = feeds;
		feeds = ap;
		break;
	    case RFEED:
		if ((sb_entry.m_param * 1024) < size)
		    goto subs;
		else
		    goto feed;
	    case EFEED:
		ap->next = efeeds;
		efeeds = ap;
		break;
	    case CNEWS:
		ap->next = cnewsers;
		cnewsers = ap;
		break;
	    }
#ifdef DEBUG
	    printf("ADD '%s %ld'\n", ap->name, sb_entry.mode);
#endif
	  susp:
	    boffset = sb_entry.group_next;
	}
	if (grp->count == 0)
	    free(grp);
    }

  end:
    return 0;

  err:
    return 1;
}

/*
 * Compare the beginning of line with the field name
 * ignoring the case. Returns 1 if matched.
 */
int fieldcmp(line, fname)
register char *line;
register char *fname;
{
    register char c;

    while (*fname) {
	c = *line++;
	if (c == '\0')
	    return 0;
	if ('A' <= c && c <= 'Z')
	    c |= 040;
	if (c != *fname++)
	    return 0;
    }
    return (*line == ':');
}

/*
 * Compare two network addresses
 */
int adrcmp(a1, a2)
char *a1, *a2;
{
    char *p1, *p2, mask;
    register char *q1, *q2;
    int n;

    p1 = a1;
    p2 = a2;
    if (index(p1, '@') != NULL) {
	if (index(p2, '@') != NULL) {
	    /*
			 * Both are Internet
			 * The trick is sorting reversed strings :-)
			 */
	    q1 = p1;
	    while (*q1)
		q1++;
	    q1--;
	    q2 = p2;
	    while (*q2)
		q2++;
	    q2--;

	    mask = 0xdf;
	    while (q1 >= p1 && q2 >= p2) {
		if (((*q1--) & mask) != ((*q2--) & mask)) {
		    n = (q2[1] & 0xff) - (q1[1] & 0xff);
		    goto ret;
		}
		if (*q1 == '@' || *q2 == '@')
		    mask = 0xff;
	    }
	    if (q1 >= p1)
		n = -1000;
	    else if (q2 >= p2)
		n = 1000;
	    else
		n = 0;
	} else
	    n = 2000;
    } else {
	if (index(p2, '@') == NULL) {
	    /* Both are UUCP */
	    n = strcmp(p1, p2);
	} else
	    n = -2000;
    }
  ret:
#ifdef DEBUG
    if (n < 0)
	printf("%s > %s (%d)\n", a1, a2, n);
    else if (n > 0)
	printf("%s < %s (%d)\n", a1, a2, n);
    else
	printf("%s = %s (%d)\n", a1, a2, n);
#endif
    return n;
}

/*
 * Sort the list of addresses eliminating duplicating ones
 */
void sortaddrs(addrptr)
struct addr **addrptr;
{
    register struct addr *ap, *wp, *pp;
    int i;

    /* sort addresses */
    ap = (*addrptr)->next;
    (*addrptr)->next = NULL;
    while (ap != NULL) {
	for (pp = NULL,wp = (*addrptr); wp != NULL; pp = wp, wp = wp->next) {
	    if ((i = adrcmp(wp->name, ap->name)) > 0) {
		if (wp->next == NULL) {
		    wp->next = ap;
		    ap = ap->next;
		    wp->next->next = NULL;
		    break;
		}
		continue;
	    }
	    if (i == 0) {
		pp = ap->next;
		free(ap);
		ap = pp;
		break;
	    }
	    if (i < 0) {
		if (pp == NULL) {
		    pp = ap->next;
		    ap->next = (*addrptr);
		    (*addrptr) = ap;
		    ap = pp;
		    break;
		}
		pp->next = ap;
		ap = ap->next;
		pp->next->next = wp;
		break;
	    }
	}
    }

}

void sortsubscribers()
{
    if (feeds != NULL)
	sortaddrs(&feeds);
    if (subscribers != NULL)
	sortaddrs(&subscribers);
    if(cnewsers != NULL)
	sortaddrs(&cnewsers);
}

int isempty(str)
char *str;
{
    register char *p;

    p = str;
    while (*p) {
	if (*p != '\n' && *p != ' ' && *p != '\t')
	    return 0;
	p++;
    }
    return 1;
}

#ifndef NNTP_ONLY
/*
 * Construct a file name
 */
char *
 gfn(grp, fn)
char *grp;
char *fn;
{
    static char buf[200];
    char g[161], *p;

    strncpy(g, grp, sizeof(g)-1);
    g[sizeof(g)-1] = '\0';
    p = g;
    while ((p = index(p, '.')) != NULL)
	*p = '/';
    sprintf(buf, "%s/%s/%s", NEWSSPOOL, g, fn);
#ifdef DEBUG
    printf("READ %s\n", buf);
#endif
    return buf;
}
#endif

/*
 * Get the next word from the list
 *      (returns 0 if no more words)
 */
char *nextword(p, word, wsize)
register char *p;
char word[];
int wsize;
{
    register char *r;

    while (ISspace(*p) || *p == ',')
	p++;
    r = word;
    while (*p && !ISspace(*p) && *p != ',' && --wsize>0)
	*r++ = *p++;
    *r = '\0';
    if (word[0] == '\0' || word[0] == '\n')
	return NULL;
    return p;
}

#ifdef MEMMAP
#undef fseek
#undef fread
#endif

#ifdef SEND_BY_SMTP
#include "mnewssmtp.h"

static int h=0;
#endif

/*
 * Relayin' machine
 */
#ifndef NNTP_ONLY
int relay(char *grpname, int artnum)
#else
int relay(FILE *input)
#define grpname ""
#endif
{
    char iline[5120];
    char tmpname[512];
    int infd, ignorecont, empty, longline;
    long size;
    register c;
#ifndef NNTP_ONLY
    FILE *tmp;
    FILE *input;
#else
    char *newsgroups=NULL;
    int  waspoint, wascr = 1;
    int  artnum=0;
#endif
    int  retcode=0;
    register char *p;
    char *q, *r;
    struct addr *ap, *nap;
    char *av[FACTOR + 5];
    struct xreflist *xl, *nxl;
#ifdef SEND_BY_SMTP
    struct mailnewsreq req;
    char vc;
#else
    int status;
    int pfd[2];
#endif


#ifndef NNTP_ONLY

#ifdef DEBUG
    printf("RELAY: group '%s' art %d\n", grpname, artnum);
#endif

    /*
	 * Open the file
	 */
    sprintf(tmpname, "%d", artnum);
    if ((input = fopen(gfn(grpname, tmpname), "r")) == NULL)
	return 1;
#endif

    /*
	 * Initialize all data structures
	 */
    XrefList = NULL;
    feeds = NULL;
    efeeds = NULL;
    subscribers = NULL;
    cnewsers = NULL;
    subject_line[0] = 0;
    originator[0] = 0;

    gethost(tmpname, sizeof(tmpname));
    snprintf(sendername, sizeof(sendername), "%s@%s", gsname(grpname), tmpname);

    ibp = inbuf;
#ifdef USE_XCLASS
    p = "X-Class: Slow\n";
    while (*p) *ibp++ = *p++;
#endif
#ifdef USE_PRECEDENCE
    p = "Precedence: ";
    while (*p)	*ibp++ = *p++;
    p = PRECEDENCE;
    while (*p)	*ibp++ = *p++;
    *ibp++ = '\n';
#endif
    p = "To: netters";
    while (*p)	*ibp++ = *p++;
    *ibp++ = '@';
    p = tmpname;
    while (*p)	*ibp++ = *p++;
    *ibp++ = '\n';
    p = "Sender: ";
    while (*p)	*ibp++ = *p++;
    p = sendername;
    while (*p)	*ibp++ = *p++;
    *ibp++ = '\n';

#ifndef NNTP_ONLY
    fseek(input, 0l, SEEK_END);
    size = ftell(input);
    fseek(input, 0l, SEEK_SET);
#endif

    /*
	 * Parse header of a message
	 */
    ignorecont = 0;
    longline = 0;
    while (sofgets(iline, sizeof iline, input) != NULL) {
      next_line:
	if (iline[0] == '\0') break;
	longline = (index(iline, '\n') == NULL);
	if (longline) {
	    retcode=-1; /* rejecting: line too long */
	    goto todevnull;
	}
#ifdef NNTP_ONLY
	if (wascr) {
	    if (strcmp(iline, ".\r\n")==0) break;
	    if (strcmp(iline, "\r\n")==0) break;
	    if (iline[0]=='.') strcpy(iline, iline+1);
	}
#endif
	p = iline+strlen(iline)-2;
	if (p>=iline && strcmp(p, "\r\n")==0) {
	    strcpy(p, "\n");
#ifdef NNTP_ONLY
	    wascr = 1;
#endif
	}
#ifdef NNTP_ONLY
	else
	    wascr = 0;
#else
	if (iline[0] == '\n')
	    break;
#endif
	if (ibp >= &inbuf[(sizeof inbuf) - sizeof(iline) - 1]) {
	    retcode=-2; /* rejecting: header too long */
	    goto todevnull;
	}
	if (ignorecont && ISspace(iline[0]))
	    continue;
	ignorecont = 0;
	if (fieldcmp(iline, "xref")) {
	    ignorecont++;
	    p = iline + 6;
	    if ((p = nextword(p, tmpname, sizeof(tmpname))) == NULL)
		continue;	/* skip the name of site */
	    while ((p = nextword(p, tmpname, sizeof(tmpname))) != NULL) {
		q = index(tmpname, ':');
		if (q == NULL)
		    continue;
		*q++ = '\0';
		if (tmpname[0] == '\0' || *q == '\0')
		    continue;
		if ((c = atoi(q)) <= 0)
		    continue;
		xl = (struct xreflist *) malloc(sizeof(struct xreflist) +
						strlen(tmpname));
		if (xl == NULL)
		    break;
		strcpy(xl->name, tmpname);
		xl->number = c;
		xl->next = XrefList;
		XrefList = xl;
	    }
	    p = "X-Ref: ";
	    while (*p)	*ibp++ = *p++;
	    for (xl = XrefList; xl != NULL; xl = xl->next) {
		if (xl != XrefList)
		    *ibp++ = ',';
		sprintf(tmpname, "%s:%d", xl->name, xl->number);
		p = tmpname;
		while (*p)  *ibp++ = *p++;
	    }
	    *ibp++ = '\n';
	    continue;
	}
	if (fieldcmp(iline, "control")) { /* gul */
todevnull:
#ifndef NNTP_ONLY
	    fclose(input);
#else
	    while (sofgets(iline, sizeof(iline), input)) {
		if (!longline && wascr && strcmp(iline, ".\r\n")==0)
		    break;
		p=iline+strlen(iline)-2;
		if (p>=iline && strcmp(p, "\r\n")==0) {
		    strcpy(p, "\n");
		    wascr=1;
		}
		else
		    wascr = 0;
		longline = (index(iline, '\n') == NULL);
	    }
	    if (strcmp(iline, ".\r\n") || !wascr)
	    {
		lexit(1);
	    }
#endif
	    goto end;
	}
	if (fieldcmp(iline, "return-receipt-to") ||
	    fieldcmp(iline, "to") ||
	    fieldcmp(iline, "sender")) {
	    ignorecont++;
	    continue;
	}
	if (fieldcmp(iline, "path")) {
	    p = "X-NNTP-";
	    while (*p) *ibp++ = *p++;
	    p = iline;
	    while (*p) *ibp++ = *p++;
	    continue;
	}
	if (fieldcmp(iline, "subject")) {
	    p = "Subject: [NEWS] ";
	    while (*p)	*ibp++ = *p++;
	    p = iline + 8;
	    q = subject_line;
s_cont:
	    while (ISspace(*p))	p++;
	    if (ibp >= &inbuf[(sizeof inbuf) - sizeof(iline) - 1])
		break;
	    while (*p && ((int) (q - subject_line)) < sizeof subject_line) *ibp++ = (*q++ = *p++);
	    *q = 0;
	    if (sofgets(iline, sizeof iline, input) != NULL) {
		if (ISspace(iline[0]) || longline) {
		    longline = (index(iline, '\n') == NULL);
		    p=iline + strlen(iline) - 2;
		    if (p>=iline && strcmp(p, "\r\n")==0) {
			strcpy(p, "\n");
#ifdef NNTP_ONLY
			wascr = 1;
#endif
		    }
#ifndef NNTP_ONLY
		    else
			wascr = 0;
#endif
		    if ((p = index(subject_line, '\n')) != NULL)
			*p = ' ';
		    p=iline;
		    *ibp++='\t';
		    goto s_cont;
		}
		else{
		    if ((p = index(subject_line, '\n')) != NULL)
			*p = 0;
		    goto next_line;
		}
	    }
	    lexit(1);
	}
	p = iline;
	while (*p)  *ibp++ = *p++;
	if (fieldcmp(iline, "from")) {
	    p = iline + 5;
	    while (ISspace(*p))	p++;
	    if ((q = index(p, '<')) != NULL) {
		if ((r = index(++q, '>')) != NULL) {
		    *r = '\0';
		    strncpy(originator, q, (sizeof originator)-1);
		    continue;
		}
	    }
	    strncpy(originator, p, (sizeof originator)-1);
	    if ((p = index(originator, ' ')) != NULL)  *p = '\0';
	    if ((p = index(originator, '\n')) != NULL)	*p = '\0';
	    continue;
	}
	if (fieldcmp(iline, "newsgroups")) {
#ifdef NNTP_ONLY
	    if (newsgroups) free(newsgroups);
	    newsgroups=malloc(strlen(iline+10));
	    if (newsgroups==NULL) {
		retcode=1; /* spooling: not enough core */
		goto todevnull;
	    }
	    strcpy(newsgroups, iline+11);
#else
	    getsubscribers(&iline[11], size);
#endif
	    while (sofgets(iline, sizeof iline, input) != NULL) {
		longline = (index(iline, '\n') == NULL);
		if (longline) {
		    retcode=-1; /* rejecting: line too long */
		    goto todevnull;
		}
		if (!ISspace(iline[0]))
		    goto next_line;
		p=strstr(iline, "\r\n");
		if (p) {
			strcpy(p, "\n");
#ifdef NNTP_ONLY
			wascr = 1;
#endif
		}
#ifdef NNTP_ONLY
		else
			wascr = 0;
#endif
		if (ibp >= &inbuf[(sizeof inbuf) - sizeof(iline) - 1])
		    break;
		p = iline;
		while (*p)  *ibp++ = *p++;
#ifdef NNTP_ONLY
		if (!newsgroups) continue; /* :-( */
		p = newsgroups + strlen(newsgroups) - 2;
		if (p>=newsgroups && strcmp(p, "\r\n")==0) *p='\0';
		else if (p+1>=newsgroups && strcmp(p+1, "\n")==0) p[1]='\0';
		if (newsgroups[strlen(newsgroups)-1]=='\n')
		    newsgroups[strlen(newsgroups)-1]='\0';
		p=malloc(strlen(newsgroups)+strlen(iline)+1);
		if (p==NULL) {
		    retcode=1; /* spooling - not enough core */
		    goto todevnull;
		}
		strcpy(p, newsgroups);
		strcat(p, iline);
		free(newsgroups);
		newsgroups=p;
#else
		getsubscribers(iline, size);
#endif
	    }
	    break;
	}
    }
#ifdef NNTP_ONLY
    if (iline[0]!='\n' && strcmp(iline, ".\r\n") && strcmp(iline, "\r\n"))
    {
	lexit(1);
    }
    if (strcmp(iline, "\r\n")==0)
	strcpy(iline, "\n");
#endif
#ifdef DEBUG
    printf("END OF HEADER\n");
#endif

#ifndef NNTP_ONLY
    if (feeds == NULL && subscribers == NULL && efeeds == NULL && cnewsers == NULL) {
	fclose(input);
	goto end;
    }
#endif
    /*
	 * Copy the body of the message
	 */
#ifdef NNTP_ONLY
    if (!longline && wascr && strcmp(iline, ".\r\n")==0) {
	retcode=-3; /* rejecting: incorrect message */
	goto end;
    }
#endif
    if (longline || strcmp(iline, "\n")) {
	retcode=-3; /* rejecting: incorrect message */
	goto todevnull;
    }
    *ibp++ = '\n';
    infd = -1;
    while (sofgets(iline, sizeof iline, input)) {
	if (longline) {
	    longline = (index(iline, '\n') == NULL);
	    empty = 0;
	    goto nb;
	}
	longline = (index(iline, '\n') == NULL);
#ifdef NNTP_ONLY
	if (wascr) {
	    if (strcmp(iline, ".\r\n")==0)
		break;
	    if (iline[0] == '.')
		strcpy(iline, iline+1);
	}
#endif
	p=iline+strlen(iline)-2;
	if (p>=iline && strcmp(p, "\r\n")==0) {
	    strcpy(p, "\n");
#ifdef NNTP_ONLY
	    wascr = 1;
#endif
	}
#ifdef NNTP_ONLY
	else
	    wascr = 0;
#endif
	if (isempty(iline))
	    continue;
	else {
	    if (strcmp(iline, "--\n") == 0 || strcmp(iline, "---\n") == 0)
		empty = 1;
	    else
		empty = 0;
	    p = iline;
	    while (*p)
		*ibp++ = *p++;
	    goto nb;
	}
    }
#ifdef NNTP_ONLY
    if (strcmp(iline, ".\r\n"))
    {
	lexit(1);
    }
#endif
    empty = 1;
    goto ei;
  nb:
#ifdef NNTP_ONLY
    waspoint=0;
#endif
    while ((c = sofgetc(input)) != EOF) {
#ifdef NNTP_ONLY
	if (c=='\r') {
	    if (wascr)
		waspoint = 0;
	    wascr = 1;
	    longline = 1;
	    continue;
	}
	if (waspoint && c=='\n' && wascr)
	    break;
	if (!longline && c=='.' && wascr) {
	    waspoint=1;
	    longline = 1;
	    wascr = 0;
	    continue;
	}
	longline = (c!='\n');
	if (longline)
	    wascr = 0;
	waspoint=0;
#endif
	if (ibp < &inbuf[sizeof inbuf])
	    *ibp++ = c;
	else {
#ifndef NNTP_ONLY
#ifdef DEBUG
	    printf("TEMP FILE\n");
#endif
	    /*
			 * Create temporary file
			 */
	    sprintf(tmpname, "/tmp/SNDL.%d\n", getpid());
	    tmp = fopen(tmpname, "w");
	    if (tmp == NULL) {
		fprintf(stderr, "feeder: cannot create temp file\n");
		lexit(75);	/* EX_TEMPFAIL */
	    }
	    /*
			 * Write out the buffer
			 */
	    p = inbuf;
	    while (p < &inbuf[sizeof inbuf])
		putc(*p++, tmp);

	    /*
			 * Copy the rest
			 */
	    putc(c, tmp);
	    while ((c = fgetc(input)) != EOF)
		putc(c, tmp);
	    fclose(tmp);

	    infd = open(tmpname, O_RDONLY);
	    if (infd < 0) {
		fprintf(stderr, "feeder: cannot reopen temp file\n");
		lexit(75);	/* EX_TEMPFAIL */
	    }
	    unlink(tmpname);

	    break;
#else /* NNTP_ONLY */
	    retcode = -4; /* reject - article too large */
	    goto todevnull;
#endif
	}
    }
#ifdef NNTP_ONLY
    if (!waspoint || !wascr || c!='\n')
    {
	lexit(1);
    }
#endif
  ei:
#ifndef NNTP_ONLY
    fclose(input);
#endif

#ifdef DEBUG
    printf("END OF INPUT, XREF LIST:\n");
    for (xl = XrefList; xl != NULL; xl = xl->next)
	printf("%s/%d ", xl->name, xl->number);
    printf("\n");
#endif

#ifdef NNTP_ONLY
    size=ibp-inbuf;
    if (newsgroups) {
	getsubscribers(newsgroups, size);
	free(newsgroups);
	newsgroups=NULL;
    }
    if (feeds == NULL && subscribers == NULL && efeeds == NULL && cnewsers == NULL)
	goto end;
#endif

    if (efeeds != NULL) {
	register struct addr *efp;
	if (empty)
	    efp = subscribers;
	else
	    efp = feeds;
	if (efp == NULL)
	    if (empty)
		subscribers = efeeds;
	    else
		feeds = efeeds;
	else {
	    while (efp->next)
		efp = efp->next;
	    efp->next = efeeds;
	}
    }
    sortsubscribers();

#ifdef DEBUG
    printf("END OF SORTING\n");
#endif

    if (feeds == NULL)
	goto subs;

#ifdef LOGFILE
    for (ap = feeds; ap != NULL; ap = ap->next) {
	for (wsl = slc; wsl != NULL; wsl = wsl->next) {
	    if (adrcmp(wsl->name, ap->name) == 0) {
		wsl->count++;
		goto bl;
	    }
	}
	if (slc == NULL) {
	    slc = (struct sl *) calloc(1, sizeof(struct sl) + strlen(ap->name));
	    slc->next = NULL;
	    slc->count = 1;
	    strcpy(slc->name, ap->name);
	} else {
	    wsl = (struct sl *) calloc(1, sizeof(struct sl) + strlen(ap->name));
	    wsl->next = slc;
	    wsl->count = 1;
	    strcpy(wsl->name, ap->name);
	    slc = wsl;
	}
      bl:;
    }
#endif
    /*
	 * Send the mail out for feeds
	 */
#ifdef SEND_BY_SMTP
    for (ap = feeds; ap != NULL;) {
	int i;
	char lastch;
	for (c = 0; c < FACTOR && ap != NULL; c++) {
	    av[c] = ap->name;
	    ap = ap->next;
	}
	av[c] = NULL;
	req.Iam=sendername;
	req.mailhub=MAILHUB;
	req.port=SMTPPORT;
#ifdef MAILHUB2
	req.mailhub2=MAILHUB2;
	req.port2=SMTPPORT2;
#endif
	req.users=av;
	h=opensmtpsession(&req,h);
	if (h<0) {
	    if (h==-2) {	/* no valid feeders */
		h=0;
		goto subs;
	    }
	    retcode=2;	/* spooling - can't open smtp */
	    h=0;
	    goto end;
	}
	lastch='\n';
	if (infd == -1) {
	    for (i=0; i<ibp-inbuf; i++) {
		if ((inbuf[i]=='.') && (lastch=='\n'))
			write(h, inbuf+i, 1);
		lastch=inbuf[i];
		if (write(h, inbuf+i, 1)!=1) {
			log("can't write to socket: %s!\n", strerror(errno));
			break;
		}
	    }
	} else {
	    lseek(infd, 0l, SEEK_SET);
	    while (read(infd, &vc, 1)==1) {
		if ((lastch=='\n') && (vc=='.'))
			write(h, &vc, 1);
		lastch=vc;
		if (write(h, &vc, 1)!=1) {
			log("can't write to socket: %s!\n", strerror(errno));
			break;
		}
	    }
	}
	if (closesmtpdata(h)) {
	    log("Can't send message (closesmtpdata error)!");
	    closesmtpsession(h);
	    h=0;
	    retcode=3; /* spooling - can't close smtp */
	    goto end;
	}
    }
#else /* not SEND_BY_SMTP */
    av[0] = "send-mail";
    av[1] = SENDER_FLAG;
    av[2] = sendername;
#ifdef MFLAG_QUEUE
    av[3] = MFLAG_QUEUE;
#endif
    for (ap = feeds; ap != NULL;) {
	for (c = 0; c < FACTOR && ap != NULL; c++) {
	    av[c + 
#ifdef MFLAG_QUEUE
	           4
#else
	           3
#endif
	             ] = ap->name;
	    ap = ap->next;
	}
	av[c +
#ifdef MFLAG_QUEUE
	       4
#else
	       3
#endif
	         ] = NULL;
	if (infd == -1) {
	    if (pipe(pfd) == -1) {
		fprintf(stderr, "feeder: cannot make a pipe\n");
		lexit(75);	/* EX_TEMPFAIL */
	    }
	    if ((c = fork()) == 0) {
		/* new process */
		close(0);
		dup(pfd[0]);	/* should be 0 :-) */
		for (c = 1; c < _NFILE; c++)
		    close(c);
#ifndef DEBUG
		open("/dev/null", O_WRONLY);
		dup(1);
#else
		execv("/bin/echo", av);
		lexit(1);
#endif
		execv(MAILER, av);
		lexit(1);
	    }
	    close(pfd[0]);
	    write(pfd[1], inbuf, ibp - inbuf);
	    close(pfd[1]);
	} else {
	    lseek(infd, 0l, SEEK_SET);

	    if ((c = fork()) == 0) {
		close(0);
		dup(infd);
		close(infd);
		infd=-1;
		for (c = 1; c < _NFILE; c++)
		    close(c);

		/* new process */
#ifndef DEBUG
		open("/dev/null", O_WRONLY);
		dup(1);
#else
		execv("/bin/echo", av);
		lexit(1);
#endif
		execv(MAILER, av);
		lexit(1);
	    }
	}
	status = 0;
#if defined(_POSIX_SOURCE) || defined(ISC)
	waitpid(c, &status, 0);
#else
	wait(&status);
#endif
	if (status != 0) {
	    retcode = 5; /* spooling - bad sendmail retcode */
	    goto end;
	}
    }
#endif /* not SEND_BY_SMTP */
  subs:

    if (infd!=-1) close(infd);

    if (subscribers == NULL)
	goto cnews;
    /*
	 * If there are any subscribers add entries for tables
	 */
    if (size < 1000)
	sprintf(tmpname, "%d", (int)size);
    else if (size < 10000)
	sprintf(tmpname, "%d.%dK",(int) size / 1024,(int) ((size * 10) / 1024) % 10);
    else
	sprintf(tmpname, "%dK",(int) size / 1024);
    for (ap = subscribers; ap != NULL; ap = ap->next) {
	for (xl = XrefList; xl != NULL; xl = xl->next) {
	    if (strcmp(xl->name, ap->group->name) == 0) {
		artnum = xl->number;
		break;
	    }
	}
#ifdef DEBUG
	printf("ARTLIST: %s %s %d %s %s %s\n",
	       ap->name, ap->group->name, artnum,
	       tmpname, originator, subject_line);
#endif
#ifndef CLOSEARTLIST
	if ( art_list == NULL)
#endif
		if ((art_list = fopen(ArtList, "a")) == NULL)
		    break;
	fprintf(art_list, "%s %s %d %s %s %s\n",
		ap->name, ap->group->name, artnum,
		tmpname, originator, subject_line);
#ifdef CLOSEARTLIST
	fclose(art_list);
#endif
    }

cnews:
    if (cnewsers == NULL)
	goto end;

    for (ap = cnewsers; ap != NULL; ap = ap->next) {
	for (xl = XrefList; xl != NULL; xl = xl->next) {
	    if (strcmp(xl->name, ap->group->name) == 0) {
		artnum = xl->number;
		break;
	    }
	}
#ifdef DEBUG
	printf("TOGO: %s %d %ld\n",
	       ap->group->name, artnum, size);
#endif
	if((q=index(ap->name,'@'))!=NULL)
		*q=0;
#ifndef CLOSEARTLIST
	if (cart_list == NULL)
#endif
	{	sprintf(tmpname,"%s/%s/togo", OUTGO_DIR,ap->name);
		if ((cart_list = fopen(tmpname, "a")) == NULL)
		    break;
	}
	strcpy(tmpname,ap->group->name);
	while((q = index(tmpname,'.'))!=NULL) *q='/';
	fprintf(cart_list, "%s/%d %d\n",
		tmpname, artnum, (int) size);
#ifdef CLOSEARTLIST
	fclose(cart_list);
#endif
    }
  end:
    /*
	 * Clean up all references
	 */
    for (xl = XrefList; xl != NULL; xl = nxl) {
	nxl = xl->next;
	free(xl);
    }
    for (ap = subscribers; ap != NULL; ap = nap) {
	nap = ap->next;
	if (--(ap->group->count) == 0)
	    free(ap->group);
	free(ap);
    }
    for (ap = feeds; ap != NULL; ap = nap) {
	nap = ap->next;
	if (--(ap->group->count) == 0)
	    free(ap->group);
	free(ap);
    }
#ifdef NNTP_ONLY
    if (newsgroups) free(newsgroups);
#endif
    return retcode;
}

#ifndef NNTP_ONLY
/*
 * The main loop
 */
void process_batch(togo)
FILE *togo;
{
    char line[200];
    char *p, *q;
    long poffset, noffset;
    int an, r;
#ifndef PERFOMANCE_CNEWS
    int nslen = strlen(NEWSSPOOL);
#endif

    poffset = 0;
    while (fgets(line, sizeof line, togo) != NULL) {

	noffset = ftell(togo);

	if (line[0] == '#') {
	    poffset = noffset;
	    continue;
	}
#ifdef PERFOMANCE_CNEWS
	p = line;
	q = rindex(p, '/');
	if (q == NULL)
	    continue;
	*q++ = '\0';
	if (*q == '\0' || *p == '\0')
	    continue;
	an = atoi(q);
	if (an <= 0)
	    continue;
	q = p;
	while ((q = index(q, '/')) != NULL)
	    *q++ = '.';
#else
	p = index(line, ' ');
	if (p != NULL)
	    *p = '\0';
	p = index(line, '\t');
	if (p != NULL)
	    *p = '\0';
	p = index(line, '\n');
	if (p != NULL)
	    *p = '\0';

	if (strncmp(line, NEWSSPOOL, nslen) || line[nslen] != '/')
	    continue;
	p = &line[nslen + 1];

	q = rindex(p, '/');
	if (q == NULL)
	    continue;
	*q++ = '\0';
	if (*q == '\0' || *p == '\0')
	    continue;
	an = atoi(q);
	if (an <= 0)
	    continue;
	q = p;
	while ((q = index(q, '/')) != NULL)
	    *q++ = '.';

#endif
	if ((r=relay(p, an))<0)
	    log("rejected %s/%u, relay() retcode %d\n", p, an, r);
	else if (r>0) {
	    log("panic at %s/%u, relay() retcode %d\n", p, an, r);
	    lexit(1);
	}
	(void) fseek(togo, poffset, SEEK_SET);
	(void) fputc('#', togo);
	(void) fflush(togo);
	(void) fseek(togo, noffset, SEEK_SET);
	poffset = noffset;
    }
#ifdef SEND_BY_SMTP
    if (h>0) {
	closesmtpsession(h);
	h = 0;
    }
#endif
#ifndef CLOSEARTLIST
    if (art_list) {
	fclose(art_list);
	art_list = NULL;
    }
    if (cart_list) {
	fclose(cart_list);
	cart_list = NULL;
    }
#endif
}
/*
 * Run out the batch
 */
int main(argc,argv)
int argc;
char **argv;
{
    FILE *togo;
    char TmpRun[100], ToGo[100];

    strncpy(pname,argv[0],sizeof(pname));
    readsnl();
    sprintf(TmpRun, "%s/%s", WORKDIR, TMPRUN);
    sprintf(ToGo, "%s/%s", WORKDIR, TOGO);
    sprintf(ArtList, "%s/%s", WORKDIR, ARTLIST);

    /*
	 * Lock run file
	 */
    lockrun(WORKDIR);
#ifdef DEBUG
    setbuf(stdout, (char *) NULL);
#endif

    if(open_base(OPEN_RDONLY))
    	lexit(1);

#ifdef LOGFILE
    slc = NULL;
#endif
    /*
	 * Process batch
	 */
    if ((togo = fopen(TmpRun, "r+")) != NULL) {
	process_batch(togo);
	fclose(togo);
	unlink(TmpRun);
    }
    if ((togo = fopen(ToGo, "r")) != NULL) {
	fseek(togo, 0L, SEEK_END);
	if (ftell(togo) > 1) {
	    register c;
	    register FILE *tmprun = fopen(TmpRun, "w");
	    fseek(togo, 0L, SEEK_SET);
	    while ((c = fgetc(togo)) != EOF)
		fputc(c, tmprun);
	    fclose(tmprun);
	    fclose(togo);
	    if ((tmprun = fopen(ToGo, "w")) == NULL) {
		fprintf(stderr, "Can't truncate togo file\n");
		goto out;
	    } else
		fclose(tmprun);
	} else
	    goto out;
    } else
	goto out;

    if ((togo = fopen(TmpRun, "r+")) != NULL) {
	process_batch(togo);
	fclose(togo);
	unlink(TmpRun);
    }
    printstat();
    /*
	 * Unlock run file
	 */
  out:
    unlockrun(WORKDIR);
    close_base();
    exit(0);
}

#else /* NNTP_ONLY */

extern int nntprecv(void);

int main(int argc, char * argv[])
{
    strncpy(pname, argv[0], sizeof(pname));
    readsnl();
    sprintf(ArtList, "%s/%s", WORKDIR, ARTLIST);

    // lockrun(WORKDIR);
#ifdef DEBUG
    setbuf(stdout, (char *) NULL);
#endif

#ifdef LOGFILE
    slc = NULL;
#endif

    nntprecv();

    return 0;
}
#endif /* NNTP_ONLY */
