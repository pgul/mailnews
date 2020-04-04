/*
 * Subscribers Data Base Access - program for local maintance of
 * subscribers data base.
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
static char sccsid[]="@(#)sdba.c 2.0 02/28/94";
#endif

#define usg(a) fprintf(stderr,a)

#include <sys/types.h>
#include <stdio.h>
#include "coms.h"
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_STDLIB 
#include <stdlib.h>
#endif
#include <fcntl.h>
#include "conf.h"
#include "dbsubs.h"
#include "version.c"
#include "nnrp.h"

#ifdef NNTP_ONLY
int sofgets(char *, int, FILE *);
#else
#define sofgets(str,size,file)	fgets(str,size,file)
#endif

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *malloc(),*calloc();
extern void free();
extern int subscribe(),check(),set_uparam(),detlang(),wildmat(),sav_hdr(),atoi();
extern int isdis(),ch_all(),chk_base(),addgroup(),create(),set_passwd(),cmdcmp();
extern long time();
extern char *getpass(),*crypt();

static int prgrp(),rmgroup(),del_domain(),chk_age(),adddomain();
static int setflags(),prdomain(),chkact(),disable();
#ifdef NNTP
static int sethost();
extern char *inet_ntoa();
#endif

char pname[80];

static int
whatcmd(command)
char * command;
{
register i,j,k;
i=j=0;
k=-1;
while(command[i]){
if(coms[j][i]==(command[i]&0xdf))
if(k==-1)
	if(coms[j+1][i]==(command[i]&0xdf))
		i++;
	else
			k=j;
else
i++;
else
if(k==-1){
if((++j)>=CMDN)
	break;
}
else{
k=-1;
break;}
}
return k;
}

static int
checkown()
{
char filename[200];
struct stat buf;
ushort puid, pgid;
int rs;

rs=0;
puid=geteuid();
pgid=getegid();

snprintf(filename,sizeof(filename),"%s/%s",BASEDIR,SUBSBASE);
if(stat(filename,&buf)){
	perror("stat"); 
	return 1;}
if(buf.st_uid!=puid || buf.st_gid!=pgid){
	fprintf(stderr,"Invalid owner of data base file: %s\n",filename);
	rs=1;}

return rs;
}

int main(argc,argv)
int argc;
char **argv;
{
int stat=0;

#ifdef OS2
  extern int _fmode_bin;
  _fmode_bin=1;
#endif

strncpy(pname, argv[0], sizeof(pname));

if(argc==1)
goto usage;

if(whatcmd(argv[1])!=C_CREATE)
if(checkown())
	exit(1);

omode=OPEN_RDWR;

switch(whatcmd(argv[1])){
case C_VERSION:
printf("E-mail to USENET gateway. %s \n\t\t\t  (C) 1991 Vadim Antonov\n\
\t\t\t  (C) 1992-1994 Stanislav Voronyi\n",version);
break;
case C_CREATE:
stat=create(0);
if(stat==2){
register c;
printf("Data base already exists. Remove y/n [n]: ");
c=getchar();
if(c=='y' || c=='Y')
stat=create(1);
else
stat=0;
}
break;
#ifdef PASSWD_PROTECT
case C_PASSWD:
stat=set_passwd();
break;
#endif
case C_RELEASE:
if(unlink(STOP_F)){
	perror("sdba");
	exit(1);
}
else{
#ifdef DAEMON
register c;
printf("Run jobs in queue now ? (y/n) [n]: ");
c=getchar();
if(c=='y' || c=='Y')
	execl(DAEMON,"newsserv","-f",(char *)NULL);
else
#endif
	exit(0);
}
case C_ADDGROUP:
if(argc<4)
stat=addgroup(argv[2],NULL,NULL);
else
stat=addgroup(argv[2],argv[3],argv[argc-1]);
break;
#ifdef UNKNOWN_DENIED
case C_ADDDOMAIN:
stat=adddomain(argv[2]);
break;
#endif
case C_RMGROUP:
stat=rmgroup(&argv[2]);
break;
case C_CHECK:
switch(whatcmd(argv[2])){
case C_USER:
stat=check(argv[3],stdout,0,1);
break;
case C_GROUP:
stat=prgrp(&argv[3]);
break;
case C_ACTIVE:
stat=chkact(&argv[3]);
break;
case C_DOMAIN:
stat=prdomain(argv[3],0);
break;
case C_SITE:
stat=prdomain(argv[3],1);
break;
#if AGE_TIME
case C_AGE:
stat=chk_age();
break;
#endif
case C_BASE:
stat=chk_base();
break;
default:
goto usage;
}
break;
case C_RMDOMAIN:
stat=del_domain(argv[2]);
break;
case C_RMUSER:
stat=subscribe(argv[2],(char **)NULL,FORGET,0,stdout);
break;
case C_SUBSCRIBE:
stat=subscribe(argv[2],&argv[3],SUBS,0,stdout);
break;
case C_CNEWS:
stat=subscribe(argv[2],&argv[3],CNEWS,0,stdout);
break;
case C_RFEED:
stat=subscribe(argv[3],&argv[4],RFEED,atoi(argv[2]),stdout);
break;
case C_FEED:
stat=subscribe(argv[2],&argv[3],FEED,0,stdout);
break;
case C_EFEED:
stat=subscribe(argv[2],&argv[3],EFEED,0,stdout);
break;
case C_UNSUBSCRIBE:
stat=subscribe(argv[2],&argv[3],UNSUBS,0,stdout);
break;
case C_SUSPEND:
stat=set_uparam(argv[2],US_SUSP,1,stdout);
break;
case C_DISABLE:
stat=disable(argv[2],1);
break;
case C_ENABLE:
stat=disable(argv[2],0);
break;
case C_LANG:
stat=set_uparam(argv[2],US_LANG,detlang(argv[3]),stdout);
break;
case C_LIMIT:
stat=set_uparam(argv[2],US_LIMIT,atoi(argv[3]),stdout);
break;
case C_RESUME:
stat=set_uparam(argv[2],US_SUSP,0,stdout);
break;
case C_FORMAT:
switch(whatcmd(argv[3])){
case C_NEW:
stat=set_uparam(argv[2],US_FMT,1,stdout);
break;
case C_OLD:
stat=set_uparam(argv[2],US_FMT,0,stdout);
break;
default:
stat=1;
}
break;
#if AGE_TIME
case C_AGING:
switch(whatcmd(argv[3])){
case C_ON:
stat=set_uparam(argv[2],US_AGING,1,stdout);
break;
case C_OFF:
stat=set_uparam(argv[2],US_AGING,0,stdout);
break;
default:
stat=1;
}
break;
#endif
case C_NEWGRP:
switch(whatcmd(argv[3])){
case C_ON:
stat=set_uparam(argv[2],US_NONGRP,0,stdout);
break;
case C_OFF:
stat=set_uparam(argv[2],US_NONGRP,1,stdout);
break;
default:
stat=1;
}
break;
case C_LHELP:
switch(whatcmd(argv[3])){
case C_ON:
stat=set_uparam(argv[2],US_NLHLP,0,stdout);
break;
case C_OFF:
stat=set_uparam(argv[2],US_NLHLP,1,stdout);
break;
default:
stat=1;
}
break;
case C_SETFLAG:
stat=setflags(&argv[2],argv[argc-2],argv[argc-1]);
break;
#ifdef NNTP
case C_SETHOST:
stat=sethost(&argv[2],argv[argc-1]);
break;
#endif
default:
case C_HELP:
stat=0;
usage:
usg("usage: sdba command [parameters]\n");
usg("Permissible commands are:\n");
usg("help                                - print this message\n");
usg("create                              - create new data base\n");
#ifdef DAEMON
usg("release				 - release suspended requests queue\n");
#endif
#ifdef PASSWD_PROTECT
usg("passwd                              - set password for protect database\n");
#endif
usg("addgroup group_name [flags]         - add new group to data base\n");
#ifdef UNKNOWN_DENIED
usg("adddomain domain_addr                - add new domen to data base\n");
#endif
usg("rmgroup group_name|pattern          - delete newsgroup(s) from data base\n"); 
usg("setflag group_name|pattern flags    - set (change) group(s) flags\n");
#ifdef NNTP
usg("sethost group_name|pattern host     - set host name for NNTP group(s)\n");
#endif
usg("check group group_name|pattern      - print all subscribers for group(s)\n");
usg("check active [-m] [-d] [-y]         - compare active and data base group list\n");
usg("check user user_addr                - print all group subscribed by user\n");
usg("check domain domain_addr|empty      - print subscribed group for all user\n");
usg("                                      of specified domain\n");
usg("check site site_name                - as previous, but for specified site\n");
#if AGE_TIME
usg("check age                           - check aging of user records & remove old\n");
#endif
usg("check base                          - check is database correct\n");
usg("lang user_addr lang                 - set language for diagnostic messages\n");
usg("limit user_addr limit               - set notify list length limit\n");
#if AGE_TIME
usg("aging user_addr on|off              - set aging for user\n");
#endif
usg("rmdomain domain_addr                - delete all users for domain domain_addr\n");
usg("rmuser user_addr                    - delete user from base\n");
usg("cnews site group_name               - subscribe site in cnews mode\n");
usg("subscribe user_addr group_name      - subscribe user to group(s)\n");
usg("rfeed size user_addr group_name     -   ''    ''    '' in rfeed mode\n");
usg("feed user_addr group_name           -   ''    ''    '' in feed mode\n");
usg("efeed user_addr group_name          -   ''    ''    '' in efeed mode\n");
usg("unsubscribe user_adr group_name|all - unsubscribe user from group(s)\n");
usg("format user_adr new|old             - set format of articles list for user\n"); 
usg("suspend user_addr                   - suspend sending newses for user\n");
usg("resume user_addr                    - resume sending after suspend\n");
usg("disable domain_addr                 - disable access to server for domain\n");
usg("enable domain_addr                  - enable access to server after disable\n");
}
if(fb!=NULL)
	close_base();
#ifdef NNTP_ONLY
	if (fsocket)
	{
		fprintf(fsocket, "QUIT\r\n");
		fflush(fsocket);
		sofgets(snntp, sizeof(snntp), fsocket);
		fclose(fsocket);
	}
#endif
exit(stat);
}

static	int allmode,csmode,csnmode,count,bi;
char **groups;
int (*func)();

static int
_prg(long entry, char *name)
{
	count++;

	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;		
	if(csmode){
		if(gr_entry.begin!=NULL)
			fprintf(stdout,"%s\n",name);
		return 0;
	}
	if(csnmode){
		if(gr_entry.begin==NULL)
			fprintf(stdout,"%s\n",name);
		return 0;
	}
	printf("Group %s\n",name);
	if(gr_entry.flag){
		if(gr_entry.flag&GF_NOSUBS)
			printf("   Subscription to this group disabled\n");
		if(gr_entry.flag&GF_NOUNS)
			printf("   Unsubscription from this group disabled\n");
		if(gr_entry.flag&GF_NOCHMOD)
			printf("   Changing of subscription mode disabled\n");
		if(gr_entry.flag&GF_RONLY)
			printf("   This group is READONLY\n");
		if(gr_entry.flag&GF_SONLY)
			printf("   This group is subscribers only\n");
		if(gr_entry.flag&GF_COM)
			printf("   This group has commercial status\n");
		if(gr_entry.flag&GF_NOXPOST)
			printf("   Crossposting in this group disabled\n");
#ifdef NNTP
		if(gr_entry.flag&GF_NNTP)
			printf("    This is NNTP %sgroup from %s\n",gr_entry.flag&GF_NNTPALWAYS?"always needed ":"",get_host_name(gr_entry.nntp_host));
#endif
	}
	if(gr_entry.begin==NULL){
		fprintf(stdout,"Nobody subscribed to group %s\n\n",name);
		return 0;
	}

	fprintf(stdout,"Subscribers of group %s:\n",name);

	boffset=gr_entry.begin;
	while(boffset!=NULL){
		if(fseek(fb,boffset,SEEK_SET))
			goto serr;
		if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
			goto serr;
		if(sb_entry.group!=entry){
			fprintf(stderr,"Data base damaged.\n");
			bserrno=BS_BASEERR;
			return 1;
		}
		if(get_uaddr(sb_entry.s_user)!=NULL){
			fprintf(stdout,"\t%s",user);
			switch(sb_entry.mode){
			case SUBS:
				fprintf(stdout,"\n");
				break;
			case FEED:
				fprintf(stdout,"\tfeed mode\n");
				break;
			case EFEED:
				fprintf(stdout,"\tefeed mode\n");
				break;
			case RFEED:
				fprintf(stdout,"\trfeed %d mode\n",sb_entry.m_param);
				break;
			case CNEWS:
				fprintf(stdout,"\tcnews mode\n");
				break;
			}
		}
		else
			return 1;
		boffset=sb_entry.group_next;
	}
	printf("\n");
	return 0;
}

static int
_chh(long entry, int *parm, char *name)
{
	register i,do_ok;


	for(i=bi,do_ok=allmode;groups[i]!=NULL;i++){
		if(*groups[i]=='!' && wildmat(name,groups[i]+1)){
			do_ok=0;
			break;
		}
		else if(!do_ok && wildmat(name,groups[i]))
			do_ok=1;
	}
	if(do_ok){
		goffset=entry;
		return ((func)(entry,name));
	}
	else
		return 0;
}

/*
 * Print all subscribers of a group
 */

static int
prgrp(char **_group)
{
	int stay_open;

	allmode=csmode=csnmode=count=bi=0;
	groups=_group;

	if(cmdcmp(groups[0],"ALL"))
		bi=allmode=1;
	if(cmdcmp(groups[0],"NEED")){
		bi=csmode=1;
		if(groups[1]==NULL)
			allmode=1;
		if(cmdcmp(groups[1],"ALL")){
			bi=2;
			allmode=1;
		}
	}
	if(cmdcmp(groups[0],"NULL")){
		bi=csnmode=1;
		if(groups[1]==NULL)
			allmode=1;
		if(cmdcmp(groups[1],"ALL")){
			bi=2;
			allmode=1;
		}
	}
		
	if(fb==NULL){
		stay_open=0;
		if(open_base(OPEN_RDONLY)){
			pberr("sdba",stderr);
			goto err;
		}
	}
	else
		stay_open=1;
	
	func=_prg;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
		pberr("sdba",stderr);
		goto err;
	}

	if(!count)
		fprintf(stdout,"Specified group(s) does not exist\n");
	if(!stay_open)
		close_base();
	return 0;

err:
	retry_base();
	return 1;
}

static int
adddomain(char *domain)
{
	int stay_open;
	char leftname[160];

	snprintf(leftname,sizeof(leftname),"a@%s",domain);
		
	if(fb==NULL){
		stay_open=0;
		if(open_base(OPEN_RDWR)){
			pberr("sdba",stderr);
			goto err;
		}
	}
	else
		stay_open=1;
	
	if(getuser(leftname,0,2)){
		pberr("sdba",stderr);
		goto err;
	}
	
	if(!stay_open)
		close_base();
	return 0;

err:
	retry_base();
	return 1;
}

static int
_delgr(long entry,char *name)
{
	count++;
	if(lock(fb,F_WRLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}
	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
err:
		return 1;
	}
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;		
	if(gr_entry.begin!=NULL){

		boffset=gr_entry.begin;
		while(boffset!=NULL){
			if(fseek(fb,boffset,SEEK_SET))
				goto serr;
			if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
				goto serr;
			if(sb_entry.group!=entry){
				fprintf(stderr,"Data base damaged.\n");
				bserrno=BS_BASEERR;
				goto err;
			}

			if(sb_entry.user_next){
				if(fseek(fb,sb_entry.user_next+offset(sb_entry,user_prev),SEEK_SET))
					goto serr;
				if(fwrite((char *)&sb_entry.user_prev,sizeof(sb_entry.user_prev),1,fb)!=1)
					goto serr;
			}
			if(sb_entry.user_prev){
				if(fseek(fb,sb_entry.user_prev+offset(sb_entry,user_next),SEEK_SET))
					goto serr;
			}
			else{
				if(fseek(fb,sb_entry.user+offset(ui_entry,begin),SEEK_SET))
					goto serr;
			}
			if(fwrite((char *)&sb_entry.user_next,sizeof(sb_entry.user_next),1,fb)!=1)
				goto serr;

			if(get_uaddr(sb_entry.s_user)!=NULL)
				printf("%s -- OK\n",user);
			else
				goto err;
			add_free(boffset);
			boffset=sb_entry.group_next;
		}
	}
	soffset=gr_entry.s_group;
	add_free(goffset);
nit:
	if(fseek(fb,soffset,SEEK_SET))
		goto serr;
	if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;
	poffset=st_entry.next;
	if (st_entry.entry==goffset) { /* gul */
		st_entry.entry=NULL;
		if (st_entry.down_ptr) {
			if(fseek(fb,soffset+offset(st_entry,entry),SEEK_SET))
				goto serr;
			if(fwrite((char *)&st_entry.entry,sizeof(st_entry.entry),1,fb)!=1)
				goto serr;
			goto end;
		}
	}
	if((st_entry.down_ptr==NULL) && (st_entry.entry==NULL /* gul */)){
		if((coffset=st_entry.up_ptr)==NULL){
			if(header.group==soffset){
				header.group=poffset;
				sav_hdr();
				add_free(soffset);
				goto end;
			}
			else{
				coffset=header.group;
				goto rmfc;
			}				
		}
		else{
			if(fseek(fb,coffset,SEEK_SET))
				goto serr;
			if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
				goto serr;
			if(soffset==st_entry.down_ptr){
				if(fseek(fb,coffset+offset(st_entry,down_ptr),SEEK_SET))
					goto serr;
				if(fwrite((char *)&poffset,sizeof(poffset),1,fb)!=1)
					goto serr;
				add_free(soffset);
				soffset=coffset;
				goto nit;
			}
		}
		coffset=st_entry.down_ptr;
rmfc:
		while(coffset){
			if(fseek(fb,coffset,SEEK_SET))
				goto serr;
			if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
				goto serr;
			if(st_entry.next==soffset){
				st_entry.next=poffset;
				if(fseek(fb,coffset,SEEK_SET))
					goto serr;
				if(fwrite((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
					goto serr;
				add_free(soffset);
				goto end;
			}
			else{
				coffset=st_entry.next;
				continue;
			}

		}
		bserrno=BS_BASEERR;
		goto err;
	}
end:
	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;}
	printf("Group %s deleted\n",name);
	bserrno=BS_OK;
	return 0;	
}

static int
rmgroup(char **_group)
{
	int stay_open;

	groups=_group;

	if(fb==NULL){
		stay_open=0;
		if(open_base(omode)){
			pberr("open_base: rmgroup",stderr);
			goto err2;
		}
	}
	else
		stay_open=1;


	func=_delgr;
	allmode=0;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
		pberr("sdba",stderr);
		goto err;
	}

	if(!count)
		fprintf(stdout,"Specified pattern does not matched with any group.\n");

	if(!stay_open)
		close_base();
	return 0;

err:
	pberr("sdba: rmgroup",stderr);
err2:
	retry_base();
	return 1;
}

char *flags;
ulong host;

static int
_setf(long entry, char *name)
{
	int i,ms;
	long mf=0L;

	count++;
	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
err:
		return 1;
	}
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;		

	if(flags[0]=='-')
		ms=-1;
	else if(flags[0]=='+')
		ms=1;
	else{
		ms=0;
		gr_entry.flag=0;
	}
	for(i=ms?1:0;flags[i];i++){
		switch(flags[i]){
		default:
		ferr:
			printf("Invalid flags %s\n",flags);
			printf("              ");
			while(i--)
				putchar(' ');
			putchar('^');
			putchar('\n');
			bserrno=BS_BADPARAM;
			return 1;
		case '+':
			if(ms==-1)
				ms=1;
			else if(ms==0)
				goto ferr;
			continue;
		case '-':
			if(ms==1)
				ms=-1;
			else if(ms==0)
				goto ferr;
			continue;
		case 'U':
			mf=GF_NOUNS;
			break;
		case 'M':
			mf=GF_COM;
			break;
		case 'S':
			mf=GF_NOSUBS;
			break;
		case 'C':
			mf=GF_NOCHMOD;
			break;
		case 'R':
			mf=GF_RONLY;
			break;
		case 'X':
			mf=GF_NOXPOST;
			break;
		case 'O':
			mf=GF_SONLY;
			break;
#ifdef NNTP
		case 'A':
			if(gr_entry.flag&GF_NNTP)
				mf=GF_NNTPALWAYS;
			else
				fprintf(stderr,"Flag 'A' can not be set without 'N' flag\n");
			break;
		case 'N':
			if(host==NULL){
				fprintf(stderr,"To set N flag use \"sdba setflag N host.domain\" command\n");
				mf=NULL;
				break;
			}
			mf=GF_NNTP;
			gr_entry.nntp_host=host;
			break;
#endif
		}/*switch*/
		if(ms==-1)
			gr_entry.flag&=(~mf);
		else
			gr_entry.flag|=mf;
	}/*for*/

	if(fseek(fb,entry,SEEK_SET))
		goto serr;
	if(lock(fb,F_WRLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}
	if(fwrite((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;		
	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}

	printf("Set flags for group %s OK\n",name);
	bserrno=BS_OK;
	return 0;
}


static int
setflags(char **_group, char *_flags, char *_param)
{
	int stay_open;

	groups=_group;
	host=NULL;
	if(fb==NULL){
		stay_open=0;
		if(open_base(omode)){
			pberr("open_base: setflags",stderr);
			goto err2;
		}
	}
	else
		stay_open=1;

#ifdef NNTP
	if(strchr(_flags,'N')){
		flags=_flags;
		if((host=get_host_addr(_param))==NULL){
			fprintf(stderr,"Can't get address of host %s\n",_param);
			return 1;
		}
	}else
#endif
	flags=_param;
	func=_setf;
	allmode=0;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
		pberr("sdba",stderr);
		goto err;
	}

	if(!count)
		fprintf(stdout,"Specified pattern does not matched with any group.\n");

	if(!stay_open)
		close_base();
	return 0;

err:
	pberr("sdba: setflags",stderr);
err2:
	retry_base();
	return 1;
}

#ifdef NNTP
static int
_sethost(long entry, char *name)
{
	count++;
	bserrno=BS_OK;
	if(!host)
		return 0;
	if(fseek(fb,entry+offset(gr_entry,nntp_host),SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fwrite((char *)&host,sizeof(host),1,fb)!=1)
		goto serr;		
	printf("Set host %s for group %s OK\n",inet_ntoa(host),name);
	return 0;
}

static int
sethost(char **_group, char *_host_name)
{
	int stay_open;

	groups=_group;
	host=NULL;
	if(fb==NULL){
		stay_open=0;
		if(open_base(omode)){
			pberr("open_base: setflags",stderr);
			goto err2;
		}
	}
	else
		stay_open=1;

	if((host=get_host_addr(_host_name))==NULL){
		fprintf(stderr,"Can't get address of host %s\n",_host_name);
		return 1;
	}
	func=_sethost;
	allmode=0;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
		pberr("sdba",stderr);
		goto err;
	}

	if(!count)
		fprintf(stdout,"Specified pattern does not matched with any group.\n");

	if(!stay_open)
		close_base();
	return 0;

err:
	pberr("sdba: sethost",stderr);
err2:
	retry_base();
	return 1;
}
#endif

struct _glist{
	struct _glist *next;
	char name[1];
} *active,*server;

static int
_add_glist(long entry, char *name)
{
	struct _glist *wrk;

	if((wrk=(struct _glist *)malloc(sizeof(struct _glist)+strlen(name)))==NULL){
		bserrno=BS_SYSERR;
		return 1;
	}

	strcpy(wrk->name,name);
	if(entry==NULL){
		wrk->next = active;
		active = wrk;
	}
	else{
		wrk->next = server;
		server = wrk;
	}

	bserrno=BS_OK;
	return 0;
}

static void
_sort_glist(struct _glist **chain)
{
	register struct _glist *wp, *pp, *ap;
	register i;

	/* sort groups */
	ap = (*chain)->next;
	(*chain)->next = NULL;
	while (ap != NULL) {
	    for (pp = NULL,wp = *chain; wp != NULL; pp = wp, wp = wp->next) {
		if ((i = strcmp(ap->name, wp->name)) > 0) {
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
			ap->next = *chain;
			*chain = ap;
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

static int
chkact(char **param)
{
	int make=0,delete=0,confirm=0,go_ok,bi=-1,i,stay_open;
	FILE *ac;
	struct _glist *cactive,*cserver,*cpactive,*cpserver;
	char name[200],*rg[2];

	active=server=NULL;
	allmode=0;

	for(i=0;param[i]!=NULL;i++){
		if(strcmp(param[i],"-m")==0)
			make=1;
		if(strcmp(param[i],"-d")==0)
			delete=1;
		if(strcmp(param[i],"-y")==0)
			confirm=1;
		if(param[i]!=NULL && param[i][0]!='-')
			if(cmdcmp(param[i],"ALL"))
				allmode=1;
			else
				bi=i;
	}
	if (bi<0) { /* gul */
		allmode=1;
		bi=i;
	}

#ifdef NNTP_ONLY
	if (connect_nnrp())
		return 1;
	fprintf(fsocket, "LIST ACTIVE\r\n");
	fflush(fsocket);
	if (!sofgets(snntp, sizeof(snntp), fsocket))
		return 1;
	if (snntp[0]!='2')
		return 1;
	ac=fsocket;
#else
	if((ac=fopen(ACTIVE,"r"))==NULL)
		return 1;
#endif
	while(sofgets(name,sizeof name,ac)!=NULL){
#ifdef NNTP_ONLY
		{	char *p=index(name, '\r');
			if (p)
				if (strcmp(p, "\r\n")==0)
					strcpy(p, "\n");
			if (strcmp(name, ".\n")==0)
				break;
		}
#endif
		if(*name=='\n')
			continue;
		{	register char *q;
			q=name;
			while( !ISspace(*q) ) q++;
			*q=0;
		}
		if(strcmp(name,"junk")==0 || strcmp(name,"general")==0 || 
		   strncmp(name,"control",7)==0 || strcmp(name,"to")==0)
			continue;

		for(i=bi,go_ok=allmode;param[i]!=NULL;i++){
			if(*param[i]=='!' && wildmat(name,param[i]+1)){
				go_ok=0;
				break;
			}
			else if(!go_ok && wildmat(name,param[i]))
				go_ok=1;
		}
		if(!go_ok)		
			continue;

		_add_glist(NULL,name);
	}
#ifndef NNTP_ONLY
	fclose(ac);
#else
	if (strcmp(name, ".\n")) {
		fprintf(stderr,"Can't read active\n");
		return 1;
	}
#endif
	if(active==NULL){
		fprintf(stderr,"Can't read active or pattern not correct\n");
		return 1;
	}
	_sort_glist(&active);

	groups=&param[bi];
	if(fb==NULL){
		stay_open=0;
		if(!make && !delete)
			omode=OPEN_RDONLY;
		if(open_base(omode)){
			pberr("open_base: check active",stderr);
			return 1;
		}
	}
	else
		stay_open=1;

	func=_add_glist;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
		pberr("sdba",stderr);
		close_base();
		return 1;
	}
	if(!make && !delete)
		if(!stay_open)
			close_base();
	if(server)
		_sort_glist(&server);
		cactive=active;
	cserver=server;
	cpactive=cpserver=NULL;
	while(cactive!=NULL && cserver!=NULL){
		while(strcmp(cactive->name,cserver->name)<0){
			cpactive=cactive;
			cactive=cactive->next;
		}
		while(strcmp(cserver->name,cactive->name)<0){
			cpserver=cserver;
			cserver=cserver->next;
		}
		if(strcmp(cserver->name,cactive->name)==0){
			if(cpactive==NULL){
				active=active->next;
				free(cactive);
				cactive=active;
			}
			else{
				cpactive->next=cactive->next;
				free(cactive);
				cactive=cpactive->next;
			}
			if(cpserver==NULL){
				server=server->next;
				free(cserver);
				cserver=server;
			}
			else{
				cpserver->next=cserver->next;
				free(cserver);
				cserver=cpserver->next;
			}
		}
		else
			continue;
	}

	if(!make && !delete){
		if(active!=NULL){
			printf("There are groups found in active, but not found in server:\n");
			cactive=active;
			while(cactive!=NULL){
				printf("%s\n",cactive->name);
				cactive=cactive->next;
			}
		}

		if(server!=NULL){
			printf("There are groups found in server, but not found in active:\n");
			cserver=server;
			while(cserver!=NULL){
				printf("%s\n",cserver->name);
				cserver=cserver->next;
			}
		}
	}
	if(make){
		cactive=active;
		while(cactive!=NULL){
			if(confirm)
				addgroup(cactive->name,NULL);
			else{
				printf("Create group %s Y/N [N]: ",cactive->name);
				fgets(name,sizeof name,stdin);
				if(*name=='Y' || *name=='y')
					addgroup(cactive->name,NULL);
			}
			cactive=cactive->next;
		}
	}

	if(delete){
		cserver=server;
		while(cserver!=NULL){
			rg[0]=cserver->name;
			rg[1]=NULL;
			if(confirm)
				rmgroup(rg);
			else{
				printf("Delete bogus group %s Y/N [Y]: ",cserver->name);
				fgets(name,sizeof name,stdin);
				if(*name!='N' && *name!='n')
					rmgroup(rg);
			}
			cserver=cserver->next;
		}
	}
	if(!stay_open && fb!=NULL)
		close_base();
	return 0;
}

static int
_de(long offs, char *p,int sflag, int *dis)
{
	register res;
	struct string_chain lst_entry;
	long lpoffset,lcoffset;
	char wlbuff[MAX_STR_EL];
	(void)strncpy(wlbuff,lower_str(p),sizeof(wlbuff));
	wlbuff[sizeof(wlbuff)-1]='\0';

	for(lpoffset=NULL,lcoffset=offs;lcoffset;
	    lpoffset=lcoffset,lcoffset=lst_entry.next){
		if(fseek(fb,lcoffset,SEEK_SET))
			return -1;
		if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
			return -1;

		if(sflag)
			if(lst_entry.down_ptr){
				if(_de(lst_entry.down_ptr,p,sflag,dis)==0){
					coffset=lcoffset;
					poffset=lpoffset;
					return 0;
				}
			}

		res=strcmp(wlbuff,lower_str(lst_entry.name));
		if(res<0)
			break;
		else if(res==0){
			if(lst_entry.entry){
				if(fseek(fb,lst_entry.entry,SEEK_SET))
					return -1;
				if(fread((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
					return -1;
				if(dm_entry.magic==DE_MAGIC){
					if(dm_entry.flag&DF_DIS)
						*dis=1;
					if(sflag)
						if(dm_entry.flag&DF_SITE){
							coffset=lcoffset;
							poffset=lpoffset;
							(void)memcpy(&st_entry,&lst_entry,sizeof(struct string_chain));
							return 0;
						}
				}
			}
			(void)memcpy(&st_entry,&lst_entry,sizeof(struct string_chain));
			coffset=lcoffset;
			poffset=lpoffset;
			return 1;
		}
		else
			continue;
	}/* for */
	return -1;
}

static int
f_domain(char *addr,int sflag,int *stay_open,int *level,int *dis)
{
	char *p,buff[80],dot=0;
	int res;

	strncpy(buff, addr, sizeof(buff));
	buff[sizeof(buff)-1]='\0';
	*level=0;

	if(fb==NULL){
		*stay_open=0;
		if(open_base(omode)){
			pberr("open_base: f_domain",stderr);
			return 1;
		}
	}
	else
		*stay_open=1;

	soffset=header.user;
nloop:
	if(sflag)
		p=buff;
	else {
		int i=strlen(buff)-1;
		for (i=strlen(buff)-1; i>=0; i--) {
			if ((buff[i]=='.') || (buff[i]=='@'))
				break;
			if (strlen(buff)-i>=MAX_STR_EL)
				break;
		}
		if (i>=0)
			p=&buff[i+1];
		else
			p=buff;
	}
	(*level)++;
	do { /* gul */
		res=_de(soffset,p,sflag,dis);
		if (res < 0) break;
		if (st_entry.dot_at == dot) break;
		if (st_entry.next == NULL) {
			res = -1;
			break;
		}
		soffset = st_entry.next;
	} while (1);
	if (p!=buff) {
		*p--=0;
		if ((*p == '.') || (*p == '@')) {
			dot=*p;
			*p=0;
		}
		else
			dot=0;
		p++;
	}

	if(res==-1){
		printf("%s %s not found\n",sflag?"Site":"Domain",addr);
		return 1;
	}
	else if(res==0)
		return 0;
	else
		if(p!=buff){
			soffset=st_entry.down_ptr;
			goto nloop;
		}
		else {
			return 0;
		}
}

static int
_rm_ui(entry,name)
long entry;
char *name;
{
	uoffset=entry;
	if(fseek(fb,uoffset,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fread((char *)&ui_entry,sizeof(struct domain_rec),1,fb)!=1)
		goto serr;
	if(ui_entry.magic==DE_MAGIC){
		add_free(entry);
		goto end;	
	}
	if(fseek(fb,uoffset,SEEK_SET))
		goto serr;
	if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		goto serr;
	
	if(ui_entry.magic==UE_MAGIC){
		printf("User %s:\n",name);
		if(ui_entry.begin){
			if(subscribe(NULL,NULL,FORGET,NULL,stdout))
				goto err;
		}
		else{
			add_free(uoffset);
			add_free(ui_entry.s_user);		
		}
	}
	else{
		bserrno=BS_BASEERR;
err:
		return 1;
	}

end:
	bserrno=BS_OK;
	return 0;
}

static int
del_domain(addr)
char *addr;
{
	int stay_open,level,dis;
	long tmpoffset,ptmpoffset,dtmpoffset;
	char *dom_addr;

	dom_addr=addr;
dtl:
	if(f_domain(dom_addr,0,&stay_open,&level,&dis))
		return 1;


	tmpoffset=coffset;
	ptmpoffset=poffset;
	dtmpoffset=st_entry.down_ptr;

	if(st_entry.next==NULL && poffset==NULL){
		if(fseek(fb,st_entry.up_ptr,SEEK_SET))
			goto err;
		if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
			goto err;
		if(st_entry.entry==NULL){
			char * p;
			for (p=dom_addr;*p && (*p!='.') && (*p!='@');p++);
			while (p-dom_addr>=MAX_STR_EL)
				p-=MAX_STR_EL-1;
			if ((*p=='.') || (*p=='@'))
				p++;
			dom_addr=p;
			goto dtl; /* remove with top level domain */
		}
	}

	if(lock(fb,F_WRLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}
	if(dtmpoffset)
		if(rm_tree(dtmpoffset,_rm_ui,level,1,dom_addr)){
err:
			retry_base();
			return 1;
		}

	if(fseek(fb,tmpoffset,SEEK_SET))
		goto err;
	if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
		goto err;
	if(st_entry.entry)
		add_free(st_entry.entry);

	if(ptmpoffset==NULL){
		if(st_entry.up_ptr==NULL){
			header.user=st_entry.next;
			sav_hdr();
		}
		else{
			if(fseek(fb,st_entry.up_ptr+offset(st_entry,down_ptr),SEEK_SET))
				goto err;
			if(fwrite((char *)&st_entry.next,sizeof(st_entry.next),1,fb)!=1)
				goto err;
		}
	}
	else{
		if(fseek(fb,ptmpoffset+offset(st_entry,next),SEEK_SET))
			goto err;
		if(fwrite((char *)&st_entry.next,sizeof(st_entry.next),1,fb)!=1)
			goto err;
	}

	add_free(tmpoffset);

	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}

	if(!stay_open)
		close_base();
	printf("Domain %s deleted\n",dom_addr);
	return 0;
}

static int
ch_emp(long entry,int *param,char *name)
{
	if(fseek(fb,entry,SEEK_SET)){
serr:	
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		goto serr;

	if(ui_entry.magic==DE_MAGIC)
		(void)fprintf(stdout,"%s\n",name);
	bserrno=BS_OK;
	return 0;	
}

static int
prdomain(char *addr,int sflag)
{
	int stay_open,level,param;
	long tmpoffset;

	if(cmdcmp(addr,"EMPTY")) {
		if(fb==NULL) {
			if(open_base(OPEN_RDONLY)) {
#ifdef SDBA
				pberr("sdba",stderr);
#endif
				return 1;
			}
		}
		else
			stay_open=1;
		fprintf(stdout,"The empty domain list follow:\n");
		get_utree(header.user,isdis,ch_emp,0,0,(char *)NULL);
		fprintf(stdout,"End of list\n");
	}
	else {

		param=0;
		omode=OPEN_RDONLY;
		if(f_domain(addr,sflag,&stay_open,&level,&param))
			return 1;

		if(st_entry.down_ptr==NULL)
			(void)fprintf(stdout,"There are no users at %s %s\n",
		        	      sflag?"site":"domain",addr);
		else {
			tmpoffset=coffset;
			if(get_utree(st_entry.down_ptr,isdis,ch_all,param,
			   level,sflag?(char *)NULL:addr)) {
				retry_base();
				return 1;
			}
		}
	}
	if(!stay_open)
		close_base();
	return 0;
}

static int
disable(char *addr, int flag)
{
	int stay_open,level,dis;

	if(f_domain(addr,0,&stay_open,&level,&dis))
		return 1;
	if(dis==flag) {
		printf("This domain are %s disabled\n",flag?"already":"not");
		goto end2;
	}
	if(lock(fb,F_WRLCK)<0) {
		bserrno=BS_ENOLOCK;
err:
		return 1;
	}
	if(flag)
		if(st_entry.entry) {
			dm_entry.flag|=DF_DIS;
			goto wr_dm;
		}
		else {
			dm_entry.magic=DE_MAGIC;
			dm_entry.flag=DF_DIS;
			dm_entry.reserved_1=NULL;
			dm_entry.reserved_2=NULL;
			dm_entry.access=D_DEFACCESS;
			dm_entry.size=strlen(addr);
			dm_entry.s_domain=coffset;
			st_entry.entry=sav_entry(DE_MAGIC,(char *)&dm_entry,
			                         sizeof(struct domain_rec));
			goto wr_st;
		}
	else
		if(dm_entry.flag&DF_SITE) {
			dm_entry.flag&=(~DF_DIS);
wr_dm:
			if(fseek(fb,st_entry.entry,SEEK_SET))
				goto err;
			if(fwrite((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
				goto err;
		}
		else {
			add_free(st_entry.entry);
			st_entry.entry=NULL;
wr_st:
			if(fseek(fb,coffset,SEEK_SET))
				goto err;
			if(fwrite((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
				goto err;
		}
	
	if(lock(fb,F_RDLCK)<0) {
		bserrno=BS_ENOLOCK;
		goto err;
	}
	
	printf("\t%s - %s\n",addr,flag?"disabled":"enabled");
end2:
	if(!stay_open)
		close_base();
	return 0;
}

#if AGE_TIME
int now;

static int
_chk_uiage(long entry, int *param, char *name)
{
	if(fseek(fb,entry,SEEK_SET)){
serr:	
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		goto serr;
	if(ui_entry.magic!=UE_MAGIC)
		return 0;
	if(!(ui_entry.flag&UF_AGING)){
		if(ui_entry.last_used+TIME_FOR_AGING_OFF < now)
			goto forget;
		else
			goto end;
	}
	if(ui_entry.flag&UF_SUSP){
		if(ui_entry.last_used+TIME_FOR_SUSPEND < now)
			goto forget;
		else
			goto end;
	}
	if(*param){
		if(ui_entry.last_used+TIME_FOR_DISABLE < now)
			goto forget;
		else
			goto end;
	}
	if(ui_entry.last_used+AGE_TIME < now){
forget:
		uoffset=entry;
		if(subscribe(NULL,NULL,FORGET,NULL,stdout))
			return 1;
		printf("sdba: check age: User %s deleted\n",name);
	}
end:
	bserrno=BS_OK;
	return 0;
}

static int
chk_age()
{
	int stay_open;

	if(fb==NULL){
		stay_open=0;	
		if(open_base(omode)){
#ifdef SDBA
			pberr("sdba",stderr);
#endif
			goto err;
		}
	}
	else 
		stay_open=1;
	now=time((long *)NULL)/86400;

	if(get_utree(header.user,isdis,_chk_uiage,0,0,(char *)NULL)){
		pberr("sdba: check age",stderr);
		goto err;
	}

if(!stay_open)
	close_base();
return 0;
err:
	retry_base();
	return 1;
}
#endif /* AGE_TIME */
