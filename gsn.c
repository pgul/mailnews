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
static char sccsid[] = "@(#)gsn.c 2.0";
#endif

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *malloc();

struct snlist {
    struct snlist *next;
    char *sender;
    char group[1];
};

static struct snlist *snlist = NULL, *sncur, *snprev;

/*
 * Read the list of sender' names
 */
void readsnl()
{
    FILE *snf = fopen(SLISTFILE, "r");
    register c;
    register char *p;
    char buf[100], *snp;
    struct snlist *x;

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

	/* Collect the sender's name */
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
	    snlist = NULL;
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
	    if ((x = (struct snlist *) malloc(strlen(buf) + sizeof(struct snlist))) == NULL) {
		snlist = NULL;
		return;
	    }
	    x->sender = snp;
	    strcpy(x->group, buf);
	    x->next = NULL;
	    if (snlist == NULL)
		snlist = x;
	    else {
		for (snprev = NULL, sncur = snlist; sncur; snprev = sncur, sncur = sncur->next) {
		    if (strlen(x->group) > strlen(sncur->group))
			if (snprev) {
			    x->next = snprev->next;
			    snprev->next = x;
			    x = NULL;
			    break;
			} else {
			    x->next = snlist;
			    snlist = x;
			    x = NULL;
			    break;
			}
		}
		if (x)
		    snprev->next = x;
	    }
	}
    }
}

/*
 * Is the group belongs to a hierarchy?
 */
int belongs(g, h)
char *g;
char *h;
{
    char *gg, *hh;

    while (g != NULL && h != NULL) {
	gg = g;
	hh = h;
	g = index(g, '.');
	h = index(h, '.');
	if (g != NULL)
	    *g = '\0';
	if (h != NULL)
	    *h = '\0';

	if (!strcmp(hh, "all") || !strcmp(gg, hh)) {
	    if (g != NULL)
		*g++ = '.';
	    if (h != NULL)
		*h++ = '.';
	    continue;
	}
	if (g != NULL)
	    *g++ = '.';
	if (h != NULL)
	    *h++ = '.';
	return 0;
    }
    if (g == NULL && h != NULL && strcmp(h, "all"))
	return 0;
    return 1;
}

/*
 * Get sender name for a group
 */
char *
 gsname(grp)
char *grp;
{
    register struct snlist *sp;

    for (sp = snlist; sp != NULL; sp = sp->next) {
	if (belongs(grp, sp->group))
	    return sp->sender;
    }
    return NEWSSENDER;
}
