#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#ifndef OS2
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "mnewssmtp.h"
void log(const char *, ...);

int smtpputs(int fd,char* fmt,...)
{ 
  	va_list ap;
  	char buf[512];
  	int num;

  	va_start(ap,fmt);
  	vsnprintf(buf,sizeof(buf),fmt,ap);
  	va_end(ap);
	num=strlen(buf);
  	buf[num]=13;
	buf[num+1]=10;
	buf[num+2]=0;
        /* Умолчание, что размер TCP-окна заведомо больше чем 512 байт.
           На самом деле во всех известных мне unix'ах - 4096 */
	write(fd,buf,num+2);
	return 0;
}

int smtpsend(int fd,unsigned reply,char* fmt,...)
{
  char buf[512];
  va_list ap;
  fd_set fd_in;
  struct timeval tm;
  int num;
  char resp[10], *p;
   
  if(fmt) { 
  	va_start(ap,fmt);
  	vsnprintf(buf,sizeof(buf),fmt,ap);
  	va_end(ap);
	num=strlen(buf);
  	buf[num]=13;
	buf[num+1]=10;
	buf[num+2]=0;
        /* Умолчание, что размер TCP-окна заведомо больше чем 512 байт.
           На самом деле во всех известных мне unix'ах - 4096 */
	write(fd,buf,num+2);
  }
  snprintf(resp,sizeof(resp),"%d",reply);
nextline:
  FD_ZERO(&fd_in);
  FD_SET(fd,&fd_in);
  tm.tv_sec=SMTP_TIMEOUT;
  tm.tv_usec=0;
  switch(select(FD_SETSIZE,&fd_in,NULL,NULL,&tm)) { 
	/* Ошибка. */
	case -1: 
		if (errno==EINTR) goto nextline;
		log ("smtpsend can't select: %s\n", strerror(errno));
		return -1;
	/* Таймаут */
	case 0:
		log ("smtpsend timeout\n");
		return -1;
	default: { 
		if(FD_ISSET(fd,&fd_in)) { 
			num=read(fd,buf,sizeof(buf));
			switch(num) { 
				/* Ошибка */
				case -1: log("smtpsend can't read: %s\n", strerror(errno));
					 return -1;
				/* EOF. На самом деле тоже ошибка */	
				case 0 : log("smtpsend can't read: timeout\n");
					 return -1;
				default: {
					if (!isdigit(buf[0]) ||
					    !isdigit(buf[1]) ||
					    !isdigit(buf[2]) ||
					    buf[3]!=' ')
						goto nextline;
					buf[sizeof(buf)-1] = '\0';
					if ((p=strpbrk(buf, "\r\n")) != NULL)
						*p = '\0';
					if (strncmp(buf, resp, 3)==0)
						return 0;
					log("smtp response %s while waiting for %s\n", buf, resp);
					buf[3]='\0';
					return atoi(buf);
				}
			}
		} /* другого способа на самом то деле быть не может,
		потому что мы ждем подьема одного из одного сокетов :) */
	}
   }
   /* строго говоря unreachible, но пусть return.. */
   log("smtpsend: internal error\n");
   return -1;
}

static int smtpconnect(char * mailhub, unsigned short port)
{
  	struct hostent* remote;
  	struct sockaddr_in sin;
	char myhostname[80];
	int  new;

	if ((remote=gethostbyname(mailhub))==NULL)
	{	log("smtpconnect: can't gethostbyname(\"%s\")\n", mailhub);
		return -1;
	}
	bcopy(remote->h_addr_list[0],&sin.sin_addr.s_addr,sizeof(sin.sin_addr.s_addr));
	sin.sin_port=htons(port);
#ifndef __linux__
  	sin.sin_len=sizeof(sin);
#endif
	sin.sin_family=AF_INET;
	if((new=socket(AF_INET,SOCK_STREAM,0))==-1) {
		log("smtpconnect can't create socket: %s\n", strerror(errno));
		return -1;
	}
	if(connect(new,(struct sockaddr*)&sin,sizeof(sin))==-1) { 
		log("smtpconnect can't connect to %s: %s\n", mailhub, strerror(errno));
		return -1;
	}
	if(smtpsend(new,220,NULL)) {
		return -1;
	}
	gethost(myhostname,sizeof(myhostname));
	if(smtpsend(new,250,"helo %s",myhostname)) {
		return -1;
	}
	return new;
}

int 
opensmtpsession(struct mailnewsreq* req, int new)
{ 
	int i, r, goodusers, badusers, reconnects;
	/* Проверка корректности переданных аргументов */
	if((!(req->mailhub))||(!(req->Iam))||(!(*req->users))) return -1;
	if(new>0) { 
		if(smtpsend(new,250,"rset"))
		{	/* timeout? */
			close(new);
			goto newconn;
		}
		reconnects=0;
	} else {
newconn:
		for (reconnects=1; reconnects<TRIES*2; reconnects++)
		{
			if ((new=smtpconnect(req->mailhub,req->port))>0)
				break;
secondhub:
			reconnects++;
			if (req->mailhub2 && (new=smtpconnect(req->mailhub2,req->port2))>0)
			{	log("Connected to secondary smtp server %s\n", req->mailhub2);
				break;
			}
			sleep(SLEEPTIME);
		}
		if (reconnects>=TRIES*2)
		{	log("Can't connect to smtp server\n");
			return -1;
		}
	}
	if(smtpsend(new,250,"mail from: %s",req->Iam)) { 
		return -1;
	}
	for (i=goodusers=badusers=0; req->users[i]; i++) { 
		if((r=smtpsend(new,250,"rcpt to: %s",req->users[i]))!=0) { 
			if (reconnects<2 && req->mailhub2 &&
			    (r/100==4 || r<0)) {
				closesmtpsession(new);
				if (!reconnects) goto newconn;
				goto secondhub;
			}
			log("Can't send mail to %s: SMTP response %d\n",
			    req->users[i], r);
			if (r/100==5)
				badusers++;
			/* return -1 */;
		}
		else
			goodusers++;
	}
	if (!goodusers) {
		closesmtpsession(new);
		return badusers ? -2 : -1;
	}
	if(smtpsend(new,354,"data")) { 
		return -1;
	}
   	return new;
}
 
int 
closesmtpdata(int fd) 
{ 
	int i;
	if((i=smtpsend(fd,250,"\r\n."))==0)
          return 0;
        if (i/100 == 5)
        { log("closesmtpdata: message rejected(%u)", i);
          return 0;
        }
	return 1;
}
int 
abortsmtpsession(int fd) 
{ 
	shutdown(fd,2);
	close(fd);
	return 0;
}


int 
closesmtpsession(int fd) 
{ 
	if(smtpsend(fd,221,"quit")) return 1;
	shutdown(fd,2);
	close(fd);
	return 0;
}
