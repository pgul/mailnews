/*
 * function get_host_addr() & get_host_name() for sdba
 */

#ifndef lint
static char sccsid[]="@(#)sdbacnv.c 2.0 03/07/94";
#endif

#include "conf.h"
#include <sys/types.h>
#if defined(ISC) || defined(LAI_TCP)
#include <sys/bsdtypes.h>
#endif
#ifndef BSD
#include <netinet/in.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef NONETDB
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef USG
#define bcopy(a,b,c)   memcpy(b,a,c)
#endif 

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
extern	int	errno;
extern	char	*Pname;
extern	char	*errmsg();
#if !defined(htons) && !defined(BSD)
extern	u_short	htons();
#endif	htons

#ifndef	NONETDB
extern	char	*inet_ntoa();
extern	u_long	inet_addr();
#endif

#ifdef __SDTC__
static u_long **name_to_address(char *);
#else
static u_long **name_to_address();
#endif

u_long
get_host_addr(host_name)
char *host_name;
{
u_long **addresses;

addresses=name_to_address(host_name);
return (*addresses[0]);
}

char *
get_host_name(addr)
u_long addr;
{
static char host_name[MAXHOSTNAMELEN];
struct hostent *shost;
static u_long saddr;

#ifndef OS2
if(saddr!=addr){
if((shost=gethostbyaddr((char *)&addr,sizeof(struct in_addr),AF_INET))){
strcpy(host_name,shost->h_name);
saddr=addr;
}else{
#endif
strcpy(host_name,"Unknown host name: ");
strcat(host_name,inet_ntoa(addr));
#ifndef OS2
}
}
#endif
return host_name;
}

#ifdef NONETDB
u_long
inet_addr(cp)
register char	*cp;
{
	u_long val, base, n;
	register char c;
 	u_long octet[4], *octetptr = octet;
#ifndef	htonl
#ifdef OS2
#define htonl(u) (u)
#else
	extern	u_long	htonl();
#endif
#endif	htonl
again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (octetptr >= octet + 4)
			return (-1);
		*octetptr++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp))
		return (-1);
	*octetptr++ = val;
	/*
	 * Concoct the address according to
	 * the number of octet specified.
	 */
	n = octetptr - octet;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = octet[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (octet[0] << 24) | (octet[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (octet[0] << 24) | ((octet[1] & 0xff) << 16) |
			(octet[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (octet[0] << 24) | ((octet[1] & 0xff) << 16) |
		      ((octet[2] & 0xff) << 8) | (octet[3] & 0xff);
		break;

	default:
		return (-1);
	}
	val = htonl(val);
	return (val);
}

char *
inet_ntoa(in)
u_long in;
{
	static char address[20];

	sprintf(address, "%u.%u.%u.%u",
			 (in>>24)&0xff,
			 (in>>16)&0xff,
			 (in>>8 )&0xff,
			 (in    )&0xff);
	return(address);
}
#endif

u_long **
name_to_address(host)
char	*host;
{
	static	u_long	*host_addresses[2];
	static	u_long	haddr;

	if (host == (char *)NULL) {
		return((u_long **)NULL);
	}

	host_addresses[0] = &haddr;
	host_addresses[1] = (u_long *)NULL;

	/*
	** Is this an ASCII internet address? (either of [10.0.0.78] or
	** 10.0.0.78). We get away with the second test because hostnames
	** and domain labels are not allowed to begin in numbers.
	** (cf. RFC952, RFC882).
	*/
	if (*host == '[' || isdigit(*host)) {
		char	namebuf[128];
		register char	*cp = namebuf;

		/*
		** strip brackets [] or anything else we don't want.
		*/
		while(*host != '\0' && cp < &namebuf[sizeof(namebuf)]) {
			if (isdigit(*host) || *host == '.')
				*cp++ = *host++;	/* copy */
			else
				host++;			/* skip */
		}
		*cp = '\0';
		haddr = inet_addr(namebuf);
		return(&host_addresses[0]);
	} else {
#ifdef	NONETDB
#ifdef OS2
		return ((u_long**)NULL);
#else
		extern	u_long	rhost();

		/* lint is gonna bitch about this (comparing an unsigned?!) */
		if ((haddr = rhost(&host)) == FAIL)
			return((u_long **)NULL);	/* no such host */
		return(&host_addresses[0]);
#endif
#else
		struct hostent	*hstp = gethostbyname(host);

		if (hstp == NULL) {
			return((u_long **)NULL);	/* no such host */
		}

		if (hstp->h_length != sizeof(u_long))
			abort();	/* this is fundamental */
#ifndef	h_addr
		/* alignment problems (isn't dbm wonderful?) */
		bcopy((caddr_t)hstp->h_addr, (caddr_t)&haddr, sizeof(haddr));
		return(&host_addresses[0]);
#else
		return((u_long **)hstp->h_addr_list);
#endif
#endif
	}
}

