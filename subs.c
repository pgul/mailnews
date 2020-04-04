/*
 * (C) Copyright 1992,1993 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software. 
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[]="@(#)subs.c 4.0 02/10/94";
#endif


#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include "conf.h"
#include "dbsubs.h"

#if defined( SYSV ) || defined( USG )
#define index   strchr
#endif

extern char *nextword();
#ifndef SDBA
extern char *mesg();
extern int lang;
#endif

extern int wildmat(),cmdcmp(),sav_hdr();
extern int hdrchd,usite_ok;
extern long time();

static FILE *file;
static int mode,param,count;
static char **groups;
static int feedcount=0;


static int
_subs(entry,name)
long entry;
char *name;
{
	int chmode;
	long ulast,glast;

	chmode=0;
	count++;
	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
err:
		return 1;
	}
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;

	boffset=ui_entry.begin;

	while(boffset){
		if(fseek(fb,boffset,SEEK_SET))
			goto serr;
		if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
			goto serr;
		if(sb_entry.group==entry){
			chmode=1;
			break;
		}
		if(sb_entry.user_next==NULL)
			break;
		else
			boffset=sb_entry.user_next;
	}
	if(chmode){
		if(mode==sb_entry.mode)
			if ((mode!=RFEED) || (param==sb_entry.m_param)) {
#ifdef SDBA
				fprintf(file,"This user already subscribed\nto group %s in this mode\n",name);
#else
				fprintf(file,mesg(34,lang));
#endif
				goto end;
			}

		if(lock(fb,F_WRLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		sb_entry.mode=mode;
		sb_entry.m_param=param;
		if(fseek(fb,boffset,SEEK_SET))
			goto serr;
		if(fwrite((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
			goto serr;

		{	char rmode[20];
			if (mode==RFEED) sprintf(rmode,"rfeed %d", param);
				
#ifdef SDBA
			fprintf(file,"Mode changed to %s\n",
#else
			fprintf(file,mesg(35,lang),
#endif
			    mode==SUBS?"subscribe":mode==FEED?"feed":mode==RFEED?rmode:"efeed");
		}
		if(lock(fb,F_RDLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		goto end;
	}
	else{
#ifndef SDBA 
		if(gr_entry.flag&GF_NOSUBS){
			fprintf(file,mesg(27,lang),group );
			goto end;
		}
#endif

		ulast=boffset;
		boffset=gr_entry.begin;
		while(boffset){
			if(fseek(fb,boffset,SEEK_SET))
				goto serr;
			if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
				goto serr;
			if(sb_entry.group_next==NULL)
				break;
			else
				boffset=sb_entry.group_next;
		}
glast=boffset;
		if(lock(fb,F_WRLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}

		if(gr_entry.flag&GF_NNTP && !glast){
			if(fseek(fb,entry+offset(gr_entry,nntp_lastused),SEEK_SET))
				goto serr;
			gr_entry.nntp_lastused=time((long *)NULL);
			if(fwrite((char *)&gr_entry.nntp_lastused,sizeof(gr_entry.nntp_lastused),1,fb)!=1)
				goto serr;
		}

		sb_entry.magic=SC_MAGIC;
		sb_entry.mode=mode;
		sb_entry.m_param=param;
		sb_entry.user_prev=ulast;
		sb_entry.group_prev=glast;
		sb_entry.user_next=NULL;
		sb_entry.group_next=NULL;
		sb_entry.group=entry;
		sb_entry.s_group=gr_entry.s_group;
		sb_entry.user=uoffset;
		sb_entry.s_user=ui_entry.s_user;

		if((boffset=sav_entry(SC_MAGIC,(char *)&sb_entry,sizeof(sb_entry)))==NULL)
			return 1;

		if(ulast){
			if(fseek(fb,ulast+offset(sb_entry,user_next),SEEK_SET))
				goto serr;
		}else{
			ui_entry.begin=boffset;
			if(fseek(fb,uoffset+offset(ui_entry,begin),SEEK_SET))
				goto serr;
		}
		if(fwrite((char *)&boffset,sizeof(boffset),1,fb)!=1)
			goto serr;

		if(glast){
			if(fseek(fb,glast+offset(sb_entry,group_next),SEEK_SET))
				goto serr;
		}else{
			gr_entry.begin=boffset;
			if(fseek(fb,entry+offset(gr_entry,begin),SEEK_SET))
				goto serr;
		}
		if(fwrite((char *)&boffset,sizeof(boffset),1,fb)!=1)
			goto serr;
		if(lock(fb,F_RDLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}

#ifdef SDBA
		fprintf(file,"\t%s --OK\n",name);
#else
		fprintf(file,mesg(24,lang),name);
		if ((mode==FEED) || (mode==RFEED) || (mode==EFEED))
			feedcount++;
#endif
	}
	end:
	bserrno=BS_OK;
	return 0;
}

static int
_chh(entry,parm,name)
long entry;
int *parm;
char *name;
{
register i,do_ok;

	for(do_ok=i=0;groups[i];i++){
	if(*groups[i]=='!'){
		if(wildmat(name,groups[i]+1)){
			do_ok=0;
			break;
		}
	}
	else
		if(wildmat(name,groups[i]))
			do_ok=1;
	}
	if(do_ok) {
#ifndef SDBA
		if ((mode==FEED) || (mode==EFEED) || (mode==RFEED))
			if (feedcount>MAXFEEDCOUNT) {
				fprintf(file, mesg(62, lang), MAXFEEDCOUNT, name);
				return 0;
			}
#endif
		return _subs(entry,name);
	}
	else
		return 0;
}

static int
_del_user(entry)
long entry;
{
	long coffset, poffset;
	soffset=ui_entry.s_user;
	add_free(entry);

nit:
	if(fseek(fb,soffset,SEEK_SET)) {
serr:
		bserrno=BS_SYSERR;
err:
		return 1;
	}
	if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;
	poffset=st_entry.next;
	if (st_entry.entry==entry) {
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
			if(header.user==soffset){
				header.user=poffset;
				add_free(soffset);
				sav_hdr();
				goto end;
			}
			else{
				coffset=header.user;
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
	bserrno=BS_OK;
	return 0;
}

int
subscribe(addr,_groups,_mode,_param,_file)
char *addr,**_groups;
int _mode,_param;
FILE *_file;
{
	int allmode,do_ok,stay_open = 0;
	long ulast;

	file=_file;
	mode=_mode;
	param=_param;
	groups=_groups;

#ifdef SDBA
	if(addr==NULL && mode==FORGET){
		stay_open=1;
		goto fgt_ent;
	}
#endif

	if ((index(addr, '@') == NULL && index(addr, '.') != NULL)
	    || index(addr, '!') != NULL || index(addr, '%') != NULL)
		return 1;	

	allmode=0;
	count=0;
	if(mode==FORGET)
		allmode=1;
	else {
		register i;
		for(i=0;groups[i];i++)
			if(cmdcmp(groups[i],"ALL") || strcmp(groups[i],"*")==0)
				allmode=1;
			else
				allmode=0;
	
		if(allmode){
			if(i>1)
				allmode=0;
			if(mode!=UNSUBS && i<2 ){
#ifdef SDBA
				fprintf(file,"You can not subscribe to all group\n");
#else
				fprintf(file,mesg(22,lang));
#endif
				goto end3;
			}
		}
	}

	if(fb==NULL){
		if(open_base(omode)){
#ifdef SDBA
			pberr("sdba",stderr);
#endif
			return 1;
		}
	}
	else
		stay_open=1;
	
	if(getuser(addr,0,
#ifdef SDBA
	   (mode!=FORGET && mode!=UNSUBS)?3:0
#else
	   (mode!=FORGET && mode!=UNSUBS)
#endif
	   )){
#ifdef SDBA
		if(bserrno==BS_DISDOMAIN)
			pberr(get_domain(dm_entry.s_domain),stderr);
		else if(bserrno==BS_ENOUSER && (mode==FORGET || mode==UNSUBS))
			goto notany;
		else
			pberr("sdba",stderr);
#else
		if(bserrno==BS_ENOUSER && (mode==FORGET || mode==UNSUBS))
			goto notany;
#ifdef UNKNOWN_DENIED
	        if(bserrno==BS_ENOUSER){
			fprintf(of, mesg(40, DEFLANG));
			goto enda;
		}
#endif
#endif
		goto err;
	}

	if(ui_entry.begin==NULL && mode==UNSUBS){
notany:
#ifdef SDBA
		fprintf(file,"User %s not subscribed to any group\n",addr);
#else
		fprintf(file,mesg(10,lang));
#endif
		goto enda;
	}

	if(allmode){
#ifdef SDBA
fgt_ent:
#endif
		if(lock(fb,F_WRLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		if((boffset=ui_entry.begin)!=NULL){
			ui_entry.begin=NULL;
			ulast=NULL;
			do{
				if(fseek(fb,boffset,SEEK_SET))
					goto err;
				if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
					goto err;
				if(fseek(fb,sb_entry.group,SEEK_SET))
					goto err;
				goffset=sb_entry.group;
				if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
					goto err;
#ifndef SDBA 
				if((gr_entry.flag&GF_NOUNS && mode!=FORGET) || (sb_entry.mode==CNEWS && !usite_ok)){
					if(sb_entry.mode==CNEWS){
						fprintf(file,mesg(56,lang));
						if(mode==FORGET)
							goto err2;
					}
					else if(get_gname(gr_entry.s_group)!=NULL)
						fprintf(file,mesg(23,lang),group);
					if(ulast==NULL){
						ui_entry.begin=boffset;
						sb_entry.user_prev=NULL;
					}
					else{
						if(fseek(fb,ulast+offset(sb_entry,user_next),SEEK_SET))
							goto err;
						if(fwrite((char *)&boffset,sizeof(boffset),1,fb)!=1)
							goto err;
						sb_entry.user_prev=ulast;
					}
					ulast=boffset;
					boffset=sb_entry.user_next;
					sb_entry.user_next=NULL;
					if(fseek(fb,ulast,SEEK_SET))
						goto err;
					if(fwrite((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
						goto err;
					continue;
				}
#endif
				if(sb_entry.group_next){
					if(fseek(fb,sb_entry.group_next+offset(sb_entry,group_prev),SEEK_SET))
						goto err;
					if(fwrite((char *)&sb_entry.group_prev,sizeof(sb_entry.group_prev),1,fb)!=1)
						goto err;
				}
				if(sb_entry.group_prev){
					if(fseek(fb,sb_entry.group_prev+offset(sb_entry,group_next),SEEK_SET))
						goto err;
				}
				else{
					gr_entry.begin=sb_entry.group_next;
					if(fseek(fb,goffset+offset(gr_entry,begin),SEEK_SET))
						goto err;
				}
				if(fwrite((char *)&sb_entry.group_next,sizeof(sb_entry.group_next),1,fb)!=1)
					goto err;

				add_free(boffset);

				if(get_gname(gr_entry.s_group)!=NULL)
#ifdef SDBA
					fprintf(file,"\t%s --OK\n",group);
#else
					fprintf(file,mesg(24,lang),group);
#endif
				boffset=sb_entry.user_next;
			}while(boffset!=NULL);
		}
		if(mode==FORGET){
			ui_entry.begin=NULL;
			_del_user(uoffset);
		}
		else{
			if(fseek(fb,uoffset,SEEK_SET))
				goto err;
			if(fwrite((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
				goto err;
		}
		if(lock(fb,F_RDLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		goto enda;
	}

	if(mode==UNSUBS){
		if(lock(fb,F_WRLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		count=0;
		boffset=ui_entry.begin;
		do{
			if(fseek(fb,boffset,SEEK_SET))
				goto err;
			if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
				goto err;
#ifndef SDBA
			if(sb_entry.mode==CNEWS && !usite_ok){
				fprintf(file,mesg(56,lang));
				goto err;
			}
#endif
			if(fseek(fb,(goffset=sb_entry.group),SEEK_SET))
				goto err;
			if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
				goto err;
			if(get_gname(gr_entry.s_group)==NULL)
				goto err;
			{
				register i;
				do_ok=0;
				for(i=0;groups[i];i++){
					if(*groups[i]=='!'){
						if(wildmat(group,groups[i]+1))
						goto nuns;
					}
					else
						if(wildmat(group,groups[i]))
					do_ok=1;
				}
			}

			if(do_ok)
				goto uns;
nuns:;
			boffset=sb_entry.user_next;
		}while(boffset!=NULL);

		if(count==0){
			char * grouplist;
			register i;
			int size=0;
			for (i=0;groups[i];i++)
				size+=strlen(groups[i])+1;
			grouplist = malloc(size+1);
			grouplist[0]='\0';
			for(i=0;groups[i];i++){
				strcat(grouplist,groups[i]);
				strcat(grouplist," ");
			}
#ifdef SDBA
			fprintf(file,"You are not subscribe to any group matched with %s\n",grouplist);
#else
			fprintf(file,mesg(25,lang),grouplist);
#endif
			free(grouplist);
		}
		if(lock(fb,F_RDLCK)<0){
			bserrno=BS_ENOLOCK;
			goto err;
		}
		goto enda;
uns:
		count++;
#ifndef SDBA 
		if(gr_entry.flag&GF_NOUNS){
			fprintf(file,mesg(23,lang),group);
			goto nuns;
		}
#endif
		if(sb_entry.user_next){
			if(fseek(fb,sb_entry.user_next+offset(sb_entry,user_prev),SEEK_SET))
				goto err;
			if(fwrite((char *)&sb_entry.user_prev,sizeof(sb_entry.user_prev),1,fb)!=1)
				goto err;
		}
		if(sb_entry.user_prev){
			if(fseek(fb,sb_entry.user_prev+offset(sb_entry,user_next),SEEK_SET))
				goto err;
		}
		else{
			ui_entry.begin=sb_entry.user_next;
			if(fseek(fb,uoffset+offset(ui_entry,begin),SEEK_SET))
				goto err;
		}
		if(fwrite((char *)&sb_entry.user_next,sizeof(sb_entry.user_next),1,fb)!=1)
			goto err;

		if(sb_entry.group_next){
			if(fseek(fb,sb_entry.group_next+offset(sb_entry,group_prev),SEEK_SET))
				goto err;
			if(fwrite((char *)&sb_entry.group_prev,sizeof(sb_entry.group_prev),1,fb)!=1)
				goto err;
		}
		if(sb_entry.group_prev){
			if(fseek(fb,sb_entry.group_prev+offset(sb_entry,group_next),SEEK_SET))
				goto err;
		}
		else{
			gr_entry.begin=sb_entry.group_next;
			if(fseek(fb,goffset+offset(gr_entry,begin),SEEK_SET))
				goto err;
		}
		if(fwrite((char *)&sb_entry.group_next,sizeof(sb_entry.group_next),1,fb)!=1)
			goto err;

		coffset=boffset;
		boffset=sb_entry.user_next;
		add_free(coffset);

		if(get_gname(gr_entry.s_group)!=NULL)
#ifdef SDBA
			fprintf(file,"\t%s --OK\n",group);
#else
			fprintf(file,mesg(24,lang),group);
#endif
		goto nuns;
	}

	count=feedcount=0;
	if(get_gtree(header.group,_chh,0,0,(char *)NULL)){
#ifdef SDBA
		pberr("sdba",stderr);
#endif
		goto err;
	}

	if(count==0){
		register i;
		int size=0;
		char *grouplist;
		for (i=0; groups[i]; i++)
			size += strlen(groups[i])+1;
		grouplist = malloc(size+1);
		grouplist[0] = '\0';
		for(i=0;groups[i];i++){
			strcat(grouplist,groups[i]);
			strcat(grouplist," ");
		}
#ifdef SDBA
		fprintf(file,"Group(s) %s does not exist\n", grouplist);
		free(grouplist);
		goto err2;
#else
		fprintf(file,mesg(3,lang),grouplist);
		free(grouplist);
		goto enda;
#endif
	}
enda:
	if(!stay_open)
		close_base();
end3:
	return 0;

err:
#ifdef SDBA
	fprintf(file,"Error in I/O operation\n");
#else
	fprintf(file,mesg(28,lang));
#endif
	retry_base();
	return 1;
err2:
	close_base();
	return 1;
}
