#include <stdio.h>

main(argc,argv)
int argc;
char **argv;
{
unsigned char tabs_1[256],tabs_2[256],LowerLine[256],UpperLine[256],*tabs;
FILE *Letters;

if((Letters=fopen(argv[1],"r"))==NULL){
perror("lettertab");
exit(1);}
if(fgets(UpperLine,sizeof UpperLine,Letters)==NULL){
perror("lettertab");
exit(1);}
if(fgets(LowerLine,sizeof LowerLine,Letters)==NULL){
perror("lettertab");
exit(1);}
fclose(Letters);
{register i;
for(i=0;i< 256;i++) tabs_1[i]=tabs_2[i]=0;}
{register i;
for(i=0;UpperLine[i];i++) tabs_1[UpperLine[i]]=i+1;}
{register i;
for(i=0;LowerLine[i];i++) tabs_1[LowerLine[i]]=i+1;}
{register i,j;
for(i=0,j=1;UpperLine[i];i++,j++) tabs_2[UpperLine[i]]=j;
for(i=0;LowerLine[i];i++,j++) tabs_2[LowerLine[i]]=j;}
tabs_1['\n']=tabs_2['\n']=0;
tabs=tabs_2;
printf("unsigned char letters[2][256]={\n");
loop:
{register i;
for(i=0;i<16;i++){
register j;
for(j=0;j<16;j++)
printf(i==15&&j==15?tabs==tabs_2?"%d},":"%d}":i==0&&j==0?"{%d":"%d,",tabs[(i*16)+j]);
printf("\n");}}
if(tabs==tabs_1){
printf("};\n");
exit(0);}
else{
tabs=tabs_1;
goto loop;}
}
