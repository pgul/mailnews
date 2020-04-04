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

char *
get_domain(st_ptr)
long st_ptr;
{
	bserrno=BS_OK;
	user[0]=0;
	for(coffset=st_ptr;coffset;coffset=st_entry.up_ptr){
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
		(void)strcat(user,st_entry.name);
		if (st_entry.up_ptr)
			if (st_entry.dot_at=='.')
				(void)strcat(user,".");
			else if (st_entry.dot_at=='@')
				(void)strcat(user,"@");
	}
	return user;

err:
	return (char *)NULL;
}
