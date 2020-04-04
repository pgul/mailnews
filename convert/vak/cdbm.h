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
 * vdatum cdbm_firstkey (CDBM *db)
 *
 * vdatum cdbm_nextkey (CDBM *db, vdatum key)
 *
 * vdatum cdbm_fetch (CDBM *db, vdatum key)
 *
 * int cdbm_store (CDBM *db, vdatum key, vdatum val, int flag)
 *
 * int cdbm_delete (CDBM *db, vdatum key)
 */

# define CDBM_INSERT  0
# define CDBM_REPLACE 1

# ifndef NO_DATUM
typedef struct {
	char *dptr;                     /* ������ */
	int dsize;                      /* ����� ������ */
} datum;
# endif

struct cdbm_elem;

typedef struct {
	char *basefile;         /* ��� ����� ���� ������ */
	char *updatefile;       /* ��� ����� ��������� */
	void *dbm;              /* ���� ������ */
	struct cdbm_elem **tab; /* ������� ��������� */
	int mode;               /* ����� ������� � ���� */
	int fd;                 /* ���������� ����� ��������� */
	int size;               /* ����� ������� tab, ������� (!) ������ */
	int cnt;                /* ���������� ��������� � tab, <=70% �� size */
	int nextindex;          /* ��������� ������ � tab ��� �������� */
	int readonly;           /* ������ ������ */
	int updatelimit;        /* ������������ ������ ����� ��������� (K) */
} CDBM;

extern CDBM     *cdbm_open ();
extern void     cdbm_close ();
extern datum    cdbm_fetch ();
extern int      cdbm_store ();
extern int      cdbm_delete ();
extern datum    cdbm_firstkey ();
extern datum    cdbm_nextkey ();

extern void     cdbm_sync ();
