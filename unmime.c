#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "dbsubs.h"

#include "raw.h"
#include "cp866.h"
#include "win.h"

typedef struct {
                 char charset[128];
                 char * xtable;
               } xtable_type;

static char * xtable;
char intsetname[128]="koi8-r",extsetname[128]="koi8-r";
xtable_type table[]={
     {"koi8-r", raw_table},
     {"x-cp866",cp866_table},
     {"x-cp866-u",cp866_table},
     {"windows-1251",win_table}};
int  ntables=sizeof(table)/sizeof(table[0]);
static char * lastword;

static int unqp(int (*getbyte)(void),int (*putbyte)(char))
{
  int  c;
  char s[3];
  char * p;

  for (;;)
  {
    c=getbyte();
nextqpbyte:
    if (c==-1) return 0;
    if (c!='=')
    { if (putbyte(c)==0)
        continue;
      else
        return -1;
    }
    c=getbyte();
    if (c==-1)
    { putbyte('=');
      break;
    }
    s[0]=(char)c;
    if (s[0]=='\n') continue;
    if (s[0]=='\r')
    { c=getbyte();
      if (c==-1)
        break;
      if (c=='\n') continue;
      goto nextqpbyte;
    }
    if (!isxdigit(s[0]))
    { putbyte('=');
      break;
    }
    c=getbyte();
    if (c==-1)
    { putbyte('=');
      putbyte(s[0]);
      break;
    }
    s[1]=(char)c;
    if (!isxdigit(s[1]))
    { putbyte('=');
      putbyte(s[0]);
      break;
    }
    s[2]=0;
    if (putbyte((char)strtol(s,&p,16)))
      return -1;
  }
  for (;c!=-1;c=getbyte())
    if (putbyte(c))
      return -1;
  return 1;
}

char cunbase64[256]={
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255, 64,255,255,
255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static int unbase64(int (*getbyte)(void),int (*putbyte)(char))
{
  int  ret=0;
  int  i;
  char s[4];
  int  c;

  for (;;)
  {
    for (i=0;i<4;)
    { c=getbyte();
      if (c==-1)
      { if (i)
        { ret=1;
          break;
        }
        return 0;
      }
      s[i]=(char)c;
      if (isspace(s[i])) continue;
      s[i]=cunbase64[(unchar)s[i]];
      if (s[i]==(char)0xff)
      { ret=1;
        break;
      }
      i++;
    }
    if (ret) break;
    if ((s[0]==64)||(s[1]==64))
    { ret=1;
      break;
    }
    if (putbyte((s[0]<<2)|(s[1]>>4))) return -1;
    if (s[2]==64)
    { if (s[3]!=64) ret=1;
      break;
    }
    if (putbyte(((s[1]<<4)|(s[2]>>2)) & 0xff)) return -1;
    if (s[3]==64)
      break;
    if (putbyte(((s[2]<<6)|s[3]) & 0xff)) return -1;
  }
  /* ждем конца файла */
  if (ret==0) c=getbyte();
  for(;c!=-1;c=getbyte())
  { if (!isspace(c))
      ret=1;
    if (ret)
      if (putbyte(c))
        return -1;
  }
  return ret;
}

static char * pstrgetbyte,* pstrputbyte;

static int strgetbyte(void)
{ char c;

  c=*pstrgetbyte;
  if (c)
    return *pstrgetbyte++;
  return -1;
}

static int strputbyte(char c)
{ *pstrputbyte++=c;
  return 0;
}

static void strunqp(char * src,char * dest)
{ char * p;
  pstrgetbyte=src;
  pstrputbyte=dest;
  /* only header! */
  for (p=src;*p;p++)
    if (*p=='_') *p=' ';
  unqp(strgetbyte,strputbyte);
  strputbyte(0);
}

static void strunb64(char * src,char * dest)
{
  pstrgetbyte=src;
  pstrputbyte=dest;
  unbase64(strgetbyte,strputbyte);
  strputbyte(0);
}

static char * set_table(char * charset,char * def_charset)
{
  int i;
  char * p;

  for (p=charset;*p;p++) *p=tolower(*p);
  if (strcmp(charset,intsetname)==0)
    return raw_table;
  for (i=0;i<ntables;i++)
    if (strcmp(charset,table[i].charset)==0)
      return table[i].xtable;
  if (charset[0]!='\0') /* указан неизвестный charset */
#if 1
    return raw_table;
#endif
  if (strcmp(def_charset,intsetname)==0)
    return raw_table;
  for (i=0;i<ntables;i++)
    if (strcmp(def_charset,table[i].charset)==0)
      return table[i].xtable;
  return raw_table;
}

static void strxlat(char * s,char * charset)
{
  xtable=set_table(charset,extsetname); /* только для header-а */
  for(;*s;s++)
    if ((unchar)*s>=128)
      *s=xtable[(unchar)*s-128];
}

void unmime_hdr(char * header)
{ char * pch,* pp;
  char * p,* cword;
  int  nq;
  static char charset[80];

  p=header;
  lastword=NULL;
  for (;;)
  {
    for (;isspace(*p);p++);
    if ((*p=='(') || (*p==')'))
    { p++;
      lastword=NULL;
    }
    if (*p=='\0')
      return;
    cword=p;
    for (nq=0;*p && !isspace(*p) && (*p!='(') && (*p!=')');p++)
      if (*p=='?') nq++;
    if (nq<4)
    { lastword=NULL;
      continue;
    }
    if (strncmp(cword,"=?",2))
    { lastword=NULL;
      continue;
    }
    if (strncmp(p-2,"?=",2))
    { lastword=NULL;
      continue;
    }
#if 0
    if (p-cword>80)
    { lastword=NULL;
      continue;
    }
#endif
    *(p-2)='\0';
    /* таки замаймленное слово - размаймиваем */
    for (pp=cword+2,pch=charset;*pp!='?';*pch++=*pp++);
    *pch++='\0';
    pp++;
    if (pp[1]!='?')
    { *(p-2)='?';
      lastword=NULL;
      continue;
    }
    if (lastword)
      cword=lastword; /* убираем пробелы */
    switch(toupper(*pp))
    { case 'Q': strunqp(pp+2,cword);
                strxlat(cword,charset);
                break;
      case 'B': strunb64(pp+2,cword);
                strxlat(cword,charset);
                break;
      default:  *(p-2)='?';
                lastword=NULL;
                continue;
    }
    cword+=strlen(cword);
    strcpy(cword,p);
    p=lastword=cword;
  }
}
