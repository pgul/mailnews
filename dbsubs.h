#ifndef _DBSUBS_H
#define _DBSUBS_H
#include <sys/types.h>

#define MEMMAP /* required if NNTP_ONLY! */

#define BASEDIR "/var/spool/mailnews/base"
#define SUBSBASE "subscribers"
#define BASEMODE 0660

#undef PASSWD_PROTECT	/* undef if need not password protction of database */

//#define AGE_TIME 28
#undef AGE_TIME

#if AGE_TIME
#define DEFAGING 1
#define TIME_FOR_AGING_OFF	365
#define TIME_FOR_SUSPEND	90
#define TIME_FOR_DISABLE	90
#else
#define DEFAGING 0
#endif

#define UNSUBS  0
#define SUBS    1
#define FEED    8
#define RFEED   2
#define EFEED	3
#define CNEWS	4

#define FORGET 255


#define ENGLISH 0
#define RUSSIAN 1
#define UKRAINIAN 2
#define DEFLANG RUSSIAN

#define F_OLD 0
#define F_NEW 1
#define DEFFMT F_NEW

#ifdef BSD
#define unchar u_char
#define ushort u_short
#define uint u_int
#define ulong u_long
#endif

#ifdef NEED_TYPES
typedef unsigned short int ushort;
typedef unsigned long int ulong;
typedef unsigned int uint;
#endif
#if defined (NEED_TYPES) || defined (NEED_UNCHAR)
typedef unsigned char unchar;
#endif

#ifdef __STDC__
extern int open_base(int),close_base(void),retry_base(void);
extern int lock(FILE *, int),getgroup(char *),sav_hdr(void),check(char *,FILE *,int,int);
extern int getuser(char *, int, int),add_free(long),issubs(char *,char *);
extern int get_gtree(long,int (*func) (long, int *, char *),int, int, char *);
extern int get_utree(long, int (*dfunc) (long, int *, char *),
int (*ufunc) (long, int *, char *), int, int, char *);
extern int isdis(long, int *, char *), strhash(char *);
extern int rm_tree(long, int (*func) (long, char *), int, int, char *);
extern int subscribe(char *,char **,int,int,FILE *),detlang(char *);
extern int set_uparam(char *,int, int, FILE *),wildmat(char *,char *);
extern int belongs(char *,char *),get_uparam(char *);
extern long get_free(ulong),sav_entry(ulong, char *, int);
extern char *get_gname(long),*get_uaddr(long),*strins(char *, char *, char);
extern char *get_domain(long),*lower_str(char *),*str_lower(char *),*mesg(int,int);
extern char *grecname(char *),*gsname(char *);
extern FILE *save(char *, char *, int );
extern void pberr(char *, FILE *),gethost(char *,int),readsnl(void),readrecl(void);
extern void log(char *, ...);
#ifdef NNTP
extern char *get_host_name(unsigned long);
extern unsigned long get_host_addr(char *);
#endif
#else
extern int open_base(),close_base(),retry_base(),lock(),getgroup(),issubs();
extern int getuser(),add_free(),get_gtree(),get_utree(),rm_tree(),check();
extern int subscribe(),set_uparam(),sav_hdr(),belong(),get_uparam(),wildmat();
extern int detlang(),isdis(),strhash();
extern long get_free(),sav_entry();
extern char *get_gname(),*get_uaddr(),*strins(),*mesg();
extern char *get_domain(),*lower_str(),*str_lower(),grecname();
extern FILE *save();
extern void pberr(),gethost(),readsnl(),readrecl(),log();
#ifdef NNTP
extern char *get_host_name();
extern unsigned long get_host_addr();
#endif
#endif

#define OPEN_RDONLY	0
#define OPEN_RDWR	1
#define OPEN_NOSAVE	2

extern struct base_header	header;
extern struct subs_chain	sb_entry;
extern struct string_chain	st_entry;
extern struct user_rec		ui_entry;
extern struct domain_rec	dm_entry;
extern struct group_rec		gr_entry;
extern FILE	*fb;
extern char	user[],group[160],wbuff[160],*ext[];
extern long	boffset,soffset,uoffset,doffset,goffset,coffset,poffset;
extern int	bserrno,omode;

#define MAX_STR_EL	16	/* must %4==0 */
#define MAXLVL		50

#define BH_MAGIC	0x970e8a11
#define UE_MAGIC	0x970e8a13
#define DE_MAGIC	0x970e8a15
#define GE_MAGIC	0x970e8a17
#define SE_MAGIC	0x970e8a19
#define SC_MAGIC	0x970e8a1b
#define FREE_MASK	0x80000000

struct base_header{
ulong	magic;
char	passwd[16];
long	user;
long	group;
long	freesp;
};

struct subs_chain{
ulong	magic;
long	group;
long	s_group;
long	user;
long	s_user;
ulong	mode:8;
ulong	m_param:24;
long	group_next;
long	group_prev;
long	user_next;
long	user_prev;
};

struct string_chain{
ulong	magic;
long	entry;
long	next;
long	up_ptr;
long	down_ptr;
ulong	reserved_1;
ushort	reserved_2;
unchar	size;
unchar	dot_at;
unchar	name[MAX_STR_EL];
};

#define DF_DIS		0x1
#define	DF_SITE		0x2
#define D_DEFACCESS	0xff

struct domain_rec{
ulong	magic;
ulong	flag;
ulong	reserved_1;
ushort	reserved_2;
unchar	access;
unchar	size;
long	s_domain;
};

#define UF_SUSP		0x1
#define UF_FMT		0x2
#define UF_AGING	0x4
#define UF_NLHLP	0x8
#define UF_NONGRP	0x10

#define US_SUSP		1
#define US_LIMIT	2
#define US_FMT		3
#define US_AGING	4
#define US_NLHLP	5
#define US_NONGRP	6
#define US_LANG		7

struct user_rec{
ulong	magic;
long	begin;
ulong	flag;
ulong	last_used;
ulong	reserved_1;
unchar	reserved_2;
unchar	lang;
unchar	limit;
unchar	size;
long	s_user;
};
#define DEFFLAGS ((DEFFMT*UF_FMT)|(DEFAGING*UF_AGING))

#define GF_NOSUBS	0x0001
#define GF_NOUNS	0x0002
#define GF_NOCHMOD	0x0004
#define GF_RONLY	0x0008
#define GF_SONLY	0x0010
#define GF_COM		0x0020
#define GF_NOXPOST	0x0040
#define GF_NNTP		0x0080
#define GF_NNTPALWAYS	0x0100
#define G_DEFACCESS	0x01

struct group_rec{
ulong	magic;
long	begin;
ulong	flag;
ulong	nntp_host;
long	nntp_lastused;
ulong	reserved_1;
ushort	reserved_2;
unchar	access;
unchar	size;
long	s_group;
};

#ifdef __STDC__
extern int f_size(struct string_chain *);
#else
extern int f_size();
#endif

struct free_sp{
ulong	magic;
long	next;
};

#define offset(struct,el)	((int)((long)&struct.el-(long)&struct))

/* bserno */
#define BS_OK		0
#define	BS_ENOFILE	1
#define BS_ENOPERM	2
#define BS_SYSERR	3
#define BS_INERR	4
#define BS_BADPASS	5
#define BS_ENOUSER	6
#define BS_ENOGROUP	7
#define BS_EFATAL	8
#define BS_RECALL	9
#define BS_ENOLOCK	10
#define BS_DISDOMAIN	11
#define BS_BASEERR	12
#define BS_NOFREE	13
#define BS_BADPARAM	14
#define BS_FATAL	15
#define BS_DENIED	16

#ifdef __linux__
# ifdef NULL
# undef NULL
# endif
# define NULL	0
#endif

/* Working around System V idiosyncrasies */
#if defined( SYSV ) || defined( USG )
#define index   strchr
#define rindex  strrchr
#endif

#endif /* _DBSUS_H */
