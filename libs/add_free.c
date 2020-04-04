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
#include <fcntl.h>
#include "dbsubs.h"

int
add_free(element)
long element;
{
struct free_sp fr_entry;

if(fseek(fb,element,SEEK_SET))
	goto serr;
if(fread((char *)&fr_entry,sizeof(struct free_sp),1,fb)!=1)
	goto serr;
if (!(fr_entry.magic & FREE_MASK)) {  /* gul */
	bserrno=BS_OK;
	return 0;
}
	
fr_entry.magic&=(~FREE_MASK);
fr_entry.next=header.freesp;

if(fseek(fb,element,SEEK_SET))
	goto serr;

if(lock(fb,F_WRLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}

if(fwrite((char *)&fr_entry,sizeof(struct free_sp),1,fb)!=1){
serr:
	bserrno=BS_SYSERR;
err:
	return 1;}

if(lock(fb,F_RDLCK)<0){
	bserrno=BS_ENOLOCK;
	goto err;}

header.freesp=element;
if(sav_hdr())
    goto err;
bserrno=BS_OK;
return 0;
}

