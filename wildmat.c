/* wildmat() */
/* supports only "*" and "?" (no "[...]" and "[^...]" */
/* Copyright (C) Pavel Gulchouck <gul@gul.kiev.ua>    */

static int mstrcmp(char *pattern, char *text)
{
  for (;;pattern++,text++)
  { if (*pattern=='*') return 0;
    if ((*pattern=='?') && (*text))
      continue;
    if ((*pattern==0) && (*text==0))
      return 0;
    if (toupper(*pattern)!=toupper(*text))
      return 1;
  }
}

int wildmat(char *text, char *pattern)
{
  if (*pattern!='*')
  { if (mstrcmp(pattern, text))
      return 0;
    while ((*pattern!='*') && (*pattern!=0))
      pattern++, text++;
    if (*pattern==0)
      return 1;
  }
  pattern++;
  for (;;)
  {
    while (mstrcmp(pattern,text))
    { if (*text==0)
        return 0;
      text++;
    }
    while ((*pattern!='*') && (*pattern!=0))
      pattern++,text++;
    if (*pattern==0)
      return 1;
    }
    pattern++;
  }
}
