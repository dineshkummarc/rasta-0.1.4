/*
 * gtkrastaliststore.c
 *
 * List model for Rasta fields.
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com>
 * All rights reserved.
 *
 * For license terms, please see notice below.
 */

/* gtkliststore.c
 * Copyright (C) 2000  Red Hat, Inc., 
 * Jonathan Blandford <jrb@redhat.com>
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

#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gtk/gtktreemodel.h>
#include "gtkrastaliststore.h"
#include <gtk/gtksignal.h>
#include <gtk/gtktreednd.h>
#include <gobject/gvaluecollector.h>

#define G_SLIST(x) ((GSList *) x)
#define GTK_RASTA_LIST_STORE_IS_SORTED(list) (GTK_RASTA_LIST_STORE (list)->sort_column_id != -1)
#define VALID_ITER(iter, rasta_list_store) (iter!= NULL && iter->user_data != NULL && rasta_list_store->stamp == iter->stamp)

static void         gtk_rasta_list_store_init            (GtkRastaListStore      *rasta_list_store);
static void         gtk_rasta_list_store_class_init      (GtkRastaListStoreClass *class);
static void         gtk_rasta_list_store_tree_model_init (GtkTreeModelIface *iface);
#if 0 /* Drag */
static void         gtk_rasta_list_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_rasta_list_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
#endif /* Drag */
static guint        gtk_rasta_list_store_get_flags       (GtkTreeModel      *tree_model);
static gint         gtk_rasta_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_rasta_list_store_get_column_type (GtkTreeModel      *tree_model,
						    gint               index);
static gboolean     gtk_rasta_list_store_get_iter        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreePath       *path);
static GtkTreePath *gtk_rasta_list_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_rasta_list_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     gtk_rasta_list_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_rasta_list_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_rasta_list_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gint         gtk_rasta_list_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_rasta_list_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    gint               n);
static gboolean     gtk_rasta_list_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


/* Drag and Drop */
#if 0 
static gboolean gtk_rasta_list_store_drag_data_delete   (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_rasta_list_store_drag_data_get      (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_rasta_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_rasta_list_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
                                                   GtkTreeModel      *src_model,
                                                   GtkTreePath       *src_path,
                                                   GtkTreePath       *dest_path);
#endif /* 0 */


static void
validate_rasta_list_store (GtkRastaListStore *rasta_list_store)
{
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_assert (rasta_list_store->screen_node != NULL);

      g_assert (rasta_list_store->screen_node->type == XML_ELEMENT_NODE);
    }
}

GtkType
gtk_rasta_list_store_get_type (void)
{
  static GType rasta_list_store_type = 0;

  if (!rasta_list_store_type)
    {
      static const GTypeInfo rasta_list_store_info =
      {
	sizeof (GtkRastaListStoreClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_rasta_list_store_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkRastaListStore),
	0,
        (GInstanceInitFunc) gtk_rasta_list_store_init,
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_rasta_list_store_tree_model_init,
	NULL,
	NULL
      };

#if 0 /* Handle DND later */
      static const GInterfaceInfo drag_source_info =
      {
	(GInterfaceInitFunc) gtk_rasta_list_store_drag_source_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo drag_dest_info =
      {
	(GInterfaceInitFunc) gtk_rasta_list_store_drag_dest_init,
	NULL,
	NULL
      };
#endif 

      rasta_list_store_type = g_type_register_static (G_TYPE_OBJECT, "GtkRastaListStore", &rasta_list_store_info, 0);
      g_type_add_interface_static (rasta_list_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
#if 0 /* Handle DND later */
      g_type_add_interface_static (rasta_list_store_type,
				   GTK_TYPE_TREE_DRAG_SOURCE,
				   &drag_source_info);
      g_type_add_interface_static (rasta_list_store_type,
				   GTK_TYPE_TREE_DRAG_DEST,
				   &drag_dest_info);
#endif
    }

  return rasta_list_store_type;
}

static void
gtk_rasta_list_store_class_init (GtkRastaListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;
}

static void
gtk_rasta_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_rasta_list_store_get_flags;
  iface->get_n_columns = gtk_rasta_list_store_get_n_columns;
  iface->get_column_type = gtk_rasta_list_store_get_column_type;
  iface->get_iter = gtk_rasta_list_store_get_iter;
  iface->get_path = gtk_rasta_list_store_get_path;
  iface->get_value = gtk_rasta_list_store_get_value;
  iface->iter_next = gtk_rasta_list_store_iter_next;
  iface->iter_children = gtk_rasta_list_store_iter_children;
  iface->iter_has_child = gtk_rasta_list_store_iter_has_child;
  iface->iter_n_children = gtk_rasta_list_store_iter_n_children;
  iface->iter_nth_child = gtk_rasta_list_store_iter_nth_child;
  iface->iter_parent = gtk_rasta_list_store_iter_parent;
}

#if 0 /* Drag n drop */
static void
gtk_rasta_list_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->drag_data_delete = gtk_rasta_list_store_drag_data_delete;
  iface->drag_data_get = gtk_rasta_list_store_drag_data_get;
}

static void
gtk_rasta_list_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = gtk_rasta_list_store_drag_data_received;
  iface->row_drop_possible = gtk_rasta_list_store_row_drop_possible;
}
#endif /* Drag n drop */

static void
gtk_rasta_list_store_init (GtkRastaListStore *rasta_list_store)
{
  rasta_list_store->stamp = g_random_int ();
  rasta_list_store->screen_node = NULL;
}

/**
 * gtk_rasta_list_store_new:
 * @node: The screen node
 *
 * Creates a new list store listing the fields in the screen
 *
 * Return value: a new #GtkRastaListStore
 **/
GtkRastaListStore *
gtk_rasta_list_store_new (xmlNodePtr node)
{
  GtkRastaListStore *retval;

  g_return_val_if_fail (node != NULL, NULL);

  retval = GTK_RASTA_LIST_STORE (g_object_new (gtk_rasta_list_store_get_type (), NULL));

  retval->screen_node = node;

  return retval;
}


#if 0 /* Don't like this, create a new model */
static void
gtk_rasta_list_store_set_node (GtkRastaListStore *rasta_list_store,
			       xmlNodePtr node)
{
  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (node != NULL);
  g_return_if_fail(node->type == XML_ELEMENT_NODE);
  g_return_if_fail((xmlStrcmp(node->name, "MENUSCREEN") == 0) ||
                   (xmlStrcmp(node->name, "DIALOGSCREEN") == 0) ||
                   (xmlStrcmp(node->name, "HIDDENSCREEN") == 0) ||
                   (xmlStrcmp(node->name, "ACTIONSCREEN") == 0));

  if (rasta_list_store->screen_node == screen_node)
    return;

}
#endif /* 0 */

/* Fulfill the GtkTreeModel requirements */
static guint
gtk_rasta_list_store_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), 0);

  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gtk_rasta_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), 0);

  return 2;
}

static GType
gtk_rasta_list_store_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (index < 2 && index >= 0, G_TYPE_INVALID);

  return G_TYPE_STRING;
}

static gboolean
gtk_rasta_list_store_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
    xmlNodePtr cur;
  gint i;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  i = gtk_tree_path_get_indices (path)[0];

  cur = GTK_RASTA_LIST_STORE(tree_model)->screen_node;
  cur = cur->children;
  while (cur != NULL)
  {
      g_assert(i >= 0);

      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
      {
          if (i == 0)
              break;
          else
              i--;
      }
      cur = cur->next;
  }
  if (cur == NULL)
      return FALSE;
  
  iter->stamp = GTK_RASTA_LIST_STORE (tree_model)->stamp;
  iter->user_data = cur;
  return TRUE;
}

static GtkTreePath *
gtk_rasta_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  xmlNodePtr cur = NULL;
  gint i = 0;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), NULL);
  g_return_val_if_fail (VALID_ITER(iter, GTK_RASTA_LIST_STORE(tree_model)), NULL);

  cur = GTK_RASTA_LIST_STORE(tree_model)->screen_node;
  cur = cur->children;
  while (cur != NULL)
  {
      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
      {
          if (cur == (xmlNodePtr)iter->user_data)
              break;
          else
              i++;
      }
      cur = cur->next;
  }
  if (cur == NULL)
    return NULL;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, i);
  return retval;
}

static void
gtk_rasta_list_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
    gchar *tmp = NULL;
    xmlNodePtr cur;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model));
  g_return_if_fail (VALID_ITER(iter, GTK_RASTA_LIST_STORE(tree_model)));
  g_return_if_fail(((xmlNodePtr)iter->user_data)->type == XML_ELEMENT_NODE);
  g_return_if_fail(xmlStrcmp(((xmlNodePtr)iter->user_data)->name, "FIELD") == 0);
  g_return_if_fail (column < 2);


  cur = (xmlNodePtr)iter->user_data;
  g_value_init(value, G_TYPE_STRING);
  if (column == 0)
      tmp = xmlGetProp(cur, "TYPE");
  else if (column == 1)
      tmp = xmlGetProp(cur, "NAME");
  else
      g_assert_not_reached();
  if (tmp == NULL)
      tmp = g_strdup("(null)");
  g_value_set_string(value, tmp);
}

static gboolean
gtk_rasta_list_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
    xmlNodePtr cur;
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail (VALID_ITER(iter, GTK_RASTA_LIST_STORE(tree_model)), FALSE);

  cur = (xmlNodePtr)iter->user_data;
  cur = cur->next;
  while (cur != NULL)
  {
      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
          break;

      cur = cur->next;
  }

  iter->user_data = cur;

  return (iter->user_data != NULL);
}

static gboolean
gtk_rasta_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
    xmlNodePtr cur;

    g_return_val_if_fail(GTK_IS_RASTA_LIST_STORE(tree_model), FALSE);
    g_return_val_if_fail(GTK_RASTA_LIST_STORE(tree_model)->screen_node != NULL, FALSE);

  /* this is a list, nodes have no children */
  if (parent)
    return FALSE;

  /* but if parent == NULL we return the list itself as children of the
   * "root"
   */

  cur = GTK_RASTA_LIST_STORE(tree_model)->screen_node;
  cur = cur->children;
  while (cur != NULL)
  {
      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
          break;
      cur = cur->next;
  }

  if (cur != NULL)
  {
      iter->stamp = GTK_RASTA_LIST_STORE (tree_model)->stamp;
      iter->user_data = cur;
      return TRUE;
  }
  else
    return FALSE;
}

static gboolean
gtk_rasta_list_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  return FALSE;
}

static gint
gtk_rasta_list_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
    xmlNodePtr cur;
    gint i = 0;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), -1);
  g_return_val_if_fail(GTK_RASTA_LIST_STORE(tree_model)->screen_node != NULL, -1);

  if (iter == NULL)
  {
      cur = GTK_RASTA_LIST_STORE(tree_model)->screen_node;
      cur = cur->children;
      while (cur != NULL)
      {
          if ((cur->type == XML_ELEMENT_NODE) &&
              (xmlStrcmp(cur->name, "FIELD") == 0))
              i++;
          cur = cur->next;
      }
      return(i);
  }

  g_return_val_if_fail (GTK_RASTA_LIST_STORE (tree_model)->stamp == iter->stamp, -1);
  return 0;
}

static gboolean
gtk_rasta_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
    xmlNodePtr cur;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail(GTK_RASTA_LIST_STORE(tree_model)->screen_node != NULL, FALSE);

  if (parent)
    return FALSE;

  cur = GTK_RASTA_LIST_STORE(tree_model)->screen_node;
  cur = cur->children;
  while (cur != NULL)
  {
      g_assert(n >= 0);

      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
      {
          if (n == 0)
              break;
          else
              n--;
      }
      cur = cur->next;
  }

  if (cur != NULL)
    {
      iter->stamp = GTK_RASTA_LIST_STORE (tree_model)->stamp;
      iter->user_data = cur;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_rasta_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  return FALSE;
}


/* Public accessors */

/**
 * gtk_rasta_list_store_get_field:
 * @store a #GtkRastaListStore
 * @iter: a row in @rasta_list_store
 *
 * Returns the XML "FIELD" node corresponding to the #GtkTreeIter
 **/
xmlNodePtr
gtk_rasta_list_store_get_field(GtkRastaListStore *rasta_list_store,
                               GtkTreeIter *iter)
{
    g_return_val_if_fail(GTK_IS_RASTA_LIST_STORE(rasta_list_store), NULL);
    g_return_val_if_fail(GTK_RASTA_LIST_STORE(rasta_list_store)->screen_node != NULL, NULL);
    g_return_val_if_fail(VALID_ITER(iter, rasta_list_store), NULL);

    return((xmlNodePtr)iter->user_data);
}


/**
 * gtk_rasta_list_store_remove:
 * @store: a #GtkRastaListStore
 * @iter: a row in @rasta_list_store
 *
 * Removes the given row from the list store, emitting the
 * "deleted" signal on #GtkTreeModel.
 *
 **/
void
gtk_rasta_list_store_remove (GtkRastaListStore *rasta_list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;
  xmlNodePtr cur;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (VALID_ITER (iter, rasta_list_store));

  cur = (xmlNodePtr)iter->user_data;
  xmlUnlinkNode(cur);
  xmlFreeNode(cur);

  rasta_list_store->stamp ++;
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (rasta_list_store), path);
  gtk_tree_path_free (path);
}

#if 0 /* Undone insertion methods */

static void
insert_after (GtkRastaListStore *rasta_list_store,
              GSList       *sibling,
              GSList       *new_list)
{
  g_return_if_fail (sibling != NULL);
  g_return_if_fail (new_list != NULL);

  /* insert new node after list */
  new_list->next = sibling->next;
  sibling->next = new_list;

  /* if list was the tail, the new node is the new tail */
  if (sibling == ((GSList *) rasta_list_store->tail))
    rasta_list_store->tail = new_list;

  rasta_list_store->length += 1;
}

/**
 * gtk_rasta_list_store_insert:
 * @store: a #GtkRastaListStore
 * @iter: iterator to initialize with the new row
 * @position: position to insert the new row
 *
 * Creates a new row at @position, initializing @iter to point to the
 * new row, and emitting the "inserted" signal from the #GtkTreeModel
 * interface.
 *
 **/
void
gtk_rasta_list_store_insert (GtkRastaListStore *rasta_list_store,
		       GtkTreeIter  *iter,
		       gint          position)
{
  GSList *list;
  GtkTreePath *path;
  GSList *new_list;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (position >= 0);

  if (position == 0 ||
      GTK_RASTA_LIST_STORE_IS_SORTED (rasta_list_store))
    {
      gtk_rasta_list_store_prepend (rasta_list_store, iter);
      return;
    }

  new_list = g_slist_alloc ();

  list = g_slist_nth (G_SLIST (rasta_list_store->root), position - 1);

  if (list == NULL)
    {
      g_warning ("%s: position %d is off the end of the list\n", G_STRLOC, position);
      return;
    }

  insert_after (rasta_list_store, list, new_list);

  iter->stamp = rasta_list_store->stamp;
  iter->user_data = new_list;

  validate_rasta_list_store (rasta_list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, position);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (rasta_list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_rasta_list_store_insert_before:
 * @store: a #GtkRastaListStore
 * @iter: iterator to initialize with the new row
 * @sibling: an existing row
 *
 * Inserts a new row before @sibling, initializing @iter to point to
 * the new row, and emitting the "inserted" signal from the
 * #GtkTreeModel interface.
 *
 **/
void
gtk_rasta_list_store_insert_before (GtkRastaListStore *rasta_list_store,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list, *prev, *new_list;
  gint i = 0;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (iter != NULL);
  if (sibling)
    g_return_if_fail (VALID_ITER (sibling, rasta_list_store));


  if (GTK_RASTA_LIST_STORE_IS_SORTED (rasta_list_store))
    {
      gtk_rasta_list_store_prepend (rasta_list_store, iter);
      return;
    }

  if (sibling == NULL)
    {
      gtk_rasta_list_store_append (rasta_list_store, iter);
      return;
    }

  new_list = g_slist_alloc ();

  prev = NULL;
  list = rasta_list_store->root;
  while (list && list != sibling->user_data)
    {
      prev = list;
      list = list->next;
      i++;
    }

  if (list != sibling->user_data)
    {
      g_warning ("%s: sibling iterator invalid? not found in the list", G_STRLOC);
      return;
    }

  /* if there are no nodes, we become the list tail, otherwise we
   * are inserting before any existing nodes so we can't change
   * the tail
   */

  if (rasta_list_store->root == NULL)
    rasta_list_store->tail = new_list;

  if (prev)
    {
      new_list->next = prev->next;
      prev->next = new_list;
    }
  else
    {
      new_list->next = rasta_list_store->root;
      rasta_list_store->root = new_list;
    }

  iter->stamp = rasta_list_store->stamp;
  iter->user_data = new_list;

  rasta_list_store->length += 1;

  validate_rasta_list_store (rasta_list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (rasta_list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_rasta_list_store_insert_after:
 * @store: a #GtkRastaListStore
 * @iter: iterator to initialize with the new row
 * @sibling: an existing row
 *
 * Inserts a new row after @sibling, initializing @iter to point to
 * the new row, and emitting the "inserted" signal from the
 * #GtkTreeModel interface.
 *
 **/
void
gtk_rasta_list_store_insert_after (GtkRastaListStore *rasta_list_store,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list, *new_list;
  gint i = 0;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (iter != NULL);
  if (sibling)
    g_return_if_fail (VALID_ITER (sibling, rasta_list_store));

  if (sibling == NULL ||
      GTK_RASTA_LIST_STORE_IS_SORTED (rasta_list_store))
    {
      gtk_rasta_list_store_prepend (rasta_list_store, iter);
      return;
    }

  for (list = rasta_list_store->root; list && list != sibling->user_data; list = list->next)
    i++;

  g_return_if_fail (list == sibling->user_data);

  new_list = g_slist_alloc ();

  insert_after (rasta_list_store, list, new_list);

  iter->stamp = rasta_list_store->stamp;
  iter->user_data = new_list;

  validate_rasta_list_store (rasta_list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (rasta_list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_rasta_list_store_prepend:
 * @store: a #GtkRastaListStore
 * @iter: iterator to initialize with new row
 *
 * Prepends a row to @store, initializing @iter to point to the
 * new row, and emitting the "inserted" signal on the #GtkTreeModel
 * interface for the @store.
 *
 **/
void
gtk_rasta_list_store_prepend (GtkRastaListStore *rasta_list_store,
			GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (iter != NULL);

  iter->stamp = rasta_list_store->stamp;
  iter->user_data = g_slist_alloc ();

  if (rasta_list_store->root == NULL)
    rasta_list_store->tail = iter->user_data;

  G_SLIST (iter->user_data)->next = G_SLIST (rasta_list_store->root);
  rasta_list_store->root = iter->user_data;

  rasta_list_store->length += 1;

  validate_rasta_list_store (rasta_list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (rasta_list_store), path, iter);
  gtk_tree_path_free (path);
}

#endif /* Insertion */

/**
 * gtk_rasta_list_store_append:
 * @store: a #GtkRastaListStore
 * @iter: iterator to initialize with the new row
 *
 a Appends a row to @store, initializing @iter to point to the
 * new row, and emitting the "inserted" signal on the #GtkTreeModel
 * interface for the @store.
 *
 **/
void
gtk_rasta_list_store_append (GtkRastaListStore *rasta_list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;
  xmlNodePtr parent, cur;
  gint length;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail(GTK_RASTA_LIST_STORE(rasta_list_store)->screen_node != NULL);
  g_return_if_fail (iter != NULL);

  parent = GTK_RASTA_LIST_STORE(rasta_list_store)->screen_node;
  cur = xmlNewNode(parent->ns, "FIELD");
  xmlSetProp(cur, "NAME", "field0");
  xmlSetProp(cur, "TEXT", "Field 0");
  xmlSetProp(cur, "TYPE", "readonly");
  xmlAddChild(parent, cur);

  iter->stamp = rasta_list_store->stamp;
  iter->user_data = cur;


  validate_rasta_list_store (rasta_list_store);

  path = gtk_tree_path_new ();
  length = gtk_rasta_list_store_iter_n_children(GTK_TREE_MODEL(rasta_list_store), NULL);
  gtk_tree_path_append_index (path, length - 1);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (rasta_list_store), path, iter);
  gtk_tree_path_free (path);
}

void
gtk_rasta_list_store_clear (GtkRastaListStore *rasta_list_store)
{
    xmlNodePtr screen, cur;
    GtkTreeIter iter;

  g_return_if_fail (GTK_IS_RASTA_LIST_STORE (rasta_list_store));
  g_return_if_fail (GTK_RASTA_LIST_STORE(rasta_list_store)->screen_node != NULL);

  screen = GTK_RASTA_LIST_STORE(rasta_list_store)->screen_node;
  cur = screen->children;
  while (cur != NULL)
  {
      if ((cur->type == XML_ELEMENT_NODE) &&
          (xmlStrcmp(cur->name, "FIELD") == 0))
      {
          iter.stamp = rasta_list_store->stamp;
          iter.user_data = cur;
          gtk_rasta_list_store_remove(rasta_list_store, &iter);
          /* inefficient, but don't trust changed xml */
          cur = screen->children;
      }
      cur = cur->next;
  }
}

#if 0 /* Drag n drop */

static gboolean
gtk_rasta_list_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
  GtkTreeIter iter;
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (drag_source), FALSE);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      gtk_rasta_list_store_remove (GTK_RASTA_LIST_STORE (drag_source), &iter);
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_rasta_list_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection_data)
{
  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (drag_source), FALSE);

  /* Note that we don't need to handle the GTK_TREE_MODEL_ROW
   * target, because the default handler does it for us, but
   * we do anyway for the convenience of someone maybe overriding the
   * default handler.
   */

  if (gtk_selection_data_set_tree_row (selection_data,
                                       GTK_TREE_MODEL (drag_source),
                                       path))
    {
      return TRUE;
    }
  else
    {
      /* FIXME handle text targets at least. */
    }

  return FALSE;
}

static gboolean
gtk_rasta_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                   GtkTreePath       *dest,
                                   GtkSelectionData  *selection_data)
{
  GtkTreeModel *tree_model;
  GtkRastaListStore *rasta_list_store;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (drag_dest), FALSE);

  tree_model = GTK_TREE_MODEL (drag_dest);
  rasta_list_store = GTK_RASTA_LIST_STORE (drag_dest);

  if (gtk_selection_data_get_tree_row (selection_data,
                                       &src_model,
                                       &src_path) &&
      src_model == tree_model)
    {
      /* Copy the given row to a new position */
      GtkTreeIter src_iter;
      GtkTreeIter dest_iter;
      GtkTreePath *prev;

      if (!gtk_tree_model_get_iter (src_model,
                                    &src_iter,
                                    src_path))
        {
          goto out;
        }

      /* Get the path to insert _after_ (dest is the path to insert _before_) */
      prev = gtk_tree_path_copy (dest);

      if (!gtk_tree_path_prev (prev))
        {
          /* dest was the first spot in the list; which means we are supposed
           * to prepend.
           */
          gtk_rasta_list_store_prepend (GTK_RASTA_LIST_STORE (tree_model),
                                  &dest_iter);

          retval = TRUE;
        }
      else
        {
          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model),
                                       &dest_iter,
                                       prev))
            {
              GtkTreeIter tmp_iter = dest_iter;
              gtk_rasta_list_store_insert_after (GTK_RASTA_LIST_STORE (tree_model),
                                           &dest_iter,
                                           &tmp_iter);
              retval = TRUE;
            }
        }

      gtk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, copy data from src
       */
      if (retval)
        {
          GtkTreeDataList *dl = G_SLIST (src_iter.user_data)->data;
          GtkTreeDataList *copy_head = NULL;
          GtkTreeDataList *copy_prev = NULL;
          GtkTreeDataList *copy_iter = NULL;
	  GtkTreePath *path;
          gint col;

          col = 0;
          while (dl)
            {
              copy_iter = _gtk_tree_data_list_node_copy (dl,
                                                         rasta_list_store->column_headers[col]);

              if (copy_head == NULL)
                copy_head = copy_iter;

              if (copy_prev)
                copy_prev->next = copy_iter;

              copy_prev = copy_iter;

              dl = dl->next;
              ++col;
            }

	  dest_iter.stamp = GTK_RASTA_LIST_STORE (tree_model)->stamp;
          G_SLIST (dest_iter.user_data)->data = copy_head;

	  path = gtk_rasta_list_store_get_path (GTK_TREE_MODEL (tree_model), &dest_iter);
	  gtk_tree_model_range_changed (GTK_TREE_MODEL (tree_model), path, &dest_iter, path, &dest_iter);
	  gtk_tree_path_free (path);
	}
    }
  else
    {
      /* FIXME maybe add some data targets eventually, or handle text
       * targets in the simple case.
       */
    }

 out:

  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

static gboolean
gtk_rasta_list_store_row_drop_possible (GtkTreeDragDest *drag_dest,
                                  GtkTreeModel    *src_model,
                                  GtkTreePath     *src_path,
                                  GtkTreePath     *dest_path)
{
  gint *indices;

  g_return_val_if_fail (GTK_IS_RASTA_LIST_STORE (drag_dest), FALSE);

  if (src_model != GTK_TREE_MODEL (drag_dest))
    return FALSE;

  if (gtk_tree_path_get_depth (dest_path) != 1)
    return FALSE;

  /* can drop before any existing node, or before one past any existing. */

  indices = gtk_tree_path_get_indices (dest_path);

  if (indices[0] <= GTK_RASTA_LIST_STORE (drag_dest)->length)
    return TRUE;
  else
    return FALSE;
}

#endif /* Drag n drop */

