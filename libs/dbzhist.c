#include <stdio.h>
#include <string.h>		/* for memcpy */
#include <errno.h>
#include <sys/types.h>
#include "dbz.h"
#include "conf.h"

#ifdef HISTORY

/* private data */
static FILE *fp = NULL;

/* libdbm imports */
extern int dbminit(), store();
extern datum fetch();

/* other imports */
extern void prefuse(),free();
extern int okrefusal;	/* flag from command line */
extern char *malloc();

char *
strsave(s)
register char *s;
{
	register char *news = malloc((unsigned)strlen(s) + 1);

	(void) strcpy(news, s);
	return news;
}
/*
 * open the history files: ascii first, then dbm.
 * Try a+ mode first, then r mode, as dbm(3) does nowadays,
 * so that this routine can be used by any user to read history files.
 */
static int
openhist()
{
	if (fp == NULL) {
		fp = fopen(HISTORY, "r");

		errno = 0;
		if (fp != NULL && dbminit(HISTORY) < 0) {
			/*
			 * no luck.  dbm's dbminit will have just honked (on
			 * stdout, alas) but dbz's won't have, so bitch.
			 */
			fprintf(stderr,
		"database files for `%s' incomprehensible or unavailable",
				HISTORY);
			(void) fclose(fp);	/* close ascii file */
			fp = NULL;		/* and mark it closed */
		}
	}
	return fp != NULL;
}

static void
sanitise(s)
register char *s;
{
	for (; *s != '\0'; ++s)
		if (*s == '\t' || *s == '\n')
			*s = ' ';
}

static datum
getposhist(msgid)		/* return seek offset of history entry */
char *msgid;
{
	register char *clnmsgid;
	datum msgidkey, keypos;

	msgidkey.dptr = NULL;
	msgidkey.dsize = 0;
	if (!openhist())
		return msgidkey;
	clnmsgid = strsave(msgid);
	sanitise(clnmsgid);
	msgidkey.dptr = clnmsgid;
	msgidkey.dsize = strlen(clnmsgid) + 1;
	keypos = dbzfetch(msgidkey);		/* offset into ascii file */
	free(clnmsgid);
	return keypos;
}

int
alreadyseen(msgid)		/* return true if found in the data base */
char *msgid;
{
	datum posdatum;

	posdatum = getposhist(msgid);
	return posdatum.dptr == NULL;
}

long
get_offset(msgid)		/* return true if found in the data base */
char *msgid;
{
	long pos=0;
	datum posdatum;

	posdatum = getposhist(msgid);
	if (posdatum.dptr != NULL && posdatum.dsize == sizeof pos) {
		(void) memcpy((char *)&pos, posdatum.dptr, sizeof pos); /* align */
		return pos;
	}
	else
		return (long)NULL;
}

void
closehist()
{
	if (fp != NULL) {
		/* dbmclose is only needed by dbz, to flush statistics to disk */
		if (dbmclose() < 0) {
			fprintf(stderr,"error closing dbm history file");
		}
		if (fclose(fp) == EOF) {
			fprintf(stderr,"error closing history file");
		}
		fp = NULL;		/* mark file closed */
	}
}

#endif
