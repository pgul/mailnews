/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * This code is public-domain thus you can use it, modify it
 * or redistribute it as long as you keep this copyright note unchanged.
 * You aren't allowed to sell it. The author is not responsible for
 * the consequences of use of this software. Modifyed versions
 * should be explicitly marked as such. This code is not a subject to
 * any license of AT&T or of the Regents of UC, Berkeley or of DEMOS, Moscow.
 */

#ifndef lint
static char sccsid[] = "@(#)notfunc.c  1.0  11/05/92";
#endif

#include <sys/types.h>
#include <string.h>
#include "conf.h"

#ifndef NULL
#define NULL 0
#endif

/*
 * Cut an address at 25 chars
 */
char *cut25(a)
char *a;
{
    register char *p;
    static char buf[100];
    char buf2[100];

    strncpy(buf, a, sizeof(buf)-1);
    while (strlen(buf) > 25) {
	if (index(buf, '%') != NULL) {
	    if ((p = index(buf, '@')) == NULL)
		p = rindex(buf, '%');
	    *p = '\0';
	    if ((p = rindex(buf, '%')) != NULL)
		*p = '@';
	    continue;
	}
	if ((p = rindex(buf, '@')) != NULL) {
	    *p++ = '\0';
	    strcpy(buf2, buf);
	    if ((p = index(p, '.')) != NULL) {
		strcat(buf2, "@");
		strcat(buf2, &p[1]);
	    }
	    strcpy(buf, buf2);
	    continue;
	}
	if ((p = index(buf, '!')) != NULL) {
	    strcpy(buf2, &p[1]);
	    strcpy(buf, buf2);
	    continue;
	}
	buf[25] = '\0';
    }
    return buf;
}

/*
 * Split the subject line into pieces n chars long
 */
char *
 subsplit(pp, n)
char **pp;
int n;
{
    register char *p, *q;
    register i;
    static char psub[80];	/* assume n < 80 */
    int n2;

    p = *pp;
    while (*p == ' ' || *p == '\t')
	p++;
    q = psub;
    for (i = n; i && *p; i--)
	*q++ = *p++;
    *q = 0;
    if (*p == 0) {
	*pp = NULL;
	return psub;
    }
    if (*p == ' ' || *p == '\t') {
	while (*p == ' ' || *p == '\t')
	    p++;
	if (*p == 0)
	    *pp = NULL;
	else
	    *pp = p;
	/* eliminate trailing spaces */
	while (*--q == ' ' || *q == '\t')
	    *q = 0;
	return psub;
    }
    i = 0;
    n2 = n / 2;
    while ((*p != ' ' && *p != '\t') && i < n2)
	p--, i++;
    if (i == n2) {		/* No word separators found */
	*pp += n;
	return psub;
    }
    *pp = &p[1];
    q -= i - 1;
    while (*--q == ' ' || *q == '\t')
	*q = 0;
    return psub;
}
