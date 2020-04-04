/*
 * ���������� � ����� (crash-proof) ���������� ���� ������
 * � ����������� NDBM.
 *
 * �������� ���� - ������ � �������� �������� ����
 * ������ �� ������, � �������� ������ ����������/���������
 * � ��������� �����.  ����� ��������� ������������� ���������� �����,
 * ��������� ����� ���� ������, � ��� �������� ���������,
 * ����� ���� ��� ���������� �� ����� ����������.
 * ���� ��������� ������ ������������ ������ � �����.
 *
 * ����� �������� �����������, ��� ���� � �����-���� ������
 * ��������� (��� ������) ������, ���� ��������� � ����������
 * ��������� � ����� �������� ������ �� ����������,
 * ������� ��� �� ���� �������� � ���� ���������.
 *
 * ����� ������ ���������, <vak@kiae.su>.
 */

/*
 * CDBM *cdbm_open (char *name, int flags, int mode)
 *
 * void cdbm_close (CDBM *db)
 *
 * datum cdbm_firstkey (CDBM *db)
 *
 * datum cdbm_nextkey (CDBM *db, datum key)
 *
 * datum cdbm_fetch (CDBM *db, datum key)
 *
 * int cdbm_store (CDBM *db, datum key, datum val, int flag)
 *
 * int cdbm_delete (CDBM *db, datum key)
 *
 * �������������� �������:
 *
 * int cdbm_sync (CDBM *db)
 *              - �������� ��������� � ����
 */
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#include <stdio.h>
# include "gdbm.h"

# define NO_DATUM
# include "cdbm.h"

/*
 * ��������� ����� ������� ���������.
 */
# define INITSZ         64

/*
 * ������������ �������� p � ������� ������� �� �����,
 * ������� ������� �������� ������.
 */
# define ALIGN(p)       (((p) + sizeof (long) - 1) / sizeof (long) * sizeof (long))

/*
 * ���������� �� ������ ������ celem ������ �����.
 * �� ��������� � ������ ����� �� ���������� celem,
 * � ������ ������������ �� ������� �������� ������.
 */
# define KEYDATA(p)     ((char *) (p) + ALIGN (sizeof (celem)))

/*
 * ���������� �� ������ ������ celem ������ ��������.
 * �� ��������� � ������ �� ���������� celem � ������,
 * � ������ ������������ �� ������� �������� ������.
 */
# define VALDATA(p)     ((char *) (p) + ALIGN (sizeof (celem)) + ALIGN ((p)->keysize))

struct cdbm_elem {
	short keysize;                  /* ����� ����� */
	short valsize;                  /* ����� ������, -1 - ������ ������� */
	short hash;                     /* ���-�������� ����� */
	/* char key [keysize]; */       /* ���� */
	/* char val [valsize]; */       /* ������ */
};

/* ������������ �������� �������� �����/�������� */
# define MAX_KEYSIZE (1024)
# define MAX_VALSIZE (1024*30)

typedef struct cdbm_elem celem;

static short hash;
static long  db_mtime;
int  UPDATETIME;

static celem **cfind ();
static short chash ();
static int crehash ();
static int cload (), cupdate ();
static void cappend ();
extern long time();

extern int errno;
extern gdbm_error gdbm_errno;

extern char *memcpy (), *malloc (), *calloc (), *strdup (), *realloc ();
extern char *strcpy ();
extern void free ();
extern int memcmp ();
extern unsigned int strlen ();
extern long lseek ();

/*
 * ��������� ���� � �����������.
 * ���� �� �����������, ���������� 0.
 * ��� ���� errno �������� -1 ���� ���� �������������
 * ����� 0.
 */
int cdbm_error = 0;
CDBM *cdbm_open (name, flags, mode)
char *name;
int flags, mode;
{
	register CDBM *db;
	int len;

	errno = 0;
	cdbm_error = 1;
	db = (CDBM *) malloc (sizeof (CDBM));
	if (! db)
		return (0);

	len = strlen (name);
	db->basefile = malloc (len + 1);
	cdbm_error = 2;
	if (! db->basefile) {
		free ((char *) db);
		return (0);
	}
	db->updatefile = malloc (len + 2);
	cdbm_error = 3;
	if (! db->updatefile) {
		free (db->basefile);
		free ((char *) db);
		return (0);
	}
	strcpy (db->basefile, name);
	strcpy (db->updatefile, name);
	db->updatefile [len] = '+';
	db->updatefile [len + 1] = 0;

	db->mode = mode;
	db->size = INITSZ;
	db->updatelimit = 32;   /* 32 ��������� */
	db->tab = (celem **) calloc (db->size, sizeof (celem *));
	cdbm_error = 4;
	if (! db->tab) {
		free (db->basefile);
		free (db->updatefile);
		free ((char *) db);
		return (0);
	}
	db->cnt = 0;
	db->nextindex = 0;

	if (access (db->updatefile, 0) != 0)
		close (creat (db->updatefile, db->mode));
	cdbm_error = 5;
	db->fd = open (db->updatefile, 2);
	if (db->fd < 0) {
		free ((char *) db->tab);
		free (db->basefile);
		free (db->updatefile);
		free ((char *) db);
		return (0);
	}

	cdbm_error = 6;
	if ((flags & O_TRUNC) == O_TRUNC) {
		if (access (db->basefile, 0) == 0 && unlink (db->basefile) < 0 ||
		    access (db->updatefile, 0) == 0 && unlink (db->updatefile) < 0) {
			close (db->fd);
			free ((char *) db->tab);
			free (db->basefile);
			free (db->updatefile);
			free ((char *) db);
			return (0);
		}
	}
	flags &= O_RDONLY | O_RDWR | O_CREAT;
	db->readonly = (flags == O_RDONLY);

	if (access (db->basefile, 0) != 0) {
		if (flags != (O_RDWR | O_CREAT))
			goto err1;
		/* ��� ������ �����, ������� ����� GDBM-���� */
		db->dbm = gdbm_open (db->basefile, 512, 3, db->mode, (char *) 0);
		if (! db->dbm) {
err1:                   close (db->fd);
			free ((char *) db->tab);
			free (db->basefile);
			free (db->updatefile);
			free ((char *) db);
			return (0);
		}
		gdbm_close (db->dbm);
	}

	/*
	 * ��������� GDBM �� ������ � ����� �������������
	 * ���������� ���������.  �� ����� ����
	 * ������ ���� �� �����.
	 */
	cdbm_error = 7;
	db->dbm = gdbm_open (db->basefile, 512, 1, db->mode, (char *) 0);
	if (! db->dbm) {
		close (db->fd);
		free ((char *) db->tab);
		free (db->basefile);
		free (db->updatefile);
		free ((char *) db);
		if (gdbm_errno == GDBM_CANT_BE_WRITER)
			errno = -1;
		return (0);
	}

	/* ��������� ������ ��������� */
	cdbm_error = 8;

	if (! cload (db)) {
		gdbm_close (db->dbm);
		close (db->fd);
		free ((char *) db->tab);
		free (db->basefile);
		free (db->updatefile);
		free ((char *) db);
		return (0);
	}
	return (db);
}

void cdbm_close (db)
register CDBM *db;
{
	int i;

	close (db->fd);
	gdbm_close (db->dbm);
	for (i=0; i<db->size; ++i)
		if (db->tab [i])
			free ((char *) db->tab [i]);
	free (db->basefile);
	free (db->updatefile);
	free ((char *) db->tab);
	free ((char *) db);
}

static datum cnextmem (db, p)
register CDBM *db;
register celem **p;
{
	datum key;

	/* ������� ������� ��������� */
	for (; p<db->tab+db->size; ++p) {
		if (*p && (*p)->valsize != -1) {
			key.dptr = KEYDATA (*p);
			key.dsize = (*p)->keysize;
			return (key);
		}
	}
	/* ��� ������ ������� */
	key.dptr = 0;
	key.dsize = 0;
	return (key);
}

datum cdbm_firstkey (db)
register CDBM *db;
{
	datum key;
	static char *garbage;

	/*
	 * ������� ���������� ��, ��� � ����.
	 * �� ���������� ��������� � ������� ���������,
	 * � ����� ������ ��� ������� ��� ����������.
	 */
	key = gdbm_firstkey (db->dbm);
	if (garbage)
		free (garbage);
	garbage = key.dptr;
	while (key.dptr) {
		if (! *cfind (db, key.dptr, key.dsize))
			return (key);
		key = gdbm_nextkey (db->dbm, key);
		if (garbage)
			free (garbage);
		garbage = key.dptr;
	}
	/* ���� � ���� ������� ���, ��������� � ������� ��������� */
	return (cnextmem (db, db->tab));
}

datum cdbm_nextkey (db, key)
register CDBM *db;
datum key;
{
	register celem **p;
	static char *garbage;

	p = cfind (db, key.dptr, key.dsize);
	if (! *p) {
		/* ���������� ������ � ����. */
		for (;;) {
			key = gdbm_nextkey (db->dbm, key);
			if (garbage)
				free (garbage);
			garbage = key.dptr;
			if (! key.dptr)
				return (cnextmem (db, db->tab));
			if (! *cfind (db, key.dptr, key.dsize))
				return (key);
		}
	}
	/* ���� � ���� ������� ���, ��������� � ������� ��������� */
	return (cnextmem (db, ++p));
}

datum cdbm_fetch (db, key)
register CDBM *db;
datum key;
{
	datum val;
	register celem **p;
	static char *garbage;

	val.dptr = 0;
	val.dsize = 0;
	if (garbage)
		free (garbage);
	garbage = 0;

	/* ������� ���� � ������� ��������� */
	p = cfind (db, key.dptr, key.dsize);
	if (*p) {
		/* ������ �������? */
		if ((*p)->valsize == -1)
			return (val);
		/* ������ �������. */
		val.dptr = VALDATA (*p);
		val.dsize = (*p)->valsize;
		return (val);
	}
	val = gdbm_fetch (db->dbm, key);
	garbage = val.dptr;
	return (val);
}

int cdbm_store (db, key, val, flag)
register CDBM *db;
datum key, val;
int flag;
{
	register celem **p;

	if (db->readonly)
		return (-1);
	p = cfind (db, key.dptr, key.dsize);
	if (*p) {
		if (! flag)
			return (1);
		--db->cnt;
		free ((char *) *p);
	}
	return (cupdate (db, p, key, val));
}

static int cupdate (db, p, key, val)
register CDBM *db;
register celem **p;
datum key, val;
{
	int sz;
	static int nnn = 0;
	long curtime = time((long *)0);

	sz = ALIGN (sizeof (celem)) + ALIGN (key.dsize);
	if (val.dptr)
		sz += val.dsize;
	*p = (celem *) malloc (sz);
	if (! *p)
		return (-1);
	(*p)->hash = hash;
	(*p)->keysize = key.dsize;
# if 0
	if ( nnn++ < 20 ) {
	       printf("cl=%d sz=%d key_size=%d data_size=%d kd=%d vd=%d\n",
	       sizeof (celem), sz, key.dsize, val.dsize, (int)(KEYDATA(*p))-(int)(*p),(int)(VALDATA(*p))-(int)(*p));
	}
# endif
	if (key.dsize)
		memcpy (KEYDATA (*p), key.dptr, key.dsize);
	if (val.dptr) {
		(*p)->valsize = val.dsize;
		if (val.dsize)
			memcpy (VALDATA (*p), val.dptr, val.dsize);
	} else
		(*p)->valsize = -1;

	++db->cnt;

	/* ������ �������� � ���� ��������� */
	cappend (db->fd, *p);

	if (db->cnt > db->size*3/4)
		if (crehash (db, db->size * 2) < 0)
			return (-1);
	if ( db_mtime == 0 ) {
	       struct stat stbuf;
	       if ( stat(db->basefile,&stbuf) == 0 )
		       db_mtime = stbuf.st_mtime;
	       else
		       db_mtime = curtime;
	}
	if ( (UPDATETIME > 0 && curtime - db_mtime > UPDATETIME*60 ) ||
	      lseek (db->fd, 0L, 1) > db->updatelimit * 1024L)
	{
		/* �� ������ ������, ���� ��� �� ���������� */
		db_mtime = curtime;
		/* ������ ������������ ���� */
		cdbm_sync (db);
	}
	return (0);
}

int cdbm_delete (db, key)
register CDBM *db;
datum key;
{
	datum val;
	register celem **p;

	if (db->readonly)
		return (-1);
	p = cfind (db, key.dptr, key.dsize);
	if (*p) {
		if ((*p)->valsize == -1)
			return (0);
		--db->cnt;
		free ((char *) *p);
	} else {
		val = gdbm_fetch (db->dbm, key);
		if (! val.dptr)
			return (-1);
		free (val.dptr);
	}
	val.dptr = 0;
	return (cupdate (db, p, key, val));
}

static celem **cfind (db, k, sz)
register CDBM *db;
char *k;
int sz;
{
	register celem **p;

	hash = chash (k, sz);
	p = db->tab + hash % db->size;
	while (*p) {
		if ((*p)->hash == hash && (*p)->keysize == sz &&
		    ! memcmp (KEYDATA (*p), k, sz))
			break;
		++p;
		if (p >= db->tab + db->size)
			p = db->tab;
	}
	return (p);
}

static short chash (p, sz)
register char *p;
register sz;
{
	register long v;

# ifdef GNUHASH
	register i;

	/* hash function from GDBM 1.5 */
	v = 0x238f13af * sz;
	for (i=0; i<sz; ++i, ++p)
		v = (v + (*p << (i*5 % 24))) & 0x7fffffff;
	v = 1103515243 * v + 12345;
# else
	/* hash function from SDBM */
	v = 0;
	while (sz--)
		v = *p++ + 65599 * v;
#endif
	return (v & 0x7fff);
}

static int crehash (db, newsz)
register CDBM *db;
int newsz;
{
	CDBM newdb;
	celem **p, **q;

	newdb.size = newsz;
	newdb.tab = (celem **) calloc (newdb.size, sizeof (celem *));
	if (! newdb.tab)
		return (-1);
	for (q=db->tab; q<db->tab+db->size; ++q)
		if (*q) {
			p = cfind (&newdb, KEYDATA (*q), (*q)->keysize);
			*p = *q;
		}
	free ((char *) db->tab);
	db->size = newdb.size;
	db->tab = newdb.tab;
	return (0);
}

static void cappend (fd, p)
int fd;
register celem *p;
{
	/* ���������� ��� ����� ������ � ����� ����� ��������� */

	write (fd, (char *) p, sizeof (celem));
	write (fd, (char *) KEYDATA (p), p->keysize);
	if (p->valsize != -1)
		write (fd, (char *) VALDATA (p), p->valsize);
}

static int cload (db)
register CDBM *db;
{
	/* �������� ����� ��������� � ������ */
	/* � ��� ��������� ��� ����� ���, ��� ���� ���� ���������,
	 * ������ ��������� �� ���������� - ������ ��������� ������ �� ��������
	 */
	celem h, *p, **q;
	int sz, rez;
	int _nrec=0;

	for (;;) {
		rez = read (db->fd, (char *) &h, sizeof (celem));
		_nrec++;
		cdbm_error = 101;
		if (rez < 0)
			return (0);
		if (rez == 0)
			return (1);
		if (rez != sizeof (celem))
			return (1);
		if (h.keysize < 0 || h.keysize > MAX_KEYSIZE) {
		       fprintf(stderr,"Bad groups+: NREC=%d",_nrec);
		       fprintf(stderr,"... shift=%ld", lseek(db->fd,0l,1) - rez);
		       fprintf(stderr,"Bad groups+: key_size = %d (reading + aborted)",h.keysize);
		       return(1);
		}
		if ( h.valsize < -1 || h.valsize >= MAX_VALSIZE) {
		       fprintf(stderr,"Bad groups+: NREC=%d",_nrec);
		       fprintf(stderr,"... shift=%ld", lseek(db->fd,0l,1) - rez);
		       fprintf(stderr,"Bad groups+: val_size = %d (reading + aborted)",h.valsize);
		       return(1);
		}
		sz = ALIGN (sizeof (celem)) + ALIGN (h.keysize);
		if (h.valsize != -1)
			sz += h.valsize;
		p = (celem *) malloc (sz);
		if (! p)
			return (0);
		*p = h;
		if (p->keysize) {
			if (read (db->fd, KEYDATA (p), p->keysize) != p->keysize)
				return (1);
		}
		if (p->valsize > 0 ) {
			if (read (db->fd, VALDATA (p), p->valsize) != p->valsize)
				return (1);
		}
		q = db->tab + p->hash % db->size;
		while (*q) {
			if ((*q)->hash == p->hash &&
			    (*q)->keysize == p->keysize &&
			    ! memcmp (KEYDATA (*q), KEYDATA (p), p->keysize))
				break;
			++q;
			if (q >= db->tab + db->size)
				q = db->tab;
		}
		*q = p;

		++db->cnt;
		if (db->cnt > db->size*3/4)
			if (crehash (db, db->size * 2) < 0)
				return (-1);
	}
}


void cdbm_sync (db)
register CDBM *db;
{
	/* �������� ���� ������ � ��������� ��������� */
	char *newname, *oldname, *oldoldname;
	char *garbage;
	int len;
	GDBM_FILE newdbm;
	datum key, val;
	register celem **p;

	if (db->cnt == 0)
		return;
	newdbm = 0;

	/* ������� ����� database~~, database~, database# */
	len = strlen (db->basefile);
	newname = malloc (len + 2);
	oldname = malloc (len + 2);
	oldoldname = malloc (len + 3);
	if (! newname || ! oldname || !oldoldname)
		return;
	strcpy (newname, db->basefile);
	strcpy (oldname, db->basefile);
	strcpy (oldoldname, db->basefile);
	newname [len] = '#';
	newname [len+1] = 0;
	oldname [len] = '~';
	oldname [len+1] = 0;
	oldoldname [len] = '~';
	oldoldname [len+1] = '~';
	oldoldname [len+2] = 0;

	/* ������� ����� ���� ������ */
	newdbm = gdbm_open (newname, 512, 3, db->mode, (char *) 0);
	if (! newdbm)
		goto ret;

	/* ������������ ���� �� ����� ����� */
	key = gdbm_firstkey (db->dbm);
	while (key.dptr) {
		val = gdbm_fetch (db->dbm, key);
		if (val.dptr) {
			if (gdbm_store (newdbm, key, val, 0)) {
				free (key.dptr);
				free (val.dptr);
				goto ret;
			}
			free (val.dptr);
		}
		garbage = key.dptr;
		key = gdbm_nextkey (db->dbm, key);
		free (garbage);
	}

	/* ������ ��������� */
	for (p=db->tab; p<db->tab+db->size; ++p) {
		if (! *p)
			continue;
		if ((*p)->valsize == -1) {
			key.dptr = KEYDATA (*p);
			key.dsize = (*p)->keysize;
			gdbm_delete (newdbm, key);
			continue;
		}
		key.dptr = KEYDATA (*p);
		key.dsize = (*p)->keysize;
		val.dptr = VALDATA (*p);
		val.dsize = (*p)->valsize;
		if (gdbm_store (newdbm, key, val, 1))
			goto ret;
	}

	/* ��������� ����� ���� */
	gdbm_close (newdbm);
	newdbm = 0;

	/*
	 * ��������������� ���� :
	 * database~ -> database~~
	 * database  -> database~
	 * database# -> database
	 */
	unlink (oldoldname);                    /* ������� database~~ */
	link (oldname, oldoldname);             /* database~ -> database~~ */
	unlink (oldname);                       /* ������� database~ */
	if (link (db->basefile, oldname) < 0)   /* database -> database~ */
		goto ret;
	unlink (db->basefile);                  /* ������� database */
	/*
	 * ����� ������ �����.
	 * ���� � ���� ������ ������ ������,
	 * ���� ������ ����� ���������!
	 * ��� ���� �� ����� ���� ���������������
	 * ��� ����� ������������, �� ���� ��������� �����...
	 */
	if (link (newname, db->basefile) < 0) { /* database# -> database */
		link (oldname, db->basefile);
		goto ret;
	}
	unlink (newname);                       /* ������� database# */

	/* ������� ������ ��������� */
	close (db->fd);
	for (p=db->tab; p<db->tab+db->size; ++p)
		if (*p)
			free ((char *) *p);
	unlink (db->updatefile);

	/* ������� ������ ������ ��������� */
	db->size = INITSZ;
	db->cnt = 0;
	db->nextindex = 0;
	free ((char *) db->tab);
	db->tab = (celem **) calloc (db->size, sizeof (celem *));
	close (creat (db->updatefile, db->mode));
	db->fd = open (db->updatefile, 2);
	if (! db->tab || db->fd < 0)    /* ����� �� ����� ����! */
		abort ();

	/* ������������� ����� ���� */
	gdbm_close (db->dbm);
	/*
	 * ��������� GDBM �� ������ � ����� �������������
	 * ���������� ���������.  �� ����� ����
	 * ������ ���� �� �����.
	 */
	db->dbm = gdbm_open (db->basefile, 512, 1, db->mode, (char *) 0);
	if (! db->dbm)
		abort ();
ret:
	free (newname);
	free (oldname);
	free (oldoldname);
	if (newdbm)
		gdbm_close (newdbm);
	db_mtime = time((long *)0);
	return;
}

# ifdef TESTCDBM
# include <stdio.h>

main ()
{
	CDBM *db;
	char keystr [256], valstr [256], *s;
	datum key, val;

	db = cdbm_open (0);
	while (! feof (stdin)) {
		scanf ("%s ", keystr);
		gets (valstr);
		for (s=valstr; *s==' ' || *s=='\t'; ++s);
		key.dptr = keystr;
		key.dsize = strlen (keystr) + 1;
		val.dptr = valstr;
		val.dsize = strlen (s) + 1;
		if (cdbm_store (db, key, val, 1) < 0)
			fprintf (stderr, "cannot store '%s' - '%s'\n", keystr, valstr);
	}
	key = cdbm_firstkey (db);
	while (key.dptr) {
		val = cdbm_fetch (db, key);
		if (val.dptr)
			printf ("%s\t%s\n", key.dptr, val.dptr);
		else
			fprintf (stderr, "cannot fetch '%s' (%d)\n",
				key.dptr, key.dsize);
		key = cdbm_nextkey (db, key);
	}
	return (0);
}
# endif
