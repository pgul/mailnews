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

int
sav_hdr()
{
if(fseek(fb,0L,SEEK_SET)){
serr:
	bserrno=BS_SYSERR;
	return 1;
}
if(fwrite((char *)&header,sizeof(struct base_header),1,fb)!=1)
	goto serr;
fflush(fb);
bserrno=BS_OK;
return 0;
}
