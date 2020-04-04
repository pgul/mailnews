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
#include "dbsubs.h"

long
sav_entry(magic,entry,size)
ulong magic;
char *entry;
int size;
{
register long loffset;

if((loffset=get_free(magic))==NULL){
if(bserrno==BS_NOFREE){
if(fseek(fb,0L,SEEK_END)){
serr:
	bserrno=BS_SYSERR;
	return NULL;}
loffset=ftell(fb);
}
else
	return NULL;
}
else
if(fseek(fb,loffset,SEEK_SET))
	goto serr;

if(fwrite(entry,size,1,fb)!=1)
	goto serr;

return loffset;
}
