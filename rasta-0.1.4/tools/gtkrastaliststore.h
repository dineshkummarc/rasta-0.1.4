/*
 * gtkrastaliststore.h
 *
 * Header for the Rasta list model.
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com>
 * All rights reserved.
 *
 * For license terms, please see below.
 */


/* gtkliststore.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_RASTA_LIST_STORE_H__
#define __GTK_RASTA_LIST_STORE_H__

#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_RASTA_LIST_STORE	       (gtk_rasta_list_store_get_type ())
#define GTK_RASTA_LIST_STORE(obj)	       (GTK_CHECK_CAST ((obj), GTK_TYPE_RASTA_LIST_STORE, GtkRastaListStore))
#define GTK_RASTA_LIST_STORE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_LISTSTORE, GtkRastaListStoreClass))
#define GTK_IS_RASTA_LIST_STORE(obj)	       (GTK_CHECK_TYPE ((obj), GTK_TYPE_RASTA_LIST_STORE))
#define GTK_IS_RASTA_LIST_STORE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_RASTA_LIST_STORE))
#define GTK_RASTA_LIST_STORE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_RASTA_LIST_STORE, GtkRastaListStoreClass))

typedef struct _GtkRastaListStore       GtkRastaListStore;
typedef struct _GtkRastaListStoreClass  GtkRastaListStoreClass;

struct _GtkRastaListStore
{
  GObject parent;

  /*< private >*/
  gint stamp;
  xmlNodePtr screen_node;
};

struct _GtkRastaListStoreClass
{
  GObjectClass parent_class;
};


GtkType       gtk_rasta_list_store_get_type        (void);
GtkRastaListStore *gtk_rasta_list_store_new        (xmlNodePtr node);
#if 0 /* not anymore */
void          gtk_rasta_list_store_set_node        (GtkRastaListStore *store,
                                                    xmlNodePtr node);
#endif /* 0 */
xmlNodePtr    gtk_rasta_list_store_get_node   (GtkRastaListStore *store);
xmlNodePtr    gtk_rasta_list_store_get_field   (GtkRastaListStore *store,
                                                GtkTreeIter *iter);
void          gtk_rasta_list_store_remove          (GtkRastaListStore *store,
					      GtkTreeIter  *iter);
#if 0 /* Undone insertion methods */
void          gtk_rasta_list_store_insert          (GtkRastaListStore *store,
					      GtkTreeIter  *iter,
					      gint          position);
void          gtk_rasta_list_store_insert_before   (GtkRastaListStore *store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *sibling);
void          gtk_rasta_list_store_insert_after    (GtkRastaListStore *store,
					      GtkTreeIter  *iter,
					      GtkTreeIter  *sibling);
void          gtk_rasta_list_store_prepend         (GtkRastaListStore *store,
					      GtkTreeIter  *iter);
#endif /* Insertion */
void          gtk_rasta_list_store_append          (GtkRastaListStore *store,
					      GtkTreeIter  *iter);
void          gtk_rasta_list_store_clear           (GtkRastaListStore *store);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RASTA_LIST_STORE_H__ */
