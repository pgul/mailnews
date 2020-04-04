/*
 * (C) Copyright 1993 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)message.c 1.0 06/29/93";
#endif

#include <stdio.h>
#include "conf.h"
char extn[4][4] =
{"eng", "rus", "ukr", "bel"};

char *
 mesg(msg, lang)
int msg, lang;
{
    static char msgbuf[80];
    char filename[200];
    FILE *mf;

    sprintf(filename, MSGFILE, extn[lang]);
    if ((mf = fopen(filename, "r")) == NULL)
	goto err;
    {
	register i;
	for (i = 0; i < msg; i++) {
	    if (fgets(msgbuf, sizeof msgbuf, mf) == NULL)
		goto err;
	}
    }
    fclose(mf);
    return msgbuf;
  err:
    fclose(mf);
    msgbuf[0] = 0;
    return msgbuf;
}
