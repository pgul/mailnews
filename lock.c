/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1992 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "conf.h"

#ifndef lint
static char sccsid[] = "@(#)lock.c   2.3  10/31/92";
#endif

extern int rand(), kill();
extern void srand();
extern int imm,find;

/*
 * Lock the run file
 */
void lockrun(dir)
char *dir;
{
    time_t tt;
    char templockname[300];
    char lockfilename[300];
    FILE *fd;
    int lpid;
    int ntry = 20;

    sprintf(templockname, "%s/L.%d", dir, getpid());
    sprintf(lockfilename,imm?(find?"%s/LOCK.f":"%s/LOCK.i"):"%s/LOCK", dir);
#ifdef DEBUG
    printf("LOCKING: %s -> %s\n", templockname, lockfilename);
#endif

	/*
	 * Create temporary lockfile
	 */
    if ((fd = fopen(templockname, "w")) == NULL) {
	fprintf(stderr, "send-list: cannot create lock file in %s\n", dir);
	exit(72);		/* EX_OSFILE */
    }
    fprintf(fd, "%d", getpid());
    fclose(fd);

	/*
	 * Link temp lockfile to the effective lock
	 */
    time(&tt);
    srand((int) tt);

  tryagain:
#ifdef DEBUG
    printf("TRY TO LINK...\n");
#endif
    if (ntry-- <= 0) {
	unlink(templockname);
	exit(0);
    }
    if ((fd = fopen(lockfilename, "r")) != NULL) {
	fscanf(fd, "%d", &lpid);
	fclose(fd);
	if (kill(lpid, 0) == 0) {
	    sleep(40 + (rand() & 07));
	    goto tryagain;
	} else {
	    unlink(lockfilename);
	    if (link(templockname, lockfilename) < 0)
		goto tryagain;
	}
    } else
	link(templockname, lockfilename);

	/*
	 * Remove temporary lockfile
	 */
    unlink(templockname);
#ifdef DEBUG
    printf("LOCKING OK\n");
#endif
}

/*
 * Unock the run file
 */
void unlockrun(dir)
char *dir;
{
    char lockfilename[300];

    sprintf(lockfilename, "%s/LOCK%s", dir, imm?(find?".f":".i"):"");
  //sprintf(lockfilename, imm?(find?"%s/LOCK.f":"%s/LOCK.i"):"%s/LOCK", dir);
#ifdef DEBUG
    printf("REMOVE LOCK %s\n", lockfilename);
#endif
    unlink(lockfilename);
}

void lexit(n)
int n;
{
    unlockrun(WORKDIR);
    exit(n);
}

#ifdef OS2
#include <malloc.h>
int link(char * to, char * from)
{
  char * buf;
  FILE * f;
  long l;
  
  f=fopen(from,"r");
  if (f==NULL) return -1;
  fseek(f,0,SEEK_END);
  l=ftell(f);
  buf=malloc(l);
  if (buf==NULL)
  { fclose(f);
    return -1;
  }
  fread(buf,l,1,f);
  fclose(f);
  f=fopen(to,"w");
  if (f==NULL)
  { free(buf);
    return -1;
  }
  fwrite(buf,l,1,f);
  fclose(f);
  free(buf);
  return 0;
}
#endif
  
