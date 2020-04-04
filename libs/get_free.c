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

long
get_free(magic)
ulong magic;
{
struct free_sp fr_entry;
long fpoffset,fcoffset;

for(fpoffset=NULL,fcoffset=header.freesp;fcoffset;fpoffset=fcoffset,fcoffset=fr_entry.next){
if(fseek(fb,fcoffset,SEEK_SET))
	goto serr;
if(fread((char *)&fr_entry,sizeof(struct free_sp),1,fb)!=1)
	goto serr;	
if((fr_entry.magic|FREE_MASK)==magic){
	if(lock(fb,F_WRLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;}
	if(fpoffset){
		if(fseek(fb,fpoffset+offset(fr_entry,next),SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		goto err;}
		if(fwrite((char *)&fr_entry.next,sizeof(fr_entry.next),1,fb)!=1){
		bserrno=BS_SYSERR;
		goto err;}
		bserrno=BS_OK;
		return fcoffset;
	}
	else{
		header.freesp=fr_entry.next;
		if(sav_hdr())
			goto err;
		bserrno=BS_OK;
		return fcoffset;
	}
	if(lock(fb,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;}
}
}
bserrno=BS_NOFREE;
err:
return (long)NULL;
}

