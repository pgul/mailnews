/*
 * (C) Copyright 1992-1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

/*
 * NETNEWS <--> E-MAIL GATEWAY CONFIGURATION
 */

#define VERSION		"MailNews 4.1.8b"

#undef NNTP
#define MNEWSDIR	"/usr/mnews/"
#define LBINDIR		MNEWSDIR "bin/"
#define LIBDIR		MNEWSDIR "lib/"
#undef UNKNOWN_DENIED	/* Unknown users can't use server */
#undef SLOW		/* gul: check and save base */
#define NOTALL		/* gul: disable {find|index|digest} by '*' */
#define SEND_BY_SMTP	/* gul: send messages by SMTP, don't exec sendmail */
#define NNTP_ONLY	/* gul: no newsspool, use NNTP only */

#undef HAVE_WILDMAT	/* gul: use internal wildmat() if not defined */

/* User names */
#define NEWSUSER        "news"		/* The user name of mailserver */
#define NEWSUSER1       NEWSUSER "@"	/* NEWSUSER + @ - to avoid loops */
#define NEWSSENDER      "news-server"	/* Name of sender (to collect bounced messages) */
#define FINDSENDER	"find-news"
#define RECEIVER	"news-server"
#define ADMIN		"newsmaster" /* Users for mailing error */

#define FACTOR          32	/* Max. number of addressees per mail */
#define MAILER         "/usr/bin/sendmail"
#undef MFLAG           "-odb"          /* additional option for MTA */
#define MFLAG_QUEUE	"-odq"
#define SENDER_FLAG "-f"	/* may be -r for uumail or -f for sendmail/smail */
#define USE_XCLASS
#define USE_PRECEDENCE

#ifdef USE_PRECEDENCE
#define PRECEDENCE "junk"	/* May be news or other */
#endif

/* inews interface */
#define SHELL           "/bin/sh"
#define INEWS           "/usr/local/news/inews -h"
#define STATFILE	"/var/log/mailnews/sterver.log"
#define LOGFILE		"/var/log/mailnews/serv.log"	/* You may comment this line if you need not logging */
#define PERFOMANCE_CNEWS
#define M_LIST		"/usr/local/news/etc/moderators"	/* Comment this line if there aren't moderators on your machine */
#define CXPOST	/* allow crossposting for commercial groups only to diff. hierar */
#undef EXT_CMD		LIBDIR "extcmds"	/* Comment it if you need not extern commands */
#undef EXT_SERV_PATH	"/usr/lib/relcom"	/* Comment it if you need not EXEC & CALL commands */


/* list of senders (per group basis) */
#define SLISTFILE       LIBDIR "news.senders"
#define RLISTFILE       LIBDIR "news.receivers"
#define BAD_USERS	LIBDIR "bad.users"
#define BAD_USERS_SH	LBINDIR "bad.users.sh" /* gul */
#define MAXFEEDCOUNT	32

/* Long help file(s) */
#define HELPFILE        LIBDIR "mnews.help.%s"

/* Notify help file(s) */
#define HELP_NOTIFY     LIBDIR "list.help.%s"

#define HELP_LIST	LIBDIR "list-help.%s"

#define MSGFILE		LIBDIR "msg.%s"

/* C-news files layout */
#ifndef NNTP_ONLY
#define HISTORY		"/usr/local/news/db/history"
#define NEWSGROUPS	"/usr/local/news/db/newsgroups"
#define ACTIVE		"/usr/local/news/db/active"
#define NEWSSPOOL	"/var/spool/news"
#define USE_DBZ		/* define it if your history file indexed by DBZ */
#endif

/* The working batch directory and files in this directory */
#define WORKDIR         "/var/spool/news/outgoing"
#define	OUTGO_DIR	"/var/spool/news/outgoing"
#define TOGO            "mailnews-server"	/* list of articles from C-news */
#define TMPRUN          "tmp.run"	/* temp file */
#define ARTLIST         "send.list"	/* list of articles for notify collected by send-list       */
#define INDEXLIST	"index.list"
#define FINDLIST	"find.list"
#define NEWGROUP	"newgr.list"
#define TMPNG		".tmpng"
#define LISTER          LBINDIR "lister"
#define SUBSCR          LBINDIR "newsserver"
#define DAEMON		LBINDIR "newsserv" /* if you use daemon */
#ifdef NNTP_ONLY
#define INBUFSIZE	(1024*1024) /* max article size, refusing if more */
#else
#define INBUFSIZE	(64*1024) /* buffer size, use tempfile if more */
#endif

/* Stop file. Must be QUEUE_DIR/mn.stop */

#define QUEUE_DIR	"/var/spool/mailnews/queue"
#define STOP_F		QUEUE_DIR "/mn.stop"

/* Unix /bin commands */
#define SORT            "/bin/sort -u +0 -2 +2bn"
#undef UUNAME          "uuname"

extern void setrunuid();
extern void lexit();
extern void lockrun();
extern void unlockrun();

#if defined( SYSV ) || defined( USG )
#define index   strchr
#define rindex  strrchr
#define bcopy(a,b,c)   memcpy(b,a,c)
#else
extern char *index(),*rindex();
#endif

#ifdef NNTP
#if defined(ISC)
#undef LAI_TCP
#endif

#ifdef	EXCELAN
#define	NONETDB
#define	OLDSOCKET
#endif

#ifdef	NONETDB
#define	IPPORT_NNTP	119		/* NNTP is on TCP port 119 */
#endif	NONETDB

#define DO_HIST_CHECK	/* coment it if you wantn't check for dupes */
#define	SYSLOG "nntpserv"
#define	TIMEOUT	(2 * 3600)

#define HISTORY_FILE	HISTORY
#define	SPOOLDIR	"/var/spool"
#define LOCKDIR		"/var/lock"
#define RNEWS		"/usr/local/news/bin/rnews"	/* Link to inews? */

#define	MAX_ARTICLES	4096	/* Maximum number of articles/group */
#endif

#ifdef NNTP_ONLY
#define IPORT		1119
#define OPORT		119
#define MAX_SERVERS	2
#define NNTP_SERVER	"news.lucky.net"
#define SWITCH_TO_READER	/* send "mode reader" to server */
#define NNTP_TIMEOUT	3600
#define REOPEN_TIME	900	/* reread database period by feeder */
#define CLOSEARTLIST	/* required! for reading send.list by lister */
#define USER		"news" /* feeder setuid to after bind() */
#endif

