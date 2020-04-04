/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software. 
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[]="@(#)mailerr.c 1.0 12/13/94";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "dbsubs.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "conf.h"

void
mail_err(message)
char *message;
{
FILE *sm;
char cmd[256];
char host[80];

gethost(host,sizeof(host));

snprintf(cmd, sizeof(cmd), "%s %s %s@%s %s %s",MAILER,SENDER_FLAG,NEWSSENDER,host,MFLAG_QUEUE,ADMIN);
sm=popen(cmd,"w");
fprintf(sm,"From: %s@%s\n",NEWSSENDER,host);
fprintf(sm,"To: %s\n",ADMIN);
fprintf(sm,"Subject: !!! Error in newsserver !!!\n");
#ifdef USE_XCLASS
fprintf(sm,"X-Class: fast\n");
#endif
#ifdef USE_PRECEDENCE
fprintf(sm,"Precedence: air-mail\n\n");
#endif
fprintf(sm,"%s\n",message);
pclose(sm);
return;
}
