/*
 * rastaedit.c
 * 
 * An editor application for RASTA config files.
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have recieved a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gtkrastaliststore.h"




/*
 * Globals
 */
GtkWidget *top, *nb;
GtkWidget *r_menu, *r_dialog, *r_hidden, *r_action;
GtkWidget *p_menu, *p_dialog, *p_hidden, *p_action;
GtkWidget *di_enabled, *di_command;
GtkWidget *de_frame, *de_none, *de_single, *de_double;
GtkWidget *df_list;
GtkWidget *he_none, *he_single, *he_double;
GtkWidget *hi_command;
GtkWidget *at_yes, *at_no, *at_self;
GtkWidget *ae_none, *ae_single, *ae_double;
GtkWidget *a_confirm, *a_command;
GtkWidget *screen_id, *screen_title, *screen_help, *fastpath;
GtkRastaListStore *df_store;
xmlDocPtr doc;
xmlNsPtr ns;
xmlNodePtr root;
extern int xmlDoValidityCheckingDefaultValue;



/*
 * Prototypes
 */
static void print_usage(gint rc);
static void do_quit(GtkWidget *widget, gpointer user_data);
static void change_screen_type(GtkWidget *widget, gpointer user_data);
static void enable_initcommand(GtkWidget *widget, gpointer user_data);
static void create_menu_edit();
static void create_dialog_edit();
static void create_hidden_edit();
static void create_action_edit();
static GtkWidget *create_screen_editor();
static gint load_xml(const gchar *filename);
static xmlNodePtr find_screen(const gchar *screenname);
static gchar *query_help(xmlNodePtr node);
static guint load_screen(xmlNodePtr screen);
static void load_menu_screen(xmlNodePtr screen);
static void load_dialog_screen(xmlNodePtr screen);
static void load_hidden_screen(xmlNodePtr screen);
static void load_action_screen(xmlNodePtr screen);
static void add_dialog_field(GtkWidget *widget, gpointer user_data);



/*
 * Functions
 */


static void print_usage(gint rc)
{
    FILE *out = rc ? stderr : stdout;

    fprintf(out, "Usage: rastaedit <filename> <screenname>\n");
    _exit(rc);
}


static void add_dialog_field(GtkWidget *widget, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreePath *path;

    if (GTK_TREE_MODEL(df_store) == gtk_tree_view_get_model(GTK_TREE_VIEW(df_list)))
    {
        gtk_rasta_list_store_append(GTK_RASTA_LIST_STORE(df_store),
                                    &iter);
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(df_store),
                                       &iter);
        g_print("Path is %d\n", gtk_tree_path_get_indices(path)[0]);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(df_list),
                                     path,
                                     NULL, TRUE, 0, 1);
    }
}


static void do_quit(GtkWidget *widget, gpointer user_data)
{
    gtk_main_quit();
}  /* do_quit() */


static void change_screen_type(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *w;
    gint page_num;

    w = GTK_WIDGET(user_data);
    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(nb), w);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page_num);
}


static void enable_initcommand(GtkWidget *widget, gpointer user_data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(di_enabled)))
    {
        gtk_widget_set_sensitive(de_frame, TRUE);
        gtk_widget_set_sensitive(di_command, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(de_frame, FALSE);
        gtk_widget_set_sensitive(di_command, FALSE);
    }
}


static void create_menu_edit()
{
    GtkWidget *label;

    p_menu = gtk_table_new(2, 2, FALSE);
    label = gtk_label_new("There are no other Menu Screen options");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_table_attach(GTK_TABLE(p_menu), label,
                     0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 0, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), p_menu, NULL);
}


static void create_dialog_edit()
{
    GtkWidget *box, *scroll, *frame, *e_box;
    GtkWidget *b_add, *b_edit, *b_remove;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell_renderer;
    GtkTreeModel *tmp_model;

    p_dialog = gtk_table_new(2, 2, FALSE);
    frame = gtk_frame_new("Initcommand");
    box = gtk_vbox_new(FALSE, 2);

    di_enabled = gtk_check_button_new_with_label("Enabled");
    gtk_box_pack_start(GTK_BOX(box), di_enabled, FALSE, FALSE, 1);

    de_frame = gtk_frame_new("Escape style");
    e_box = gtk_hbox_new(TRUE, 2);
    de_none = gtk_radio_button_new_with_label(NULL, "None");
    de_single = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(de_none), "Single quoted");
    de_double = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(de_single), "Double quoted");

    gtk_box_pack_start(GTK_BOX(e_box), de_none, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(e_box), de_single, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(e_box), de_double, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(de_frame), e_box);
    gtk_box_pack_start(GTK_BOX(box), de_frame, FALSE, FALSE, 1);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    di_command = gtk_text_view_new ();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(di_command), FALSE);
    g_signal_connect(G_OBJECT(di_enabled), "clicked",
                     G_CALLBACK(enable_initcommand), NULL);
    gtk_container_add(GTK_CONTAINER(scroll), di_command);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 1);
    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_table_attach(GTK_TABLE(p_dialog), frame,
                     0, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0,
                     1, 1);

    tmp_model = GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_STRING,
                                                  G_TYPE_STRING));
    df_list = gtk_tree_view_new_with_model(tmp_model);
    g_object_unref(G_OBJECT(tmp_model));
    cell_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Field type",
                                                       cell_renderer,
                                                       "text", 0,
                                                       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(df_list), column);
    column = gtk_tree_view_column_new_with_attributes ("Field name",
                                                       cell_renderer,
                                                       "text", 1,
                                                       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(df_list), column);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), df_list);
    gtk_table_attach(GTK_TABLE(p_dialog), scroll,
                     0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                     1, 1);

    box = gtk_vbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(box),
                              GTK_BUTTONBOX_START);
    b_add = gtk_button_new_with_label("Add");
    g_signal_connect(G_OBJECT(b_add), "clicked",
                     G_CALLBACK(add_dialog_field), NULL);
    gtk_container_add(GTK_CONTAINER(box), b_add);
    b_edit = gtk_button_new_with_label("Edit");
    gtk_container_add(GTK_CONTAINER(box), b_edit);
    b_remove = gtk_button_new_with_label("Remove");
    gtk_container_add(GTK_CONTAINER(box), b_remove);


    gtk_table_attach(GTK_TABLE(p_dialog), box,
                     1, 2, 1, 2,
                     0, GTK_FILL | GTK_EXPAND,
                     1, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), p_dialog, NULL);
}


static void create_hidden_edit()
{
    GtkWidget *box, *scroll, *frame, *e_frame, *e_box;

    p_hidden = gtk_table_new(1, 1, FALSE);

    frame = gtk_frame_new("Initcommand");
    box = gtk_vbox_new(FALSE, 2);
    e_frame = gtk_frame_new("Escape style");
    e_box = gtk_hbox_new(TRUE, 2);
    he_none = gtk_radio_button_new_with_label(NULL, "None");
    he_single = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(he_none), "Single quoted");
    he_double = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(he_single), "Double quoted");

    gtk_box_pack_start(GTK_BOX(e_box), he_none, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(e_box), he_single, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(e_box), he_double, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(e_frame), e_box);
    gtk_box_pack_start(GTK_BOX(box), e_frame, FALSE, FALSE, 1);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    hi_command = gtk_text_view_new ();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(hi_command), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll), hi_command);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 1);
    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_table_attach(GTK_TABLE(p_hidden), frame,
                     0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                     1, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), p_hidden, NULL);
}



static void create_action_edit()
{
    GtkWidget *frame, *box;

    p_action = gtk_table_new(2, 4, FALSE);
    frame = gtk_frame_new("Terminal output");
    box = gtk_hbox_new(TRUE, 2);
    at_yes = gtk_radio_button_new_with_label(NULL, "Yes");
    at_no = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(at_yes), "No");
    at_self = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(at_no), "Handled by the subprocess ");

    gtk_box_pack_start(GTK_BOX(box), at_yes, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), at_no, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), at_self, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_table_attach(GTK_TABLE(p_action), frame,
                     0, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 1, 1);

    frame = gtk_frame_new("Escape style");
    box = gtk_hbox_new(TRUE, 2);
    ae_none = gtk_radio_button_new_with_label(NULL, "None");
    ae_single = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ae_none), "Single quoted");
    ae_double = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ae_single), "Double quoted");

    gtk_box_pack_start(GTK_BOX(box), ae_none, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), ae_single, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), ae_double, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_table_attach(GTK_TABLE(p_action), frame,
                     0, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 1, 1);

    a_confirm =
        gtk_check_button_new_with_label("Requires confirmation");
    gtk_table_attach(GTK_TABLE(p_action), a_confirm,
                     0, 1, 2, 3,
                     GTK_FILL | GTK_EXPAND, 0, 1, 1);

    frame = gtk_frame_new("Command");
    box = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(box),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    a_command = gtk_text_view_new ();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(a_command), FALSE);
    gtk_container_add(GTK_CONTAINER(box), a_command);
    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_table_attach(GTK_TABLE(p_action), frame,
                     0, 2, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                     1, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), p_action, NULL);
}


static GtkWidget *create_screen_editor()
{
    GtkWidget *m_box, *box, *label, *id_table, *frame;

    m_box = gtk_vbox_new(FALSE, 2);

    id_table = gtk_table_new(2, 2, FALSE);
    label = gtk_label_new("Screen ID:");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_table_attach(GTK_TABLE(id_table), label,
                     0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 0, 0);
    screen_id = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(id_table), screen_id,
                     1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 0, 0);
    label = gtk_label_new("Screen title:");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_table_attach(GTK_TABLE(id_table), label,
                     0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 0, 0);
    screen_title = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(id_table), screen_title,
                     1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(m_box), id_table, FALSE, FALSE, 0);

    fastpath = gtk_check_button_new_with_label("Allow fastpath");
    gtk_box_pack_start(GTK_BOX(m_box), fastpath, FALSE, FALSE, 0);

    frame = gtk_frame_new("Screen help text");
    box = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(box),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    screen_help = gtk_text_view_new ();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(screen_help), FALSE);
    gtk_container_add(GTK_CONTAINER(box), screen_help);
    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_box_pack_start(GTK_BOX(m_box), frame, TRUE, TRUE, 1);


    frame = gtk_frame_new("Screen type");
    box = gtk_hbox_new(TRUE, 2);
    r_menu = gtk_radio_button_new_with_label(NULL, "Menu Screen");
    r_dialog = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(r_menu), "Dialog Screen");
    r_hidden = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(r_dialog), "Hidden Screen");
    r_action = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(r_hidden), "Action Screen");

    gtk_box_pack_start(GTK_BOX(box), r_menu, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), r_dialog, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), r_hidden, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), r_action, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_box_pack_start(GTK_BOX(m_box), frame, FALSE, FALSE, 1);

    nb = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nb), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(nb), FALSE);

    create_menu_edit();
    create_dialog_edit();
    create_hidden_edit();
    create_action_edit();

    gtk_box_pack_start(GTK_BOX(m_box), nb, TRUE, TRUE, 0);

    /*
     * This requires the pages to have been built in the
     * create_*_edit() functions
     */
    g_signal_connect(G_OBJECT(r_menu), "clicked",
                     G_CALLBACK(change_screen_type), p_menu);
    g_signal_connect(G_OBJECT(r_dialog), "clicked",
                     G_CALLBACK(change_screen_type), p_dialog);
    g_signal_connect(G_OBJECT(r_hidden), "clicked",
                     G_CALLBACK(change_screen_type), p_hidden);
    g_signal_connect(G_OBJECT(r_action), "clicked",
                     G_CALLBACK(change_screen_type), p_action);

    return(m_box);
}


static gint load_xml(const gchar *filename)
{
    gint rc, have;
    xmlNodePtr cur;

    rc = -EINVAL;
    xmlDoValidityCheckingDefaultValue = 1;
    doc = xmlParseFile(filename);
    if (doc == NULL)
        return(rc);

    root = xmlDocGetRootElement(doc);
    if (root == NULL)
        goto err;

    ns = xmlSearchNsByHref(doc, root, RASTA_NAMESPACE);
    if (ns == NULL)
        goto err;
    
    if (xmlStrcmp(root->name, "RASTA") != 0)
        goto err;

    cur = root->children;
    have = 0;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "SCREENS") == 0)
            {
                if (cur->ns != ns)
                    goto err;
                else
                    have++;
            }
            else if (xmlStrcmp(cur->name, "PATH") == 0)
            {
                if (cur->ns != ns)
                    goto err;
                else
                    have++;
            }
        }
        cur = cur->next;
    }
    if (have == 2)
    {
        rc = 0;
        goto out;
    }

err:
    xmlFreeDoc(doc);

out:
    return(rc);
}


static xmlNodePtr find_screen(const gchar *screenname)
{
    xmlNodePtr cur;
    gchar *test_id;
    gint rc;

    g_return_val_if_fail(screenname != NULL, NULL);

    cur = root->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(cur->name, "SCREENS") == 0))
        {
            if (cur->ns != ns)
                return(NULL);
            else
                break;
        }
        cur = cur->next;
    }
    if (cur == NULL)
        return(NULL);

    cur = cur->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            ((xmlStrcmp(cur->name, "MENUSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "DIALOGSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "HIDDENSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "ACTIONSCREEN") == 0)))
        {
            test_id = xmlGetProp(cur, "ID");
            rc = xmlStrcmp(test_id, screenname);
            g_free(test_id);
            if (rc == 0)
                break;
        }
        cur = cur->next;
    }

    return(cur);
}


static gchar *query_help(xmlNodePtr node)
{
    gchar *help;
    
    g_return_val_if_fail(node != NULL, NULL);
    
    node = node->children;

    if (node == NULL)
        return(NULL);
    
    help = NULL;
    while (node != NULL)
    {
        if ((node->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(node->name, "HELP") == 0))
        {
            help = xmlNodeListGetString(doc, node->children, 1);
            break;
        }
        node = node->next;
    }
    
    return(help);
}


static void load_menu_screen(xmlNodePtr screen)
{
    /* Do nothing */
}


static void load_dialog_screen(xmlNodePtr screen)
{
    gchar *tmp;
    xmlNodePtr cur;
    GtkTextBuffer *buffer;

    tmp = xmlGetProp(screen, "ESCAPESTYLE");
    if (tmp == NULL)
        tmp = g_strdup("none");
    if (strcmp(tmp, "none") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(de_none), TRUE);
    else if (strcmp(tmp, "single") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(de_single),
                                     TRUE);
    else if (strcmp(tmp, "double") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(de_double),
                                     TRUE);
    else
        g_assert_not_reached();
    g_free(tmp);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(di_enabled), FALSE);

    df_store = gtk_rasta_list_store_new(screen);
    gtk_tree_view_set_model(GTK_TREE_VIEW(df_list),
                            GTK_TREE_MODEL(df_store));
    g_object_unref (G_OBJECT (df_store));

    cur = screen->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "INITCOMMAND") == 0)
            {
                tmp = xmlNodeListGetString(doc, cur->children, 1);
                if (tmp != NULL)
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(di_enabled), TRUE);
                    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(di_command));
                    gtk_text_buffer_set_text(buffer, tmp, -1);
                    g_free(tmp);
                }
            }
        }
        cur = cur->next;
    }
}


static void load_hidden_screen(xmlNodePtr screen)
{
    gchar *tmp;
    xmlNodePtr cur;
    GtkTextBuffer *buffer;

    tmp = xmlGetProp(screen, "ESCAPESTYLE");
    if (tmp == NULL)
        tmp = g_strdup("none");
    if (strcmp(tmp, "none") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(he_none), TRUE);
    else if (strcmp(tmp, "single") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(he_single),
                                     TRUE);
    else if (strcmp(tmp, "double") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(he_double),
                                     TRUE);
    else
        g_assert_not_reached();
    g_free(tmp);

    cur = screen->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "INITCOMMAND") == 0)
            {
                tmp = xmlNodeListGetString(doc, cur->children, 1);
                if (tmp != NULL)
                {
                    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hi_command));
                    gtk_text_buffer_set_text(buffer, tmp, -1);
                    g_free(tmp);
                }
            }
        }
        cur = cur->next;
    }
}


static void load_action_screen(xmlNodePtr screen)
{
    gchar *tmp;
    GtkTextBuffer *buffer;

    tmp = xmlGetProp(screen, "TTY");
    if (tmp == NULL)
        tmp = g_strdup("yes");
    if (strcmp(tmp, "yes") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(at_yes), TRUE);
    else if (strcmp(tmp, "no") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(at_no), TRUE);
    else if (strcmp(tmp, "self") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(at_self), TRUE);
    else
        g_assert_not_reached();
    g_free(tmp);

    tmp = xmlGetProp(screen, "ESCAPESTYLE");
    if (tmp == NULL)
        tmp = g_strdup("none");
    if (strcmp(tmp, "none") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ae_none), TRUE);
    else if (strcmp(tmp, "single") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ae_single),
                                     TRUE);
    else if (strcmp(tmp, "double") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ae_double),
                                     TRUE);
    else
        g_assert_not_reached();
    g_free(tmp);

    tmp = xmlGetProp(screen, "CONFIRM");
    if (tmp == NULL)
        tmp = g_strdup("false");
    if (strcmp(tmp, "true") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a_confirm),
                                     TRUE);
    else if (strcmp(tmp, "false") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a_confirm),
                                     FALSE);
    else
        g_assert_not_reached();
    g_free(tmp);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(a_command));
    tmp = xmlNodeListGetString(doc, screen->children, 1);
    gtk_text_buffer_set_text(buffer, tmp ? tmp : "", -1);
    g_free(tmp);
}


static guint load_screen(xmlNodePtr screen)
{
    gchar *tmp;
    GtkTextBuffer *buffer;

    g_return_val_if_fail(screen != NULL, FALSE);

    enable_initcommand(NULL, NULL);

    tmp = xmlGetProp(screen, "TEXT");
    gtk_entry_set_text(GTK_ENTRY(screen_title), tmp ? tmp : "");
    g_free(tmp);

    tmp = xmlGetProp(screen, "ID");
    gtk_entry_set_text(GTK_ENTRY(screen_id), tmp ? tmp : "");
    g_free(tmp);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(screen_help));
    tmp = query_help(screen);
    gtk_text_buffer_set_text(buffer, tmp ? tmp : "", -1);
    g_free(tmp);

    tmp = xmlGetProp(screen, "FASTPATH");
    if (tmp == NULL)
        tmp = g_strdup("true");
    if (xmlStrcmp(tmp, "false") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fastpath),
                                     FALSE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fastpath),
                                     TRUE);
    g_free(tmp);


    if (xmlStrcmp(screen->name, "MENUSCREEN") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r_menu), TRUE);
        load_menu_screen(screen);
    }
    else if (xmlStrcmp(screen->name, "DIALOGSCREEN") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r_dialog), TRUE);
        load_dialog_screen(screen);
    }
    else if (xmlStrcmp(screen->name, "HIDDENSCREEN") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r_hidden), TRUE);
        load_hidden_screen(screen);
    }
    else if (xmlStrcmp(screen->name, "ACTIONSCREEN") == 0)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r_action), TRUE);
        load_action_screen(screen);
    }
    else
        g_assert_not_reached();

    return(FALSE);
}


/*
 * Main program
 */


gint main(gint argc, gchar *argv[])
{
    gint rc;
    GtkWidget *m_box;
    xmlNodePtr screen;

    gtk_set_locale();
    gtk_init(&argc, &argv);

    if (argc < 3)
        print_usage(-EINVAL);

    rc = load_xml(argv[1]);
    if (rc < 0)
        print_usage(rc);


    top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(top), "RastaEdit");

    m_box = create_screen_editor();
    gtk_container_add(GTK_CONTAINER(top), m_box);

    gtk_widget_show_all(top);

    g_signal_connect(G_OBJECT(top), "destroy",
                     G_CALLBACK(do_quit), NULL);
    g_signal_connect_swapped(G_OBJECT(top), "delete_event",
                             G_CALLBACK(do_quit), NULL);

    screen = find_screen(argv[2]);
    if (screen == NULL)
    {
        fprintf(stderr, "Unable to find screen \"%s\"\n", argv[2]);
        exit(-ENOENT);
    }

    g_idle_add((GSourceFunc)load_screen, screen);

    gtk_main();

    xmlFreeDoc(doc);

    return(0);
}  /* main() */
