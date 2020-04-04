/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software. 
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[]="@(#)dbfunc.c 1.0 01/08/94";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "dbsubs.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "conf.h"

#ifdef __STDC__
extern char *malloc(size_t),*getpass(const char *),*crypt(const char *, const char *);
#ifndef OS2
extern long time(time_t *);
#endif
extern int restore(char *,char *,FILE *),chk_base(void);
extern unsigned sleep(unsigned int);
extern FILE *save(char *, char *, int);
void mail_err();
int open_base(int mode),close_base(void),retry_base(void);
#else
extern char *malloc(),*getpass(),*crypt();
extern long time();
extern int restore(),chk_base();
extern unsigned sleep();
extern FILE *save;
void mail_err(char *);
int open_base(),close_base(),retry_base();
#endif

extern char *pname;

struct base_header	header;
struct subs_chain	sb_entry;
struct string_chain	st_entry;
struct user_rec		ui_entry;
struct domain_rec	dm_entry;
struct group_rec	gr_entry;
FILE	*fb;
char	group[160],wbuff[160];
long	boffset,soffset,uoffset,doffset,goffset,coffset,poffset;
int	bserrno,omode;
char 	*bufb;

#ifndef _POSIX_SOURCE
void sigcont()
{return;}
#endif
int
open_base(mode)
int mode;
{
char	filename[200];
#if MAXMEM && defined(ISC)
long	size;
struct stat flstat;
#endif
#if defined(SDBA) && defined(PASSWD_PROTECT)
char	*passwd,*passbuf;
char	salt[3];
#endif

omode=mode;
#ifndef NNTPSERV
if(access(STOP_F,F_OK)==0)
#ifndef SDBA
	exit(1);
#else
	fprintf(stdout,"PROCESSING OF REQUESTS QUEUE STOPED !!!\n\n");
#endif
#endif

#ifdef SLOW /* gul */
/* Now check base for errors */
if(mode!=OPEN_RDONLY)
if(chk_base()){
	bserrno=BS_FATAL;
#ifndef SDBA
		mail_err(
#else
		fprintf(stderr,
#endif
"	Fatal error !!!\n\
Database file corrupted !!! Processing of requests stoped !!!\n");
close(open(STOP_F,O_CREAT|O_TRUNC,0444)); /* stop processing of requests */
		exit(1);
}
#endif /* gul */

if(header.magic){
bserrno=BS_RECALL;
goto err;
}
(void)sprintf(filename,"%s/%s",BASEDIR,SUBSBASE);

if(mode==OPEN_RDONLY || mode==OPEN_NOSAVE){
	if((fb=fopen(filename,mode==OPEN_NOSAVE?"r+":"r"))==NULL){
open_err:
		bserrno=BS_FATAL;
#ifndef SDBA
		mail_err(
#else
		fprintf(stderr,
#endif
"	Fatal error in fopen() call!!!\n
Check database file !!! Processing of requests stoped !!!\n");
		goto err;
	} 
}
else if(mode==OPEN_RDWR){
#ifdef SLOW
	if((fb=save(BASEDIR,SUBSBASE,0))==NULL)
#else
	if((fb=fopen(filename,"r+"))==NULL)
#endif
		goto open_err;	
}
else{
	bserrno=BS_BADPARAM;
	goto err;
}
#if MAXMEM && defined(ISC) /* under isc it work very fast, in other system very slow */
	/* set in memory buffer for database */
	if(mode==OPEN_RDONLY){
	if(!fstat(fileno(fb),&flstat))
		size=flstat.st_size;
	else
		size=1024;
	if(MAXMEM<(size/1024))
		size=MAXMEM*1024;
	if((bufb=malloc(size))!=NULL)
		(void)setvbuf(fb,bufb,_IOFBF,size);
	}
#endif

boffset=soffset=uoffset=goffset=coffset=poffset=0;

if(lock(fb,mode==OPEN_RDONLY?F_RDLCK:F_WRLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}

if(fread((char *)&header,sizeof(struct base_header),1,fb)!=1){
	bserrno=BS_SYSERR;
	goto err;}


if(lock(fb,F_RDLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}

#ifdef SDBA
if(header.passwd[0]&&mode){
#ifdef PASSWD_PROTECT
if((passbuf=getpass("Enter base password: "))==NULL)
	goto perr;
(void)strncpy(salt,header.passwd,2);
if((passwd=crypt(passbuf,salt))==NULL)
	goto perr;
if(strcmp(passwd,header.passwd)){
perr:
close_base();
bserrno=BS_BADPASS;
pberr("sdba",stderr);
exit(1);
}
/* #else
close_base();
bserrno=BS_BADPASS;
goto err; */
#endif
}
#endif
bserrno=BS_OK;
return 0;

err:
#ifdef SDBA
	pberr("sdba",stderr);
#else
	log("ERR: FATAL ERROR: %s can't open database!\n", pname);
#endif

close(open(STOP_F,O_CREAT|O_TRUNC,0444)); /* stop processing of requests */

return 1;
}

int
close_base()
{
#ifdef SLOW
/* check base for errors before closing */
if(omode!=OPEN_RDONLY)
if(chk_base()){
	bserrno=BS_FATAL;
#ifndef SDBA
	log("ERR: FATAL ERROR: %s Checkbase return error! Base restored from copy.\n",
	 pname);
#endif
	retry_base();		
	exit(1);
}
#endif
(void)fclose(fb);
header.magic=(long)NULL;
boffset=soffset=uoffset=goffset=coffset=poffset=0;
fb=NULL;
bserrno=BS_OK;
return 0;
}

int
retry_base()
{
#ifdef SLOW
if(restore(BASEDIR,SUBSBASE,fb)){
	bserrno=BS_FATAL;
	pberr("retry_base",stderr);
	return 1;
}
fb=NULL;
bserrno=BS_OK;
return 0;
#else
bserrno=BS_FATAL;
pberr("retry_base",stderr);
return 1;
#endif
}

void
mail_err(message)
char *message;
{
FILE *sm;
char cmd[256];
char host[80];

gethost(host,80);

sprintf(cmd,"%s %s %s@%s %s %s",MAILER,SENDER_FLAG,NEWSSENDER,host,
#ifdef MFLAG_QUEUE
MFLAG_QUEUE,
#else
"",
#endif
ADMIN);
sm=popen(cmd,"w");
fprintf(sm,"From: %s@%s\n",NEWSSENDER,host);
fprintf(sm,"To: %s\n",ADMIN);
fprintf(sm,"Subject: !!! Error in newsserver !!!\n");
#ifdef USE_XCLASS
fprintf(sm,"X-Class: fast\n");
#endif
#ifdef USE_PRECEDENCE
fprintf(sm,"Precedence: air-mail\n\n");
#endif
fprintf(sm,"%s\n",message);
pclose(sm);
return;
}
