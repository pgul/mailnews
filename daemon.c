/*
 * Usenet to E-mail Gateway
 *
 * Requests queue for mailnews daemon.
 *
 * (C) Copyright 1992-1995 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is public-domain thus you can use it, modify it
 * or redistribute it as long as you keep this copyright note unchanged.
 * You aren't allowed to sell it. The author is not responsible for
 * the consequences of use of this software. Modifyed versions
 * should be explicitly marked as such. This code is not a subject to
 * any license of AT&T or of the Regents of UC, Berkeley or of DEMOS, Moscow.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sysexits.h>
#include "conf.h"
#include "nnrp.h"

#ifndef lint
static char sccsid[] = "@(#)news.c   1.2  11/21/92";
#endif

char pname[80];

extern int sofgets(char *, int, FILE *);

extern int do_queue();

void
main(argc,argv)
int argc;
char **argv;
{
	char tempfilename[300];
	int c,setr;
	FILE *fd;

	strcpy(pname, argv[0]);
	
	setr=0;
	if(argc>1){
		if(strcmp(argv[1],"-q")==0)
			goto cq;
		if(strcmp(argv[1],"-r")==0)
			setr=1;
		if(strcmp(argv[1],"-f")==0)
			goto proc;
	}

	sprintf(tempfilename, "%s/N.%d", QUEUE_DIR, getpid());

	/*
	 * Create file 
	 */
	if( (fd = fopen(tempfilename,"w")) == NULL) 
		exit(EX_CANTCREAT);  

	if(setr)
		fprintf(fd,"From %s\n",argv[2]);

	while((c=getchar())!=EOF)        
		fputc(c,fd);
	fclose(fd);
proc:
	if(access(STOP_F,F_OK)==0)
		exit(EX_OK);

	sprintf(tempfilename, "%s/LOCK", QUEUE_DIR);
cq:
	if((fd=fopen(tempfilename,"r"))!=NULL){
		fscanf(fd,"%d",&c);
		fclose(fd);
		if(kill(c,0)==0)
			exit(EX_OK);
		else
			unlink(tempfilename);
	}

	switch(fork()){
		case 0:
			if((c=open(tempfilename,O_WRONLY|O_CREAT|O_EXCL,0660))!=-1){
				fd=fdopen(c,"w");
				fprintf(fd,"%d",getpid());
				fclose(fd);
				c=do_queue();
				// sleep(1);
#ifdef NNTP_ONLY
				if (fsocket) {
					char str[80];
					fprintf(fsocket, "QUIT\r\n");
					fflush(fsocket);
					sofgets(str, sizeof(str), fsocket);
					fclose(fsocket);
				}
#endif
				unlink(tempfilename);
				exit(c);
			}
			else
				exit(0);
		case -1:
			exit(EX_OSERR);
		default:
			exit(EX_OK);
	}
}

int
do_queue()
{
	char cfnam[15];
	char tempfilename[300];
	DIR *sdir;
	struct dirent *direntry;
	int fd,found,status,pid,cnt;

	if((sdir=opendir(QUEUE_DIR))==NULL){
		fprintf(stderr, "Can't read spool directory\n");
		return 2;
	}

one_more:
	cnt=0;

	do{
		found=0;
		while((direntry=readdir(sdir))!=NULL){
			if(direntry->d_ino==0)
				continue;
			if(strncmp(direntry->d_name,"N.",2)==0){
				strcpy(cfnam,direntry->d_name);
				found=1;
				break;
			}
		}

		if(found){
#ifdef NNTP_ONLY
			if (connect_nnrp()) {
				log("Can't connect to NNTP server\n");
				return 2;
			}
#endif
			log("JOB %s STARTED\n",cfnam);
			sprintf(tempfilename, "%s/%s", QUEUE_DIR, cfnam);
			switch(pid=fork()){
				case 0:
					signal(SIGCLD,SIG_DFL);
					if((fd=open(tempfilename,O_RDONLY))==-1)
						return EX_IOERR;
					close(0);
					dup(fd);
					{ register i;
					  for(i=3;i<20;i++)
#ifdef NNTP_ONLY
					    if (i!=sockfd)
#endif
						close(i);
					}
#ifdef NNTP_ONLY
					{ char str[10];
					  sprintf(str, "%u", sockfd);
#ifdef DEBUG
					  printf("exec '" SUBSCR " -s %s'\n", str);
#endif
					  execl(SUBSCR,"server","-s",str,NULL);
					}
#else
					execl(SUBSCR,"server",NULL);
#endif
					return EX_OSERR;
				case -1:
					fprintf(stderr, "Can' fork \n");
					return 2;
				default:
	    				status=0;
#if defined(_POSIX_SOURCE) || defined(ISC)
	   				waitpid(pid, &status, 0);
#else
	    				wait(&status);
#endif
					if(status == 0)
						if(unlink(tempfilename))
						{	log("Can't unlink %s\n",tempfilename);
							return EX_OSERR;
						}
						else{
							cnt++;
	    						log("JOB %s FINISHED\n",cfnam);
						}
					else
					{
						char badname[4096];
						log("server retcode %d\n",status);
						sprintf(badname, "%s/bad.%s", QUEUE_DIR, cfnam);
						rename(tempfilename, badname);
						return EX_OSERR;
					}
			}
		}
		if(access(STOP_F,F_OK)==0)
			return EX_OK;
	}while(found);
	if(cnt){
		rewinddir(sdir);
		goto one_more;
	}
	closedir(sdir);
	return EX_OK;
}
