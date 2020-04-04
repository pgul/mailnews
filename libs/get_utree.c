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
#include "dbsubs.h"
#endif

#ifdef __STDC__
int
get_utree(long int begin, int (*dfunc) (long, int *, char *),\
  int (*ufunc) (long, int *, char *), int param, int level, char *name)
#else
int
get_utree(begin,dfunc,ufunc,param,level,name)
long begin;
int (*dfunc)(),(*ufunc)();
int param,level;
char *name;
#endif
{
long curoffset;
struct string_chain lst_entry;
char ubuff[160],cbuff[160];
int lparam;

if(++level>MAXLVL){
berr:
bserrno=BS_BASEERR;
err:
return 1;
}

if(name)
	(void)strncpy(cbuff,name,sizeof(cbuff));
else
	cbuff[0]=0;	

for(curoffset=begin;curoffset;curoffset=lst_entry.next){
	lparam=param;
	if(fseek(fb,curoffset,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;}
	if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;

	if(curoffset==begin && !name && lst_entry.up_ptr)
		strncpy(cbuff,get_uaddr(lst_entry.up_ptr),sizeof(cbuff));

	if(!lst_entry.entry && !lst_entry.down_ptr)
		goto berr;

	if(cbuff[0]){
		(void)strncpy(ubuff,cbuff,sizeof(ubuff));
		(void)strins(ubuff,lst_entry.name,lst_entry.dot_at);
	}
	else
		if(lst_entry.name[0]=='@')
			ubuff[0]=0;
		else
			(void)strncpy(ubuff,lst_entry.name,sizeof(ubuff));

	if(lst_entry.down_ptr){
		if(lst_entry.entry)
			if((dfunc)(lst_entry.entry,&lparam,ubuff))
				goto err;
		if(get_utree(lst_entry.down_ptr,dfunc,ufunc,lparam,level,ubuff))
			goto err;
	}
	else{
		if(lst_entry.entry)
			if((ufunc)(lst_entry.entry,&lparam,ubuff))
				goto err;
	}
}
bserrno=BS_OK;
return 0;
}
