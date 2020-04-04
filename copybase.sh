#!/bin/sh
if sdba check base
then
cd /usr/spool/mailnews
tar cf - base | gzip -9 > base.tar.gz
exit $?
else
echo Restoring from copy needed !!!
exit 1
fi

