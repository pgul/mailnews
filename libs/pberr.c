/*
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#include <stdio.h>
#include "dbsubs.h"

void
pberr(str,file)
char *str;
FILE *file;
{
char *msg;
switch(bserrno){
case BS_ENOFILE:
msg="Can't find file\n";
break;
case BS_ENOPERM:
msg="Permition denied\n";
break;
case BS_SYSERR:
msg="Error on I/O operation";
break;
case BS_INERR:
msg="Internal base error\n";
break;
case BS_BADPASS:
msg="Password incorrect\n";
break;
case BS_ENOUSER:
msg="User not found\n";
break;
case BS_ENOGROUP:
msg="Group not founr\n";
break;
case BS_EFATAL:
msg="Fatal error. Database damaged\n";
break;
case BS_RECALL:
msg="Base already opened\n";
break;
case BS_ENOLOCK:
msg="Can't lock database\n";
break;
case BS_DISDOMAIN:
msg="Domain disabled\n";
break;
case BS_BASEERR:
msg="Database file corrupted\n";
break;
case BS_NOFREE:
msg="No free entry\n";
break;
case BS_BADPARAM:
msg="Bad function argument\n";
break;
case BS_FATAL:
msg="Fatal error on database file. Database damaged!\n";
break;
case BS_DENIED:
msg="Unknown user access denied!\n";
break;
default:
msg="Undefined error\n";
}
if(str==NULL)
(void)fprintf(file,"dbfunc: %s\n",msg);
else
(void)fprintf(file,"%s: %s\n",str,msg);
}
