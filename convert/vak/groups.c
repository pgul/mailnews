/*
 * ���������� � �������� �������� � ������� DBM.
 * DBM ����� ���� �������������� ������� � ���������
 * ��������� ��������������:
 *
 * ��� ������       --> ��� ������
 * ��� ������       --> ��� ������
 * ��� ������       --> ����� ��������� ������
 * ��� ������       --> ������� ��������
 * ��� ������������ --> ��� ������������
 * ��� ������������ --> ��� ������������
 * ��� ������������ --> ����� ������������
 *
 * ���� ������ � ������������ ������������ �����
 * ������� ����� ����:
 *  ------------------------------
 *  | 1 | N | X |      �����     |
 *  ------------------------------
 *        |   |
 *        |   ����� ��� �������
 *       ���, 0 - ������, 1 - ������������
 *
 * �����, ��� ��� ���� ���������� �� ����.
 */

# include <stdio.h>
# include <fcntl.h>
# include "cdbm.h"
# include "vdbm.h"
# include "tagdefs.h"
# include "groups.h"

# define DFLTLIMIT      32      /* ����. ����� ����� ��������� */

static CDBM *dbf;       /* ���������� ���� ������ */
static char *groupsfile = "groups";

extern errno;
extern char *malloc (), *mktemp (), *strcpy (), *strcat (), *strncpy ();
extern char *strchr ();
extern long time ();

/*
 * �������� ���� ������.
 * ������ DBM ��������� ����, ����� ������ ������
 * ���� admin �� ����� �� ���������.
 * ���������� 1, ���� ���� ������� �������,
 * 0, ���� ���� ������� �� �������,
 * -1, ���� ���� �������������.
 */

int loadgroups (waitflag)
{
	char name [200];

	name [0] = 0;
	if (! strchr (groupsfile, '/')) {
		strcat (name, "/usr/spool/newsserv");
		strcat (name, "/");
	}
	strcat (name, groupsfile);
tryagain:
	dbf = cdbm_open (name, O_RDWR | O_CREAT, 0664);
	if (! dbf) {
		if (errno < 0) {
			if (waitflag) {
				sleep (2);
				goto tryagain;
			}
			return (-1);
		}
		fprintf(stderr,"cannot open /usr/spool/newsserv/groups database");
		return (0);
	}
	dbf->updatelimit = DFLTLIMIT;
	return (1);
}

/*
 * ������ ���� ������.
 */

int savegroups ()
{
	if (dbf) {
		cdbm_close (dbf);
		dbf = 0;
	}
	return (1);
}

void setgroupsfile (name)
char *name;
{
	if (name)
		groupsfile = name;
	else
		groupsfile = "groups";
}

/*
 * ������� �� ���� ������� ������ �� ����.  ����������� ���
 * �������������� ���� � ��� ��� ������ ������� ��������.
 * ���������� ��������� �� ���������� �����.
 * ���������� ������� ������ ��� ������ ���������� �� ������ �����.
 * ���� ������ �� ������, ���������� 0.
 */

static char *getAddrByTag (tag)
long tag;
{
	datum key, rez;

	key.dptr = (char *) &tag;
	key.dsize = sizeof (tag);
	rez = cdbm_fetch (dbf, key);
	if (! rez.dptr)
		return (0);     /* ��� ������ */
	return (rez.dptr);
}

/*
 * ��������� ���� DBM �� ������������ ���� typ (G ��� U).
 * ����������, ��� ������ ���� ���� �������������� �� ����
 * � ��� ������ ��� ������������.
 * ���� ��� �� ���, ���������� ����� � ����.
 * ������� ����� - NULL � ���� key.dptr.
 */

static datum matchtag (key, typ)
datum key;
{
	long tag;

	for (;;) {
		if (! key.dptr)
			return (key);
		if (key.dsize == sizeof (tag) &&
		    (tag = *(long *) key.dptr) & TAGLABEL)
			switch (typ) {
			case 'G':
				if ((tag & (TAGUSER | TAGFLAGMASK)) ==
				    TAGNAME)
					return (key);
				break;
			case 'U':
				if ((tag & (TAGUSER | TAGFLAGMASK)) ==
				    (TAGUSER | TAGNAME))
					return (key);
				break;
			}
		key = cdbm_nextkey (dbf, key);
	}
}

long *taglist (typ, cnt)
int *cnt;
{
	long *tab;
	long tag;
	int len, ptr;
	datum key;

	*cnt = ptr = 0;
	len = 512;

	key = cdbm_firstkey (dbf);
	key = matchtag (key, typ);
	if (! key.dptr)
		return (0);

	tab = (long *) malloc (len * sizeof (long));
	if (! tab)
		return (0);

	while (key.dptr) {
		tag = *(long *) key.dptr;
		tag &= ~TAGFLAGMASK;
		if (ptr >= len) {
			len += 512;
			tab = (long *) realloc ((char *) tab, len * sizeof (long));
			if (! tab)
				return (0);
		}
		tab [ptr++] = tag;

		key = cdbm_nextkey (dbf, key);
		key = matchtag (key, typ);
	}
	if (! ptr) {
		free ((char *) tab);
		return (0);
	}
	if (ptr < len) {
		tab = (long *) realloc ((char *) tab, ptr * sizeof (long));
		if (! tab)
			return (0);
	}
	*cnt = ptr;
	return (tab);
}

/*
 * ���������� ������ ����� ���� �����.
 * ������ ��� ������ ���������� ����������� malloc(),
 * ����������� ������ ������ ���������� �������.
 */

long *grouplist (cnt)
int *cnt;
{
	return (taglist ('G', cnt));
}

/*
 * ������ ������� �������� �� ���� ������.
 * � cnt ���������� ���������� ������������� � �������
 * ��� 0.
 */

struct subscrtab *groupsubscr (g, cnt)
long g;
int *cnt;
{
	datum key, rez;

	g |= TAGSUBSCR;
	key.dptr = (char *) &g;
	key.dsize = sizeof (g);
	rez = cdbm_fetch (dbf, key);
	*cnt = rez.dsize / sizeof (struct subscrtab);
	if (!rez.dptr || !*cnt) {
		*cnt = 0;
		return (0);     /* ��� ������ */
	}
	return ((struct subscrtab *) rez.dptr);
}

char *groupname (tag)
long tag;
{
	return (getAddrByTag (tag | TAGNAME));
}

char *username (tag)
long tag;
{
	return (getAddrByTag (tag | TAGNAME));
}


