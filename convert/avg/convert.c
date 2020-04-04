/*
 * Convertor database format from avg's mail-news to mn-4.*
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
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../conf.h"
#include "../../dbsubs.h"

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *malloc(), *calloc();
extern void free();

extern int addgroup(), create(), subscribe(), atoi();
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

static char *
gfn(grp, fn)
	char *grp;
	char *fn;
{
	static char buf[200];
	char g[80], *p;

	strcpy(g, grp);
	p = g;
	while( (p = index(p, '.')) != NULL )
		*p = '/';
	sprintf(buf, "%s/%s/%s", NEWSSPOOL, g, fn);
	return buf;
}

static int
convert()
{
int stat;
FILE *fil,*subs;
char iline[256];
char sline[256];
char *q;
char *piline[2];
piline[0]=iline;
piline[1]=(char *)NULL;

/* read active and make group index */
if((fil=fopen(ACTIVE,"r"))==NULL){
	stat=1;
	goto end;
}
while(fgets(iline,sizeof iline,fil)!=NULL){
if(*iline=='\n')
continue;
{register char *p;
p=iline;
while(*p && *p != ' ' & *p != '\t')p++;
*p=0;}

stat=addgroup(iline,(char *)NULL,(char *)NULL);
if(stat) goto end;
}

rewind(fil);
/* convert subscribers data base */
while(fgets(iline,sizeof iline,fil)!=NULL){
{register char *pp;
pp=iline;
while(*pp && *pp != ' ' & *pp != '\t')pp++;
*pp=0;}
if((subs=fopen(gfn(iline,"SUBSCRIBERS"),"r"))==NULL)
continue;

while(fgets(sline,sizeof sline,subs)!=NULL){
if((q=index(sline,'\n'))!=NULL)
*q=0;
if(index(sline,'@')==NULL||index(sline,'!')!=NULL||index(sline,'%')!=NULL)
continue;
q=sline;
while(!ISspace(*q) && *q!=0 )q++;
if(*q==0){
stat=subscribe(sline,piline,SUBS,0,stdout);
goto next;}
*(q++)=0;
while(*q==' ' || *q=='\t')q++;
if(*q=='F'){
stat=subscribe(sline,piline,FEED,0,stdout);
goto next;}
else if(*q=='R'){
if(*(++q)=='F')
q++;
while(*q==' ' || *q=='\t')q++;
stat=subscribe(sline,piline,RFEED,atoi(q),stdout);
}
next:
if(stat) goto end;
}
fclose(subs);
}

end:
return stat;
}
