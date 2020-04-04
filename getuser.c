/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#ifndef MEMMAP
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "dbsubs.h"
#include "conf.h"
#else
#define IN_LISTER
#endif

#define E_NONE	0
#define E_SITE	1
#define E_USER	2

#ifndef OS2
#ifdef __STDC__
extern long time(time_t *);
#else
extern long time();
#endif
#else
extern ulong time(time_t *);
#endif

#ifndef IN_LISTER
static int
#ifdef __STDC__
add_shie(long cioffset, long pioffset, int ptype, long up_ptr, char *name, int etype, int dot_at)
#else
add_shie(cioffset,pioffset,ptype,up_ptr,name,etype,dot_at)
long cioffset,pioffset,up_ptr;
int ptype,etype,dot_at;
char *name;
#endif
{

	st_entry.magic=SE_MAGIC;
	st_entry.next=cioffset;
	st_entry.up_ptr=up_ptr;
	st_entry.down_ptr=NULL;
	st_entry.reserved_1=NULL;
	st_entry.reserved_2=NULL;
	st_entry.dot_at=dot_at;
	if((st_entry.size=strlen(name))>=MAX_STR_EL){
		bserrno=BS_BADPARAM;
		goto err;
	}
	{	register i;
		for(i=st_entry.size;i<MAX_STR_EL;i++) st_entry.name[i]=0;
	}
	(void)strcpy(st_entry.name,name);

	if((soffset=get_free(SE_MAGIC))==NULL){
		if(fseek(fb,0L,SEEK_END))
			goto serr;
		soffset=ftell(fb);
	}
	else
		if(fseek(fb,soffset,SEEK_SET))
			goto serr;

	if(fwrite((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;

	switch(etype){
		case E_NONE:
			st_entry.entry=NULL;
			break;
		case E_SITE:
			dm_entry.magic=DE_MAGIC;
			dm_entry.flag=DF_SITE;
			dm_entry.reserved_1=NULL;
			dm_entry.reserved_2=NULL;
			dm_entry.access=D_DEFACCESS;
			dm_entry.size=strlen(user)-strlen(wbuff);
			dm_entry.s_domain=soffset;

			if((doffset=sav_entry(DE_MAGIC,(char *)&dm_entry,sizeof(struct domain_rec)))==NULL)
				goto err;

			st_entry.entry=doffset;
			break;
		case E_USER:
			ui_entry.magic=UE_MAGIC;
			ui_entry.begin=NULL;
			ui_entry.flag=DEFFLAGS;
			ui_entry.last_used=time((long *)NULL)/86400;
			ui_entry.reserved_1=NULL;
			ui_entry.reserved_2=NULL;
			ui_entry.lang=DEFLANG;
			ui_entry.limit=63;
			ui_entry.size=strlen(user);
			ui_entry.s_user=soffset;

			if((uoffset=sav_entry(UE_MAGIC,(char *)&ui_entry,sizeof(struct user_rec)))==NULL)
				goto err;

			st_entry.entry=uoffset;
			break;
		default:
			bserrno=BS_BADPARAM;
			goto err;
	}

	if(fseek(fb,soffset+offset(st_entry,entry),SEEK_SET))
		goto serr;
	if(fwrite((char *)&st_entry.entry,sizeof(st_entry.entry),1,fb)!=1)
		goto serr;

	if(ptype){
		if(pioffset){
			if(fseek(fb,pioffset+offset(st_entry,down_ptr),SEEK_SET))
				goto serr;
			if(fwrite((char *)&soffset,sizeof(st_entry.down_ptr),1,fb)!=1)
				goto serr;
		}
		else{
			if(cioffset==header.user){
				header.user=soffset;
				if(sav_hdr())
					goto err;
			}
			else{
				bserrno=BS_BADPARAM;
				goto err;
			}
		}
	}
	else{
		if(fseek(fb,pioffset+offset(st_entry,next),SEEK_SET))
			goto serr;
		if(fwrite((char *)&soffset,sizeof(st_entry.next),1,fb)!=1){
serr:
			bserrno=BS_SYSERR;
err:
			return 1;
		}
	}


	bserrno=BS_OK;
	return 0;
}
#endif

static char *
#ifdef __STDC__
get_ael(char *buf, int *el, int *site, int *last, int *userf, int *dot_at)
#else
get_ael(buf,el,site,last,userf,dot_at)
char *buf;
int *el,*site,*last,*userf,*dot_at;
#endif
{
	register i;
	static char prt[MAX_STR_EL];

	i=strlen(buf)-1;
	(*el)++;
	*dot_at=buf[i];
	if(*dot_at!='.' && *dot_at!='@')
		*dot_at=0;
	else
		buf[i--]=0;

	while(i && buf[i]!='.' && buf[i]!='@') {
		if (strlen(buf)-i>=MAX_STR_EL)
			break;
		i--;
	}

	if(i){
		strncpy(prt,&buf[i+1],MAX_STR_EL-1);
		prt[MAX_STR_EL-1]=0;
		buf[i+1]=0;
		if (*site) /* gul */
			*userf=1;
		if(buf[i]=='@')
			*site=1;
		else /* gul */
			*site=0;
	}else{
		if(*site){
			*last=(*userf=1);
		}else if(*userf){
			*last=1;
		}
		if(*el==1){
			strcpy(prt,"@");
			*userf=1;
			goto end;
		}
		strncpy(prt,buf,sizeof(prt)-1);
		prt[sizeof(prt)-1]=0;
	}
end:
	return prt;
}

int
#ifdef __STDC__
getuser(char *addr, int disign, int add)
#else
getuser(addr,disign,add)
char *addr;
int disign,add;
#endif
{
char *p,wlbuff[MAX_STR_EL];
int el=0,site=0,last=0,userf=0,res,dot_at,adding=0;

poffset=doffset=uoffset=0;
bserrno=BS_OK;

(void)strcpy(user,addr);
(void)strcpy(wbuff,addr);

st_entry.up_ptr=0;
soffset=header.user;

do{
	p=get_ael(wbuff,&el,&site,&last,&userf,&dot_at);
#ifndef IN_LISTER
	if(adding)
		goto addh;
#endif

	(void)strcpy(wlbuff,lower_str(p));

	for(coffset=soffset;coffset;poffset=coffset,coffset=st_entry.next){
		if(fseek(fb,coffset,SEEK_SET)){
			bserrno=BS_SYSERR;
			goto err;
		}
		if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1){
			bserrno=BS_SYSERR;
			goto err;
		}
		if(st_entry.magic!=SE_MAGIC){
			bserrno=BS_BASEERR;
			goto err;
		}
		res=strcmp(wlbuff,lower_str(st_entry.name));
		if ((res==0) && (st_entry.dot_at!=dot_at)) /* gul */
			res=1;
		if(res>0)
			continue;
		else if(res==0)
			goto nl;
#ifndef IN_LISTER
		else if(add)
			break;
#endif
		else{
			bserrno=BS_ENOUSER;
			goto err;
		}
	}/* for */

#ifndef IN_LISTER
	if((add
#ifdef UNKNOWN_DENIED
	&& site) || (add > 1
#endif
	)){
		if(lock(fb,F_WRLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
addh:
#ifdef UNKNOWN_DENIED
		if(add==2 && userf){
			bserrno=BS_OK;
			break;
		}
#endif
		if(add_shie(coffset, poffset, (st_entry.up_ptr==poffset)||adding,
		   adding?poffset:st_entry.up_ptr, p, userf?(last?E_USER:E_NONE):(site?E_SITE:E_NONE),dot_at))
			goto err;
		adding=1;
		poffset=soffset;
		coffset=NULL;
		continue;
	}
	else{
#else /* IN_LISTER */
	if(!add){
#endif
#ifdef UNKNOWN_DENIED
		bserrno=site?BS_ENOUSER:BS_DENIED;
#else
		bserrno=BS_ENOUSER;
#endif
		goto err;
	}

nl:

	if(last){
		if(st_entry.down_ptr){
			bserrno=BS_BASEERR;
			goto err;
		}

		if(!doffset&&st_entry.dot_at){
			bserrno=BS_BASEERR;
			goto err;
		}

		if(st_entry.entry){
			uoffset=st_entry.entry;
			if(fseek(fb,uoffset,SEEK_SET))
				goto serr;
			if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
				goto serr;
			if(ui_entry.magic!=UE_MAGIC){
				bserrno=BS_BASEERR;
				goto err;
			}
			break;
		}
		else{
			bserrno=BS_BASEERR;
			goto err;
		}
	}
	else{
		poffset=coffset;
		if((soffset=st_entry.down_ptr)==NULL){
			if(add){
				adding=1;
				coffset=0;
				continue;
			}
			else{
				bserrno=BS_ENOUSER;
				goto err;
			}
		}
	}

	if(st_entry.entry && !userf){
		doffset=st_entry.entry;
		if(fseek(fb,doffset,SEEK_SET))
			goto serr;
		if(fread((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1){
serr:
			bserrno=BS_SYSERR;
err:
			return 1;
		}
		if(dm_entry.magic!=DE_MAGIC){
			bserrno=BS_BASEERR;
			goto err;
		}
#ifndef SDBA
		if(dm_entry.flag & DF_DIS){
			bserrno=BS_DISDOMAIN;
			if(disign)
				continue;
			else
				goto err;
		}
#endif
	}
	else{
		if(site && !userf)
#ifndef IN_LISTER
			if(add){
				/*create dm_entry */
				dm_entry.magic=DE_MAGIC;
				dm_entry.flag=DF_SITE;
				dm_entry.reserved_1=NULL;
				dm_entry.reserved_2=NULL;
				dm_entry.access=D_DEFACCESS;
				dm_entry.size=strlen(user)-strlen(wbuff);
				dm_entry.s_domain=coffset;

				if((doffset=sav_entry(DE_MAGIC,(char *)&dm_entry,sizeof(struct domain_rec)))==NULL)
					goto err;
				if(fseek(fb,coffset+offset(st_entry,entry),SEEK_SET))
					goto serr;
				if(fwrite((char *)&doffset,sizeof(doffset),1,fb)!=1)
					goto serr;
			}
			else
#endif
			{
#ifdef UNKNOWN_DENIED
				bserrno=site?BS_ENOUSER:BS_DENIED;
#else
				bserrno=BS_ENOUSER;
#endif
				goto err;
			}
	}

}while(!last);
#ifndef IN_LISTER
if(adding)
	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;
	}
#endif
return 0;
}
