/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1992-1995 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved
 *
 * (C) Copyright 1996-1998 by Pavel Gulchouck, Kiev, Ukraine
 * All rights reserved
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "conf.h"
#include "coms.h"
#include "dbsubs.h"
#include "letters.h"
#include "nnrp.h"
#include <dirent.h>
#ifdef SEND_BY_SMTP
#include "mnewssmtp.h"

static int h=0;
#endif

#ifdef NNTP_ONLY
int sofgetc(FILE *);
int sofgets(char *, int, FILE *);
#else
#define sofgetc(file)		getc(file)
#define sofgets(str,size,file)	fgets(str,size,file)
#endif

#ifndef lint
static char sccsid[] = "@(#)newsserv.c 4.0 31/01/94";
#endif

#ifndef _NFILE
#ifdef BSD
#define _NFILE FOPEN_MAX
#else
#define _NFILE 20
#endif
#endif

#define ISspace(x) ( x==' ' || x=='\t' )
#define IS_CU(x) (((x[0x33]+x[0x6d])>>1)==x[0x33])

extern char *version;

extern FILE *fdopen();
extern char *calloc(), *malloc();
extern void free(),closehist();
extern long strtol(),get_offset();
extern int lock(), atoi(), wait();
extern int snprintf __P ((char *, size_t, const char *, ...));
#if defined(_POSIX_SOURCE) || defined(ISC)
extern int waitpid();
#endif

struct aentry {
    struct aentry *next;
    ulong flag;
    unchar size;
    char *description;
    char name[1];
} *active = (struct aentry *) NULL;

struct a_seen {
int	hash;
int	len;
int	fl;
struct a_seen *next;
} *seen_ptr;

struct shablon {
    unchar body[160];
    int  uncase;
    struct shablon *next;
};

char pname[80];
char replyaddr[200];
char sname[100];
int lang,limit,index_art,find_art;
char *av[20];
char hn[80];
char *cpr="Mail-News Gateway\nCopyrights (C) Vadim Antonov (avg@rodan.uu.net) 1991\n"
"           (C) Stanislav Voronyi (stas@uanet.kharkov.ua) 1992-1995\n"
"           (C) Pavel Gulchouck (gul@lucky.net) 1996-1998\n";
char *adate();

struct hdrfields {
    char *field;
    char *contents;
}

hfields[] = {
#define H_FROM          0
    {"From", NULL},
#define H_MESSAGE_ID    1
    {"Message-ID", NULL},
#define H_NEWSGROUPS    2
    {"Newsgroups", NULL},
#define H_DISTRIBUTION  3
    {"Distribution", NULL},
#define H_SUBJECT       4
    {"Subject", NULL},
#define H_SUMMARY       5
    {"Summary", NULL},
#define H_KEYWORDS      6
    {"Keywords", NULL},
#define H_ORGANIZATION  7
    {"Organization", NULL},
#define H_FOLLOWUP_TO   8
    {"Followup-to", NULL},
#define H_EXPIRES       9
    {"Expires", NULL},
#define H_REPLY_TO      10
    {"Reply-to", NULL},
#define H_REFERENCES    11
    {"References", NULL},
#define H_APPROVED      12
    {"Approved", NULL},
#define H_DATE          13
    {"Date", NULL},
#define H_SUPERSEDES	14
    {"Supersedes", NULL},
#define H_MIME          15
    {"Mime-Version", NULL},
#define H_CONTTYPE      16
    {"Content-Type", NULL},
#define H_CONTENC       17
    {"Content-Transfer-Encoding", NULL},
#define H_CONTLENGTH    18
    {"Content-Length", NULL},
#define H_XCOMMENT      19
    {"X-Comment-To", NULL},
#define H_XTO		20
    {"X-To", NULL}
};

#define nhfields        (sizeof hfields/sizeof(struct hdrfields))

char tmpbuf[16384];
char iline[2000];
char frombuf[500];
char ufrom[100];
char usite[10];
int usite_ok;

static void
bexit(i)
int i;
{
if(fb!=NULL)
	close_base();
exit(i);
}

#ifdef NNTP_ONLY

int dont_disconnect=0;

#define connect_nnrpd()	if (connect_nnrp()) bexit(1)

void lexit(int r)
{
    log("Can't read from socket: %s\n", strerror(errno));
    bexit(r);
}

#endif /* NNTP_ONLY */

#ifndef HAVE_WILDMAT
#include "wildmat.c"
#endif

/*
 * Compare the beginning of line with the field name
 * ignoring the case. Returns 1 if matched.
 */
int fieldcmp(line, fname)
register char *line;
register char *fname;
{
    register char c, d;

    while (*fname) {
	c = *line++;
	if (c == '\0')
	    return 0;
	if ('A' <= c && c <= 'Z')
	    c |= 040;
	d = *fname++;
	if ('A' <= d && d <= 'Z')
	    d |= 040;
	if (c != d)
	    return 0;
    }
    return (*line == ':');
}

/*
 * Compare the command words
 */
int cmdcmp(cmd, pattern)
register char *cmd;
register char *pattern;
{
    int n = 0;

    while (*cmd && (*cmd & ~040) == *pattern)
	cmd++, pattern++, n++;
    return (n >= 3) && (*cmd == '\0');
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
	printf("%s > %s (%d)\n", p1, p2, n);
    else if (n > 0)
	printf("%s < %s (%d)\n", p1, p2, n);
    else
	printf("%s = %s (%d)\n", p1, p2, n);
#endif
    return n;
}


/*
 * Print help file
 */
void help(of)
FILE *of;
{
    FILE *hf;
    int hlp = 0;
    register c;
    char filnam[200];
#ifdef SEND_BY_SMTP
    int was_eol=1;
#endif

    sprintf(filnam, HELPFILE, ext[lang]);
    if ((hf = fopen(filnam, "r")) != NULL) {
	hlp++;
	while ((c = getc(hf)) != EOF) {
	    if (c == '$') {
		c = getc(hf);
		if (c == EOF)
		    break;
		if (c == '$')
		    fprintf(of, "%s@%s", NEWSUSER, hn);
		continue;
	    }
#ifdef SEND_BY_SMTP
	    if (was_eol && (c=='.'))
		fputs(".", of);
	    was_eol = (c == '\n');
#endif
	    putc(c, of);
	}
	fclose(hf);
    }
    if (!hlp) {
	fprintf(of, mesg(9, lang));

    }
    return;
}

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

    while (ISspace(*p) || *p == ',' || *p == '\n')
	p++;
    r = word;
    while (*p && !ISspace(*p) && *p != ',' && *p != '\n') {
	*r++ = *p++;
	if (r-word>=wsize-1) break;
    }
    *r = '\0';
    if (word[0] == '\0')
	return NULL;
    return p;
}

#ifndef NNTP_ONLY
/*
 * Construct a file name
 */
char *gfn(grp, fn)
char *grp;
char *fn;
{
    static char buf[200];
    char g[160], *p;

    strncpy(g, grp, sizeof(g)-1);
    p = g;
    while ((p = index(p, '.')) != NULL)
	*p = '/';
    snprintf(buf, sizeof(buf)-1, "%s/%s/%s", NEWSSPOOL, g, fn);
    return buf;
}
#endif

char *
 subsplit(pp, n)
char **pp;
int n;
{
    register char *p, *q;
    register i;
    static char psub[80];	/* assume n < 80 */
    int n2;

    p = *pp;
    while (*p == ' ' || *p == '\t')
	p++;
    q = psub;
    for (i = n; i && *p; i--)
	*q++ = *p++;
    *q = 0;
    if (*p == 0) {
	*pp = NULL;
	return psub;
    }
    if (*p == ' ' || *p == '\t') {
	while (*p == ' ' || *p == '\t')
	    p++;
	if (*p == 0)
	    *pp = NULL;
	else
	    *pp = p;
	/* eliminate trailing spaces */
	while (*--q == ' ' || *q == '\t')
	    *q = 0;
	return psub;
    }
    i = 0;
    n2 = n / 2;
    while ((*p != ' ' && *p != '\t') && i < n2)
	p--, i++;
    if (i == n2) {		/* No word separators found */
	*pp += n;
	return psub;
    }
    *pp = &p[1];
    q -= i - 1;
    while (*--q == ' ' || *q == '\t')
	*q = 0;
    return psub;
}

/*
 * List available groups
 */

static void
read_newsgroups()
{
FILE *ng;
char line[512];
char *descr,*name;
register struct aentry *ap;
register char *p;
static int newsgroups_readed=0;

    if (newsgroups_readed) return;

#ifdef NNTP_ONLY
    connect_nnrpd();
    fputs("LIST NEWSGROUPS\r\n", fsocket);
    fflush(fsocket);
#ifdef DEBUG
    printf(">> LIST NEWSGROUPS\n");
#endif
    if (!sofgets(snntp, sizeof(snntp), fsocket))
	lexit(1);
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='2') {
	log("server response to 'LIST NEWSGROUPS': %s", snntp);
	return;
    }
    ng=fsocket;
#else
    if ((ng = fopen(NEWSGROUPS, "r")) == NULL)
	return;
#endif
while (sofgets(line, sizeof line, ng) != NULL) {
    if ((p = strpbrk(line, "\n\r")) != NULL)
	*p = '\0';
#ifdef NNTP_ONLY
    if (strcmp(line, ".")==0) {
#ifdef DEBUG
	printf("<< ...\n<< .\n");
#endif
	break;
    }
#endif
    p = line;
    while (*p && ISspace(*p))	p++;
    name=p;
    while (*p && !ISspace(*p)) p++;
    *p++=0;
    while (*p && ISspace(*p))	p++;
    descr=p;
    for (ap = active; ap != NULL; ap = ap->next) {
	if (strcmp(name, ap->name) == 0) {
	    ap->description=malloc(strlen(descr)+1);
	    strcpy(ap->description,descr);
	    break;
	}
    }/*for*/
}
#ifndef NNTP_ONLY
fclose(ng);
#else
if (strcmp(line, "."))
{
	lexit(1);
}
#endif
newsgroups_readed=1;
}

#define L_LIST 0
#define L_ILIST 1
#define L_ULIST 2

void list(of, pp, flag)
FILE *of;
char *pp;
int flag;
{
    char word[160];
    char dnam[200];
    char *descr;
    FILE *ng;
    int do_g = 0, ab = 0, ae = 0, i, n;
    register struct aentry *ap;
    struct _hie {
	ushort size;
	ushort neg;
	int cnt;
	struct _hie *next;
	char name[1];
    }
    *hies = NULL, *wrk;
#ifndef NNTP_ONLY
    struct dirent *dirs;
    DIR *fdir;
    register char *q;
    char ngg[80];
#endif

    while ((pp = nextword(pp, word, sizeof(word))) != NULL) {
	if (cmdcmp(word, "ALL")) {
	    wrk = (struct _hie *) malloc(sizeof(struct _hie));
	    wrk->neg = 0;
	    wrk->size = 0;
	    wrk->cnt = 0;
	    wrk->next = hies;
	    wrk->name[0] = 0;
	    hies = wrk;
	    continue;
	} {
	    register char *p = word;
	    while (*p) {
		if (*p >= 'A' && *p <= 'Z')
		    *p += 32;
		p++;
	    }
	}
	i = strlen(word);
	wrk = (struct _hie *) malloc(sizeof(struct _hie) + i);
	if (*word == '!')
	    wrk->neg = 1;
	else
	    wrk->neg = 0;
	wrk->size = i - wrk->neg;
	wrk->cnt = 0;
	strcpy(wrk->name, word + wrk->neg);
	wrk->next = hies;
	hies = wrk;
    }

    if (hies != NULL) {
	read_newsgroups();
	for (ap = active; ap != NULL; ap = ap->next) {
		for (wrk = hies, do_g = 0; wrk != NULL; wrk = wrk->next) {
		    if (wrk->size == 0) {
			wrk->cnt++;
			do_g = 1;
			continue;
		    }
		    /* gul */
		    if (strpbrk(wrk->name, "[]?*")) {
			if (wildmat(ap->name, wrk->name)) {
			    if (wrk->neg) {
				do_g = 0;
			    } else
			      do_g = 1;
			    wrk->cnt++;
			}
			continue;
		    }
		    if (wrk->size > ap->size)
			continue;
		    if (ap->name[wrk->size] != '.' && ap->name[wrk->size] != 0)
			continue;
		    if (wrk->neg && strncmp(ap->name, wrk->name, wrk->size) == 0) {
			do_g = 0;
			wrk->cnt++;
			// break;
			continue;
		    }
		    if (strncmp(ap->name, wrk->name, wrk->size) == 0) {
			do_g = 1;
			wrk->cnt++;
			continue;
		    }
		}

		if (flag == L_ULIST && do_g) 
		    if(issubs(replyaddr,ap->name))
			continue;
		if (!do_g)
		    continue;
		if (flag == L_ILIST) {
#ifdef NNTP_ONLY
			connect_nnrpd();
			fprintf(fsocket, "GROUP %s\r\n", ap->name);
			fflush(fsocket);
#ifdef DEBUG
			printf(">> GROUP %s\n", ap->name);
#endif
			if (!sofgets(snntp, sizeof(snntp), fsocket))
			    lexit(1);
#ifdef DEBUG
			printf("<< %s", snntp);
#endif
			if (snntp[0]=='2')
			    sscanf(snntp+4, "%u %u %u", &n, &ab, &ae);
#else
			strncpy(ngg, ap->name, sizeof(ngg));
			q = ngg;
			while ((q = index(q, '.')) != NULL) *q = '/';
#if defined(USG) || defined(SYSV)
			if((q = rindex(ngg,'/'))!=NULL)
			    q[15]=0;
#endif
			ab = ae = 0;
			snprintf(dnam, sizeof(dname), "%s/%s", NEWSSPOOL, ngg);
			fdir = opendir(dnam);
			while ((dirs = readdir(fdir)) != NULL) {
			    n = (int) strtol(dirs->d_name, (char **) NULL, 10);
			    if (n == 0)
				continue;
			    if (ab == 0 || ab > n)
				ab = n;
			    if (ae == 0 || ae < n)
				ae = n;
			}
			closedir(fdir);
#endif
		}
	    if(ap->description==NULL)
	    	descr="???";
	    else
	    	descr=ap->description;
	    if (flag == L_ILIST)
		if(ab==0 && ae==0)
		if(ap->flag&GF_NNTP)
		fprintf(of, "%s%s[ *UNSUBS* ]\t%s\n", ap->name,
			ap->size > 31 ? "\t" : ap->size > 23 ? "\t\t" : ap->size > 15 ? "\t\t\t" : ap->size > 7 ? "\t\t\t\t" : "\t\t\t\t\t",
			subsplit(&descr, 22));
		else
		fprintf(of, "%s%s[ *EMPTY* ]\t%s\n", ap->name,
			ap->size > 31 ? "\t" : ap->size > 23 ? "\t\t" : ap->size > 15 ? "\t\t\t" : ap->size > 7 ? "\t\t\t\t" : "\t\t\t\t\t",
			subsplit(&descr, 22));
		else
		fprintf(of, "%s%s[%d - %d]%s%s\n", ap->name,
			ap->size > 31 ? "\t" : ap->size > 23 ? "\t\t" : ap->size > 15 ? "\t\t\t" : ap->size > 7 ? "\t\t\t\t" : "\t\t\t\t\t",
			ab, ae, (ab>100000||ae>100000)?" ":"\t",
			subsplit(&descr, 22));
	    else
		fprintf(of, "%s\t%s%s\n", ap->name,
			ap->size > 31 ? "" : ap->size > 23 ? "\t" : ap->size > 15 ? "\t\t" : ap->size > 7 ? "\t\t\t" : "\t\t\t\t",
			subsplit(&descr, 38));
		    while (descr != NULL)
		fprintf(of, ">\t\t\t\t\t%s\n", subsplit(&descr, 38));
	}
	do {
	    wrk = hies;
	    hies = wrk->next;
	    if (!wrk->cnt)
		fprintf(of, mesg(46, lang), wrk->name);
	    free(wrk);
	} while (hies != NULL);
    } else {
	sprintf(dnam, HELP_LIST, ext[lang]);
	if ((ng = fopen(dnam, "r")) != NULL) {
	    while ((i = getc(ng)) != EOF)
		fputc(i, of);
	    fclose(ng);
	} else if (flag == L_ILIST)
	    fprintf(of, "Use ILIST ALL or ILIST HIERACIES command\n");
	else if (flag == L_ULIST)
	    fprintf(of, "Use ULIST ALL or ULIST HIERACIES command\n");
	else
	    fprintf(of, "Use LIST ALL or LIST HIERACIES command\n");

    }
}

#ifndef NNTP_ONLY
char *
 find_msgid(msgid)
char *msgid;
{
    FILE *hfil;
    char iline[256];
    static char art[200];
    register char *p, *q;
    int l;
#ifdef USE_DBZ
    long ofs;
#endif

    if ((hfil = fopen(HISTORY, "r")) == NULL)
	return NULL;

    l = strlen(msgid);
#ifdef USE_DBZ
nseek:
if((ofs=get_offset(msgid))==NULL){
	closehist();
	fclose(hfil);
	return NULL;}
if(fseek(hfil,ofs,SEEK_SET)){
	closehist();
	fclose(hfil);
	return NULL;}
fgets(iline, sizeof iline, hfil);
if (strncmp(iline, msgid, l) == 0){
	closehist();
	fclose(hfil);
	goto found;}
else
	goto nseek;
#else
    while (fgets(iline, sizeof iline, hfil) != NULL) {
	if (strncmp(iline, msgid, l) == 0)
	    goto found;
    }
    fclose(hfil);
    return NULL;
#endif
  found:
    fclose(hfil);
    q = p = iline;
    {
	register s = 2;
	while (s) {
	    while (!ISspace(*p)) p++;
	    while (ISspace(*p)) p++;
	    s--;
	}
    }
    while (!ISspace(*p) && *p && *p !='\n')
	*q++ = *p++;
    *q = 0;
    p = iline;
    while ((p = index(p, '.')) != NULL)
	*p = '/';
    sprintf(art, "%s/%s", NEWSSPOOL, iline);
    return art;
}

static int issubstr(line, shb)
char *line;
struct shablon *shb;
{
    char *p;
    unchar s;
    register struct shablon *i;

    p = line;
    do {
	for (i = shb; i; i = i->next) {
	    register k = 0;
	    register unsigned char *q = (unsigned char *)p;

	    switch (i->body[k]) {
	    case 0:
		return 0;
	    case '?':
		break;
	    case '\\':
		k++;
		if ((s=letters[i->uncase][i->body[k]])?(letters[i->uncase][*q]==s):(*q == i->body[k]))
		continue;
		else
		break;
	    case '[':
	      loop:
		if (i->body[++k] == ']')
		    continue;
		if (i->body[k] == '\\')
		    k++;
		if (i->body[k + 1] == '-') {
		    if ((s=letters[i->uncase][i->body[k]])?
((letters[i->uncase][*q] >= s) && (letters[i->uncase][*q] <= letters[i->uncase][i->body[k + 2]])):
(*q >= i->body[k] && *q<=i->body[k+2])){
			while (i->body[++k] != ']');
			break;
		    } else {
			k += 2;
			goto loop;
		    }
		} else if ((s=letters[i->uncase][i->body[k]])?(letters[i->uncase][*q] == s):(*q==i->body[k])) {
		    while (i->body[++k] != ']');
		    break;
		} else
		    goto loop;
	    default:
		if ((s=letters[i->uncase][i->body[k]])?(letters[i->uncase][*q] == s):(*q==i->body[k]))
		    break;
		else
		    continue;
	    }

	    while (*q && i->body[k]) {
		if ((s=letters[i->uncase][i->body[++k]])?
(letters[i->uncase][*++q]!=s):(*++q != i->body[k])) {
		    switch (i->body[k]) {
		    case 0:
			return 1;
		    case '?':
			continue;
		    case '\\':
			k++;
			if ((s=letters[i->uncase][i->body[k]])?(letters[i->uncase][*q]==s):(*q == i->body[k]))
			continue;
			else
			break;
		    case '[':
		      cloop:
			if (i->body[++k] == ']')
			    break;
			if (i->body[k] == '\\')
			    k++;
			if (i->body[k + 1] == '-') {
		    if ((s=letters[i->uncase][i->body[k]])?
((letters[i->uncase][*q] >= s) && (letters[i->uncase][*q] <= letters[i->uncase][i->body[k + 2]])):
(*q >= i->body[k] && *q<=i->body[k+2])){
				while (i->body[++k] != ']');
				continue;
			    } else {
				k += 2;
				goto cloop;
			    }
			} else if ((s=letters[i->uncase][i->body[k]])?(letters[i->uncase][*q] == s):(*q==i->body[k])) {
			    while (i->body[++k] != ']');
			    continue;
			} else
			    goto cloop;

		    }
		    break;
		}
	    }

	}
    } while (*++p);
    return 0;
}

FILE *digest;
int d_size,d_group;

static void
start_digest()
{
char lhead[2048];

d_size=0;
d_group=0;

    sprintf(tmpbuf,"/tmp/digest_%d",getpid());
    if((digest=fopen(tmpbuf,"w+"))==NULL){
    	log("ERR: Can't open tmp file for digest\n");
    	return;
    }
    unlink(tmpbuf);
    /*
     * make the header of the message
     */
    sprintf(iline, "Sender: %s@%s\n", FINDSENDER, hn);
    strcpy(lhead,iline);
#ifdef USE_XCLASS
    sprintf(iline, "X-Class: slow\n");
    strcat(lhead,iline);
#endif
#ifdef USE_PRECEDENCE
    sprintf(iline, "Precedence: %s\n", PRECEDENCE);
    strcat(lhead,iline);
#endif
    sprintf(iline, "From: News Mailing Service <%s@%s>\n", NEWSUSER, hn);
    strcat(lhead,iline);
    /* gul */
    sprintf(iline,"Reply-To: %s@%s\n", NEWSUSER, hn);
    strcat(lhead,iline);
    sprintf(iline,"Errors-To: %s@%s\n", FINDSENDER, hn);
    strcat(lhead,iline);
    sprintf(iline, "Date: %%s\n");
    strcat(lhead,iline);
    sprintf(iline, "To: %%s\n");
    strcat(lhead,iline);
    sprintf(iline, "Subject: Requested digest of USENET articles\n\n");
    strcat(lhead,iline);
    d_size += fprintf(digest, lhead, adate(), replyaddr);
}

static int
mail_digest()
{
int status,i;

    	fflush(digest);
    	fseek(digest,0L,SEEK_SET);
    	/* mail digest */
    sprintf(sname, "%s@%s", FINDSENDER, hn);
    av[0] = MAILER;
    av[1] = SENDER_FLAG;
    av[2] = sname;
    av[3] = replyaddr;
    av[4] = (char *) NULL;

    switch(i = fork()){
    case 0:
	/* new proc */
	close(0);
	dup(fileno(digest)); /* should be 0 ;-) */
	for (i = 1; i <= _NFILE; i++)
		close(i);
	open("/dev/null", 1);	/* fd 1 */
	dup(1);			/* fd 2 */

	execv(MAILER, av);
	exit(1);
    case -1:
    	fclose(digest);
	return 0;
    default:
#if defined(_POSIX_SOURCE) || defined(ISC)
	waitpid(i, &status, 0);
#else
	wait(&status);
#endif
    }
	fclose(digest);
	if(!status)
	    log("Sent digest to %s\n", replyaddr);
	else
	    return 0;
	return 1;
}

#endif /* not NNTP_ONLY */

void printsize(char *m_size, long size)
{
	if (size < 1000)
	    sprintf(m_size, "%3ld", size);
	else if (size < 10000)
	    sprintf(m_size, "%1ld.%1ldK", size / 1024, ((size * 10) / 1024) % 10);
	else
	    sprintf(m_size, "%3ldK", size / 1024);
}

void getaddr(char *m_from, char *iline, int size)
{
    char *p, *q;
    int  incomment=0;

    for (p=iline, q=m_from; *p; p++)
    {
	if (*p=='(')
	{   incomment++;
	    continue;
	}
	if (*p==')' && incomment)
	{   incomment--;
	    continue;
	}
	if (incomment) continue;
	*q++=*p;
    }
    *q='\0';

    if ((p=index(m_from, '<'))!=NULL)
    {
	strcpy(m_from, p+1);
	if ((p=index(m_from, '>'))!=NULL)
	    *p='\0';
    }
    for (p=m_from; *p && isspace(*p); p++);
    if (p!=m_from) strcpy(m_from, p);
    if (m_from[0]=='\0') return;
    for (p=m_from+strlen(m_from)-1; isspace(*p); *p--='\0');
}

void putarttolist(char *group, int m_number, char *m_size,
                  char *m_from, char *m_subject, int find)
{
	FILE *art;

	sprintf(iline, "%s/%s", WORKDIR, find ? FINDLIST : INDEXLIST);

	if ((art = fopen(iline, "a")) == NULL) {
#ifdef DEBUG
	    printf("Can't open %s: %s\n", iline, strerror(errno));
	    fflush(stdout);
#endif
	    bexit(1);
	}
	fprintf(art, "%s %s %d %s %s %s\n",
	    replyaddr, group, m_number,
	    m_size, m_from, m_subject);
	fclose(art);
}	

#ifndef NNTP_ONLY

int ind_art(of, pp, m_number, group, shbs, find)
FILE *of;
char *pp;
int m_number;
char *group;
struct shablon *shbs;
int find;
{
    char m_from[1024];
    char m_subject[8192],*sub;
    char m_size[7];
    char iline[1024];
#if 0 /* gul */
    struct a_seen *wseen_ptr;
#endif
    int ff, fs, size, ok, body;
    long body_beg=0; /* initialize to satisfy compiler */
    FILE *art;
    register char *p, *q;

#ifdef DEBUG
    printf("ind_art started, artname=%s\n", pp);
    fflush(stdout);
#endif
    if ((art = fopen(pp, "r")) == NULL) {
#ifdef DEBUG
        printf("Can't open art: %s\n", strerror(errno));
        fflush(stdout);
#endif
	return 0;
    }
#ifdef DEBUG
    printf("fileno(art)=%d\n",fileno(art));
    fflush(stdout);
#endif
    body = ok = fs = ff = 0;
    if (!find)
	ok = 1;
    while (fgets(iline, sizeof iline, art) != NULL) {
next_line:
	if(!body){
	if (iline[0] == '\n') {
	    body = 1;
	    body_beg = ftell(art);
	    continue;
	}
	if (fieldcmp(iline, "from")) {
	    getaddr(m_from, iline+6, sizeof(m_from));
	    ff = 1;
	}
	if (fieldcmp(iline, "subject")) {
	    p = iline + 9;
	    q = m_subject;
s_cont:
	    while (ISspace(*p))	p++;
	    while (*p && ((int) (q - m_subject)) < sizeof m_subject - 2) *q++ = *p++;
	    *q = 0;
	    if ((p = index(m_subject, '\n')) != NULL)
		*p = ' ';
	    if (fgets(iline, sizeof iline, art) != NULL)
		if (ISspace(iline[0])){
		    p=iline;
		    goto s_cont;
		}
		else{
		    fs=1;
		    if (find != 0)
			if (issubstr(m_subject, shbs))
			    ok = 1;
		    goto next_line;
		}
	}
	}
	if ((find == C_FIND || find == C_DIGEST) && body)
	    if (issubstr(iline, shbs))
		ok = 1;
	if (ff && fs && ok && body)
	    break;
    }

    if (!ok){
	fclose(art);
#ifdef DEBUG
	printf("!ok, return 0\n");
        fflush(stdout);
#endif
	return 0;
    }
    
    fseek(art, 0L, SEEK_END);
    size = ftell(art);

#if 0 /* gul */
    {
	register int len,hash,fl;

	len=strlen(m_subject);
	hash=strhash(m_subject);
	fl=(*((int *)&m_subject[len-4]));

    	/* check already seen */
	for(wseen_ptr=seen_ptr;wseen_ptr;wseen_ptr=wseen_ptr->next)
		if((wseen_ptr->len == len) && 
			(wseen_ptr->hash == hash) && (wseen_ptr->fl == fl))
	{		fclose(art);
#ifdef DEBUG
			printf("already seen, return 0\n");
    			fflush(stdout);
#endif
			return 0;
	}

    	/* if new append to list */
    	wseen_ptr=(struct a_seen *)calloc(1,sizeof(struct a_seen));
    	wseen_ptr->len=len;
    	wseen_ptr->hash=hash;
    	wseen_ptr->fl=fl;
    	wseen_ptr->next=seen_ptr;
    	seen_ptr=wseen_ptr;
    }
#endif
    
    if( find == C_FIND || find == C_SFIND || !find ){
	fclose(art);
	printsize(m_size, size);
	putarttolist(group, m_number, m_size, m_from, m_subject, find);
    }
    else{ /* make digest */
	if( !digest )
	    start_digest();

	if(!d_group){
	    d_group=1;
	    d_size += fprintf(digest,"GROUP %s\n",group);
	}
	d_size += fprintf(digest,"ART %d from %s\n",m_number,m_from);
	sub = m_subject;
	while (sub != NULL)
	    d_size += fprintf(digest, ">\t%s\n", subsplit(&sub, 65));
    	fseek(art, body_beg, SEEK_SET); /* write body */
	while (fgets(iline, sizeof iline, art) != NULL) {
	    d_size += fprintf(digest,"# %s",iline);
    	}
    	fclose(art);
	if(d_size > limit ){
	    if(!mail_digest()) {
#ifdef DEBUG
		printf("mail_digest error\n");
    		fflush(stdout);
#endif
	    	return -1;
	    }
	    start_digest();
	    d_group=0;
	}
    }
    if(find)
    	find_art++;
    else
    	index_art++;
#ifdef DEBUG
    printf("Ok, find_art=%d\n", find_art);
    fflush(stdout);
#endif
    return 1;
}

#endif /* not NNTP_ONLY */

int do_index(of, pp, find)
char *pp;
FILE *of;
int find;
{
    char word[80];
    int ab = 0, ae = 0, an = 0, acnt = 0, ecnt = 0, cnt = 0, do_g;
    struct shablon *ws, *shbs = NULL,*hies = NULL;
    long atime=0;
    register struct aentry *ap;
    struct aeptr {
	struct aentry *ptr;
	struct aeptr *next;
    } *ilist=NULL,*ilast=NULL;
    char *p;
#ifdef NNTP_ONLY
    char m_from[1024], m_subj[8192], m_size[12], m_date[128];
    int  m_number;
    char *q;
    long size;
    char *montab[]={"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
#else
    struct stat s_buf;
    DIR *fdir;
    struct dirent *dirs;
    char line[200];
    char ngg[160];
    int  i;
#endif

    seen_ptr = NULL;
#ifndef NNTP_ONLY
    digest = NULL;
    
    if (find) {
	while ((pp = nextword(pp, word, sizeof(word))) != NULL) {
	    if(*word=='@')
	    	break;
	    if (shbs == NULL) {
		shbs = (struct shablon *) calloc(1, sizeof(struct shablon));
		if(*word=='*'){
		strncpy(shbs->body, word+1, sizeof(shbs->bosy));
		shbs->uncase=1;
		}
		else{
		strncpy(shbs->body, word, sizeof(shbs->body));
		shbs->uncase=0;
		}
	    } else {
		ws = (struct shablon *) calloc(1, sizeof(struct shablon));
		if(*word=='*'){
		    strncpy(ws->body, word+1, sizeof(ws->body));
		    ws->uncase=1;
		}
		else{
		    strncpy(ws->body, word, sizeof(ws->body));
		    ws->uncase=0;
		}
		ws->next = shbs;
		shbs = ws;
	    }
	}
	if(pp==NULL){
	    fprintf(of, mesg(2, lang), word);
	    return -1;
	}	
    }
#endif

	while ((pp = nextword(pp, word, sizeof(word))) != NULL) {
	    if(*word=='-' || (*word > '0' && *word < '9') ){
		acnt = (int) strtol(word, &p, 10);
		if(acnt<0){
			if(*p=='h')
				atime=time((long *)NULL)+acnt*3600;
			else if(*p=='d')
				atime=time((long *)NULL)+acnt*86400;
			if(atime<0)
				atime=0;
		}
	    	break;
	    }
#ifdef NOTALL /* gul */
	    if (strcmp(word,"*")==0)
		continue;
#endif
	    if (hies == NULL) {
		hies = (struct shablon *) calloc(1, sizeof(struct shablon));
		if(*word=='!'){
		    strncpy(hies->body, word+1, sizeof(hies->body));
		    hies->uncase=1;
		}
		else{
		    strncpy(hies->body, word, sizeof(hies->body));
		    hies->uncase=0;
		}
	    } else {
		ws = (struct shablon *) calloc(1, sizeof(struct shablon));
		if(*word=='!'){
		    strncpy(ws->body, word+1, sizeof(ws->body));
		    ws->uncase=1;
		}
		else{
		    strncpy(ws->body, word, sizeof(ws->body));
		    ws->uncase=0;
		}
		ws->next = hies;
		hies = ws;
	    }
	}
	if (acnt>0 && (pp = nextword(pp, word, sizeof(word))) != NULL)
	    ecnt = (int) strtol(word, (char **) NULL, 10);

	for (ap = active; ap; ap = ap->next) {
		for(do_g=0,ws=hies;ws;ws = ws->next){
		if (wildmat(ap->name, ws->body)) {
		    if(ws->uncase){
			do_g=0;
		    	break;
		    }
		    if ((ap->flag & GF_SONLY)&&
			!issubs(replyaddr, ap->name)) {
			fprintf(of, mesg(20, lang), ap->name);
			do_g=0;
			break;
		    }
		    do_g=1;
		}
		}
		if(do_g){
		    cnt++;
		    if(ilist==NULL){
		    	ilist=ilast=(struct aeptr *)calloc(1,sizeof(struct aeptr));
		    	ilist->ptr=ap;
		    	ilist->next=NULL;
		    }
		    else{
		    	ilast->next=(struct aeptr *)calloc(1,sizeof(struct aeptr));
		    	ilast->next->ptr=ap;
		    	ilast->next->next=NULL;
		    	ilast=ilast->next;
		    }
		}
	}
	if(!cnt){
	    fprintf(of, mesg(2, lang), word);
	    return -1;
	}
	if(cnt>1)
	    if(acnt>0)
	    	acnt=ecnt=0;
	cnt=0;
	if(acnt<0)
		an=acnt;

    for(ilast=ilist;ilast;acnt=ecnt=0,ilast=ilast->next){
    ab=ae=0;	
    if(an<0)
	acnt=an;

#ifdef NNTP_ONLY
    connect_nnrpd();
    fprintf(fsocket, "GROUP %s\r\n", ilast->ptr->name);
    fflush(fsocket);
#ifdef DEBUG
    printf(">> GROUP %s\n", ilast->ptr->name);
#endif
    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
	lexit(1);
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='2')
 	continue;	/* should never happen */
    for (p=snntp+3; *p && isspace(*p); p++);
    for (;*p && isdigit(*p); p++); /* number of articles */
    for (;*p && isspace(*p); p++);
    ab=atol(p);
    for (;*p && isdigit(*p); p++); /* first article */
    for (;*p && isspace(*p); p++);
    ae=atol(p);
    if (!isdigit(*p)) continue;

#else

    strcpy(word, ilast->ptr->name);
    strcpy(ngg, ilast->ptr->name);
 
    p = word;
    while ((p = index(p, '.')) != NULL)
	*p = '/';
    sprintf(line, "%s/%s", NEWSSPOOL, word);
    fdir = opendir(line);
    if (fdir != NULL) {
	while ((dirs = readdir(fdir)) != NULL) {
		i = (int) strtol(dirs->d_name, (char **) NULL, 10);
		if (i == 0)
			continue;
		if(atime){
			sprintf(line, "%s/%s/%d", NEWSSPOOL, word, i);
			if(stat(line,&s_buf)!=0)
				continue;
			if(s_buf.st_mtime<atime)
				continue;
		}
		if (ab == 0 || ab > i)
			ab = i;
		if (ae == 0 || ae < i)
			ae = i;
	}
	closedir(fdir);
    }

#endif

    if (acnt < 0 && atime==0) {
	ecnt = ae;
	acnt += (ae + 1);
    }
    if (acnt < ab)
	acnt = ab;
    if (ecnt > ae || ecnt == 0)
	ecnt = ae;

#ifdef DEBUG
#ifndef NNTP_ONLY
    printf("Search group %s\n", ngg);
    fflush(stdout);
#endif
#endif

#ifdef NNTP_ONLY
    fprintf(fsocket, "XOVER %d-%d\r\n", acnt, ecnt);
    fflush(fsocket);
#ifdef DEBUG
    printf(">> XOVER %d-%d\n", acnt, ecnt);
#endif
    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
	lexit(2);
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='2') continue;
    while (sofgets(snntp, sizeof(snntp), fsocket))
    {   struct tm t;

	if ((p=strchr(snntp, '\r')) != NULL) {
	    if (strcmp(p, "\r\n")==0)
		strcpy(p, "\n");
	    else if (p[1]=='\0')
		p[0]='\0';
	}
	if (strcmp(snntp, ".\n")==0)
	    break;
	/* art subj from date size lines xref */
	if (!isdigit(snntp[0]))
	    continue;
	m_number=atol(snntp);
subj_again:
	for (p=snntp; *p && *p!='\t' && *p!='\n'; p++);
	if (*p=='\n') continue;
	if (*p=='\0')
	{   if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    goto subj_again;
	}
	q=++p;
	m_subj[0]='\0';
subj_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    strncpy(m_subj+strlen(m_subj), q, sizeof(m_subj)-strlen(m_subj)-1);
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=q=snntp;
	    goto subj_cont;
	}
	else if (*p=='\n') continue;
        *p++='\0';
	strncpy(m_subj+strlen(m_subj), q, sizeof(m_subj)-strlen(m_subj)-1);
	q=p;
	m_from[0]='\0';
from_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    strncpy(m_from+strlen(m_from), q, sizeof(m_from)-strlen(m_from)-1);
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=q=snntp;
	    goto from_cont;
	}
	else if (*p=='\n') continue;
        *p++='\0';
	strncpy(m_from+strlen(m_from), q, sizeof(m_from)-strlen(m_from)-1);
	q=p;
	m_date[0]='\0';
date_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    strncpy(m_date+strlen(m_date), q, sizeof(m_date)-strlen(m_date)-1);
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=q=snntp;
	    goto date_cont;
	}
	else if (*p=='\n') continue;
        *p++='\0';
	strncpy(m_date+strlen(m_date), q, sizeof(m_date)-strlen(m_date)-1);
msgid_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=snntp;
	    goto msgid_cont;
	}
	if (*p++=='\n') continue;
ref_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=snntp;
	    goto ref_cont;
	}
	if (*p++=='\n') continue;
	q=p;
	m_size[0]='\0';
size_cont:
	p=strpbrk(p, "\n\t");
	if (p==NULL)
	{
	    strncpy(m_size+strlen(m_size), q, sizeof(m_size)-strlen(m_size)-1);
	    if (!sofgets(snntp, sizeof(snntp), fsocket)) lexit(1);
	    p=q=snntp;
	    goto size_cont;
	}
	else if (*p=='\n') continue;

        *p++='\0';
	strncpy(m_size+strlen(m_size), q, sizeof(m_size)-strlen(m_size)-1);
	q=p;

	while (strchr(q, '\n')==NULL)
	{   if (!sofgets(snntp, sizeof(snntp), fsocket))
		lexit(1);
	    p=snntp;
	}

	if ((size=atol(m_size))==0) continue;

	/* parse date */
	/* Fri, 14 Aug 1998 16:41:27 +0300 */
	for (p=m_date; *p && !isdigit(*p); p++);
	if (atime && sscanf(p, "%d %s %d %d:%d:%d %s",
	    &t.tm_mday, iline, &t.tm_year,
	    &t.tm_hour, &t.tm_min, &t.tm_sec, snntp)==7)
	{
	    if (t.tm_year>1900) t.tm_year-=1900;
	    if (t.tm_year<50) t.tm_year+=100;
	    for (t.tm_mon=0; t.tm_mon<12; t.tm_mon++)
		if (strcmp(montab[t.tm_mon], iline)==0)
		    break;
	    if (t.tm_mon<12)
	    {   time_t tt;

		t.tm_isdst=0;
		tt=mktime(&t)+timezone;
		if (snntp[0]=='-')
		    tt-=(atoi(snntp+1)/100)*3600;
		else if (snntp[0]=='+')
		    tt+=(atoi(snntp+1)/100)*3600;
		if (tt<atime) continue;
	    }
	}

	getaddr(m_from, m_from, sizeof(m_from));
	printsize(m_size, size);
	putarttolist(ilast->ptr->name, m_number, m_size, m_from, m_subj, 0);
    	index_art++;
	cnt++;
    }
    if (strcmp(snntp, ".\n"))
    {
	lexit(1);
    }
#ifdef DEBUG
    printf("<< ...\n<< .\n");
#endif

#else /* not NNTP_ONLY */

    for (i = ((find == C_DIGEST || find == C_SDIGEST)?acnt:ecnt);
	((find == C_DIGEST || find == C_SDIGEST)?i <= ecnt:i >= acnt);
	((find == C_DIGEST || find == C_SDIGEST)?i++:i--)) {
	sprintf(word, "%d", i);
	if (access((p = gfn(ngg, word)), R_OK) == 0) {
	    ab = ind_art(of,p,i, ngg, shbs, find);
#ifdef DEBUG
	    printf("ind_art exit code %d\n", ab);
	    fflush(stdout);
#endif
	    if (ab < 0)
		return -1;
	    else if (ab == 1)
		cnt++;
	}
    }

#ifdef DEBUG
    printf("Group %s done, do next group\n", ngg);
#endif

#endif /* not NNTP_ONLY */
    }
    
	/* free memory */
    if(ilist)
	ilast=ilist->next;
    while(ilist){
    	free(ilist);
    	ilist=ilast;
    	if(ilast)
 	    ilast=ilast->next;
    }
    if(hies)
	ws=hies->next;
    while(hies){
    	free(hies);
    	hies=ws;
    	if(ws)
  	    ws=ws->next;
    }
    if(shbs)
	ws=shbs->next;
    while(shbs){
    	free(shbs);
    	shbs=ws;
   	if(ws)
  	    ws=ws->next;
     }
    if(seen_ptr)
    	ws=(struct shablon *)seen_ptr->next;
    while(seen_ptr){
    	free(seen_ptr);
    	seen_ptr=(struct a_seen *)ws;
    	if(ws)
    	    ws=(struct shablon *)seen_ptr->next;
    }
#ifndef NNTP_ONLY
    if(digest){
	if(!mail_digest())
	    cnt=0;
    }
#endif
    if (cnt)
	return 0;
    else
	return 2;
}

/*
 * Read the active file into memory
 */
static struct aentry *ap_last;

static int
_add_active(entry,parm,name)
long entry;
int *parm;
char *name;
{
    register struct aentry *ap;

	if(fseek(fb,entry,SEEK_SET)){
serr:
	bserrno=BS_SYSERR;
	return 1;}
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;
			
	if((ap=(struct aentry *)malloc(sizeof(struct aentry)+strlen(name)))!=NULL){
	strcpy(ap->name, name);
	ap->flag = gr_entry.flag;
	ap->size = gr_entry.size;
	ap->description = NULL;
	ap->next = NULL;

	if(active==NULL)
		active = ap;
	else
		ap_last->next=ap;
	ap_last=ap;
	return 0;
	}
	else
	return 1;
}

void readactive()
{
    active = NULL;

    if(open_base(OPEN_RDWR))
	exit(1);

	if(get_gtree(header.group,_add_active,0,0,(char *)NULL))
		goto err;
    return;
err:
    bexit(1);
}

static int
check_usite(char *usite)
{
    char chk_buf[80];
    struct stat st_buf;

    snprintf(chk_buf,sizeof(chk_buf),"%s/%s",OUTGO_DIR,usite);
    if(stat(chk_buf,&st_buf))
	return 0;
    if((st_buf.st_mode&(S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR))==(S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR))
	return 1;
    else
	return 0;
}

void subs_parse(pp, mode, of)
FILE *of;
char *pp;
int mode;
{
    char word[160];
    int param,i;
    char *grps[20];

    param = 0;
    if (mode == RFEED) {
	pp = nextword(pp, word, sizeof(word));
	if ((param = atoi(word)) == 0) {
	    fprintf(of, "First parameter must be a digit\n");
	    return;
	}
    }
    for(i=0;i< 20;i++){
	while(ISspace(*pp) || (*pp==',')) *(pp++)=0;
	grps[i]=pp;
	if((pp=nextword(pp,word,sizeof(word)))==NULL){
		grps[i]=(char *)NULL;
		break;}
    }
#if 1 /* gul */
    grps[19]=(char *)NULL;
    if (pp!=(char *)NULL) *pp=0;
#endif
	usite_ok=strlen(usite);
	if(usite_ok)
	    usite_ok=strncmp(usite,replyaddr,usite_ok)?0:1;
	else if(mode==CNEWS){
	    fprintf(of,mesg(55,lang));
	    return;
	}
	if(usite_ok)
	    usite_ok=check_usite(usite);
	if(!usite_ok && mode==CNEWS){
	    fprintf(of,mesg(56,lang));
	    return;
	}
	subscribe(replyaddr, grps, mode, param, of);
}

/*
 * Strip out RFC822 comments
 */
char *stripcomm(s)
register char *s;
{
    int plv = 0;
    static char buf[300];
    register char *p;

    p = buf;
    while (*s) {
	if (*s == '(')
	    plv++;
	else if (plv > 0 && *s == ')')
	    plv--;
	else if (plv == 0)
	    *p++ = *s;
	s++;
	if (p-buf >= sizeof(buf)-2) break;
    }
    *p = '\0';
    return buf;
}

/*
 * Submit an article, never returns
 */
void submit(list)
char *list;
{
    register struct aentry *ap;
    char *p, *q, *r;
    register c;
    int nlines, status, i, com, ncom, cnt, xpd;
#ifdef M_LIST
    int mod = 0;
#endif
    FILE *of, *iff;
    char tmpname[80];
    char tmpbuf[512];
    int pfd[2];
    long now;
    struct stat sfb;
    struct tm *t;
#ifdef CXPOST
    struct xh {
	char name[15];
	struct xh *next;
    } *xch = NULL, *wch;
#endif
#ifdef NNTP_ONLY
    int waseol;
#endif

    sprintf(sname, "%s@%s", NEWSSENDER, hn);
    /*
	 * Copy the input into temp file
	 */
    tmpbuf[0] = '\0';
    sprintf(tmpname, "/tmp/M2NS.%d\n", getpid());
    if ((of = fopen(tmpname, "w")) == NULL) {
      tf_err:
	r = mesg(12, lang);
	iff = NULL;
	unlink(tmpname);
	goto report_error;
    }
    if ((iff = fopen(tmpname, "r")) == NULL)
	goto tf_err;
    unlink(tmpname);		/* A Unix kludge */
    nlines = 0;
    while ((c = getchar()) != EOF) {
	if (c == '\n')
	    nlines++;
	putc(c, of);
    }
    fclose(of);

    /*
	 * Parse the Newsgroups: contents
	 */
    if ((p = index(list, '\n')) != NULL)
	*p = '\0';
    p = list;
    q = tmpbuf;
    cnt = xpd = com = ncom = 0;
    while ((p = nextword(p, iline, sizeof(iline))) != NULL) {
	cnt++;
	/* Try to find a group */
	for (ap = active; ap != NULL; ap = ap->next) {
	    if (strcmp(iline, ap->name) == 0) {
#ifdef M_LIST
		mod = 0;
		{
		    register FILE *ml;
		    if ((ml = fopen(M_LIST, "r")) != NULL) {
			while (fgets(tmpname, sizeof tmpname, ml) != NULL) {
			    if (*tmpname == '#' || *tmpname == '\n')
				continue;
			    if ((r = index(tmpname, '\n')) != NULL)
				*r = 0;
			    r = tmpname;
			    while (*r && !ISspace(*r))	r++;
			    *r++ = 0;
			    while (*r && ISspace(*r)) r++;
			    if (adrcmp(replyaddr, tmpname) == 0 && belongs(ap->name, r))
				mod = 1;
			}
			fclose(ml);
		    }
		}
#endif
		if ((ap->flag & GF_RONLY)
#ifdef M_LIST
		    && !mod
#endif
		    ) {
		    sprintf(tmpbuf,mesg(21, lang),ap->name);
		    r = tmpbuf;
		    goto report_error;
		}
		if ((ap->flag & GF_SONLY) &&
		    !issubs(replyaddr, iline)) {
		    sprintf(tmpbuf,mesg(20, lang),ap->name);
		    r = tmpbuf;
		    goto report_error;
		}
		if (ap->flag & GF_COM)
#ifdef CXPOST
		    if (com++) {
			register char *p, *q;
			for (wch = xch; wch; wch = wch->next) {
			    p = iline;
			    q = wch->name;
			    while (*p && *q && *p == *q) { p++; q++; }
			    if ((*p == '.' || !*p) && !*q) {
				strcpy(tmpbuf, mesg(54, lang));
				r = mesg(53, lang);
				goto report_error;
			    }
			}
			wch = (struct xh *) calloc(1, sizeof(struct xh));
			p = iline;
			q = wch->name;
			while (*p && *p != '.')  *q++ = *p++;
			*q = 0;
			wch->next = xch;
			xch = wch;
		    } else {
			register char *p, *q;
			xch = (struct xh *) calloc(1, sizeof(struct xh));
			p = iline;
			q = xch->name;
			while (*p && *p != '.')  *q++ = *p++;
			*q = 0;
		    }
#else
		    com++;
#endif
		else if (com)
		    goto crerr;
		else
		    ncom++;
		if ((ap->flag & GF_COM) && ncom) {
		  crerr:
		    r = mesg(36, lang);
		    tmpbuf[0] = '\0';
		    goto report_error;
		}
		if (ap->flag & GF_NOXPOST)
		    xpd = 1;
		if (xpd && cnt > 1) {
		    r = mesg(39, lang);
		    sprintf(tmpbuf, "\t%s\n", ap->name);
		    goto report_error;
		}
		goto found;
	    }
	}

	/*
		 * A group was not found in the current active file,
		 * reply the error
		 */
	q = tmpbuf;
	p = list;
	while ((p = nextword(p, iline, sizeof(iline))) != NULL) {
	    for (ap = active; ap != NULL; ap = ap->next) {
		if (strcmp(iline, ap->name) == 0)
		    goto skip;
	    }
	    if (q - tmpbuf + strlen(iline) + 2 >= sizeof (tmpbuf))
		goto skip;
	    *q++ = '\t';
	    r = iline;
	    while (*r) *q++ = *r++;
	    *q++ = '\n';
	  skip:;
	}
	*q = '\0';
	r = mesg(13, lang);
	goto report_error;

      found:;
        if (q - tmpbuf + strlen(iline) + 2 >= sizeof (tmpbuf))
		continue;
	r = iline;
	while (*r) *q++ = *r++;
	*q++ = ',';
    }

    if (q == tmpbuf) {
	r = mesg(14, lang);
	tmpbuf[0] = '\0';
	goto report_error;
    }
    *--q = '\0';		/* replace the last ',' */
#ifdef CXPOST
    for (wch = xch; wch; wch = xch) {
	xch = wch->next;
	free(wch);
    }
#endif
    /*
     * The group list is OK, call inews
     */
#ifndef NNTP_ONLY
    if (pipe(pfd) == -1)
	bexit(75);		/* EX_TEMPFAIL */
    of = fdopen(pfd[1], "w");
    if (of == NULL)
	bexit(75);		/* EX_TEMPFAIL */
    sprintf(tmpname, "/tmp/M2NE.%d", getpid());
    switch(i=fork()) {
	case 0:
	/* new proc */
	close(0);
	dup(pfd[0]);		/* should be 0 :-) */
	for (i = 1; i < _NFILE; i++)
	    close(i);
	creat(tmpname, 0600);	/* fd 1 */
	dup(1);			/* fd 2 */

	execl(SHELL, SHELL, "-ce", INEWS, 0);
	exit(1);
	case -1:
		bexit(75);
	default:
    /*
     * Write the header
     */
#else
    connect_nnrpd();
    fputs("POST\r\n", fsocket);
    fflush(fsocket);
#ifdef DEBUG
    printf(">> POST\n");
#endif
    if (!sofgets(snntp, sizeof(snntp), fsocket))
	lexit(1);
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='3') {
	log("server response to 'POST': %s", snntp);
	bexit(1);
    }
    of = fsocket;
#endif

    fprintf(of, "Newsgroups: %s\n", tmpbuf);
    for (c = 0; c < nhfields; c++) {
	if (c == H_NEWSGROUPS)
	    continue;
	q = hfields[c].contents;
	if (c == H_SUBJECT && q != NULL) {
	    if (!strncmp(q, "[NEWS] ", 7)) {
		q += 7;
	    } else if (!strncmp(q, "re: [NEWS] ", 11) ||
		       !strncmp(q, "Re: [NEWS] ", 11)) {
		q += 7;
		q[0] = 'R';
		q[1] = 'e';
		q[2] = ':';
		q[3] = ' ';
	    } else if (!strncmp(q, "re:[NEWS] ", 10) ||
		       !strncmp(q, "Re:[NEWS] ", 10)) {
		q += 6;
		q[0] = 'R';
		q[1] = 'e';
		q[2] = ':';
		q[3] = ' ';
	    }
	    hfields[c].contents = q;
	}
	if (c == H_DATE) {
	    if (q == NULL)
		q = adate();
	    q = stripcomm(q);
	}
	if (q != NULL)
	    fprintf(of, "%s: %s", hfields[c].field, q);
    }
    if (hfields[H_DISTRIBUTION].contents == NULL)
	fprintf(of, "Distribution: world\n");
#ifdef M_LIST
    if (mod && hfields[H_APPROVED].contents == NULL)
	fprintf(of, "Approved: %s@%s\n", NEWSUSER, hn);
#endif
    if (hfields[H_ORGANIZATION].contents == NULL)
	fprintf(of, "Organization: unknown\n");
    if (hfields[H_MESSAGE_ID].contents == NULL) {
	time(&now);
	fprintf(of, "Message-ID: <%u.%lu@%s>\n", getpid(), now, hn);
    }
    fprintf(of, "Sender: %s@%s\n", NEWSSENDER, hn);
    fprintf(of, "X-Return-Path: %s\n", ufrom);
    if (hfields[H_REPLY_TO].contents == NULL)
	fprintf(of, "Reply-To: %s\n", replyaddr);
    if (hfields[H_SUBJECT].contents == NULL)
	fprintf(of, "Subject: <none>\n");
    fprintf(of, "Lines: %u\n\n", nlines);

    /*
     * Write the body of the message
     */
#ifdef NNTP_ONLY
    waseol=1;
    while ((c = getc(iff)) != EOF)
    {
	if (c=='.' && waseol)
	    putc('.', of);
	if (c=='\n')
	{
	    putc('\r', of);
	    waseol=1;
	}
	else
	    waseol=0;
	putc(c, of);
    }
    if (!waseol)
	fputs("\r\n", of);
    fputs(".\r\n", of);
#ifdef DEBUG
    printf(">> ...\n>> .\n");
#endif
    fflush(of);
    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
	lexit(1);
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]=='2')
	status=0;
    else
	status=1;
#else /* not NNTP_ONLY */

    while ((c = getc(iff)) != EOF)
	putc(c, of);
    fclose(of);

    /*
     * Check the result
     */
#if defined(_POSIX_SOURCE) || defined(ISC)
	    waitpid(i, &status, 0);
#else
	    wait(&status);
#endif
    }
#endif /* NNTP_ONLY */
    if (status == 0) {
	readrecl();
	fstat(fileno(iff), &sfb);
	time(&now);
	t = localtime(&now);
	if ((of = fopen(STATFILE, "a")) != NULL) {
	    p = tmpbuf;
	    while ((p = nextword(p, iline, sizeof(iline))) != NULL)
		fprintf(of, "%s!%s\t%s@%s\t%d\t%02d:%02d:%02d\t%d.%d.%d\n",
			(usite[0] == 0 ? "" : usite), (ufrom[0] == 0 ? replyaddr : ufrom),
		 grecname(iline), hn, (int)sfb.st_size, t->tm_hour, t->tm_min,
			t->tm_sec, t->tm_mday, t->tm_mon + 1, t->tm_year);
	    fclose(of);
	}
	log("Article from %s injected\n", replyaddr);
	unlink(tmpname);
	bexit(0);
    }
#ifdef NNTP_ONLY
    strcpy(tmpbuf, snntp);
    p=strpbrk(tmpbuf, "\r\n");
    if (p) *p='\0';
    strcat(tmpbuf, "\n");
#else
    tmpbuf[0] = '\0';
    if ((i = open(tmpname, 0)) > 0) {
	c = read(i, tmpbuf, (sizeof tmpbuf) - 1);
	if (c < 0)
	    c = 0;
	tmpbuf[c] = '\0';
	close(i);
    }
    unlink(tmpname);
#endif
    r = mesg(15, lang);

    /*
	  * Print the error message
	  */
  report_error:
    av[0] = MAILER;
    av[1] = SENDER_FLAG;
    av[2] = sname;
    av[3] = replyaddr;
    av[4] = (char *) NULL;
    if (pipe(pfd) == -1)
	bexit(75);		/* EX_TEMPFAIL */
    switch(i = fork()){
	case 0:
	/* new proc */
	close(0);
	dup(pfd[0]);		/* should be 0 :-) */
	for (i = 1; i <= _NFILE; i++)
	    close(i);
	open("/dev/null", 1);	/* fd 1 */
	dup(1);			/* fd 2 */

	execv(MAILER, av);
	exit(1);
    case  -1:
	bexit(75);		/* EX_TEMPFAIL */
    default:
    close(pfd[0]);
    of = fdopen(pfd[1], "w");
    if (of == NULL)
	bexit(70);		/* EX_SOFTWARE */

    /* Print headers, etc */
    fprintf(of, "From: News Mailing Service <%s@%s>\n", NEWSUSER, hn);
    fprintf(of, "Sender: %s@%s\n", NEWSSENDER, hn);
    fprintf(of, "Date: %s\n", adate());
    fprintf(of, "To: %s\n", replyaddr);
    fprintf(of, "Subject: cannot submit your article\n\n");

    fprintf(of, "*** %s\n", r);

    if (tmpbuf[0] != '\0') {
	q = tmpbuf;
	fprintf(of, "\n");
	while (*q)
	    putc(*q++, of);
    }
    if (iff != NULL) {
	fprintf(of, "\n------- The unsent message follows ------\n");
	fprintf(of, "Sender: %s\n", replyaddr);
	for (c = 0; c < nhfields; c++) {
	    if (hfields[c].contents != NULL)
		fprintf(of, "%s: %s", hfields[c].field,
			hfields[c].contents);
	    if (c == H_NEWSGROUPS)
		fprintf(of, "\n");
	}
	fprintf(of, "\n");
	while ((c = getc(iff)) != EOF)
	    putc(c, of);
    }
    fclose(of);
    log("ERR: Can't submit article from %s by reason %s\n",
	replyaddr, r);
#if defined(_POSIX_SOURCE) || defined(ISC)
	    waitpid(i, &status, 0);
#else
	    wait(&status);
#endif
    }
    bexit(status);
}

/*
 * Send article to user
 */

int send_article(FILE *input, FILE *diag, char *sender)
{
    char iline[1024];
    char sendername[100];
    int  ignorecont;
    char tmpname[40];
#ifdef SEND_BY_SMTP
    char * users[2];
    struct mailnewsreq req;
#else
    int  status;
    int  pfd[2];
    FILE *tmp;
#if defined(_POSIX_SOURCE) || defined(ISC)
    int  pid;
#endif
#endif
#ifdef NNTP_ONLY
    int was_point, was_eol, wascr;
    register c;
#else
#ifdef SEND_BY_SMTP
    int was_eol;
#else
    register c;
#endif
#endif
    register char *p;
    char *ibp;
    char *hdrbuf=NULL, *newhdrbuf;
    int  hdrbufsize=sizeof(tmpbuf);

    gethost(tmpbuf, 80);
    sprintf(sendername, "%s@%s", sender, tmpbuf);

    hdrbuf=malloc(hdrbufsize);
    if (hdrbuf==NULL) {
outofmem:
	log ("malloc fail!\n");
	if (hdrbuf) free(hdrbuf);
	goto sendfail;
    }
    ibp = hdrbuf;
#ifdef USE_XCLASS
    p = "X-Class: Slow\n";
    while (*p)
	*ibp++ = *p++;
#endif
#ifdef USE_PRECEDENCE
    p = "Precedence: ";
    while (*p)
	*ibp++ = *p++;
    p = PRECEDENCE;
    while (*p)
	*ibp++ = *p++;
    *ibp++ = '\n';
#endif
    p = "Sender: ";
    while (*p)
	*ibp++ = *p++;
    p = sendername;
    while (*p)
	*ibp++ = *p++;
    *ibp++ = '\n';
    p = "To: ";
    while (*p)
	*ibp++ = *p++;
    p = replyaddr;
    while (*p)
	*ibp++ = *p++;
    *ibp++ = '\n';

    /*
	 * Parse header of a message
	 */
    ignorecont = 0;
    while (sofgets(iline, sizeof iline - 1, input) != NULL) {

	if (ibp-hdrbuf >= hdrbufsize - sizeof(iline) - 1)
	    if ((newhdrbuf=realloc(hdrbuf, hdrbufsize+=sizeof(iline)))==NULL)
		goto outofmem;
	    else {
		ibp += (newhdrbuf-hdrbuf);
		hdrbuf = newhdrbuf;
	    }
#ifdef NNTP_ONLY
	p=strrchr(iline, '\r');
	if (p && strcmp(p, "\r\n")==0) {
	    strcpy(p, "\n");
	    wascr = 1;
	}
	else
	    wascr = 0;
#endif
	if (iline[0] == '\n' || iline[0] == '\0')
	    break;
#ifdef SEND_BY_SMTP
	p=strchr(iline, '\n');
	if (p) strcpy(p, "\r\n");
#endif
	if (ignorecont && ISspace(iline[0]))
	    continue;
	ignorecont = 0;
	if (fieldcmp(iline, "to") ||
	    fieldcmp(iline, "return-receipt-to") ||
	    fieldcmp(iline, "sender")) {
	    ignorecont++;
	    continue;
	}
	if (fieldcmp(iline, "path")) {
	    p = "X-NNTP-";
	    while (*p)
		*ibp++ = *p++;
	    p = iline;
	    while (*p)
		*ibp++ = *p++;
	    continue;
	}
	if (fieldcmp(iline, "xref")) {
	    p = "X-Ref: ";
	    while (*p)
		*ibp++ = *p++;
	    p = iline + 6;
	    if ((p = nextword(p, tmpname, sizeof(tmpname))) == NULL)
		continue;	/* skip the name of site */
	    while (ISspace(*p))
		p++;
	    while (*p)
		*ibp++ = *p++;
	}
	/* Should be removed later */
	if (fieldcmp(iline, "subject")) {
	    p = "Subject: [NEWS] ";
	    while (*p)
		*ibp++ = *p++;
	    p = iline + 9;
	    while (ISspace(*p))
		p++;
	    while (*p)
		*ibp++ = *p++;
	    continue;
	}
	p = iline;
	while (*p)
	    *ibp++ = *p++;
    }
    if (iline[0]!='\n')
    {
	free(hdrbuf);
sendfail:
#ifdef NNTP_ONLY
	was_point=0;
	was_eol=wascr=1;
	while ((c = sofgetc(input)) != EOF) {
		if (was_point && wascr && (c=='\n')) break;
		if (c=='\n') {
		    if (was_eol)
			wascr = was_point = 0;
		    was_eol = 1;
		    continue;
		}
		if (c=='\r') {
		    if (wascr)
			was_point = was_eol = 0;
		    wascr = 1;
		    was_eol = 0;
		    continue;
		}
		was_point = (was_eol && wascr && c=='.');
		was_eol = wascr = 0;
	}
#endif
	return 1;
    }
#ifdef DEBUG
    printf("END OF HEADER\n");
#endif

    /*
	 * Copy the body of the message
	 */
    *ibp++ = '\n';
#ifdef SEND_BY_SMTP
    req.mailhub=MAILHUB;
    req.port=SMTPPORT;
    req.mailhub2=MAILHUB2;
    req.port2=SMTPPORT2;
    req.Iam=sendername;
    req.users=users;
    users[0]=replyaddr;
    users[1]=NULL;
    h=opensmtpsession(&req, h);
    if (h<0) {
	fprintf(diag, mesg(19, lang), -1);
	free(hdrbuf);
	h=0;
	goto sendfail;
    }
    was_eol=wascr=1;

    write(h, hdrbuf, ibp-hdrbuf);
    free(hdrbuf);

    while (sofgets(tmpbuf, sizeof(tmpbuf)-1, input)!=NULL) {
#ifdef NNTP_ONLY
	if (was_eol && wascr && strcmp(tmpbuf, ".\r\n")==0)
	    break;
	wascr = 0;
	if ((p=rindex(tmpbuf, '\r')) != NULL) {
	    if (strcmp(p, "\r\n")==0) {
		strcpy(p, "\n");
		wascr = 1;
	    }
	    else if (p[1]=='\0')
		p[0]='\0';
	}
	if (was_eol && tmpbuf[0]=='.')
	    write(h, ".", 1);
#endif
	p=index(tmpbuf, '\n');
	if (p) strcpy(p, "\r\n");
	write(h, tmpbuf, strlen(tmpbuf));
	was_eol = (p != NULL);
    }
#ifdef NNTP_ONLY
    if (!was_eol || !wascr || strcmp(tmpbuf, ".\r\n"))
    {
	close(h);	/* abort */
	h=0;
	return 1;
    }
#ifdef DEBUG
    printf("<< ...\n<< .\n");
#endif
#endif

    if (closesmtpdata(h)) {
	fprintf(diag, mesg(19, lang), -1);
	closesmtpsession(h);
	h=0;
	return 1;
    }
#else /* not SEND_BY_SMTP */
#ifdef NNTP_ONLY
    was_eol=wascr=1;
    was_point=0;
#endif
    while ((c = sofgetc(input)) != EOF) {
#ifdef NNTP_ONLY
	if (was_point && wascr && (c=='\n')) break;
	if (c=='\r') {
	    wascr=1;
	    continue;
	}
	else if (c!='\n' || was_eol)
	    wascr=0;
	was_eol = (c=='\n');
	if (wascr && was_eol && c=='.') {
	    wascr = was_eol = 0;
	    was_point = 1;
	    continue;
	}
	was_point = 0;
#endif
	if (ibp-hdrbuf >= hdrbufsize)
	    if ((hdrbuf=realloc(hdrbuf, hdrbufsize+=sizeof(iline)))==NULL)
		goto outofmem;
	    else {
		ibp += (newhdrbuf-hdrbuf);
		hdrbuf = newhdrbuf;
	    }
	*ibp++ = c;
    }
#ifdef NNTP_ONLY
    if (!was_point || !wascr || c!='\n')
    {
	free(hdrbuf);
	return 1;
    }
#endif
#ifdef DEBUG
#ifdef NNTP_ONLY
    printf("<< ...\n<< .\n");
#else
    printf("END OF INPUT\n");
#endif
#endif

    /*
	 * Send the mail out
	 */
    c = 0;
    av[c++] = "send-mail";
#ifdef MFLAG_QUEUE
    av[c++] = MFLAG_QUEUE;
#endif
#ifdef MFLAG
    av[c++] = MFLAG;
#endif
    av[c++] = SENDER_FLAG;
    av[c++] = sendername;
    av[c++] = replyaddr;
    av[c] = NULL;
    if (pipe(pfd) == -1) {
	fprintf(diag, mesg(18, lang));
	free(hdrbuf);
	return 1;
    }
   try1:
    switch (
#if defined(_POSIX_SOURCE) || defined(ISC)
	(pid = fork())
#else
	fork()
#endif
	) {
	case 0:
	    /* new process */
	    close(0);
	    dup(pfd[0]);	/* should be 0 :-) */
	    for (c = 3; c <= _NFILE; c++)
		close(c);

	    execv(MAILER, av);
	    exit(1);
	case -1:
	    sleep(30);
	    goto try1;
	default:
	    close(pfd[0]);
	    write(pfd[1], hdrbuf, ibp - hdrbuf);
#ifdef NNTP_ONLY
	    write(pfd[1], ".\r\n", 3);
#endif
	    close(pfd[1]);
    }
    free(hdrbuf);

    status = 0;
#if defined(_POSIX_SOURCE) || defined(ISC)
    waitpid(pid, &status, 0);
#else
    wait(&status);
#endif
    if (status != 0) {
	fprintf(diag, mesg(19, lang), status);
	return 1;
    }
    sleep(1);
#endif
    return 0;
}


#if !defined(_POSIX_SOURCE) && !defined(BSD)
char *
 strstr(s1, s2)
char *s1, *s2;
{
    int len1, len2;
    register char *p;
    register i, j;

    len1 = strlen(s1);
    len2 = strlen(s2);

    if (len2 > len1)
	return (char *) NULL;

    p = s1;
    for (i = 0; i <= len1 - len2; i++, p++)
	if (*p == *s2) {
	    for (j = 0; j < len2 && *(p + j) == *(s2 + j); j++);
	    if (*(s2 + j) == 0)
		return p;
	}
    return (char *) NULL;
}

#endif

static int whatcmd(command)
char *command;
{
    int i, j, k;
    i = j = 0;
    k = -1;
    while (command[i]) {
	if (coms[j][i] == (command[i] & 0xdf))
	    if (k == -1)
		if (coms[j + 1][i] == (command[i] & 0xdf))
		    i++;
		else
		    k = j;
	    else
		i++;
	else if ((k == -1) && (strncmp(coms[j],coms[j+1],i)==0)) {
	    if ((++j) >= CMDN)
		break;
	} else {
	    k = -1;
	    break;
	}
    }
    return k;
}

#ifdef EXT_CMD
int parse_ext(com, param)
char *com, *param;
{
    FILE *ecf;
    char lin[128], *p;
    static char lbuf[128];
    int idx, i;

    idx = strlen(com);
    if ((ecf = fopen(EXT_CMD, "r")) == NULL)
	return 0;
    while (fgets(lin, sizeof lin, ecf) != NULL) {
	if ((p = index(lin, '\n')) != NULL)
	    *p = 0;

	for (i = 0; (com[i] && lin[i] && letters[1][(int)lin[i]] == letters[1][(int)com[i]]); i++);
	if (i == idx) {
	    strcpy(lbuf, lin);
	    break;
	} else {
	    fclose(ecf);
	    return 0;
	}
    }
    fclose(ecf);
    if (idx == 0)
	return 0;
    p = lbuf;
    while (*p && !ISspace(*p))
	p++;
    while (*p && ISspace(*p))
	p++;
    if (*p)
	av[0] = p;
    else
	return 0;
    av[1] = "-r";
    av[2] = replyaddr;
    i = 3;
    while (*p) {
	while (*p && !ISspace(*p))
	    p++;
	if (*p)
	    *p++ = 0;
	while (*p && ISspace(*p))
	    p++;
	if (*p)
	    av[i++] = p;
    }
    p = param;
    while (*p) {
	if (*p)
	    av[i++] = p;
	while (*p && !ISspace(*p))
	    p++;
	if (*p)
	    *p++ = 0;
	while (*p && ISspace(*p))
	    p++;
    }
    av[i] = (char *) NULL;
    return i;
}

#endif

int main(argc, argv)
int argc;
char **argv;
{
    char command[200];
    int sendergot, sprio, i, j, line_bad, line_read, nf_, status, bad_user=0;
    int reply_required, articles_ignored, index_cnt = 0, find_cnt = 0;
    register char *p, *q, *r;
    FILE *of, *iff;
    char tempfile[40];
    char sendgroup[160];
#ifdef EXT_SERV_PATH
    char ext_server[25];
#endif
    long clock;
    register struct aentry *ap;

#ifdef OS2
    extern int _fmode_bin;
    _fmode_bin=1;
#endif

    strcpy(pname,argv[0]);
    gethost(hn, 80);

    find_art=index_art=0;
    nf_ = 0;
    ufrom[0] = 0;
    for (i=1; i<argc; i++) {
	if (i+1 < argc && strcmp(argv[i], "-r") == 0) {
	    nf_ = 1;
	    strcpy(ufrom, argv[++i]);
	    continue;
	}
#ifdef NNTP_ONLY
	if (i+1 < argc && strcmp(argv[i], "-s") == 0) {
	    sockfd=atoi(argv[++i]);
#ifdef DEBUG
	    printf("Param sockfd=%d\n", sockfd);
#endif
	    dont_disconnect=1;
	    continue;
	}
#endif
	fprintf(stderr, "Incorrect param '%s' ignored\n", argv[i]);
    }
    /* Parse header */
    sendergot = 0;
    replyaddr[0] = 0;
    usite[0] = 0;
  loop:
    while (fgets(iline, sizeof iline, stdin) != NULL) {
	if (strncmp(iline, "From ", 5) == 0 && !nf_) {
	    p = iline + 5;
	    q = ufrom;
	    while (*p && (!ISspace(*p)) && *p != '\n')
		*q++ = *p++;
	    *q = 0;
	    p = strstr(iline, "remote from");
	    if (p != NULL) {
		p += 12;
		q = usite;
		while (*p && *p != '\n' && !ISspace(*p))
		    *q++ = *p++;
		*q = 0;
	    }
	    else{
		    p = iline + 5;
		    q = usite;
		    while (*p && *p != '!' && !ISspace(*p))
			    *q++ = *p++;
		    *q = 0;
	    }
	    continue;
	}
      next_header_line:
	/* End of header? */
	if (iline[0] == '\n' || iline[0] == '\r' || iline[0] == '\0')
	    break;
	if (ISspace(iline[0]))
	    continue;
	for (i = 1; i < nhfields; i++) {	/* skip H_FROM */
	    if (hfields[i].contents != NULL)
		continue;
	    if (!fieldcmp(iline, hfields[i].field))
		continue;

	    strcpy(tmpbuf, iline + 1 + strlen(hfields[i].field));
	    line_read = 0;
	    if (index(iline, '\n') == NULL) {
		do {
		    if (fgets(iline, sizeof iline, stdin) != NULL)
			if (strlen(tmpbuf) + strlen(iline) < sizeof tmpbuf)
			    strcat(tmpbuf, iline);
		} while (index(iline, '\n') == NULL);
		goto ccont;
	    }
	  hcont:
	    if (fgets(iline, sizeof iline, stdin) != NULL)
		line_read++;
	    if (ISspace(iline[0])) {
		if (strlen(tmpbuf) + strlen(iline) < sizeof tmpbuf)
		    strcat(tmpbuf, iline);
		goto hcont;	/* RFC-style line continuation */
	    }
	  ccont:
	    p = tmpbuf;
	    while (ISspace(*p))
		p++;
	    j = strlen(p);
	    if (j != 0) {
		q = (char *) malloc(j + 1);
		if (q == NULL) {
		    fprintf(stderr, "newsserv: cannot get %d bytes of core\n", i + 1);
		} else {
		    strcpy(q, p);
		    hfields[i].contents = q;
		}
	    }
	    if (line_read)
		goto next_header_line;
	    goto loop;
	}
	if (fieldcmp(iline, "reply-to")) {
	    i = 9;
	    sprio = 4;
	    goto parsefrom;
	}
	if (fieldcmp(iline, "from")) {
	    i = 5;
	    sprio = 3;
	    goto parsefrom;
	}
	if (fieldcmp(iline, "resent-from")) {
	    i = 12;
	    sprio = 2;
	    goto parsefrom;
	}
	if (fieldcmp(iline, "sender")) {
	    i = 7;
	    sprio = 1;

	  parsefrom:
	    if (sendergot >= sprio)
		continue;

	    strcpy(frombuf, iline + i);

	    /*
	     * Parse ugly RFC 822  Name <addr>  form
	     */
#if 0 /* gul - prevent "user (<alias>) <address>" */
	    if ((p = index(iline + i, '<')) == NULL)
		p = iline + i;
	    else
		p++;
#else
	    p = iline + i;
#endif
	    i = 0;		/* comment flag */
	    q = tmpbuf;
	  rfc_cont:
	    for (; *p != '\n' && *p != '\0'; p++) {
		if (ISspace(*p))
		    continue;
		if (*p == '(') {
		    i++;
		    continue;
		}
		if (i) {
		    if (*p == ')')
			i--;
		    continue;
		}
#if 1 /* gul */
		if (*p == '<') {
			q = tmpbuf;
			continue;
		}
#endif
		if (*p == '>')
		    break;
		*q++ = *p;
	    }
	    line_read = 0;
	    if (*p == '\n' && fgets(iline, sizeof iline, stdin) != NULL) {
		if (ISspace(iline[0])) {
		    strcat(frombuf, iline);
		    p = iline;
		    goto rfc_cont;	/* RFC-style line continuation */
		}
		line_read++;
	    }
	    *q = '\0';
	    if (q != tmpbuf) {
		sendergot = sprio;
		strcpy(replyaddr, tmpbuf);
	    }
	    if (line_read)
		goto next_header_line;
	}
    }

    if (replyaddr[0] == '\0'
	|| strcmp(replyaddr, "uucp") == 0 || strncmp(replyaddr, "uucp@", 5) == 0
	|| strcmp(replyaddr, NEWSUSER) == 0 || strncmp(replyaddr, NEWSUSER1, strlen(NEWSUSER1)) == 0
	|| strcmp(replyaddr, "MAILER-DAEMON") == 0 || strncmp(replyaddr, "MAILER-DAEMON@", 14) == 0)
	exit(0);
    if ((of = fopen(BAD_USERS, "r")) != NULL) {
	while (fgets(iline, sizeof iline, of) != NULL) {
	    if ((p = index(iline, '\n')) != NULL)
		*p = 0;
	    if (*iline == '#')
		continue;
	    if(*iline == '!' && wildmat(replyaddr, iline+1)){
		bad_user=0;
		break;		
	    }
	    if (wildmat(replyaddr, iline))
		bad_user=1;
	}
	fclose(of);
	if(bad_user){
ignore_user:
	    log("Bad user: %s rejected\n", replyaddr);
	    exit(0);
	}
    }
    if (strchr(replyaddr, '\\') || strchr(replyaddr, '\''))
	goto ignore_user;
    snprintf(iline, sizeof(iline), "%s '%s' nomail 2>/dev/null", BAD_USERS_SH, replyaddr);
    of = popen(iline, "r");
    /* log("'%s' returns %d\n", iline, i); */
    if (of == NULL)
	log("popen() failed: %s\n", strerror(errno));
    if (fgets(iline, sizeof(iline), of)) {
	strncpy(replyaddr, iline, sizeof(replyaddr));
	while(fgets(iline, sizeof(iline), of));
	if ((p=strchr(replyaddr, '\n'))!=NULL) *p='\0';
    }
    i=pclose(of);
    if ((i==1) || (i==256)) {
	/* check envelope-from */
	/* convert atgua!atg.kiev.ua!san@kozlik.carrier.kiev.ua -> san@atg.kiev.ua */
#if 0
	if ((of = fopen(BAD_USERS, "r")) != NULL) {
	    while (fgets(iline, sizeof iline, of) != NULL) {
		if ((p = index(iline, '\n')) != NULL)
		    *p = 0;
		if (*iline == '#')
		    continue;
		if(*iline == '!' && wildmat(ufrom, iline+1)) {
		    bad_user=0;
		    break;		
		}
		if (wildmat(ufrom, iline))
		    bad_user=1;
	    }
	    fclose(of);
	    if(bad_user)
		goto ignore_user;
	}
#endif
	if (strchr(ufrom, '\\') || strchr(ufrom, '\''))
	    goto ignore_user;
	snprintf(iline, sizeof(iline), "%s '%s' 2>/dev/null", BAD_USERS_SH, ufrom);
	of = popen(iline, "r");
	/* log("'%s' returns %d\n", iline, i); */
	if (of == NULL)
	    log("popen() failed: %s\n", strerror(errno));
	if (fgets(iline, sizeof(iline), of)) {
	    strncpy(ufrom, iline, sizeof(ufrom));
	    while(fgets(iline, sizeof(iline), of));
	    if ((p=strchr(ufrom, '\n'))!=NULL) *p='\0';
	}
	i=pclose(of);
	/* log("'%s' returns %d\n", iline, i); */
	if ((i==1) || (i==256))
	    goto ignore_user;
	strncpy(replyaddr, ufrom, sizeof(replyaddr));
    }

    hfields[H_FROM].contents = frombuf;

    if(IS_CU(cpr))
	readsnl();
    else
	(void)fread(tmpbuf, sizeof(char), sizeof tmpbuf, stdin);

    /* Copy active file into memory */
    readactive();

	/*
	 * Create a temp. file for reply
	 */
    sprintf(tempfile, "/tmp/M2NR.%d", getpid());
    if ((of = fopen(tempfile, "w")) == NULL)
	bexit(75);		/* EX_TEMPFAIL */


    /* Print headers, etc */
    gethost(command, 80);
    fprintf(of, "From: News Mailing Service <%s@%s>\n", NEWSUSER, command);
    fprintf(of, "Sender: %s@%s\n", NEWSSENDER, command);
    fprintf(of, "Date: %s\n", adate());
    fprintf(of, "To: %s\n", replyaddr);
    fprintf(of, "Subject: reply from USENET server\n\n");

    if(get_uparam(replyaddr)){
    if (bserrno==BS_DISDOMAIN) {
	fprintf(of, mesg(40, ui_entry.lang));
	reply_required = 1;
	goto eee;
    }
#ifdef UNKNOWN_DENIED
    else if(bserrno==BS_DENIED){
    fprintf(of, mesg(40, DEFLANG));
    reply_required = 1;
    goto eee;
    }
#endif
    else{
    	lang=DEFLANG;
    	limit=65535;
    }
    }
    else{
	lang=ui_entry.lang;
	limit=ui_entry.limit*1024;
    }
    if (index(replyaddr, '!') != NULL || index(replyaddr, '%') != NULL) {
	fclose(of);
	bexit(0);
    }
    fprintf(of,cpr);
    fprintf(of, "%s\n\n\n", version);
    fprintf(of, mesg(11, lang), replyaddr);

	/*
	 * There is Newsgroups: - submit a new article
	 */

    if ((p = hfields[H_NEWSGROUPS].contents) != NULL){
	fclose(of);
	unlink(tempfile); /* Dont need in submit() */
	submit(p);		/* Never returns */
    }
    /* Check the active file */
    if (active == NULL) {
	fprintf(of, mesg(1, lang));
	fclose(of);
	bexit(0);
    }

	/*
	 * Process the input
	 */
    line_read = 0;
    line_bad = 0;
    reply_required = 0;
    articles_ignored = 0;
    sendgroup[0] = 0;
    while (fgets(iline, sizeof iline, stdin) != NULL) {
	/*
		 * Collect a command
		 */
	p = iline;
	if (!strncmp(p, "--", 2))
	    goto el;
	if (*p == '>' || *p == '-' || *p == '#' || *p == '\n')
	    continue;

	while (*p != '\n' && *p != '\r' && *p)
	    p++;
	*p = '\0';
	p = iline;
	while (ISspace(*p))
	    p++;
	q = p;
	while (*p && !ISspace(*p))
	    p++;
	r = command;
	while (q < p)
	    *r++ = *q++;
	*r = '\0';
	while (*p && ISspace(*p))
	    p++;
	if (*command == '>' || *command == '-' || *command == '#' || *command == '\n')
	    continue;
	/*
	 * Try to recognize a command
	 */
	switch (whatcmd(command)) {
	case C_QUIT:
	case C_END:
	case C_EXIT:
	    // line_read++;
	    goto el;
	case C_TIME:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--TIME: ");
	    clock = time((long *) NULL);
#ifdef ISC
	    cftime(command, "%H:%M:%S", &clock);
	    fprintf(of, "%s\n", command);
#else
#ifdef _POSIX_SOURCE
	    strftime(command, sizeof command, "%H:%M:%S", localtime(&clock));
	    fprintf(of, "%s\n", command);
#else
	    fprintf(of, "%s\n", asctime(localtime(&clock)));
#endif
#endif
	    continue;
	case C_HELP:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--HELP\n");
	    help(of);
	    log("sent help to %s\n", replyaddr);
	    continue;
	case C_LANGUAGE:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SET LANGUAGE TO %s\n", p);
	    if(!set_uparam(replyaddr,US_LANG,detlang(p),of)){
		log("%s change language to %s\n", replyaddr, p);
	    }else{
		log("ERR: failed changing language for %s\n",replyaddr);
	    }
	    lang=detlang(p);
	    continue;
	case C_LIST:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--LIST %s\n", p);
	    log("sent LIST %s to %s\n", p, replyaddr);
	    list(of, p, L_LIST);
	    continue;
	case C_ILIST:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--ILIST %s\n", p);
	    log("sent ILIST %s to %s\n", p, replyaddr);
	    list(of, p, L_ILIST);
#ifdef NNTP_ONLY
	    if (sendgroup[0]) {
		fprintf(fsocket, "GROUP %s\r\n", sendgroup);
		fflush(fsocket);
		if (sofgets(snntp, sizeof(snntp), fsocket)==0)
		    lexit(1);
#ifdef DEBUG
		printf(">> GROUP %s\n<< %s", sendgroup, snntp);
#endif
	    }
#endif
	    continue;
	case C_ULIST:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--ULIST %s\n", p);
	    log("sent ULIST %s to %s\n", p, replyaddr);
	    list(of, p, L_ULIST);
	    continue;
	case C_LIMIT:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SET LIMIT TO %s\n", p);
	    if(!set_uparam(replyaddr,US_LIMIT,atoi(p),of)){
		log("%s change limit to %s\n", replyaddr, p);
	    }else{
	    	log("ERR: failed changing limit for %s\n", replyaddr);
	    }
	    continue;
	case C_INDEX:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--INDEX %s\n", p);
	    log("INDEX %s from %s\n", p, replyaddr);
	    switch (do_index(of, p, 0)) {
	    case 0:
		index_cnt++;;
		continue;
	    case 2:
		fprintf(of, mesg(38, lang));
		continue;
	    default:
		fprintf(of, mesg(32, lang));
	    }
#ifdef NNTP_ONLY
	    if (sendgroup[0]) {
		fprintf(fsocket, "GROUP %s\r\n", sendgroup);
		fflush(fsocket);
		if (sofgets(snntp, sizeof(snntp), fsocket)==0)
		    lexit(1);
#ifdef DEBUG
		printf(">> GROUP %s\n<<%s", sendgroup, snntp);
#endif
	    }
#endif
	    continue;
#ifndef NNTP_ONLY
	case C_FIND:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--FIND %s\n", p);
	    log("FIND %s from %s\n", p, replyaddr);
	    switch (do_index(of, p, C_FIND)) {
	    case 0:
		find_cnt++;;
		continue;
	    case 2:
		fprintf(of, mesg(59, lang));
		continue;
	    default:
		fprintf(of, mesg(51, lang));
	    }
	    continue;
	case C_SFIND:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SFIND %s\n", p);
	    log("SFIND %s from %s\n", p, replyaddr);
	    switch (do_index(of, p, C_SFIND)) {
	    case 0:
		find_cnt++;;
		continue;
	    case 2:
		fprintf(of, mesg(59, lang));
		continue;
	    default:
		fprintf(of, mesg(51, lang));
	    }
	    continue;
	case C_DIGEST:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--DIGEST %s\n", p);
	    log("DIGEST %s from %s\n", p, replyaddr);
	    switch (do_index(of, p, C_DIGEST)) {
	    case 0:
		find_cnt++;;
		continue;
	    case 2:
		fprintf(of, mesg(59, lang));
		continue;
	    default:
		fprintf(of, mesg(51, lang));
	    }
	    continue;
	case C_SDIGEST:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SDIGEST %s\n", p);
	    log("SDIGEST %s from %s\n", p, replyaddr);
	    switch (do_index(of, p, C_SDIGEST)) {
	    case 0:
		find_cnt++;;
		continue;
	    case 2:
		fprintf(of, mesg(59, lang));
		continue;
	    default:
		fprintf(of, mesg(51, lang));
	    }
	    continue;
#endif /* not NNTP_ONLY */
	case C_CHECK:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--CHECK\n");
	    log("CHECK from %s\n", replyaddr);
	    check(replyaddr, of, 0, 1);
#if 0
	    if(strlen(usite))
	    	check(usite,of,0,0);
#endif
	    continue;
	case C_SCHECK:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SCHECK\n");
	    log("SCHECK from %s\n", replyaddr);
	    check(replyaddr, of, 1,1);
#if 0
	    if(strlen(usite))
	    	check(usite,of,1,0);
#endif
	    continue;
	case C_FORGET:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--FORGET\n");
	    subscribe(replyaddr,(char **)NULL, FORGET, 0, of);
	    log("FORGET command from %s\n", replyaddr);
	    continue;
	case C_SUBSCRIBE:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SUBSCRIBE %s\n", p);
	    log("SUBS %s to %s\n", replyaddr, p);
	    subs_parse(p, SUBS, of);
	    continue;
	case C_FORMAT:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--FORMAT %s\n", p);
	    log("FORMAT %s from %s\n", p, replyaddr);
	    if (cmdcmp(p, "NEW"))
		set_uparam(replyaddr,US_FMT,F_NEW,of);
	    else if (cmdcmp(p, "OLD"))
		set_uparam(replyaddr,US_FMT,F_OLD,of);
	    else
		fprintf(of, mesg(45, lang));
	    continue;
#if AGE_TIME
	case C_AGING:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--AGING %s\n", p);
	    log("AGING %s from %s\n", p, replyaddr);
	    if (cmdcmp(p, "OFF"))
		set_uparam(replyaddr,US_AGING, 0, of);
	    else
		set_uparam(replyaddr,US_AGING, 1, of);
	    continue;
#endif
	case C_LHELP:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--LHELP %s\n", p);
	    log("LHELP %s from %s\n", p, replyaddr);
	    if (cmdcmp(p, "OFF"))
		set_uparam(replyaddr,US_NLHLP, 1, of);
	    else
		set_uparam(replyaddr,US_NLHLP, 0, of);
	    continue;
	case C_NEWGRP:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--NEWGRP %s\n", p);
	    log("NEWGRP %s from %s\n", p, replyaddr);
	    if (cmdcmp(p, "OFF"))
		set_uparam(replyaddr,US_NONGRP, 1, of);
	    else
		set_uparam(replyaddr,US_NONGRP, 0, of);
	    continue;
	case C_FEED:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--FEED %s\n", p);
	    log("FEED %s to %s\n", replyaddr, p);
	    subs_parse(p, FEED, of);
	    continue;
	case C_EFEED:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--EFEED %s\n", p);
	    log("EFEED %s from %s\n", p, replyaddr);
	    subs_parse(p, EFEED, of);
	    continue;
	case C_RFEED:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--RFEED %s\n", p);
	    log("RFEED %s from %s\n", p, replyaddr);
	    subs_parse(p, RFEED, of);
	    continue;
#if 0
	case C_CNEWS:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--CNEWS %s\n", p);
	    log("CNEWS %s to %s\n", replyaddr, p);
	    subs_parse(p, CNEWS, of);
	    continue;
#endif
	case C_UNSUBSCRIBE:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--UNSUBSCRIBE %s\n", p);
	    log("UNSUBS %s from %s\n", replyaddr, p);
	    subs_parse(p, UNSUBS, of);
	    continue;
	case C_SUSPEND:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--SUSPEND\n");
	    if(!set_uparam(replyaddr,US_SUSP, 1, of)){
		log("%s suspended\n", replyaddr);
	    }else{
	    	log("ERR: failed suspending %s\n", replyaddr);
	    }
	    continue;
	case C_RESUME:
	    line_read++;
	    reply_required++;
	    fprintf(of, "--RESUME\n");
	    if(!set_uparam(replyaddr,US_SUSP, 0, of)){
		log("%s resumed\n", replyaddr);
	    }else{
	    	log("ERR: failed resuming %s\n", replyaddr);
	    }
	    continue;
	case C_GROUP:
	    line_read++;
	    r = nextword(p, sendgroup, sizeof(sendgroup));
	    if (r == NULL || (q = nextword(r, tmpbuf, sizeof(tmpbuf))) != NULL) {
		reply_required++;
		fprintf(of, "--GROUP %s\n", p);
		fprintf(of, mesg(2, lang));
	    }
	    if (r == NULL) {
		sendgroup[0] = '\0';
		articles_ignored = 0;
		continue;
	    }
	    for (ap = active; ap != NULL; ap = ap->next) {
		if (strcmp(sendgroup, ap->name) == 0) {
		    if ((ap->flag & GF_SONLY) &&
			!issubs(replyaddr, sendgroup)) {
			articles_ignored = 1;
			sendgroup[0] = '\0';
			fprintf(of, mesg(20, lang),sendgroup);
			goto grpskip;
		    }
#ifdef NNTP_ONLY
		    connect_nnrpd();
		    fprintf(fsocket, "GROUP %s\r\n", sendgroup);
		    fflush(fsocket);
#ifdef DEBUG
		    printf(">> GROUP %s\n", sendgroup);
#endif
		    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
			lexit(1);	/* connection lost */
#ifdef DEBUG
		    printf("<< %s", snntp);
#endif
		    if (snntp[0]!='2')
			break;		/* should never happans */
#endif
		    goto grpfound;
		}
	    }
	    if (q == NULL)
		fprintf(of, "--GROUP%s\n", p);
	    reply_required++;
	    fprintf(of, mesg(3, lang), sendgroup);
	    sendgroup[0] = '\0';
	  grpfound:articles_ignored = 0;
	  grpskip:continue;
	case C_ARTICLE:
	    line_read++;
	    if (nextword(p, tmpbuf, sizeof(tmpbuf)) == NULL) {
	      badart:fprintf(of, "--ARTICLE %s\n", p);
		fprintf(of, mesg(4, lang));
		reply_required++;
		continue;
	    }
	    if (index(tmpbuf, '<') != NULL) {
#ifndef NNTP_ONLY
		iff = fopen(find_msgid(tmpbuf), "r");
		goto asend;
#else
		connect_nnrpd();
		goto achecked;
#endif
	    }
	    /* check the number */
	    for (q = tmpbuf; *q; q++)
		if (*q < '0' || *q > '9')
		    goto badart;

	    /* skip leading zeroes */
	    q = tmpbuf;
	    while (*q == '0' && q[1] != '\0')
		q++;

	    if (sendgroup[0] == '\0') {
		if (!articles_ignored) {
		    articles_ignored++;
		    fprintf(of, "--ARTICLE ...\n");
		    fprintf(of, mesg(5, lang));
		    reply_required++;
		}
		continue;
	    }
#ifndef NNTP_ONLY
	    /* Try to open the file of the message */
	    iff = fopen((q = gfn(sendgroup, tmpbuf)), "r");
	  asend:
	    if (iff == NULL) {
		reply_required++;
		fprintf(of, mesg(6, lang), sendgroup, tmpbuf);
		continue;
	    }
#else
	  achecked:
	    fprintf(fsocket, "ARTICLE %s\r\n", tmpbuf);	
	    fflush(fsocket);
#ifdef DEBUG
	    printf(">> ARTICLE %s\n", tmpbuf);
#endif
	    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
		lexit(1);	/* connection lost :-( */
#ifdef DEBUG
	    printf("<< %s", snntp);
#endif
	    if (strncmp(snntp, "423", 3) == 0)
	    {
		reply_required++;
		fprintf(of, mesg(6, lang), sendgroup, tmpbuf);
		continue;
	    }
	    if (snntp[0]!='2') goto badart;
	    iff=fsocket;
#endif
	    /* Send it to user */
	    q = malloc(strlen(tmpbuf) + 1);
	    if (q) strcpy(q, tmpbuf);
	    else q="";
	    if (send_article(iff, of, gsname(sendgroup))) {
//		fclose(iff);
		reply_required++;
		fprintf(of, "\n");
		fprintf(of, mesg(7, lang), sendgroup, q);
		log("ERR: Can't send %s/%s to %s\n", sendgroup, q, replyaddr);
	    }
	    else
		log("Sent %s/%s to %s\n", sendgroup, q, replyaddr);
#ifndef NNTP_ONLY
	    fclose(iff);
#endif
	    if (q[0])
		free(q);
	    continue;
	case C_CALL:
#ifdef EXT_SERV_PATH
	    line_read++;
	    log("CALL server %s by user %s\n", p, replyaddr);
	    sprintf(tmpbuf, "%s/%s", EXT_SERV_PATH, nextword(p, ext_server, sizeof(ext_server)));
	    av[0] = ext_server;
	    av[1] = SENDER_FLAG;
	    av[2] = replyaddr;
	    av[3] = (char *) NULL;

	    sprintf(tempfile, "/tmp/ES.%d", getpid());
	    if ((iff = fopen(tempfile, "w")) == NULL) {
		fprintf(of, "%s\n", mesg(17, lang));
		break;
	    }
	    fprintf(iff, "From %s\n", ufrom);
	    for (i = 0; i < nhfields; i++) {
		switch (i) {
		default:
		    continue;
		case H_FROM:
		case H_MESSAGE_ID:
		case H_SUBJECT:
		case H_DATE:
		case H_REPLY_TO:
		    if (hfields[i].contents != NULL)
			fprintf(iff, "%s: %s", hfields[i].field, hfields[i].contents);
		}
	    }
	    fprintf(iff, "\n");


	    while (fgets(iline, sizeof iline, stdin) != NULL) {
		p = iline;
		p = nextword(p, command, sizeof(command));
		switch (whatcmd(command)) {
		case C_QUIT:
		case C_END:
		case C_EXIT:
		    break;
		default:
		    fputs(iline, iff);
		    continue;
		}
		break;
	    }
	    fclose(iff);
	    if ((i = fork()) == 0) {
		/* new proc */
		close(0);
		open(tempfile, 0);	/* should be 0 :-) */
		unlink(tempfile);
		close(1);
		close(2);
		dup(fileno(of));
		dup(fileno(of));
		for (i = 3; i <= _NFILE; i++)
		    close(i);

		execv(tmpbuf, av);
		bexit(1);
	    }
	    if (i == -1) {
		unlink(tempfile);
		fprintf(of, "%s\n", mesg(17, lang));
	    }
	    break;
#endif
	case C_EXEC:
#ifdef EXT_SERV_PATH
	    line_read++;
	    log("EXEC server %s by user %s\n", p, replyaddr);
	    sprintf(tmpbuf, "%s/%s", EXT_SERV_PATH, nextword(p, ext_server, sizeof(ext_server)));
	    av[0] = ext_server;
	    av[1] = SENDER_FLAG;
	    av[2] = replyaddr;
	    av[3] = (char *) NULL;

	    sprintf(tempfile, "/tmp/ES.%d\n", getpid());
	    if ((iff = fopen(tempfile, "w")) == NULL) {
		fprintf(of, "%s\n", mesg(17, lang));
		break;
	    }
	    fprintf(iff, "From %s\n", ufrom);
	    for (i = 0; i < nhfields; i++) {
		switch (i) {
		default:
		    continue;
		case H_FROM:
		case H_MESSAGE_ID:
		case H_SUBJECT:
		case H_DATE:
		    if (hfields[i].contents != NULL)
			fprintf(iff, "%s: %s\n", hfields[i].field, hfields[i].contents);
		}
	    }
	    fprintf(iff, "\n");


	    while (fgets(iline, sizeof iline, stdin) != NULL) {
		fputs(iline, iff);
	    }
	    fclose(iff);
	    if ((i = fork()) == 0) {
		/* new proc */
		close(0);
		open(tempfile, 0);	/* should be 0 :-) */
		unlink(tempfile);
		for (i = 1; i <= _NFILE; i++)
		    close(i);
		open("/dev/null", 1);	/* fd 1 */
		dup(1);		/* fd 2 */

		execv(tmpbuf, av);
		bexit(1);
	    } else if (i == -1) {
		unlink(tempfile);
		fprintf(of, "%s\n", mesg(17, lang));
	    } else
		goto el;
	    break;
#endif
	case -1:
	default:
#ifdef EXT_CMD
	    if (parse_ext(command, p)) {
		reply_required++;
		fprintf(of, "--%s\n\n", command);
		log("Invoke external command %s for %s\n",
		     command, replyaddr);
		fflush(of);
		switch (fork()) {
		case -1:
		    fprintf(of, "%s\n", mesg(17, lang));
		    break;
		case 0:
		    close(0);
		    open("/dev/null", O_RDONLY);	/* should be 0 :-) */
		    close(1);
		    dup(fileno(of));	/* stdout */
		    close(2);
		    dup(1);	/* stderr */
		    for (i = 3; i <= _NFILE; i++)
			close(i);

		    if (execv(av[0], av) == -1)
			if (errno != ENOEXEC)
			    bexit(1);
		    for (i = 0; av[i]; i++);
		    for (; i >= 0; i--)
			av[i + 1] = av[i];
		    av[0] = "sh";
		    execv(SHELL, av);
		    bexit(1);

		default:
		    wait();
		}
	    } else {
#endif				/* EXT_CMD */
		log("ERR: Bad command: %s %s from %s\n",
		    command, p, replyaddr);
		if (line_bad++ > 100)
		    goto el;
#ifdef EXT_CMD
	    }
#endif
	}			/*switch*/
    }
  el:
    if (!line_read || line_bad > 20) {
	fprintf(of, mesg(8, lang));
	reply_required++;
	help(of);
    }
  eee:
#ifdef SEND_BY_SMTP
    if (h>0)
	closesmtpsession(h);
#endif
#ifdef NNTP_ONLY
    if (sockfd!=-1 && !dont_disconnect) {
	fputs("QUIT\r\n", fsocket);
	fflush(fsocket);
	sofgets(snntp, sizeof(snntp), fsocket);
#ifdef DEBUG
	printf(">> QUIT\n<< %s", snntp);
#endif
    }
    if (fsocket) {
	fclose(fsocket);
	fsocket=NULL;
	sockfd=-1;
    }
    if (sockfd!=-1) {
	close(sockfd);
	sockfd=-1;
    }
#endif
    close_base();  /* avoid waiting loops */

    if (index_cnt) {
	switch ((i = fork())) {
	case -1:
	    fprintf(of, mesg(32, lang));
	    break;
	case 0:
	    for (i = 0; i <= _NFILE; i++)
		close(i);
	    open("/dev/null", O_RDWR);	/* should be 0 :-) */
	    dup(0);		/* stdout */
	    dup(1);		/* stderr */

	    av[0]="lister";
	    av[1]="-i";
	    av[2]=(char *)NULL;
	    execv(LISTER, av);
	    exit(1);

	default:
	    status = 1;
#if defined(_POSIX_SOURCE) || defined(ISC)
	    waitpid(i, &status, 0);
#else
	    wait(&status);
#endif
	}

	if (status == 0)
	    fprintf(of, mesg(33, lang),index_art,index_cnt);
	else
	    fprintf(of, mesg(32, lang));
    }
    if (find_cnt) {
	switch ((i = fork())) {
	case -1:
	    fprintf(of, mesg(51, lang));
	    break;
	case 0:
	    for (i = 0; i <= _NFILE; i++)
		close(i);
	    open("/dev/null", O_RDWR);	/* should be 0 :-) */
	    dup(0);		/* stdout */
	    dup(1);		/* stderr */

	    av[0]="lister";
	    av[1]="-f";
	    av[2]=(char *)NULL;
	    execv(LISTER, av);
	    exit(1);

	default:
#if defined(_POSIX_SOURCE) || defined(ISC)
	    waitpid(i, &status, 0);
#else
	    wait(&status);
#endif
	}

	if (status == 0)
	    fprintf(of, mesg(52, lang),find_art,find_cnt);
	else
	    fprintf(of, mesg(51, lang));
    }
    fclose(of);

    if (!reply_required) {
	unlink(tempfile);
	exit(0);
    }
    /*
	 * Call a mailer
	 */
    sprintf(sname, "%s@%s", NEWSSENDER, hn);
    av[0] = MAILER;
    av[1] = SENDER_FLAG;
    av[2] = sname;
    av[3] = replyaddr;
    av[4] = (char *) NULL;

    switch(i = fork()){
    case 0:
	/* new proc */
	for (i = 0; i <= _NFILE; i++)
		close(i);
	open(tempfile, 0);	/* should be 0 :-) */
	open("/dev/null", 1);	/* fd 1 */
	dup(1);			/* fd 2 */
	unlink(tempfile);
	execv(MAILER, av);
	exit(1);
    case -1:
	unlink(tempfile);
	exit(75);		/* EX_TEMPFAIL */
    default:
#if defined(_POSIX_SOURCE) || defined(ISC)
	waitpid(i, &status, 0);
#else
	wait(&status);
#endif
    }
    unlink(tempfile);
    exit((int)status);
}
