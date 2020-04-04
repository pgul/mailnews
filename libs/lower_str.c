char *
lower_str(str)
char *str;
{
static char sbuff[160];
register char *p;
register i;
p=str;
i=0;
do
{
	sbuff[i++]=((*p>='A' && *p<='Z')?(*p+32):*p);
	if (i>=sizeof(sbuff)) break;
}
while(*p++);
sbuff[sizeof(sbuff)-1]='\0';
return sbuff;
}
