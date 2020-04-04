/*
 * Convertor database format from Stas's mn-3.* to mn-4.*
 *
 * (C) Copyright 1994 by Stanislav Voronyi, Kharkov, Ukraine
 * All rights reserved.
 *
 * This code is part of Mail-News gateway from Stanislav Voronyi.
 * This code is not public domain or free software.
 * See file COPYRIGHT for more details.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)conert.c 1.0 03/07/94";
#endif

#define usg(a) fprintf(stderr,a)

#if defined( SYSV ) || defined( USG )
#define index   strchr
#define rindex  strrchr
#endif

#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "conf.h"
#include "dbsubs.h"
#include "olddbsubs.h"

#define ISspace(x) ( x==' ' || x=='\t' )

extern char *malloc(), *calloc();
extern void free();
extern long time();

extern int addgroup(), create(), subscribe(),atoi();
static int convert();

main()
{
int stat;
char iline[20];

stat=create(0);
if(stat==2){
printf("Data base already exists. Remove y/n [n]: ");
gets(iline);
if(*iline=='y' || *iline=='Y')
stat=create(1);
else{
stat=0;
goto end;
}
}

if(stat)
goto end;

open_base(OPEN_NOSAVE);

stat=convert();

close_base();
save(BASEDIR,SUBSBASE,1);

end:
exit(stat);
}

static int 
convert()
{
    char filename[200];
    char flags[40];
    struct group_idx *ogr_entry;
    struct user_idx *oui_entry;
    struct subsbase *osb_entry;
    long oboffset, ogoffset;
    int mode,param;
    char *sgroup[2]={{(char *)NULL},{(char *)NULL}};
    FILE *ofg, *ofu, *ofb, *agef;

    ogr_entry = (struct group_idx *) malloc(sizeof(struct group_idx) + 80);
    oui_entry = (struct user_idx *) malloc(sizeof(struct user_idx) + 80);
    osb_entry = (struct subsbase *) malloc(sizeof(struct subsbase));

    sprintf(filename, "%s/%s", OLDBASEDIR, GROUPIND);
    if ((ofg = fopen(filename, "r")) == NULL)
	goto err3;

    sprintf(filename, "%s/%s", OLDBASEDIR, USERIND);
    if ((ofu = fopen(filename, "r")) == NULL)
	goto err2;

    sprintf(filename, "%s/%s", OLDBASEDIR, OLDSUBSBASE);
    if ((ofb = fopen(filename, "r")) == NULL)
	goto err1;

    oboffset = 0;

    if (lock(ofg, F_RDLCK) < 0)
	goto err;
    if (lock(ofu, F_RDLCK) < 0)
	goto err;
    if (lock(ofb, F_RDLCK) < 0)
	goto err;

    while (!feof(ofg)) {
	ogoffset = ftell(ofg);
	if (fread((char *) ogr_entry, sizeof(struct group_idx), 1, ofg) != 1)
	    if (feof(ofg))
		 break;
	    else
		goto err;
	if (fread((char *) (&ogr_entry->group[1]), sizeof(char), ogr_entry->size, ofg) !=
	    ogr_entry->size)
	     goto err;

	flags[0]=0;
    if (ogr_entry->flags.flag != 0) {
	if (ogr_entry->flags.bits.delete)
		continue;
	if (ogr_entry->flags.bits.not_subs)
	    strcat(flags,"S");
	if (ogr_entry->flags.bits.not_uns)
	    strcat(flags,"U");
	if (ogr_entry->flags.bits.not_chmod)
	    strcat(flags,"C");
	if (ogr_entry->flags.bits.read_only)
	    strcat(flags,"R");
	if (ogr_entry->flags.bits.subs_only)
	    strcat(flags,"O");
	if (ogr_entry->flags.bits.commerc)
	    strcat(flags,"M");
	if (ogr_entry->flags.bits.not_xpost)
	    strcat(flags,"X");
    }
	if(flags[0])
	    addgroup(ogr_entry->group,flags,(char *)NULL);
	else
	    addgroup(ogr_entry->group,(char *)NULL,(char *)NULL);

    if (ogr_entry->begin == NULL)
	continue;

    oboffset = ogr_entry->begin;
    while (oboffset != NULL) {
	if (fseek(ofb, oboffset, SEEK_SET))
	    goto err;
	if (fread((char *) osb_entry, sizeof(struct subsbase), 1, ofb) != 1)
	     goto err;
	if (fseek(ofu, osb_entry->user, SEEK_SET))
	    goto err;
	if (fread((char *) oui_entry, sizeof(struct user_idx), 1, ofu) != 1)
	     goto err;
	if (fread((char *) (&oui_entry->user[1]), sizeof(char),
		  oui_entry->size, ofu) != oui_entry->size)
	     goto err;

	switch (osb_entry->mode) {
	case OSUBS:
	    mode=SUBS;
            param=0;
	    break;
	case OFEED:
	    mode=FEED;
	    param=0;
	    break;
	case OEFEED:
	    mode=EFEED;
            param=0;
	    break;
	case ORFEED:
	    mode=RFEED;
	    param=osb_entry->m_param;
	}
	sgroup[0]=ogr_entry->group;
	printf("%s",oui_entry->user);
	subscribe(oui_entry->user,sgroup,mode,param,stdout);
	oboffset = osb_entry->group_next;
        }
    }
    rewind(ofu);
    printf("Convert user's flags and aging\n");
    sprintf(filename, "%s/%s", BASEDIR, AGE_FILE);
    agef = fopen(filename, "r");
    while (!feof(ofu)) {
	if (fread((char *) oui_entry, sizeof(struct user_idx), 1, ofu) != 1)
	    if (feof(ofu))
		 break;
	    else
		goto err;
	if (fread((char *) (&oui_entry->user[1]), sizeof(char), oui_entry->size, ofu) !=
	    oui_entry->size)
	     goto err;
	if(oui_entry->flags.bits.delete)
		continue;
	printf("%s\n",oui_entry->user);
	if(getuser(oui_entry->user,1,1)){
		pberr("convert",stderr);
		goto err;}
	if(oui_entry->flags.bits.susp)
		ui_entry.flag|=UF_SUSP;
	if(!oui_entry->flags.bits.notify_format)
		ui_entry.flag|=UF_FMT;
	if(oui_entry->flags.bits.no_lhlp)
		ui_entry.flag|=UF_NLHLP;	
	if(oui_entry->flags.bits.no_newgrp)
		ui_entry.flag|=UF_NONGRP;
	ui_entry.limit=oui_entry->limit;
	ui_entry.lang=oui_entry->lang;
	if (agef != NULL){
	rewind(agef);
	while (fgets(filename, sizeof filename, agef) != NULL) {
	    if (!strncmp(filename, oui_entry->user, oui_entry->size)) {
		if((param=atoi(index(filename,':')+1)))
		    ui_entry.last_used=param;
	    }
	}
	}
	if(fseek(fb,uoffset,SEEK_SET))
		goto err;
	if(fwrite((char *)&ui_entry,sizeof(struct user_rec),1,fb)!=1)
		goto err;
	if(oui_entry->flags.bits.disable)
		if(!(dm_entry.flag&DF_DIS)){
		dm_entry.flag|=DF_DIS;
		if(fseek(fb,doffset,SEEK_SET))
			goto err;
		if(fwrite((char *)&dm_entry,sizeof(struct domain_rec),1,fb)!=1)
			goto err;
		}
    }
    if(agef)
	fclose(agef);
    fclose(ofu);
    fclose(ofb);
    fclose(ofg);
    free(ogr_entry);
    free(oui_entry);
    free(osb_entry);
    return 0;

  err:
    fclose(ofb);
  err1:fclose(ofu);
  err2:fclose(ofg);
  err3:fprintf(stderr, "Error in I/O operation\n");
    free(ogr_entry);
    free(oui_entry);
    free(osb_entry);
    return 1;
}
