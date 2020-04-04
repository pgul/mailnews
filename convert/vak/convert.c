/*
 * Convertor database format from vak 2.7 to mn-4.*
 *
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)conert.c 1.0 03/07/94";
#endif

#define usg(a) fprintf(stderr,a)

#if defined( SYSV ) || defined( USG )
#define index   strchr
#define rindex  strrchr
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "../../conf.h"
#include "../../dbsubs.h"
#include "groups.h"

extern char *malloc(), *calloc();
extern void free();

extern int addgroup(), create();
static int convert();

main()
{
int stat;
char iline[20];

stat=create(0);
if(stat==2){
printf("Data base already exists. Remove y/n [n]: ");
gets(iline);
if(*iline=='y' || *iline=='Y')
stat=create(1);
else{
stat=0;
goto end;
}
}

if(stat)
goto end;

open_base(OPEN_NOSAVE);

stat=convert();

close_base();
save(BASEDIR,SUBSBASE,1);

end:
exit(stat);
}

static int 
convert()
{
    int mode,param,gtl_size,utl_size,i,j,l;
    char gname[160],uname[160],*p;
    long *grtaglist;
    struct subscrtab *utaglist;
    char *sgroup[2]={{(char *)NULL},{(char *)NULL}};

setgroupsfile(NULL);
if(loadgroups(0)!=1)
	return 1;
grtaglist=grouplist(&gtl_size);

for(i=0;i<gtl_size;i++){
	if((p=groupname(grtaglist[i]))==NULL)
		continue;
	l=strlen(p);
	if(l<1 || l > sizeof gname)
		continue;
	strcpy(gname,p);
	if(addgroup(gname,(char *)NULL,(char *)NULL))
		continue;
	sgroup[0]=gname;
	utaglist=groupsubscr(grtaglist[i],&utl_size);
	for(j=0;j<utl_size;j++){
		if((p=username(utaglist[j].tag))==NULL)
			continue;
		l=strlen(p);
		if(l<1 || l > sizeof uname)
			continue;
		strcpy(uname,p);
		switch(utaglist[j].mode){
		default:
			continue;
		case 'f':
			mode=FEED;
			param = 0;
			break;
		case 's':
			mode=SUBS;
			param = 0;
			break;
		case 'A':
			mode=RFEED;
			param = 2;
			break;
		case 'B':
			mode=RFEED;
			param = 3;
			break;
		case 'C':
			mode=RFEED;
			param = 5;
			break;
		case 'D':
			mode=RFEED;
			param = 8;
			break;
		case 'E':
			mode=RFEED;
			param = 10;
			break;
		case 'F':
			mode=RFEED;
			param = 15;
			break;
		case 'G':
			mode=RFEED;
			param = 20;
			break;
		case 'H':
			mode=RFEED;
			param = 30;
			break;
		case 'I':
			mode=RFEED;
			param = 50;
			break;
		case 'J':
			mode=RFEED;
			param = 80;
			break;
		case 'K':
			mode=RFEED;
			param = 200;
			break;
		}
		printf("%s",uname);
		subscribe(uname,sgroup,mode,param,stdout);
	}
}
savegroups();
return 0;
}
