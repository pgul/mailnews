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
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "dbsubs.h"
#endif

int
isdis(entry,param,name)
long entry;
int *param;
char *name;
{
	if(fseek(fb,entry,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;}
	if(fread((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
		goto serr;
	if(dm_entry.flag&DF_DIS)
		*param=1;
bserrno=BS_OK;
return 0;
}