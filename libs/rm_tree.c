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

int
#ifdef __STDC__
rm_tree(long int begin, int (*func) (long, char *), int level, int g_n, char *name)
#else
rm_tree(begin,func,level,g_n,name)
long begin;
int (*func)();
int g_n,level;
char *name;
#endif
{
	long curoffset;
	struct string_chain lst_entry;
	char lwbuff[160];

	if(++level>10){
berr:
		bserrno=BS_BASEERR;
err:
		return 1;
	}

	for(curoffset=begin;curoffset;curoffset=lst_entry.next){
		if(fseek(fb,curoffset,SEEK_SET)){
serr:
			bserrno=BS_SYSERR;
			return 1;
		}
		if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
			goto serr;

		if(!lst_entry.entry && !lst_entry.down_ptr)
			goto berr;

		if(g_n) /* domains */
			if(lst_entry.up_ptr){
				(void)strncpy(lwbuff,name,sizeof(lwbuff));
				(void)strins(lwbuff,lst_entry.name,lst_entry.dot_at);
			}
			else
				(void)strncpy(lwbuff,lst_entry.name,sizeof(lwbuff));
		else /* groups */
			if(name){
				(void)strncpy(lwbuff,name,sizeof(lwbuff));
				(void)strncat(lwbuff,lst_entry.name,sizeof(lwbuff));
			}
			else
				(void)strncpy(lwbuff,lst_entry.name,sizeof(lwbuff));

		if(lst_entry.down_ptr) {
			if ((lst_entry.dot_at=='.') && !g_n)
				strncat(lwbuff,".",sizeof(lwbuff));
			if(rm_tree(lst_entry.down_ptr,func,level,g_n,lwbuff))
				goto err;
			if (lst_entry.dot_at=='.')
				lwbuff[strlen(lwbuff)-1]=0;
		}
		if(lst_entry.entry)
			if((func)(lst_entry.entry,lwbuff))
				goto err;
		if(header.freesp!=curoffset)
			add_free(curoffset);
	}
	bserrno=BS_OK;
	return 0;
}

