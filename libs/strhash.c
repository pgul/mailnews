#include <string.h>

extern char *calloc();
extern void free();

int
strhash(s)
char *s;
{
int *str,hash,i;


str=(int *)calloc(strlen(s)/4+2,sizeof(int));
strcpy((char *)str, s);

for(i=0,hash=strlen(s)>5?(*((int *)&s[2])):12984560;str[i];i++) hash ^= str[i];

free(str);
return hash;
}
