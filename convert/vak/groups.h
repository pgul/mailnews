/*
 * Информация о подписке.
 */

struct subscrtab {
	long    tag;
	short   mode;
};

# ifdef __STDC__
#    define ARGS(x) x
#    define ARGS2(x,y) x,y
#    define ARGS3(x,y,z) x,y,z
#    define ARGS4(x,y,z,t) x,y,z,t
# else
#    define ARGS(x)
#    define ARGS2(x,y)
#    define ARGS3(x,y,z)
#    define ARGS4(x,y,z,t)
# endif

/* Загрузка базы данных */
extern int loadgroups (ARGS( int ));

/* Запись базы данных */
extern int savegroups (ARGS( void ));

/* Выдает таблицу подписки группы */
extern struct subscrtab *groupsubscr (ARGS2( long tag, int *cnt ));

/* Название группы */
extern char *groupname (ARGS( long group ));

/* Адрес пользователя */
extern char *username (ARGS( long user ));

/* Память возвращаемого списка должен освобождать вызывающий */
extern long *grouplist (ARGS( int * ));

# undef ARGS
# undef ARGS2
# undef ARGS3
