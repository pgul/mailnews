
/*
 * (C) Copyright 1992-1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software. 
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[]="@(#)check.c 4.0 30/1/94";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "dbsubs.h"

struct ch_gr{
ulong	mode:8;
ulong	m_param:24;
struct ch_gr *next;
char name[1];
} *ch_groups,*ch_wrk;

extern char *calloc();
extern void free();
#ifndef SDBA
extern char *mesg();
extern int lang;
#else
extern int cmdcmp();
#endif

static int
ui_check()
{
for(boffset=ui_entry.begin;boffset;boffset=sb_entry.user_next){
if(fseek(fb,boffset,SEEK_SET)){
serr:
	bserrno=BS_SYSERR;
	return 1;
	}	
if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
	goto serr;
ch_wrk=(struct ch_gr *)calloc(1,sizeof(struct ch_gr)+strlen(get_gname(sb_entry.s_group)));
ch_wrk->mode=sb_entry.mode;
ch_wrk->m_param=sb_entry.m_param;
(void)strcpy(ch_wrk->name,group);
ch_wrk->next=ch_groups;
ch_groups=ch_wrk;

}
{/*sorting of groups*/
	register struct ch_gr *wp, *pp, *ap;
	register i;

	ap = ch_groups->next;
	pp = NULL;
	ch_groups->next = NULL;
	while (ap != NULL) {
	    for (wp = ch_groups; wp != NULL; pp = wp, wp = wp->next) {
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
		    if (wp == ch_groups) {
			pp = ap->next;
			ap->next = ch_groups;
			ch_groups = ap;
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
bserrno=BS_OK;
return 0;
}

static void
pr_chg(sflag,file)
int sflag;
FILE *file;
{
while(ch_groups){
ch_wrk=ch_groups;

#ifndef SDBA
		if(sflag)
		switch(ch_wrk->mode){
			case SUBS:
				(void)fprintf(file,">SUBSCRIBE %s\n",ch_wrk->name);
				break;
			case FEED:
				(void)fprintf(file,">FEED %s\n",ch_wrk->name);
				break;
			case EFEED:
				(void)fprintf(file,">EFEED %s\n",ch_wrk->name);
				break;
			case RFEED:
				(void)fprintf(file,">RFEED %d %s\n",
				    ch_wrk->m_param,ch_wrk->name);
				break;
			case CNEWS:
				(void)fprintf(file,">CNEWS %s\n",ch_wrk->name);
				break;
		}
		else
#endif
		{
		(void)fprintf(file,"\t%s",ch_wrk->name);
		switch(ch_wrk->mode){
			case SUBS:
				(void)fprintf(file,"\n");
				break;
			case FEED:
				(void)fprintf(file,"\tfeed mode\n");
				break;
			case EFEED:
				(void)fprintf(file,"\tefeed mode\n");
				break;
			case RFEED:
				(void)fprintf(file,"\trfeed %d mode\n",ch_wrk->m_param);
				break;
			case CNEWS:
				(void)fprintf(file,"\tcnews mode\n");
				break;
		}
		}
ch_groups=ch_wrk->next;
free(ch_wrk);
}
}

static void
pr_ui(sflag,file)
int sflag;
FILE *file;
{
#ifndef SDBA
char buf[40];
#endif

	if(ui_entry.flag&UF_SUSP)
#ifdef SDBA
		(void)fprintf(file,"Subscription of this user suspended\n");
#else
		(void)fprintf(file,mesg(29,lang));
#endif

#if AGE_TIME
#ifdef SDBA
	(void)fprintf(file,"Aging of subscription for this user %s\n",ui_entry.flag&UF_AGING?"on":"off");
#else
	if(sflag)
	(void)fprintf(file,">AGING %s\n",ui_entry.flag&UF_AGING?"ON":"OFF");
	else{
	(void)strcpy(buf,ui_entry.flag&UF_AGING?mesg(48,lang):mesg(49,lang));
	(void)fprintf(file,mesg(50,lang),buf);
	}
#endif
#endif

	if(ui_entry.limit!=0)
#ifdef SDBA
	(void)fprintf(file,"Limit size for notify lists %d kb\n"
#else
	if(sflag)
		(void)fprintf(file,">LIMIT %d\n",ui_entry.limit);
	else
		(void)fprintf(file,mesg(30,lang)
#endif
		    ,ui_entry.limit);

#ifdef SDBA
	(void)fprintf(file,"%s articles list format\n",
	    ui_entry.flag&UF_FMT?"New":"Old");
#else
	if(sflag)
	(void)fprintf(file,">FORMAT %s\n",ui_entry.flag&UF_FMT?"NEW":"OLD");
	else{
	(void)strcpy(buf,ui_entry.flag&UF_FMT?mesg(43,lang):mesg(44,lang));
	(void)fprintf(file,mesg(42,lang),buf);
	}
#endif
}

#ifdef SDBA
int
ch_all(entry,param,name)
long entry;
int *param;
char *name;
{
	if(fseek(fb,entry,SEEK_SET)){
serr:	
	bserrno=BS_SYSERR;
	return 1;
	}
	if(fread((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		goto serr;

	if(ui_entry.magic==DE_MAGIC)
		(void)fprintf(stdout,"There are no users at domain %s\n\n",name);
	else if(ui_entry.magic==UE_MAGIC){
		(void)fprintf(stdout,"User %s:\n",name);

	if(*param)
		(void)fprintf(stdout,"*** Subscription of this user disabled!\n");

	pr_ui(0,stdout);

	if(ui_entry.begin==NULL)
		(void)fprintf(stdout,"This user not subscribed to any group\n");
	else{
	ch_groups=NULL;
	if(ui_check())
		return 1;
	(void)fprintf(stdout,"This user subscribed to follows group(s):\n");
	pr_chg(0,stdout);
	}
	}
bserrno=BS_OK;
return 0;	
}
#endif

int
check(addr,file,sflag,noflag)
char *addr;
FILE *file;
int sflag,noflag;
{
	int stay_open = 0;
#ifdef SDBA
	int allmode=0;
	if(cmdcmp(addr,"ALL"))
		allmode=1;
#endif
ch_groups=NULL;

if(fb==NULL){
	if(open_base(OPEN_RDONLY)){
#ifdef SDBA
		pberr("sdba",stderr);
#endif
		return 1;}
}
else
	stay_open=1;

#ifdef SDBA
	if(allmode)
		get_utree(header.user,isdis,ch_all,0,0,(char *)NULL);
	else
#endif
	{
	if(getuser(addr,1,0)){
#ifdef SDBA
			pberr("sdba",stderr);
#else
		if(bserrno==BS_ENOUSER){
			if(noflag)
				fprintf(file, mesg(10, lang));
			return 0;
		}
#endif
		goto err;}

	if(bserrno==BS_DISDOMAIN)
#ifdef SDBA
		(void)fprintf(file,"*** Subscription of this user disabled!\n");
#else
		(void)fprintf(file,mesg(40,lang));
#endif

	pr_ui(sflag,file);

	if(ui_entry.begin==NULL){
	if(noflag)
#ifdef SDBA
	(void)fprintf(file,"This user not subscribed to any group\n");
#else
	(void)fprintf(file,mesg(10,lang));
#endif
		goto end2;
	}

	if(ui_check()){
#ifdef SDBA
		pberr("sdba",stderr);
#endif
		goto err;
	}	

	pr_chg(sflag,file);

	}
end2:
if(!stay_open)
	close_base();
	return 0;

#ifndef SDBA
err:
	(void)fprintf(file,mesg(28,lang));
#else
	(void)fprintf(file,"Error in I/O operation\n");
err:
#endif
	return 1;

}
