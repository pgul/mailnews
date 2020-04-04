#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#ifndef OS2
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include "conf.h"
#include "dbsubs.h"

#ifdef NNTP_ONLY
int sofgetc(FILE *f)
{
  static unsigned char buf[512];
  static int ibuf=0, nbuf=0;
  fd_set fd_in;
  struct timeval tm;
  int fd=fileno(f);

  if (ibuf<nbuf)
	return buf[ibuf++];
  ibuf=0;
  FD_ZERO(&fd_in);
  FD_SET(fd,&fd_in);
again:
  tm.tv_sec=NNTP_TIMEOUT;
  tm.tv_usec=0;
  switch(select(FD_SETSIZE,&fd_in,NULL,NULL,&tm)) { 
	/* Ошибка. */
	case -1:
		if (errno==EINTR)
			goto again;
#ifdef DEBUG
		printf("Can't select(), fd=%d: %s\n", fd, strerror(errno));
#endif
		log("Can't select(): %s\n", strerror(errno));
		return -1;
	/* Таймаут */
	case 0:
#ifdef DEBUG
		printf("Can't select(), fd=%d: timeout\n", fd);
#endif
		log("Can't select(): timeout\n");
		return -1;
	default: { 
		if(FD_ISSET(fd,&fd_in)) { 
			nbuf=read(fd,buf,sizeof(buf));
			switch(nbuf) { 
				/* Ошибка */
				case -1:
#ifdef DEBUG
					printf("Can't read, fd=%d: %s\n",fd,strerror(errno));
#endif
					log("Can't read: %s\n", strerror(errno));
					return -1;
				/* EOF. На самом деле тоже ошибка */	
				case 0 :
#ifdef DEBUG
					printf("Can't read, fd=%d: EOF\n",fd);
#endif
					log("Can't read: EOF\n");
					return -1;
				default: return buf[ibuf++];
			}
		} /* другого способа на самом то деле быть не может,
		потому что мы ждем подьема одного из одного сокетов :) */
	}
   }
   /* строго говоря unreachible, но пусть return.. */
#ifdef DEBUG
   printf("Can't sofgetc(), fd=%d: unknown (%s)\n", fd, strerror(errno));
#endif
   log("Can't sofgetc(): unknown (%s)\n", strerror(errno));
   return -1;
}

int sofgets(char *str, int size, FILE *f)
{
  int i, c;

  for (i=0; i<size-1;) {
	c=sofgetc(f);
	if (c==-1)
		break;
	str[i++]=(char)c;
	if (c=='\n')
		break;
  }
  str[i]='\0';
  if (c==-1) return 0;
  return i;
}
#endif

