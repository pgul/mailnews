#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "conf.h"
#include "dbsubs.h"

#ifdef NNTP_ONLY
#include "nnrp.h"
extern int sofgets(char *, int, FILE *);

#ifndef lint
static char sccsid[] = "@(#)newsserv.c 4.0 31/01/94";
#endif

int sockfd=-1;
FILE *fsocket=NULL;
char snntp[4096];

int connect_nnrp(void)
{
    struct sockaddr_in sin;
    struct in_addr servaddr;
    char *servname=NNTP_SERVER;
    struct hostent *remote;
    int already=0;

    if (fsocket) return 0;
    if (sockfd==-1) {
	if (!isdigit(servname[0]) || \
	    (servaddr.s_addr=inet_addr(servname))==INADDR_NONE)
	{
	    if ((remote=gethostbyname(servname))==NULL)
	    {   log("Unknown or illegal hostname %s\n", servname);
		return(1);
	    }
	    if (remote->h_addr_list[0]==NULL)
	    {   log("Can't gethostbyname for %s\n", servname);
		return(1);
	    }
	    memcpy(&servaddr, remote->h_addr_list[0], sizeof(servaddr));
	}
	sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd==-1)
	{   log("can't create socket: %s\n", strerror(errno));
	    return(1);
	}
	sin.sin_family=AF_INET;
	sin.sin_port=htons(OPORT);
	memcpy(&sin.sin_addr, &servaddr, sizeof(sin.sin_addr));
	
	if (connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)))
	{   log("can't connect to server: %s\n", strerror(errno));
	    close(sockfd);
	    sockfd=-1;
	    return(1);
	}
    }
    else
	already=1;
    if (fsocket==NULL)
    {   fsocket=fdopen(sockfd, "r+");
	if (fsocket==NULL)
	{   log("can't fdopen socket: %s\n", strerror(errno));
	    close(sockfd);
	    sockfd=-1;
	    return(1);
	}
    }
    if (already)
	return 0; /* connected, fdopen only */
    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
    {
	log("can't read from socket: %s\n", strerror(errno));
	fclose(fsocket);
	sockfd=-1;
	fsocket=NULL;
	return(1);	/* connection closed */
    }
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='2')
    {
	log("server response: %s", snntp);
	fprintf(fsocket, "QUIT\r\n");
	sofgets(snntp, sizeof(snntp), fsocket);
	fclose(fsocket);
	fsocket=NULL;
	sockfd=-1;
	return(2);
    }
#ifdef SWITCH_TO_READER
    fputs("MODE READER\r\n", fsocket);
    fflush(fsocket);
#ifdef DEBUG
    printf(">> MODE READER\n");
#endif
    if (sofgets(snntp, sizeof(snntp), fsocket)==NULL)
    {
	log("can't read from socket: %s\n", strerror(errno));
	fclose(fsocket);
	fsocket=NULL;
	sockfd=-1;
	return(1);	/* connection closed */
    }
#ifdef DEBUG
    printf("<< %s", snntp);
#endif
    if (snntp[0]!='2')
    {
	log("server response: %s", snntp);
	fprintf(fsocket, "QUIT\r\n");
	fgets(snntp, sizeof(snntp), fsocket);
	fclose(fsocket);
	fsocket=NULL;
	sockfd=-1;
	return(2);
    }
#endif
    return 0;
}

#endif /* NNTP_ONLY */

