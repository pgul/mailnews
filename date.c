/*
 * Usenet to E-mail Gateway
 *
 * (C) Copyright 1991 by Vadim Antonov, Moscow, USSR
 * All rights reserved.
 *
 * (C) Copyright 1992 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

/*
 * THIS PROCEDURE IS SYSTEM-DEPENDENT
 */
#include <time.h>
#ifdef BSD
#include <sys/types.h>
#include <sys/timeb.h>
#ifdef BSD
#include <sys/time.h>
#endif
#endif

extern int sprintf();

static char *wdays[] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

#ifdef BSD
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

ftime(tbp)
struct timeb *tbp;
{
    struct timezone tz;
    struct timeval t;

    if (gettimeofday(&t, &tz) < 0)
	return (-1);
    tbp->millitm = t.tv_usec / 1000;
    tbp->time = t.tv_sec;
    tbp->timezone = tz.tz_minuteswest;
    tbp->dstflag = tz.tz_dsttime;

    return (0);
}

#endif

/*
 * ARPA-styled date
 * RFC822 format  'Sun, 20 Nov 91 17:35:54 PST\0' or
 *                'Fri, 26 May 85 02:13:10 -0700 (PST)\0'
 */
char *adate()
{
    static char out[50];
    long now;
    struct tm *t;
#ifdef _POSIX_SOURCE
    long timezone;
#endif
    int tzshift;
    char tzsign;
#if  defined(BSD) || defined(demos)
    struct timeb tb;

    ftime(&tb);
    t = localtime(&tb.time);
    tzshift = tb.timezone;
#else				/* not BSD */
    tzset();
    time(&now);
    t = localtime(&now);
#ifdef _POSIX_SOURCE
    timezone = mktime(gmtime(&now)) - now;
#endif
    t = localtime(&now);
    tzshift = timezone / 60;
#endif				/* not BSD */
    tzsign = tzshift <= 0 ? '+' : '-';
    if (tzshift < 0)
	tzshift = -tzshift;
    tzshift = 100 * (tzshift / 60) + (tzshift % 60);
    sprintf(out, "%3.3s, %2d %3.3s %02d %02d:%02d:%02d %c%04d (%s)",
	    wdays[t->tm_wday],
	    t->tm_mday,
	    months[t->tm_mon],
	    t->tm_year % 100,
	    t->tm_hour, t->tm_min, t->tm_sec,
	    tzsign,
#ifdef BSD
	    tzshift + (tb.dstflag != 0) * 100,
	    timezone(tb.timezone, tb.dstflag)
#else
	    tzshift,
	    tzname[t->tm_isdst]	/* Name of local timezone */
# endif
	);
    return out;
}

#ifdef TEST
main()
{
    printf("%s\n", adate());
}

#endif
