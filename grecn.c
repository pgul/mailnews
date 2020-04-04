/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include "conf.h"

#ifndef lint
static char sccsid[] = "@(#)grecn.c 2.0";
#endif

extern int belongs();
extern char *malloc();

#define ISspace(x) ( x==' ' || x=='\t' )

struct reclist {
    struct reclist *next;
    char *receiver;
    char group[1];
};

static struct reclist *reclist = NULL, *rlcur, *rlprev;

/*
 * Read the list of receiver' names
 */
void readrecl()
{
    FILE *snf = fopen(RLISTFILE, "r");
    register c;
    register char *p;
    char buf[100], *snp;
    struct reclist *x;

    if (snf == NULL)
	return;
    while ((c = getc(snf)) != EOF) {
	/*
		 * Skip comments
		 */
	if (c == '#' || c == ' ' || c == '\t') {
	    while ((c = getc(snf)) != '\n' && c != EOF);
	    continue;
	}
	if (c == '\n')
	    continue;
	if (c == EOF)
	    break;

	/* Collect the receiver's name */
	p = buf;
	*p++ = c;
	while ((c = getc(snf)) != ' ' && c != '\t' &&
	       c != '\n' && c != EOF && c != '#')
	    *p++ = c;
	*p = 0;
	if (c == '#')
	    while ((c = getc(snf)) != '\n' && c != EOF);
	if (c == EOF)
	    break;
	if ((p = (char *) malloc(strlen(buf) + 1)) == NULL) {
	    reclist = NULL;
	    return;
	}
	strcpy(p, buf);
	snp = p;

	/*
		 * Collect the groups names
		 */
	for (;;) {
	    /* Skip spaces */
	    while (ISspace(c))
		c = getc(snf);
	    if (c == '#')
		while ((c = getc(snf)) != '\n' && c != EOF);
	    if (c == '\n') {
		c = getc(snf);
		if (c == ' ' || c == '\t')
		    continue;
		if (c == '#') {
		    while ((c = getc(snf)) != '\n' && c != EOF);
		    continue;
		}
		if (c == EOF)
		    return;
		ungetc(c, snf);
		break;		/* End of group's list */
	    }
	    if (c == EOF)
		return;

	    /* Collect the name of hierarchy */
	    p = buf;
	    *p++ = c;
	    while ((c = getc(snf)) != ' ' && c != '\t' &&
		   c != '\n' && c != EOF && c != '#')
		*p++ = c;
	    *p = 0;
	    if ((x = (struct reclist *) malloc(strlen(buf) + sizeof(struct reclist))) == NULL) {
		reclist = NULL;
		return;
	    }
	    x->receiver = snp;
	    strcpy(x->group, buf);
	    x->next = NULL;
	    if (reclist == NULL)
		reclist = x;
	    else {
		for (rlprev = NULL, rlcur = reclist; rlcur; rlprev = rlcur, rlcur = rlcur->next) {
		    if (strlen(x->group) > strlen(rlcur->group))
			if (rlprev) {
			    x->next = rlprev->next;
			    rlprev->next = x;
			    x = NULL;
			    break;
			} else {
			    x->next = reclist;
			    reclist = x;
			    x = NULL;
			    break;
			}
		}
		if (x)
		    rlprev->next = x;
	    }
	}
    }
}

/*
 * Get receiver name for a group
 */
char *
 grecname(grp)
char *grp;
{
    register struct reclist *sp;

    for (sp = reclist; sp != NULL; sp = sp->next) {
	if (belongs(grp, sp->group))
	    return sp->receiver;
    }
    return RECEIVER;
}
