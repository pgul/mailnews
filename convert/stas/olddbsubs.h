#define OLDBASEDIR "/usr/spool/mailnews/base1"
#define OLDSUBSBASE "subscribers"
#define USERIND "user.index"
#define GROUPIND "group.index"
#define LOCKF "LOCK"

#define AGE_FILE "user.last"

#define OSUBS    1
#define OFEED    2
#define ORFEED   3
#define OEFEED	4

struct subsbase {
    long group;
    long user;
    ulong mode:8;
    ulong m_param:24;
    long group_next;
    long user_next;
};

struct user_idx {
    long begin;
    union u_flag {
	unchar flag;
	struct u_bits {
	    unchar susp:1;
	    unchar disable:1;
	    unchar notify_format:1;
	    unchar aging:1;
	    unchar no_lhlp:1;
	    unchar no_newgrp:1;
	    unchar flag_6:1;
	    unchar delete:1;
	} bits;
    } flags;
    unchar lang:2;
    unchar limit:6;
    unchar size:8;
    char user[1];
};

struct group_idx {
    long begin;
    union g_flag {
	unchar flag;
	struct g_bits {
	    unchar not_subs:1;
	    unchar not_uns:1;
	    unchar not_chmod:1;
	    unchar read_only:1;
	    unchar subs_only:1;
	    unchar commerc:1;
	    unchar not_xpost:1;
	    unchar delete:1;
	} bits;
    } flags;
    unchar nntp:1;
    unchar nntp_xfer:1;
    unchar nntp_host:3;
    unchar unuse:3;
    unchar size;
    char group[1];
};
