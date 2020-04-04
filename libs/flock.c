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
#include <fcntl.h>
#include <errno.h>

extern unsigned sleep();

#define MAX_TRY 1800 /* one hour */
#define SLEEP 2

int
lock(file, l_type)
FILE *file;
int l_type;
{
int try,slt=3;
struct flock lck;

try=0;

#ifndef OS2
lck.l_whence=0;
lck.l_start=0L;
lck.l_len=0L;

do{
lck.l_type=l_type;
if(fcntl(fileno(file),F_GETLK,&lck)<0)
	return -1;
if(lck.l_type!=F_UNLCK){
if((try++)>MAX_TRY)
return -1;
(void)sleep(SLEEP);
}
}while(lck.l_type!=F_UNLCK);

lck.l_type=l_type;
lck.l_whence=0;
lck.l_start=0L;
lck.l_len=0L;
while(fcntl(fileno(file),F_SETLK, &lck)<0){
#else
while (0) { /* dummy lock... */
#endif
  if(errno==EAGAIN || errno==EACCES){
     if(try++>MAX_TRY){
	(void)sleep(slt);
	slt+=SLEEP;
	continue;
     }
   return -1;
  }
  perror("fcntl");
  return -1;
}
return 0;
}
