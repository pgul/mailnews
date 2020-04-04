char *
strins(s1,s2,c)
char *s1,*s2,c;
{
	int l2;
	register i,j,k;

	i=strlen(s1);
	l2=j=strlen(s2);
	if(c)
		s2[j++]=c;
	k=i+j;
	s1[k]=0;
	do{
		s1[--k]=i==0?s2[--j]:s1[--i];
	}while(k);
	if (c) s2[l2]=0;
	return s1;
}
