/*
 * Информация о подписке хранится в формате DBM.
 * DBM игрет роль ассоциативного массива и выполняет
 * следующие преобразования:
 *
 * имя группы       --> тег группы
 * тег группы       --> имя группы
 * тег группы       --> номер последней статьи
 * тег группы       --> таблица подписки
 * имя пользователя --> тег пользователя
 * тег пользователя --> имя пользователя
 * тег пользователя --> флаги пользователя
 *
 * Теги группы и пользователя представляют собой
 * длинное целое вида:
 *  ------------------------------
 *  | 1 | N | X |      номер     |
 *  ------------------------------
 *        |   |
 *        |   флаги при выборке
 *       тип, 0 - группа, 1 - пользователь
 *
 * Важно, что все теги отличаются от нуля.
 */

# include <stdio.h>
# include <fcntl.h>
# include "cdbm.h"
# include "vdbm.h"
# include "tagdefs.h"
# include "groups.h"

# define DFLTLIMIT      32      /* Макс. длина файла изменений */

static CDBM *dbf;       /* дескриптор базы данных */
static char *groupsfile = "groups";

extern errno;
extern char *malloc (), *mktemp (), *strcpy (), *strcat (), *strncpy ();
extern char *strchr ();
extern long time ();

/*
 * Загрузка базы данных.
 * Причем DBM блокирует базу, чтобы всякие прочие
 * типа admin не могли ее испортить.
 * Возвращает 1, если база успешно открыта,
 * 0, если базу открыть не удается,
 * -1, если база заблокирована.
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
 * Запись базы данных.
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
 * Выборка из базы массива байтов по тегу.  Применяется для
 * преобразования тега в имя или поиска таблицы подписки.
 * Возвращает указатель на внутренний буфер.
 * Вызывающая функция должна эту память переписать на нужное место.
 * Если объект не найден, возвращает 0.
 */

static char *getAddrByTag (tag)
long tag;
{
	datum key, rez;

	key.dptr = (char *) &tag;
	key.dsize = sizeof (tag);
	rez = cdbm_fetch (dbf, key);
	if (! rez.dptr)
		return (0);     /* нет такого */
	return (rez.dptr);
}

/*
 * Проверяем ключ DBM на соответствие типу typ (G или U).
 * Фактически, это должен быть ключ преобразования из тега
 * в имя группы или пользователя.
 * Если тип не тот, продолжаем поиск в базе.
 * Признак конца - NULL в поле key.dptr.
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
 * Возвращает список тегов всех групп.
 * Память под список выделяется посредством malloc(),
 * Освобождать память должна вызывающая сторона.
 */

long *grouplist (cnt)
int *cnt;
{
	return (taglist ('G', cnt));
}

/*
 * Выдает таблицу подписки по тегу группы.
 * В cnt записывает количество пользователей в таблице
 * или 0.
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
		return (0);     /* нет такого */
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


