/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include "dbsubs.h"

#ifdef __STDC__
int
get_gtree(long int begin, int (*func) (long, int *, char *), int param, int level, char *name)
#else
int
get_gtree(begin,func,param,level,name)
long begin;
int (*func)();
int param,level;
char *name;
#endif
{
long curoffset;
struct string_chain lst_entry;
char gbuff[160];

if(++level>MAXLVL){
berr:
bserrno=BS_BASEERR;
err:
return 1;
}

for(curoffset=begin;curoffset;curoffset=lst_entry.next){
	if(fseek(fb,curoffset,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;}
	if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;

	if(!lst_entry.entry && !lst_entry.down_ptr)
		goto berr;

	if(name){
		(void)strncpy(gbuff,name,sizeof(gbuff));
		(void)strncat(gbuff,lst_entry.name,sizeof(gbuff));
	}
	else
		(void)strncpy(gbuff,lst_entry.name,sizeof(gbuff));

	if(lst_entry.entry)
		if((func)(lst_entry.entry,&param,gbuff))
			goto err;
	if(lst_entry.down_ptr) {
		if (lst_entry.dot_at)
			(void)strncat(gbuff,".",sizeof(gbuff));
		if(get_gtree(lst_entry.down_ptr,func,param,level,gbuff))
			goto err;
	}
}
bserrno=BS_OK;
return 0;
}
