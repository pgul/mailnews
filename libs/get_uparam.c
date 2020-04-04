/*
 * (C) Copyright 1994-1995 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#ifndef MEMMAP
#include <stdio.h>
#include "dbsubs.h"
#endif

int 
get_uparam(addr)
char *addr;
{
int stay_open;

bserrno=BS_OK;
if(fb==NULL){
	stay_open=0;
	if(open_base(OPEN_RDONLY))
		goto err;
}
else
	stay_open=1;

if(getuser(addr,0,0)){
	if(bserrno==BS_ENOUSER)
		goto end2;
	else if(bserrno==BS_DISDOMAIN)
		goto end2;
	else
		goto err;
}
if(!stay_open)
    close_base();
return 0;
err:
    retry_base();
    return 1;
end2:
if(!stay_open)
    close_base();
return 1;
}
