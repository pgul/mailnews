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
#include <errno.h>
#include "dbsubs.h"

#ifdef __STDC__
extern int link(const char *, const char *),unlink(const char *);
#else
extern int link(),unlink();
#endif

static char filename[200],cpbuf[1024];
static long fsize;
static int cr;

FILE *
save(dir,file,flag)
char *dir;
char *file;
int flag;
{
	FILE *fil;

	bserrno=BS_OK;
	(void)sprintf(filename,"%s/%s",dir,file);
	if((fil=fopen(filename,"r+"))==NULL){
		if(errno==ENOENT)
			bserrno=BS_BASEERR;
		else
			bserrno=BS_SYSERR;
		goto end;
	}
	if(lock(fil,F_WRLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;}

	if(fseek(fil,0L,SEEK_END))
		goto err;

	fsize=ftell(fil);
	if(fseek(fil,0L,SEEK_SET))
		goto err;
	
	(void)sprintf(filename,"%s/.%s",dir,file);
	{register FILE *tmpf;
	if((tmpf=fopen(filename,"w"))==NULL)
		goto err;

	while((cr=fread((char *)cpbuf,sizeof(char),sizeof cpbuf,fil))!=NULL){
		if(fwrite((char *)cpbuf,sizeof(char),cr,tmpf)!=cr)
			goto err;
		else
			fsize-=cr;
	}
	if(fsize)
		goto err;
	(void)fclose(tmpf);
	}
	rewind(fil);

	if(lock(fil,F_RDLCK)<0){
		bserrno=BS_ENOLOCK;
		goto err;}

end:
if(flag)
(void)fclose(fil);
return fil;
err:
(void)fclose(fil);
return (FILE *)NULL;
}

int
restore(dir,file,fil)
char *dir;
char *file;
FILE *fil;
{
	char lfilname[200];

	(void)sprintf(lfilname,"%s/.%s",dir,file);
	if(fil==NULL){
	if((fil=fopen(lfilname,"r"))==NULL)
		goto err;
	}
	else{
	if((fil=freopen(lfilname,"r",fil))==NULL)
		goto err;
	}

	if(fseek(fil,0L,SEEK_END))
		goto err;
	fsize=ftell(fil);
	if(fseek(fil,0L,SEEK_SET))
		goto err;

	(void)sprintf(filename,"%s/%s~",dir,file);
	{register FILE *tmpf;
	if((tmpf=fopen(filename,"w"))==NULL)
		goto err2;

	while((cr=fread((char *)cpbuf,sizeof(char),sizeof cpbuf,fil))!=NULL){
		if(fwrite((char *)cpbuf,sizeof(char),cr,tmpf)!=cr)
			goto err2;
		else
			fsize-=cr;
	}
	if(fsize)
		goto err2;

	(void)fclose(tmpf);
	}
(void)fclose(fil);
	(void)sprintf(lfilname,"%s/%s",dir,file);
	if(unlink(lfilname))
		goto err;
	if(!link(filename,lfilname))
		(void)unlink(filename);
	else
		goto err;
return 0;
err2:
(void)fclose(fil);
err:
return 1;
}
