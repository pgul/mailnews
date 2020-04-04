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
static char sccsid[]="@(#)chkbase.c 1.0 03/06/94";
#endif

#include <stdio.h>
#include "dbsubs.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

long bs_size,sbs_size;

static int
chk_tree(begin,func,g_u,level,name)
long begin;
int (*func)();
int g_u,level;
char *name;
{
	long curoffset;
	struct string_chain lst_entry;
	char lwbuff[160];

	if(++level>20){
berr:
		bserrno=BS_BASEERR;
err:
		return 1;
	}

	for(curoffset=begin;curoffset;curoffset=lst_entry.next){
		if(fseek(fb,curoffset,SEEK_SET)){
serr:
			bserrno=BS_SYSERR;
			return 1;
		}
		if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
			goto serr;
		bs_size+=sizeof(struct string_chain);
		if(!lst_entry.entry && !lst_entry.down_ptr)
			goto berr;
		if(lst_entry.magic!=SE_MAGIC || (lst_entry.down_ptr==(long)NULL 
		    && lst_entry.up_ptr==(long)NULL && lst_entry.entry==(long)NULL))
			goto berr;
		if(lst_entry.down_ptr%4 || lst_entry.up_ptr%4 || lst_entry.entry%4 ||lst_entry.next%4)
			goto berr;
		if(strlen(lst_entry.name)!=lst_entry.size)
			goto berr;
		if(!lst_entry.entry && !lst_entry.down_ptr)
			goto berr;

		if(g_u)
			if(name){
				(void)strcpy(lwbuff,name);
				(void)strcat(lwbuff,lst_entry.name);
			}
			else
				(void)strcpy(lwbuff,lst_entry.name);
		else
		if(name){
			(void)strcpy(lwbuff,name);
			(void)strins(lwbuff,lst_entry.name,lst_entry.dot_at);
		}
		else
			if(lst_entry.name[0]=='@')
				lwbuff[0]=0;
			else
				(void)strcpy(lwbuff,lst_entry.name);

		if(lst_entry.entry)
			if((func)(lst_entry.entry,lwbuff,curoffset,lst_entry.size))
				goto err;
		if(lst_entry.down_ptr) {
			if (g_u && (lst_entry.dot_at=='.'))
				strcat(lwbuff,".");
			if(chk_tree(lst_entry.down_ptr,func,g_u,level,lwbuff))
				goto err;
		}
	}
	bserrno=BS_OK;
	return 0;
}

static int
chk_free()
{
	struct free_sp fr_entry;

	for(coffset=header.freesp;coffset;coffset=fr_entry.next){
		if(fseek(fb,coffset,SEEK_SET))
			goto serr;
		if(fread((char *)&fr_entry,sizeof(struct free_sp),1,fb)!=1){
serr:
			bserrno=BS_SYSERR;
			return 1;
		}
		if(fr_entry.next%4){
berr:
			bserrno=BS_BASEERR;
			return 1;
		}
		switch(fr_entry.magic){
			default:
				goto berr;
			case UE_MAGIC^FREE_MASK:
				bs_size+=sizeof(struct user_rec);
				break;
			case DE_MAGIC^FREE_MASK:
				bs_size+=sizeof(struct domain_rec);
				break;
			case GE_MAGIC^FREE_MASK:
				bs_size+=sizeof(struct group_rec);
				break;
			case SE_MAGIC^FREE_MASK:
				bs_size+=sizeof(struct string_chain);
				break;
			case SC_MAGIC^FREE_MASK:
				bs_size+=sizeof(struct subs_chain);
		}
	}
	bserrno=BS_OK;
	return 0;
}

static int
_chk_entry(entry,name,back,size)
long entry,back;
int size;
char *name;
{
	ulong magic;
	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;
	}
	if(fread((char *)&magic,sizeof(ulong),1,fb)!=1)
		goto serr;
	if(fseek(fb,entry,SEEK_SET))
		goto serr;
	switch(magic){
		default:
berr:
			bserrno=BS_BASEERR;
			return 1;
		case UE_MAGIC:
			if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
				goto serr;
			bs_size+=sizeof(struct user_rec);
			if(ui_entry.size!=strlen(name) || ui_entry.s_user!=back)
				goto berr;
			if(ui_entry.begin){
				if(ui_entry.begin%4)
					goto berr;
				boffset=ui_entry.begin;
				goto chsbs;
			}
			break;
		case DE_MAGIC:
			if(fread((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
				goto serr;
			bs_size+=sizeof(struct domain_rec);
			if(dm_entry.size!=strlen(name) || dm_entry.s_domain!=back)
				goto berr;
			break;
		case GE_MAGIC:
			if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
				goto serr;
			bs_size+=sizeof(struct group_rec);
			if(gr_entry.size!=strlen(name) || gr_entry.s_group!=back)
				goto berr;
			if(gr_entry.begin){
				if(gr_entry.begin%4)
					goto berr;
				boffset=gr_entry.begin;
chsbs:
				poffset=(long)NULL;
				while(boffset){
					if(fseek(fb,boffset,SEEK_SET))
						goto serr;
					if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
						goto serr;
					sbs_size+=sizeof(struct subs_chain);
					if(sb_entry.magic!=SC_MAGIC)
						goto berr;
					if(magic==GE_MAGIC){
						if(sb_entry.group!=entry || sb_entry.s_group!=back || sb_entry.group_prev!=poffset)
							goto berr;
						if(sb_entry.group_next%4)
							goto berr;
					}
					else{
						if(sb_entry.user!=entry || sb_entry.s_user!=back || sb_entry.user_prev!=poffset)
							goto berr;
						if(sb_entry.user_next%4)
							goto berr;
					}
					poffset=boffset;
					if(magic==GE_MAGIC)
						boffset=sb_entry.group_next;
					else
						boffset=sb_entry.user_next;
				}/* while */

			}
	}

	bserrno=BS_OK;
	return 0;
}

int
chk_base()
{
	int stay_open;
	long size,pos = 0L;
	char	filename[200];

	if(fb==NULL){
		stay_open=0;	
		(void)sprintf(filename,"%s/%s",BASEDIR,SUBSBASE);
		if((fb=fopen(filename,"r+"))==NULL)
			goto err;
		if(lock(fb,F_WRLCK)<0)
			exit(1);
		if(fread((char *)&header,sizeof(struct base_header),1,fb)!=1)
			goto err;
	}
	else{ 
		pos=ftell(fb);
		stay_open=1;
		if(fseek(fb,0l,SEEK_SET)){
			bserrno=BS_SYSERR;
			goto err;
		}
	}

	bs_size=sizeof(struct base_header);
	sbs_size=0;

	if(chk_tree(header.user,_chk_entry,0,0,(char *)NULL))
		goto err;
	if(chk_tree(header.group,_chk_entry,1,0,(char *)NULL))
		goto err;
	if(chk_free())
		goto err;
	if(fseek(fb,0l,SEEK_END)){
		bserrno=BS_SYSERR;
		goto err;
	}
	size=ftell(fb);
	if(size!=(bs_size+sbs_size/2)){
		printf("Database has invalid size\n");
		bserrno=BS_BASEERR;
		goto err;
	}

	if(!stay_open){
		fclose(fb);
		fb=NULL;
		header.magic=(long)NULL;
	}
	else
		if(fseek(fb,pos,SEEK_SET)){
			bserrno=BS_SYSERR;
			goto err;
		}

	return 0;

err:
	pberr("check base",stdout);
	return 1;
}
