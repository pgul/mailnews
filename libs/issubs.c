/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "dbsubs.h"

int 
issubs(addr,gname)
char *addr;
char *gname;
{
int stay_open,stat;

bserrno=BS_OK;
stat=0;
if(fb==NULL){
	stay_open=0;
	if(open_base(OPEN_RDONLY))
		goto err;
}
else
	stay_open=1;

if(getuser(addr,0,0)){
	if(bserrno==BS_ENOUSER){
		if(!stay_open)
			close_base();
		goto end2;
	}
	else if(bserrno==BS_DISDOMAIN)
		return 0;
	else
		goto err;
}
if(getgroup(gname))
	goto err;

boffset=ui_entry.begin;
while(boffset){
if(fseek(fb,boffset,SEEK_SET))
	goto err;
if(fread((char *)&sb_entry,sizeof(struct subs_chain),1,fb)!=1)
	goto err;
if(sb_entry.group==goffset){
	stat=1;
	break;}
boffset=sb_entry.user_next;
}

if(!stay_open)
    close_base();
return stat;

  err:
    retry_base();
  end2:
return 0;
}
