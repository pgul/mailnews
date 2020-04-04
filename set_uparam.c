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
static char sccsid[]="@(#)set_uparam.c 4.0 01/26/94";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "dbsubs.h"

#ifndef SDBA
extern int lang;
#endif
extern int cmdcmp();
extern long time();

#define ISspace(x) ( x==' ' || x=='\t' )

int 
detlang(lang)
char *lang;
{
	register char *p;
	register lg;

	p=lang;
	while(ISspace(*p))p++;
	for(lg=0;lg<3;lg++){
	if(!strcmp(p,ext[lg]))
		break;
	}
	if(lg>=3)
		lg=DEFLANG;
return lg;
}


static int
sav_user()
{
	if(fseek(fb,uoffset,SEEK_SET))
		return 1;
	if(lock(fb,F_WRLCK)<0)
		return 1;
	if(fwrite((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		return 1;
	if(lock(fb,F_RDLCK)<0)
		return 1;
	return 0;
}

int
set_uparam(addr,mode,param,file)
char *addr;
int mode,param;
FILE *file;
{
int stay_open = 0;
#ifndef SDBA
char buf[40];
#endif
register flags;

if(fb==NULL){
	if(open_base(omode)){
#ifdef SDBA
		pberr("sdba",stderr);
#endif
		return 1;}
}
else
	stay_open=1;
	
	if(getuser(addr,0,(mode==US_LANG || mode==US_LIMIT))){
#ifdef SDBA
		if(bserrno==BS_DISDOMAIN)
			pberr(get_domain(dm_entry.s_domain),stderr);
		else
#endif	
			if(bserrno==BS_ENOUSER){
#ifndef SDBA
				fprintf(file,mesg(10,lang));
#else
				fprintf(file,"User %s unknown\n",addr);
#endif
				return 0;
			}
#ifdef SDBA
		else
			pberr("sdba",stderr);
#endif
		
		goto err;}

	switch(mode){
case US_SUSP:
	flags=ui_entry.flag;
	if(param)
		ui_entry.flag|=UF_SUSP;
	else
		ui_entry.flag&=(~UF_SUSP);
	

	if(flags==ui_entry.flag){
#ifdef SDBA
		fprintf(file,"Subscription are %s suspended\n",param?"already":"not");
#else
		fprintf(file,param?mesg(29,lang):mesg(31,lang));
#endif
		break;
	}
	if(sav_user())
		goto err2;
#ifdef SDBA
	fprintf(file,"\t%s --OK\n",param?"suspend":"resume");
#else
	fprintf(file,mesg(24,lang),param?"suspend":"resume");
#endif
	break;
case US_LIMIT:
	if(param>255 || param<1)
		param=0;
	if(ui_entry.limit!=param){
		ui_entry.limit=param;
		if(sav_user())
			goto err2;
	}
#ifdef SDBA
	fprintf(file,"\tlimit %d --OK\n",param);
#else
	sprintf(buf,"%d",param);
	fprintf(file,mesg(24,lang),buf);
#endif
	break;
case US_FMT:
	flags=ui_entry.flag;
	if(param)
		ui_entry.flag|=UF_FMT;
	else
		ui_entry.flag&=(~UF_FMT);

	if(flags==ui_entry.flag){
#ifdef SDBA
		fprintf(file,"This format alredy set for user %s\n",addr);
#else
		fprintf(file,mesg(41,lang));
#endif
		break;
	}
	if(sav_user())
		goto err2;
#ifdef SDBA
	fprintf(file,"\t%s --OK\n",param?"new":"old");
#else
	strcpy(buf,param?mesg(43,lang):mesg(44,lang));
	fprintf(file,mesg(42,lang),buf);
#endif
	break;	
#if AGE_TIME
case US_AGING:
	flags=ui_entry.flag;
	if(param)
		ui_entry.flag|=UF_AGING;
	else
		ui_entry.flag&=(~UF_AGING);
	

	if(flags==ui_entry.flag){
#ifdef SDBA
		fprintf(file,"Aging already %s for user %s\n",param?"on":"off",addr);
#else
		strcpy(buf,param?mesg(48,lang):mesg(49,lang));
		fprintf(file,mesg(47,lang),buf);
#endif
		break;
	}
	else if(param)
		ui_entry.last_used=time((long *)NULL)/86400;

	if(sav_user())
		goto err2;
#ifdef SDBA
	fprintf(file,"\tAging %s --OK\n",param?"on":"off");
#else
	strcpy(buf,param?mesg(48,lang):mesg(49,lang));
	fprintf(file,mesg(50,lang),buf);
#endif
	break;	
#endif
case US_NLHLP:
	flags=ui_entry.flag;
	if(param)
		ui_entry.flag|=UF_NLHLP;
	else
		ui_entry.flag&=(~UF_NLHLP);
	

	if(flags==ui_entry.flag){
#ifdef SDBA
		fprintf(file,"List help already %s\n",param?"disabled":"enabled");
#else
		fprintf(file,param?mesg(58,lang):mesg(57,lang));
#endif
		break;
	}
	if(sav_user())
		goto err2;
#ifdef SDBA
	fprintf(file,"List help file %s.\n",param?"disabled":"enabled");
#else
	fprintf(file,param?mesg(58,lang):mesg(57,lang));
#endif
	break;
case US_NONGRP:
	flags=ui_entry.flag;
	if(param)
		ui_entry.flag|=UF_NONGRP;
	else
		ui_entry.flag&=(~UF_NONGRP);
	

	if(flags==ui_entry.flag){
#ifdef SDBA
		fprintf(file,"New group lists already %s\n",param?"disabled":"enabled");
#else
		fprintf(file,param?mesg(61,lang):mesg(60,lang));
#endif
		break;
	}
	if(sav_user())
		goto err2;
#ifdef SDBA
	fprintf(file,"New group list %s.\n",param?"disabled":"enabled");
#else
	fprintf(file,param?mesg(61,lang):mesg(60,lang));
#endif
	break;
case US_LANG:
	if(ui_entry.lang!=param){
	ui_entry.lang=param;
	if(sav_user())
		goto err2;
	}
#ifdef SDBA
	fprintf(file,"\t%s --OK\n",ext[param]);
#else
	fprintf(file,mesg(24,lang),ext[param]);
#endif
	}
if(!stay_open)	
	close_base();
return 0;

err2:
#ifndef SDBA
err:
	fprintf(file,mesg(28,lang));
#else
	fprintf(file,"Error in I/O operation\n");
err:
#endif
	retry_base();
	return 1;
}
