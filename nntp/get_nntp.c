/**
**	USG		- you're on System III/V (you have my sympathies)
**	NONETDB		- old hostname lookup with rhost()
**	OLDSOCKET	- different args for socket() and connect()
**
** Erik E. Fair <fair@ucbarpa.berkeley.edu>
**
*/

#include "conf.h"
#include <sys/types.h>
#ifdef LAI_TCP
#include <sys/bsdtypes.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stdio.h>
#ifndef	NONETDB
#include <netdb.h>
#endif	NONETDB

extern	int	errno;
extern	char	*Pname;
extern	char	*errmsg();
extern int socket(),connect(),close();

#ifndef	NONETDB
extern	char	*inet_ntoa();
extern	u_long	inet_addr();
#else

char *
inet_ntoa(in)
struct in_addr in;
{
	static char address[20];

	sprintf(address, "%u.%u.%u.%u",
			 (in.s_addr>>24)&0xff,
			 (in.s_addr>>16)&0xff,
			 (in.s_addr>>8 )&0xff,
			 (in.s_addr    )&0xff);
	return address;
}
#endif	NONETDB

u_short
gservice()
{
#ifdef	NONETDB
		return (u_short)IPPORT_NNTP;
#else
		struct servent	*srvp = getservbyname("nntp", "tcp");

		if (srvp == (struct servent *)NULL)
			return((u_short)0);
		return (u_short)srvp->s_port;
#endif	NONETDB
}

/*
** given a host name (either name or internet address) and service name
** (or port number) (both in ASCII), give us a TCP connection to the
** requested service at the requested host (or give us FAIL).
*/
int
get_nntp_conn(host)
u_long	host;
{
	register int	sock;
	struct sockaddr_in	sadr;
#ifdef	OLDSOCKET
	struct sockproto	sp;

	sp.sp_family	= (u_short)AF_INET;
	sp.sp_protocol	= (u_short)IPPROTO_TCP;
#endif	OLDSOCKET

	sadr.sin_family = (u_short)AF_INET;	/* Only internet for now */
	if ((sadr.sin_port = gservice()) == 0)
		return -1;

	bcopy((caddr_t)&host, (caddr_t)&sadr.sin_addr,
			sizeof(sadr.sin_addr));

#ifdef	OLDSOCKET
	if ((sock = socket(SOCK_STREAM, &sp, (struct sockaddr *)NULL, 0)) < 0)
		return -1;

	if (connect(sock, (struct sockaddr *)&sadr) < 0) {
#else
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return -1;

	if (connect(sock, (struct sockaddr *)&sadr, sizeof(sadr)) < 0) {
#endif	OLDSOCKET
		int	e_save = errno;

		fprintf(stderr, "%s: [%s]: %s\n", Pname,
			inet_ntoa(sadr.sin_addr), errmsg(errno));
		(void) close(sock);	/* dump descriptor */
		errno = e_save;
	} else{
#ifdef USE_KEEPALIVES
	      int on = 1;
	      if (setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,(char *)&on,sizeof(on)) < 0)
	       { int e_save;
		 e_save = errno;
		 fprintf(stderr,"%s: [%s]: setsockopt KEEPALIVE: %s\n",Pname,inet_ntoa(sadr.sin_addr),errmsg(errno));
		 /* don't bother erroring out, just note it and ignore it */
	       }
#endif
      return sock;
	}
return -1;
}
