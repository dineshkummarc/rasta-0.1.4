/* 
 * renumeration.c
 *
 * Code for opaque enumerations.
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have recieved a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */
 
/*
 * MT safe
 */

#include "config.h"

#include <sys/types.h>
#include <glib.h>

#include "rasta.h"


/* Structures
 */
struct _REnumeration
{
  gpointer context;
  REnumerationFunc      has_more_func;
  REnumerationFunc      get_next_func;
  GDestroyNotify        notify_func;
};


static gpointer r_enumeration_list_has_more       (gpointer   context);
static gpointer r_enumeration_list_get_next       (gpointer   context);
static void     r_enumeration_list_destroy_notify (gpointer   context);



/* Enumerations
 */
REnumeration*
r_enumeration_new (gpointer               context,
                   REnumerationFunc       has_more_func,
                   REnumerationFunc       get_next_func,
                   GDestroyNotify         notify_func)
{
  REnumeration *enumeration;

  enumeration = g_new (REnumeration, 1);
  enumeration->context = context;
  enumeration->has_more_func = has_more_func;
  enumeration->get_next_func = get_next_func;
  enumeration->notify_func = notify_func;

  return enumeration;
}  /* r_enumeration_new() */

REnumeration*
r_enumeration_new_from_list (GList *init_list)
{
  REnumeration *enumeration;
  GList *list_copy, *header;

  /* The list is copied here, the caller is responsible for the
   * data items.  On _free(), the list is removed and the data items
   * left alone.  If the caller wants different semantics, the
   * caller can specify their own functions with _new() */
  list_copy = g_list_copy(init_list);

  /* This is a header element to refer to the list */
  header = g_list_prepend(NULL, (gpointer) list_copy);

  enumeration = r_enumeration_new ((gpointer) header,
                                   r_enumeration_list_has_more,
                                   r_enumeration_list_get_next,
                                   r_enumeration_list_destroy_notify);

  return(enumeration);
}  /* r_enumeration_new_from_list() */

gboolean
r_enumeration_has_more (REnumeration *enumeration)
{
  gpointer result;

  g_return_val_if_fail (enumeration != NULL, FALSE);

  result = (*enumeration->has_more_func) (enumeration->context);

  return (gboolean)GPOINTER_TO_INT(result);
}  /* r_enumeration_has_more() */

gpointer
r_enumeration_get_next (REnumeration *enumeration)
{
  gpointer result;

  g_return_val_if_fail (enumeration != NULL, NULL);

  result = (*enumeration->get_next_func) (enumeration->context);

  return result;
}  /* r_enumeration_get_next() */

void
r_enumeration_free (REnumeration *enumeration)
{
  (*enumeration->notify_func) (enumeration->context);

  g_free(enumeration);
}  /* r_enumeration_free() */

static gpointer
r_enumeration_list_has_more (gpointer context)
{
  GList *header, *elem;
  gboolean result;

  g_return_val_if_fail (context != NULL, (gpointer) FALSE);

  header = (GList *) context;
  elem = (GList *) header->data;

  result = (elem != NULL);

  return GINT_TO_POINTER(result);
}  /* r_enumeration_list_has_more() */

static gpointer
r_enumeration_list_get_next (gpointer context)
{
  GList *header, *elem;
  gpointer result;

  g_return_val_if_fail(context != NULL, NULL);

  header = (GList *) context;
  elem = (GList *) header->data;

  /* User should have called has_more() */
  g_return_val_if_fail(elem != NULL, NULL);

  result = elem->data;

  header->data = (gpointer) g_list_next(elem);

  g_list_free_1(elem);

  return result;
}  /* r_enumeration_list_get_next() */

static void
r_enumeration_list_destroy_notify (gpointer context)
{
  GList *header, *elem;

  g_return_if_fail (context != NULL);

  header = (GList *) context;
  elem = (GList *) header->data;

  g_list_free(elem);  /* NULL if at end of list */

  g_list_free(header);
}  /* r_enumeration_list_destroy_notify() */
