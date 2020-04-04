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

int
f_size(lst_entry)
struct string_chain *lst_entry;
{
register s=lst_entry->size+1;
return (sizeof(struct string_chain)-MAX_STR_EL)+((s%4)?(s+(4-(s%4))):s);
}
