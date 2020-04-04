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

char user[160];

char *
get_uaddr(st_ptr)
long st_ptr;
{
	bserrno=BS_OK;
	{	register i;
		for(i=0;i<sizeof user;i++) user[i]=0;
	}
	for(coffset=st_ptr;coffset;coffset=st_entry.up_ptr){
		if(fseek(fb,coffset,SEEK_SET)){
serr:
			bserrno=BS_SYSERR;
err:
			return (char *)NULL;
		}
		if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
			goto serr;
		if(st_ptr!=coffset && st_entry.entry){
			if(fseek(fb,st_entry.entry,SEEK_SET))
				goto serr;
			if(fread((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
				goto serr;
			if(dm_entry.flag&DF_DIS)
				bserrno=BS_DISDOMAIN;
		}
		if(st_entry.magic!=SE_MAGIC){
			bserrno=BS_BASEERR;
			goto err;
		}
		if(user[0])
			(void)strcat(user,st_entry.name);
		else
			(void)strcpy(user,st_entry.name);
		user[strlen(user)]=st_entry.dot_at;
	}
	return user;
}
