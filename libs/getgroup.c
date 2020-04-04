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

extern char *index();

int
getgroup(gname)
char *gname;
{
	int res;
	char *p,*q;

	goffset=0;

	if (strlen(gname)>=160) /* 160=sizeof(group)=sizeof(wbuff) */
	{	bserrno=BS_ENOGROUP;
		goto err;
	}
	(void)strcpy(group,gname);
	(void)strcpy(wbuff,gname);

	soffset=header.group;
	poffset=NULL;
	p=wbuff;
	q=index(p,'.');
	while(p){
		char dot_at;
		dot_at=0;
		if (q==NULL) {
			if (strlen(p)>=MAX_STR_EL)
				q=p+MAX_STR_EL-1;
		}
		else if (q-p>=MAX_STR_EL)
			q=p+MAX_STR_EL-1;
		if(q) {
			dot_at=*q;
			*q++=0;
		}

		for(coffset=soffset;coffset;poffset=coffset,coffset=st_entry.next){
			if(fseek(fb,coffset,SEEK_SET))
				goto serr;
			if(fread((char *)&st_entry,sizeof(struct string_chain),1,fb)!=1)
				goto serr;
			if(st_entry.magic!=SE_MAGIC){
				bserrno=BS_BASEERR;
				goto err;
			}
			res=strcmp(p,st_entry.name);
			if ((res==0) && q)
				if (((dot_at=='.') && (st_entry.dot_at!='.')) ||
				    ((dot_at!='.') && (st_entry.dot_at=='.')))
					res=1;
			if(res>0)
				continue;
			if(res==0)
				goto nl;
			else if(res<0){
				if(q)
					sprintf(wbuff,"%s%c%s",p,dot_at,q);
				else
					strcpy(wbuff,p);
				bserrno=BS_ENOGROUP;
				goto err;
			}
		}/* for */
		if(q)
			sprintf(wbuff,"%s%c%s",p,dot_at,q);
		else
			strcpy(wbuff,p);
		bserrno=BS_ENOGROUP;
		goto err;
nl:
		if (q && (dot_at!='.'))
			*--q=dot_at;
		p=q;
		if(p){
			q=index(p,'.');
			poffset=coffset;
			if((soffset=st_entry.down_ptr)==NULL){
				strcpy(wbuff,p);
				bserrno=BS_ENOGROUP;
				goto err;
			}
		}
		else
			if(st_entry.entry==NULL){
				wbuff[0]=0;
				bserrno=BS_ENOGROUP;
				goto err;
			}
	}/*while*/

	goffset=st_entry.entry;
	if(fseek(fb,goffset,SEEK_SET))
		goto serr;
	if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
		goto serr;
	if(gr_entry.magic!=GE_MAGIC){
		bserrno=BS_BASEERR;
		goto err;
	}

	bserrno=BS_OK;
	return 0;

serr:
	bserrno=BS_SYSERR;
err:
	return 1;
}
