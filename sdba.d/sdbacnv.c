/*
 * Subscribers Data Base Access - program for local maintance of
 * subscribers data base.
 *
 * Function use by sdba and convert.
 *
 * (C) Copyright 1992-1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software. 
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[]="@(#)sdbacnv.c 2.0 03/07/94";
#endif

#include <sys/types.h>
#include <stdio.h>
#include "dbsubs.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_STDLIB 
#include <stdlib.h>
#endif
#include "conf.h"
#include "nnrp.h"
#ifdef __linux__
#undef NULL
#define NULL 0
#endif

extern int sofgets(char *,int,FILE *);

#define ISspace(x) ( x==' ' || x=='\t' )

extern long time();
extern char *getpass(),*crypt();
extern int sav_hdr();
extern ulong get_host_addr();

#ifdef PASSWD_PROTECT
int
set_passwd()
{
char	*passwd,*passbuf;
char	salt[3],pbuff[9];
long tm;
int stay_open,count;

count=0;
if(fb==NULL){
stay_open=0;
if(open_base(omode)){
err:
	pberr("set_passwd",stderr);
	return 1;
}
}
else
	stay_open=1;
	
printf("Change password for data base\n");
if(header.passwd[0]){
if((passbuf=getpass("Enter old base password: "))==NULL)
	goto perr;
(void)strncpy(salt,header.passwd,2);
if((passwd=crypt(passbuf,salt))==NULL)
	goto perr;
if(strcmp(passwd,header.passwd)){
bserrno=BS_BADPASS;
close_base();
goto err;
}
}
dnl:
if((passbuf=getpass("Enter new base password: "))==NULL)
	goto perr;
strncpy(pbuff,passbuf,sizeof(pbuff));
pbuff[sizeof(pbuff)-1]='\0';
if((passbuf=getpass("Re-enter new base password: "))==NULL)
	goto perr;
if(strcmp(pbuff,passbuf)){
if(++count>5)
	goto perr;
printf("Passwords does not match. Try again.\n");
goto dnl;
perr:
close_base();
fprintf(stderr,"Can't set password\n");
return 1;
}

tm=time((long *)NULL)%100;

sprintf(salt,"%02d",tm);

if((passwd=crypt(pbuff,salt))==NULL)
	goto perr;
tm=tm%10;

(void)strncpy(salt,passwd+tm,2);

if((passwd=crypt(pbuff,salt))==NULL)
	goto perr;

strncpy(header.passwd,passwd,sizeof(header.passwd));

if(fseek(fb,0L,SEEK_SET))
	goto perr;
if(lock(fb,F_WRLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}
if(fwrite((char *)&header,sizeof(header),1,fb)!=1)
	goto perr;
if(lock(fb,F_RDLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}
if(!stay_open)
	close_base();
return 0;
}
#endif

int
create(flag)
int flag;
{
char filename[200];
int c;

sprintf(filename,"%s/%s",BASEDIR,SUBSBASE);
if(!flag && access(filename,F_OK)==0)
return 2;

unlink(filename);
if((c=open(filename,O_RDWR|O_CREAT|O_TRUNC,0644))==-1){
	perror("sdba: create");
	exit(1);
}
fb=fdopen(c,"w");
header.magic=BH_MAGIC;
header.user=NULL;
header.group=NULL;
header.freesp=NULL;
{register i;
for(i=0;i<sizeof header.passwd;i++)
header.passwd[i]=0;
}
if(fwrite((char *)&header,sizeof(header),1,fb)!=1){
	c=1;
	goto err;
}
else
	c=0;
#ifdef PASSWD_PROTECT
c=set_passwd();
#endif
err:
fclose(fb);
header.magic=NULL;
fb=NULL;
return c;
}

int
addgroup(_group,flag,param)
char *_group,*flag,*param;
{
	char filename[200];
	FILE *fr;
	int stay_open;
	char *p,*q;

	strncpy(group,_group,sizeof(group));
	group[sizeof(group)-1]='\0';
#ifdef NNTP_ONLY
	if (connect_nnrp()) {
		fprintf(stderr, "Can't connect to NNTP-server\n");
		return 1;
	}
	fprintf(fsocket, "GROUP %s\r\n", group);
	fflush(fsocket);
	if (!sofgets(snntp, sizeof(snntp), fsocket)) {
		fprintf(stderr, "Can't get active\n");
		return 1;
	}
	if (snntp[0]=='2') goto proc;
#else
	fr=fopen(ACTIVE,"r");
	if (fr == NULL) {
		fprintf(stderr, "Can't open " ACTIVE);
		return 1;
	}
	while(fgets(filename,sizeof filename,fr)!=NULL){
		register char *q;
		q=filename;
		if(*q=='\n')
			continue;
		while( *q && !ISspace(*q) ) q++;
		*q=0;
		if(strcmp(group,filename)==0)
			goto proc;
	}
	fclose(fr);
#endif
	fprintf(stderr,"Group %s not found in Cnews active file\n",group);
	return 1;


proc:
#ifndef NNTP_ONLY
	fclose(fr);
#endif

	if(fb==NULL) {
		stay_open=0;
		if(open_base(omode)) {
			pberr("open_base: addgroup",stderr);
			goto err2;
		}
	}
	else
		stay_open=1;

	if(lock(fb,F_WRLCK)<0) {
		bserrno=BS_ENOLOCK;
		goto err;
	}

	if(!getgroup(_group)) {
		fprintf(stdout,"Group %s already exist\n",group );
		if(!stay_open)
			close_base();
		return 0;
	}
	else
		if(bserrno!=BS_ENOGROUP)
			goto err;
	if(!wbuff[0])
		goto adde;
	p=wbuff;
	q=index(p,'.');
	while(p){
		char dot_at;
		dot_at='.';
		if (q==NULL) {
			if (strlen(p)>=MAX_STR_EL)
				q=p+MAX_STR_EL-1;
		}
		else if (q-p>=MAX_STR_EL)
			q=p+MAX_STR_EL-1;
		if(q) {
			dot_at=*q;
			*q++=0;
		}

		st_entry.magic=SE_MAGIC;
		st_entry.entry=NULL;
		st_entry.reserved_1=NULL;
		st_entry.reserved_2=NULL;
		st_entry.dot_at=(dot_at=='.') ? '.' : 0;
		if((st_entry.size=strlen(p))>=MAX_STR_EL) {
			printf("Too long element: %s\n",p);
#ifndef CONVERT
			goto err2;
#else
			return 1;
#endif
		}
		{	register i;
			for(i=st_entry.size;i<MAX_STR_EL;i++) st_entry.name[i]=0;
		}
		(void)strncpy(st_entry.name,p,sizeof(st_entry.name));

		if(soffset==coffset) {
			st_entry.next=soffset;
			st_entry.up_ptr=poffset;
			st_entry.down_ptr=NULL;
			coffset=sav_entry(SE_MAGIC,(char *)&st_entry,sizeof(st_entry));
			if(poffset==NULL) {
				header.group=coffset;
				sav_hdr();
				goto nx;
			}
			else
				if(fseek(fb,poffset+offset(st_entry,down_ptr),SEEK_SET))
					goto serr;
		}
		else if(coffset==NULL) {
			st_entry.next=NULL;
			st_entry.down_ptr=NULL;
			coffset=sav_entry(SE_MAGIC,(char *)&st_entry,sizeof(st_entry));
			if(fseek(fb,poffset+offset(st_entry,next),SEEK_SET))
				goto serr;
		}
		else if(soffset==NULL) {
			soffset=coffset;
			st_entry.up_ptr=soffset;
			st_entry.down_ptr=NULL;
			st_entry.next=NULL;

			coffset=sav_entry(SE_MAGIC,(char *)&st_entry,sizeof(st_entry));
			if(fseek(fb,soffset+offset(st_entry,down_ptr),SEEK_SET))
				goto serr;
		}
		else{
			st_entry.next=coffset;
			st_entry.down_ptr=NULL;
			coffset=sav_entry(SE_MAGIC,(char *)&st_entry,sizeof(st_entry));
			if(fseek(fb,poffset+offset(st_entry,next),SEEK_SET)){
serr:
				bserrno=BS_SYSERR;
				goto err;
			}
		}
		if(fwrite((char *)&coffset,sizeof(coffset),1,fb)!=1)
			goto serr;
nx:
		soffset=NULL;

		if (dot_at!='.')
			*--q=dot_at;
		p=q;
		if(p){
			q=index(p,'.');
			poffset=coffset;
		}
	}/* while */

adde:
	gr_entry.magic=GE_MAGIC;
	gr_entry.begin=NULL;
	gr_entry.flag=0;
	gr_entry.nntp_host=NULL;
	gr_entry.nntp_lastused=time((long *)NULL);
	gr_entry.reserved_1=NULL;
	gr_entry.reserved_2=NULL;
	gr_entry.access=G_DEFACCESS;
	gr_entry.size=strlen(group);
	gr_entry.s_group=coffset;

	if(flag!=NULL){
		if(index(flag,'U')!=NULL)
			gr_entry.flag|=GF_NOUNS;

		if(index(flag,'M')!=NULL)
			gr_entry.flag|=GF_COM;

		if(index(flag,'S')!=NULL)
			gr_entry.flag|=GF_NOSUBS;

		if(index(flag,'C')!=NULL)
			gr_entry.flag|=GF_NOCHMOD;

		if(index(flag,'R')!=NULL)
			gr_entry.flag|=GF_RONLY;

		if(index(flag,'X')!=NULL)
			gr_entry.flag|=GF_NOXPOST;

		if(index(flag,'O')!=NULL)
			gr_entry.flag|=GF_SONLY;

#if defined(NNTP) && !defined(CONVERT)
		if(index(flag,'A')!=NULL && index(flag,'N')!=NULL)
			gr_entry.flag|=GF_NNTPALWAYS;

		if(index(flag,'N')!=NULL){
			if((gr_entry.nntp_host=get_host_addr(param))!=NULL)
				gr_entry.flag|=GF_NNTP;
			else
				fprintf(stderr,"Can't find IP address for host %s\nGroup will be create without NNTP flag\n",param);
		}
#endif

	}

	goffset=sav_entry(GE_MAGIC,(char *)&gr_entry,sizeof(gr_entry));
	if(fseek(fb,coffset+offset(st_entry,entry),SEEK_SET))
		goto serr;
	if(fwrite((char *)&goffset,sizeof(goffset),1,fb)!=1)
		goto serr;

	printf("New group %s created\n",group);

	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}
	sprintf(filename, "%s/%s", WORKDIR, NEWGROUP);
	if ((fr = fopen(filename, "a")) != NULL) {
		fprintf(fr, "%s\n", group);
		fclose(fr);
	}

	if(!stay_open)
	close_base();
	return 0;

err:
	pberr("sdba: addgroup",stderr);
	err2:
	retry_base();
	return 1;
}

/*
 * Compare the command words
 */
int
cmdcmp(cmd, pattern)
	register char *cmd;
	register char *pattern;
{
	int n = 0;

	while( *cmd && (*cmd&~040) == *pattern )
		cmd++, pattern++, n++;
	return (n >= 3) && (*cmd == '\0');
}
