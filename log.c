#include <stdarg.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "conf.h"

extern char pname[];

void
log(char *fmt, ...)
{
#ifdef LOGFILE
	va_list args;
	time_t now;
	FILE *logf;
	char tmpbuf[512];
	char *p;
	
	va_start(args, fmt);
        
	if ((logf = fopen(LOGFILE, "a")) != NULL) {
	    now = time((long *) NULL);
#ifdef ISC
	    cftime(tmpbuf, "%d %h %H:%M:%S", &now);
#else
#ifdef _POSIX_SOURCE
	    strftime(tmpbuf, sizeof tmpbuf, "%d %h %H:%M:%S", localtime(&now));
#else
	    sprintf(tmpbuf, "%s\n", asctime(localtime(&now)));
	    tmpbuf[19] = 0;
#endif
#endif
	    if ((p=rindex(pname, '/'))==NULL)
		p=pname;
	    else
		p++;
	    sprintf(tmpbuf+strlen(tmpbuf), " %s [%u] ", p, getpid());
	    vsnprintf(tmpbuf+strlen(tmpbuf), sizeof(tmpbuf)-strlen(tmpbuf)-1,
	              fmt, args);
	    if (tmpbuf[strlen(tmpbuf)-1]!='\n')
		strcat(tmpbuf, "\n");
	    fputs(tmpbuf, logf);
	    fclose(logf);
	}
	va_end(args);
#endif
}
