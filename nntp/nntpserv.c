#ifndef lint
static char * scsid = "@(#)nntpserv.c 1.10 93/12/11";
#endif

#include "conf.h"

#include <sys/types.h>
#ifdef LAI_TCP
#include <sys/bsdtypes.h>
#endif
#ifdef USG
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <setjmp.h>
#ifndef NONETDB
#include <netdb.h>
#endif
#include <signal.h>
#include "dbsubs.h"
#include "local_conf.h"

char	*malloc();
char	*strcpy();
char	*strcat();
char	*rindex();
long	time();
u_long	inet_addr();

extern int errno;

#ifdef USE_DBZ
extern void closehist();
extern int alreadyseen();
#endif

extern int alarm(),getpid(),unlink();
extern void free();
extern char *strstr();

char *artlist[MAX_ARTICLES];
int server;			/* stream socket to the nntp server */
FILE * rd_fp, * wr_fp;
int newart, dupart, misart;
char * Pname;

extern char *calloc(),*inet_ntoa();
extern int read(),write(),open(),close(),get_nntp_conn();

struct nntp_job {
long	last_time;
char	*hies;
int	counter;
char	*name;
long	offset;
struct	nntp_job *next;
};

struct nntp_servers {
struct	nntp_job *begin;
u_long	server;
struct	nntp_servers *next;
} *servers;

struct nntp_levels {
struct	nntp_job *begin;
int	counter;
int	all_need;
} levels[MAXLVL];

char pname[80];

static	jmp_buf	SFGstack;

static void
to_sfgets()
{
	longjmp(SFGstack, 1);
}

int
sockread(buf)
char *buf;
{
	int	esave, rz;
	char * ret;
	if (setjmp(SFGstack)) {
		(void) alarm(0);	/* reset alarm clock */
		(void) signal(SIGALRM, SIG_DFL);
#if 0
def _IOERR
		rd_fp->_flag |= _IOERR;	/* set stdio error */
else
		rd_fp->_flags |= __SERR; /* New BSD version */
#endif
#ifndef ETIMEDOUT
		errno = EPIPE;		/* USG doesn't have ETIMEDOUT */
#else
		errno = ETIMEDOUT;		/* connection timed out */
#endif
		(void) perror("nntpserv: read error on server socket");
		(void) close(server);
		exit(1);
	}
	(void) signal(SIGALRM, to_sfgets);
	(void) alarm(TIMEOUT);
	ret  = fgets(buf, BUFSIZ, rd_fp);
	esave = errno;
	(void) alarm(0);			/* reset alarm clock */
	(void) signal(SIGALRM, SIG_DFL);	/* reset SIGALRM */
	errno = esave;
	rz = strlen(buf);
	buf[rz-2] = '\0';
	if (ret  == (char * ) 0) {
		(void) perror("nntpserv: read error on server socket");
		(void) fclose(rd_fp);
		exit(1);
	}
	return 0;
}

void
sockwrite(buf)
char *buf;
{
register int sz;
char buf2[BUFSIZ];
#ifdef DEBUG
	(void) printf(">>> %s\n", buf);
#endif
(void) strcpy(buf2,buf);
(void) strcat(buf2,"\r\n");
sz = strlen(buf2);
if (write(server,buf2,sz) != sz)
	{
	(void) printf("nntpserv: write error on server socket\n");
	(void) close(server);
	exit(1);
	}
}

void
artfetch(articleid)
char *articleid;
{
#ifdef DEBUG
	int lines = 0;
#endif
	char buf[BUFSIZ];
	FILE *inews,*batch;

	/* now, ask for the article */
	(void) sprintf(buf,"ARTICLE %s", articleid);
	sockwrite(buf);
	(void) sockread(buf);
#ifdef DEBUG
	(void) printf("%s\n", buf);
#endif
	if (buf[0] == '4')	/* missing article, just skipit */
		{
		misart++;
		return;
		}

	if (buf[0] != '2')	/* uh-oh, something's wrong! */
		{
		(void) printf("%s: protocol error: got '%s'\n", Pname, buf);
		(void) close(server);
		exit(1);
		}
	if ( (inews = fopen("/tmp/.art", "w+")) == (FILE *) 0)
		{
		perror("/tmp/.art");
		exit(1);
		}

	/* and here comes the article, terminated with a "." */
#ifdef DEBUG
	(void) printf("data\n");
#endif
	while (1)
		{
		(void) sockread(buf);
		if (buf[0] == '.' && buf[1] == '\0')
			break;
#ifdef DEBUG
		lines++;
#endif
		(void) strcat(buf,"\n");
		(void) fputs(((buf[0] == '.') ? buf + 1 : buf),
			   inews);
		}
#ifdef DEBUG
	(void) printf(".\n%d lines\n", lines);
#endif
	(void) fflush(inews);
	if ( (batch = fopen("/tmp/.batch", "a+")) == (FILE *) 0)
		{
		perror("/tmp/.batch");
		exit(1);
		}
	fprintf(batch,"#! rnews %d\n",(int)ftell(inews));
	rewind(inews);
	while(fgets(buf,sizeof buf,inews)!=NULL)
		(void) fputs(buf,batch);
	fclose(inews);		
	if(ftell(batch) >=BATCHSIZE){
	rewind(batch);
#ifdef DEBUG
	(void) printf("command: %s...", RNEWS);
#endif
	if ( (inews = popen(RNEWS, "w")) == (FILE *) 0)
		{
		perror(RNEWS);
		exit(1);
		}

	while(fgets(buf,sizeof buf,batch)!=NULL)
		(void) fputs(buf,inews);

	(void) fflush(inews);
	(void) pclose(inews);
#ifdef DEBUG
	(void) printf("Done\n");
#endif
	(void) unlink("/tmp/.batch");
	}
	(void) fclose(batch);
	return;
}

int
wewant(articleid)
char *articleid;
	{
#ifdef USE_DBZ
return alreadyseen(articleid);
#else
#ifdef DO_HIST_CHECK
	char id[BUFSIZ];
	char *p;
	char line[BUFSIZ];
	int len;
	FILE *histfp;
	
	/* remove any case sensitivity */
	(void) strcpy(id, articleid);
	p = id;
	histfp = fopen(HISTORY_FILE, "r");
	if (histfp == NULL) 
		{
		return 1;
		}
	len = strlen(articleid);
	while (fgets(line, sizeof (line), histfp)!=NULL)
		if(!strncmp(line,articleid,len))
			goto dup;

		(void) fclose(histfp);
#ifdef DEBUG
		(void) printf("new: '%s'\n", articleid);
#endif
		return 1;
dup:
	(void) fclose(histfp);
#ifdef DEBUG
	(void) printf("dup: '%s' %s\n", articleid,line);
#endif
	return 0;
#else
return 1;
#endif
#endif
}

char *
errmsg(code)
int code;
{
	extern int sys_nerr;
	//extern char *sys_errlist[];
	static char ebuf[6+5+1];

	if (code > sys_nerr || code < 0) {
		(void) sprintf(ebuf, "Error %d", code);
		return ebuf;
	} else
		return sys_errlist[code];
}

static int 
_add_job(entry,name,beginh)
long entry;
char *name;
struct nntp_job **beginh;
{
struct nntp_servers *swrk;
struct nntp_job *jwrk;

if(fseek(fb,entry,SEEK_SET)){
serr:
	bserrno=BS_SYSERR;
	return 1;
}
if(fread((char *)&gr_entry,sizeof(struct group_rec),1,fb)!=1)
	goto serr;		
if(!(gr_entry.flag&GF_NNTP))
	return 10;
if(gr_entry.begin==NULL && !(gr_entry.flag&GF_NNTPALWAYS))
	return 10;
for(swrk=servers;swrk;swrk=swrk->next){
	if(swrk->server!=gr_entry.nntp_host)
		continue;
	for(jwrk=swrk->begin;jwrk->next;jwrk=jwrk->next);
	if((jwrk->next=(struct nntp_job *)calloc(1,sizeof(struct nntp_job)))==NULL)
		goto serr;
	jwrk->next->next=NULL;
	jwrk->next->last_time=gr_entry.nntp_lastused;
	jwrk->next->offset=entry+offset(gr_entry,nntp_lastused);
	jwrk->next->hies=NULL;
	jwrk->next->counter=0;
	if((jwrk->next->name=calloc(strlen(name)+1,sizeof(char)))==NULL)
		goto serr;
	strcpy(jwrk->next->name,name);
	if(beginh)
		*beginh=jwrk->next;
	return 0;
}
if((swrk=(struct nntp_servers *)calloc(1,sizeof(struct nntp_servers)))==NULL)
	goto serr;
swrk->next=servers;
swrk->server=gr_entry.nntp_host;
if((swrk->begin=(struct nntp_job *)calloc(1,sizeof(struct nntp_job)))==NULL)
	goto serr;
swrk->begin->next=NULL;
swrk->begin->last_time=gr_entry.nntp_lastused;
swrk->begin->offset=entry+offset(gr_entry,nntp_lastused);
swrk->begin->hies=NULL;
swrk->begin->counter=0;
if((swrk->begin->name=calloc(strlen(name)+1,sizeof(char)))==NULL)
	goto serr;
strcpy(swrk->begin->name,name);
servers=swrk;
if(beginh)
	*beginh=swrk->begin;
return 0;
}

int
get_gntree(begin,level,name)
long begin;
int level;
char *name;
{
long curoffset;
struct string_chain lst_entry;
struct nntp_job *jwrk;
char gbuff[80];

if(++level>MAXLVL){
berr:
bserrno=BS_BASEERR;
err:
return 1;
}

levels[level].all_need=1;
levels[level].begin=NULL;
levels[level].counter=0;

for(curoffset=begin;curoffset;curoffset=lst_entry.next){
	if(fseek(fb,curoffset,SEEK_SET)){
serr:
		bserrno=BS_SYSERR;
		return 1;}
	if(fread((char *)&lst_entry,sizeof(struct string_chain),1,fb)!=1)
		goto serr;

	if(!lst_entry.entry && !lst_entry.down_ptr)
		goto berr;

	if(name){
		(void)strcpy(gbuff,name);
		(void)strcat(gbuff,".");
		(void)strcat(gbuff,lst_entry.name);
	}
	else
		(void)strcpy(gbuff,lst_entry.name);

	if(lst_entry.entry){
		switch(_add_job(lst_entry.entry,gbuff,
		  levels[level].begin==NULL?&levels[level].begin:
		  (struct nntp_job **)NULL)){
		case 0:  /* all OK */
			levels[level].counter++;
			break;
		case 1:  /* error */
			return 1;
		case 10: /* OK, but not needed */
			levels[level].all_need=0;
		}
	}
	if(lst_entry.down_ptr)
		if(get_gntree(lst_entry.down_ptr,level,gbuff))
			goto err;
		else
			if(levels[level+1].all_need){
				levels[level].counter+=levels[level+1].counter;
				if(levels[level].begin==NULL)
					levels[level].begin=levels[level+1].begin;
			}
			else
				levels[level].all_need=0;
}
if(levels[level].all_need && levels[level].counter>1){
	jwrk=levels[level].begin;
	{register i;
	for(i=0;i<levels[level].counter;i++,jwrk=jwrk->next){
		if(jwrk->hies){
			free(jwrk->hies);
			jwrk->hies=NULL;}
		if(jwrk->name){
			free(jwrk->name);
			jwrk->name=NULL;}
		if(jwrk->last_time<levels[level].begin->last_time)
			levels[level].begin->last_time=jwrk->last_time;
	}
	}
	(void)strcpy(gbuff,name);
	(void)strcat(gbuff,".*");
	levels[level].begin->counter=levels[level].counter;
	levels[level].begin->hies=calloc(strlen(gbuff)+1,sizeof(char));
	strcpy(levels[level].begin->hies,gbuff);
}
bserrno=BS_OK;
return 0;
}

int
main(argc, argv)
int argc;
char *argv[];
{
	char buf[BUFSIZ];
	int i,omitupdate;
	long clock;
	long lastdate, lasttime;
	FILE *inews,*batch;
	struct tm *now;
	struct nntp_servers *swrk;
	struct nntp_job *jwrk;

	strcpy(pname,argv[0]);
	Pname = ((Pname = rindex(argv[0], '/')) ? Pname + 1 : argv[0]);

	{register FILE *sfile;
	(void) sprintf(buf,"%s/nntpserv",LOCKDIR);
	if((sfile=fopen(buf,"r"))!=NULL){
		fscanf(sfile,"%d",&i);
		if(kill(i,0)==0)
			exit(0);
		fclose(sfile);
		goto wrlock;
	}
	else 
wrlock:
	if((sfile=fopen(buf,"w"))!=NULL){
		fprintf(sfile,"%d",getpid());
		fclose(sfile);
	}
	else
		exit(72);
	}	
servers=NULL;

if(open_base(OPEN_RDONLY)){
	pberr("nntpserv",stderr);
	goto err;}
if(get_gntree(header.group,0,(char *)NULL)){
	pberr("nntpserv",stderr);
	goto err;
}
close_base();
if(servers==NULL)
	exit(0);


	for(swrk=servers;swrk;swrk=swrk->next){
#ifdef DEBUG
		printf("system: [%s]\n",inet_ntoa(swrk->server));
#endif

	if ((server = get_nntp_conn(swrk->server)) < 0) 
		{
		perror("nntpserv: could not open socket");
		continue;
		}
	if ((rd_fp = fdopen(server,"r")) == (FILE *) 0){
		perror("nntpserv: could not fdopen socket");
		continue;
		}

#ifdef DEBUG
	printf("connected to nntp server at [%s]\n", inet_ntoa(swrk->server));
#endif

#ifdef LOGFILE
	    log("LOG: nntpserv connect to [%s]\n", buf,inet_ntoa(swrk->server));
#endif

	/*
	* ok, at this point we're connected to the nntp daemon 
	* at the distant host.
	*/
	/* get the greeting herald */
	(void) sockread(buf);
#ifdef DEBUG
	(void) printf("%s\n", buf);
#endif
	if (buf[0] != '2')	/* uh-oh, something's wrong! */
		{
		(void) printf("%s: protocol error: got '%s'\n", Pname,buf);
		(void) close(server);
		continue;
		}


	if(strstr(buf,"INN")==NULL)
		/* first, tell them we're a slave process to get priority */
		sockwrite("SLAVE");
	else
		sockwrite("MODE READER");

	(void) sockread(buf);
#ifdef DEBUG
	(void) printf("%s\n", buf);
#endif
	if (buf[0] != '2')	/* uh-oh, something's wrong! */
		{
		(void) printf("%s: protocol error: got '%s'\n", Pname,buf);
		(void) close(server);
		continue;
		}
	
	for(jwrk=swrk->begin;jwrk;jwrk=jwrk->next){
		clock = jwrk->last_time;
		now = gmtime(&clock);
		lastdate = (now->tm_year * 10000) +
			((now->tm_mon + 1) * 100) + now->tm_mday;
		lasttime = (now->tm_hour * 10000) +
			(now->tm_min * 100) + now->tm_sec;

		clock = time((long *)0);
#ifdef DEBUG
	printf("lastdate is %06d\n",lastdate);
	printf("lasttime is %06d\n",lasttime);
	printf("newsgroups is '%s'\n",jwrk->counter?jwrk->hies:jwrk->name);
#endif
	/* now, ask for a list of new articles */
	(void) sprintf(buf,"NEWNEWS %s %06d %06d GMT", 
		jwrk->counter?jwrk->hies:jwrk->name, (int)lastdate, (int)lasttime);
	sockwrite(buf);
	omitupdate=0;
	(void) sockread(buf);
#ifdef DEBUG
	(void) printf("%s\n", buf);
#endif
	if (buf[0] != '2')	/* uh-oh, something's wrong! */
		{
		(void) printf("%s: protocol error: got '%s'\n", Pname,buf);
		break;
		}
	/* and here comes the list, terminated with a "." */
#ifdef DEBUG
	(void) printf("data\n");
#endif
	misart=dupart = newart = 0;
	while (1)
		{
		(void) sockread(buf);
		if (!strcmp(buf,"."))
			break;
		if (wewant(buf))
			{
			if (newart >= MAX_ARTICLES)
				{
				omitupdate=1;
				continue;
				}
			artlist[newart] = malloc((unsigned)(strlen(buf)+1));
			(void) strcpy(artlist[newart], buf);
			newart++;
			}
		else
			dupart++;
		}
#ifdef DEBUG
	(void) printf(".\n%d new, %d dup articles\n", newart, dupart);
#endif

	/* now that we know which articles we want, retrieve them */
	for (i=0; i < newart; i++)
		(void) artfetch(artlist[i]);
#ifdef DEBUG
	(void) printf("%d missing articles\n", misart);
#endif

#ifdef LOGFILE
	    log("LOG: nntpserv get %s: %d new, %d dup, %d mising\n",
	     buf,jwrk->counter?jwrk->hies:jwrk->name, newart, dupart, misart);
#endif

	/* do we want to update the timestamp ? */
	if(!omitupdate){
		sprintf(buf,"%s/%s",BASEDIR,SUBSBASE);
		if((fb=fopen(buf,"r+"))==NULL)
			goto err2;
		if(lock(fb,F_WRLCK)<0)
			goto err2;
		if(fseek(fb,jwrk->offset,SEEK_SET))
			goto err2;
		if(fwrite((char *)&clock,sizeof(clock),1,fb)!=1)
			goto err2;		
		for(i=jwrk->counter-1;i>0;i--,jwrk=jwrk->next){
		if(fseek(fb,jwrk->next->offset,SEEK_SET))
			goto err2;
		if(fwrite((char *)&clock,sizeof(clock),1,fb)!=1)
			goto err2;		
		}
		fclose(fb);	
	}
	}/* jwrk */
	/* we're all done, so tell them goodbye */
	sockwrite("QUIT");
	(void) sockread(buf);
#ifdef DEBUG
	(void) printf("%s\n", buf);
#endif
	if (buf[0] != '2')	/* uh-oh, something's wrong! */
		{
		(void) printf("%s: error: got '%s'\n", Pname,buf);
		}
	(void) close(server);
}

#ifdef USE_DBZ
closehist();
#endif

	unlink("/tmp/.art");
	if ( (batch = fopen("/tmp/.batch", "r")) != (FILE *) 0){
#ifdef DEBUG
	(void) printf("command: %s...", RNEWS);
#endif
	if ( (inews = popen(RNEWS, "w")) == (FILE *) 0)
		{
		perror(RNEWS);
		exit(1);
		}

	while(fgets(buf,sizeof buf,batch)!=NULL)
		(void) fputs(buf,inews);

	(void) fflush(inews);
	(void) pclose(inews);
#ifdef DEBUG
	(void) printf("Done\n");
#endif
	(void) unlink("/tmp/.batch");
	(void) fclose(batch);
	}
#ifdef DEBUG
	(void) printf("All done\n");
#endif

(void) sprintf(buf,"%s/nntpserv",LOCKDIR);
unlink(buf);
exit(0);
err2:
exit(3);
err:
exit(1);
}

