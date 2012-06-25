/*
 * gtkrasta.c
 *
 * GTK+ frontend for RASTA
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "rasta.h"


/*
 * Defines
 */
#define GTK_RASTA_ERROR gtk_rasta_error_quark()
#define NUM_READ_PAGES 4

#ifdef DEBUG
#define d_print g_print
#else
#define d_print(...) 
#endif  /* DEBUG */



/*
 * Typedefs
 */
typedef struct _GtkRastaContext GtkRastaContext;



/*
 * Enums
 */
typedef enum
{
    GTK_RASTA_ERROR_LOAD
} GtkRastaError;



/*
 * Structures
 */
struct _GtkRastaContext
{
    gchar *filename;
    gchar *fastpath;
    RastaContext *ctxt;
    GtkWidget *main_pane;
    GIOChannel *child_out_chan;
    GIOChannel *child_err_chan;
    gchar *child_out_data;
    gchar *child_err_data;
    pid_t child_pid;
    gint child_status;
    gpointer child_data;
};



/*
 * Prototypes
 */

/* Errors */
static GQuark gtk_rasta_error_quark();
static void print_usage(gint rc);
static void pop_error(GtkRastaContext *main_ctxt, const gchar *error);
static void pop_help(GtkRastaContext *main_ctxt, const gchar *help);
static gboolean pop_confirm(GtkRastaContext *main_ctxt,
                            const gchar *info);

/* UI creation */
static void create_menu_screen_panel(GtkWidget *nb);
static void create_dialog_screen_panel(GtkWidget *nb);
static void create_action_screen_panel(GtkWidget *nb);
static void create_screen_panel(GtkRastaContext *main_ctxt);
static void create_button_panel(GtkRastaContext *main_ctxt);
static void create_status_panel(GtkRastaContext *main_ctxt);

/* Enable and disable */
static void start_activity(GtkRastaContext *main_ctxt,
                           const gchar *info);
static void finish_activity(GtkRastaContext *main_ctxt);

/* Signal handlers */
static void do_menu_help(GtkWidget *widget, gpointer user_data);
static void do_dialog_help(GtkWidget *widget, gpointer user_data);
static void do_menu_item_toggled(GtkWidget *widget, gpointer user_data);
static void do_quit(GtkWidget *widget, gpointer user_data);
static void do_previous(GtkWidget *widget, gpointer user_data);
static void do_next(GtkWidget *widget, gpointer user_data);
static void do_cancel(GtkWidget *widget, gpointer user_data);
static void do_list(GtkWidget *widget, gpointer user_data);
static void do_browse(GtkWidget *widget, gpointer user_data);
static void do_file_selected(GtkWidget *widget, gpointer user_data);
static void do_numeric_changed(GtkWidget *widget, gpointer user_data);
static void do_numeric_insert_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer arg3, gpointer user_data);
static void do_numeric_delete_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer user_data);
#ifdef ENABLE_DEPRECATED
static void do_ip_addr_changed(GtkWidget *widget, gpointer user_data);
static void do_ip_addr_insert_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer arg3, gpointer user_data);
static void do_ip_addr_delete_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer user_data);
#endif  /* ENABLE_DEPRECATED */
static void do_ring_response(GtkWidget *widget,
                             gint response,
                             gpointer user_data);
static void do_list_response(GtkWidget *widget,
                             gint response,
                             gpointer user_data);


/* Initcommands */
static void ic_start(GtkRastaContext *main_ctxt);
static gboolean ic_timeout(gpointer user_data);

static gboolean ic_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);
static gboolean ic_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);

/* Listcommands */
static void lc_start(GtkRastaContext *main_ctxt,
                     RastaDialogField *field);
static gboolean lc_timeout(gpointer user_data);
static void lc_maybe_add(GtkRastaContext *main_ctxt,
                         GString *data,
                         gboolean done);
static gboolean lc_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);
static gboolean lc_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);
static void pop_choice_foreach(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer res);
static void pop_choice_dialog(GtkRastaContext *main_ctxt,
                              gboolean multiple);
static void add_choice_item(GtkRastaContext *main_ctxt,
                            const gchar *item,
                            gboolean selected);
static void pop_list_dialog(GtkRastaContext *main_ctxt,
                            GtkWidget *widget);
static void pop_ring_dialog(GtkRastaContext *main_ctxt,
                            GtkWidget *widget);
static gboolean ring_dialog_idle(gpointer user_data);

/* Actions */
static gint ac_start(GtkRastaContext *main_ctxt,
                     RastaScreen *screen);
static void ac_tag_output(GtkRastaContext *main_ctxt,
                          const gchar *stream);
static void ac_maybe_output(GtkRastaContext *main_ctxt,
                            GString *data,
                            gboolean done);
static gboolean ac_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);
static gboolean ac_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data);
static gboolean ac_timeout(gpointer user_data);

/* Initialization */
static gboolean init_in_func(GIOChannel *chan,
                             GIOCondition cond,
                             gpointer user_data);
static gboolean init_start_func(gpointer user_data);
static gboolean load_options(gint argc, gchar *argv[],
                             GtkRastaContext *main_ctxt);

/* Screen monkeying */
static void build_screen(GtkRastaContext *main_ctxt);
static gboolean build_menu_screen(GtkRastaContext *main_ctxt,
                                  RastaScreen *screen);
static gboolean build_dialog_screen(GtkRastaContext *main_ctxt,
                                    RastaScreen *screen);
static gboolean build_action_screen(GtkRastaContext *main_ctxt,
                                    RastaScreen *screen);
static void create_dialog_widget_readonly(GtkRastaContext *main_ctxt,
                                          GtkWidget *parent,
                                          RastaDialogField *field);
static void create_dialog_widget_entry(GtkRastaContext *main_ctxt,
                                       GtkWidget *parent,
                                       RastaDialogField *field);
static void create_dialog_widget_entrylist(GtkRastaContext *main_ctxt,
                                           GtkWidget *parent,
                                           RastaDialogField *field);
static void create_dialog_widget_file(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field);
static void create_dialog_widget_list(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field);
static void create_dialog_widget_ring(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field);
#ifdef ENABLE_DEPRECATED
static void attach_ip_handlers(GtkRastaContext *main_ctxt,
                               GtkWidget *widget);
static void create_dialog_widget_ip_addr(GtkRastaContext *main_ctxt,
                                         GtkWidget *parent,
                                         RastaDialogField *field);
static void create_dialog_widget_volume(GtkRastaContext *main_ctxt,
                                        GtkWidget *parent,
                                        RastaDialogField *field);
#endif  /* ENABLE_DEPRECATED */
static void do_menu_screen_next(GtkRastaContext *main_ctxt);
static void do_dialog_screen_next(GtkRastaContext *main_ctxt);



/*
 * Functions
 */


/*
 * static gboolean pop_confirm(GtkRastaContext *main_ctxt,
 *                             const gchar *info)
 *
 * Asks the user if they would like to continue.
 */
static gboolean pop_confirm(GtkRastaContext *main_ctxt,
                            const gchar *info)
{
    gint rc;
    GtkWidget *top, *message;

    g_assert(main_ctxt != NULL);

    /* TIMBOIZE!! */
    top = gtk_widget_get_toplevel(main_ctxt->main_pane);
    message = gtk_message_dialog_new(GTK_WINDOW(top),
                                     GTK_DIALOG_MODAL | 
#if 0  /* SEPARATORS ARE FUCKING UGLY */
                                     GTK_DIALOG_NO_SEPARATOR |
#endif
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_YES_NO,
                                     "%s",
                                     info != NULL ?
                                     info :
                                     "The action about to run may modify your system irrevocably.  Do you wish to continue?");
    rc = gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);

    return(rc == GTK_RESPONSE_YES);
}  /* pop_confirm() */


/*
 * static void pop_help(GtkRastaContext *main_ctxt, const gchar *help)
 *
 * Just pops up an help message.
 */
static void pop_help(GtkRastaContext *main_ctxt, const gchar *help)
{
    GtkWidget *top, *message;

    g_assert(main_ctxt != NULL);

    /* TIMBOIZE */
    top = gtk_widget_get_toplevel(main_ctxt->main_pane);
    message = gtk_message_dialog_new(GTK_WINDOW(top),
                                     GTK_DIALOG_MODAL | 
#if 0  /* SEPARATORS ARE FUCKING UGLY */
                                     GTK_DIALOG_NO_SEPARATOR |
#endif
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     "%s",
                                     help != NULL ?
                                     help :
                                     "There is no help available on this topic.");
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
}  /* pop_help() */


/*
 * static void pop_error(GtkRastaContext *main_ctxt, const gchar *error)
 *
 * Just pops up an error message.
 */
static void pop_error(GtkRastaContext *main_ctxt, const gchar *error)
{
    GtkWidget *top, *message;

    g_assert(main_ctxt != NULL);

    top = gtk_widget_get_toplevel(main_ctxt->main_pane);
    /* TIMBOIZE */
    message = gtk_message_dialog_new(GTK_WINDOW(top),
                                     GTK_DIALOG_MODAL | 
#if 0  /* SEPARATORS ARE FUCKING UGLY */
                                     GTK_DIALOG_NO_SEPARATOR |
#endif
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK,
                                     "%s",
                                     error != NULL ?
                                     error :
                                     "An error occurred.");
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
}  /* pop_error() */


#ifdef ENABLE_DEPRECATED
/*
 * static void do_ip_addr_changed(GtkWidget *widget, gpointer user_data)
 *
 * Handler for the "changed" signal on ip_addr entries.
 * AFAIK, all the work is in insert/delete_text.
 */
static void do_ip_addr_changed(GtkWidget *widget, gpointer user_data)
{
    /* Do nothing so far */
}  /* do_ip_addr_changed() */


/* static void do_ip_addr_insert_text(GtkWidget *widget,
 *                                    gpointer arg1, gpointer arg2,
 *                                    gpointer arg3, gpointer user_data)
 *
 * Handler for the "insert_text" signal on ip_addr entries.  It
 * prevents users from entering text, et al.
 * 
 * FIXME:  This is a strict check for ASCII arabic numerals on the
 * assumption that all IPs are written this way the world over.  It
 * also assumes that the user can input arabic numerals no matter
 * what their system configuration.  Let me know if this is a
 * broken assumption.
 */
static void do_ip_addr_insert_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer arg3, gpointer user_data)
{
    gboolean err;
    gint val, position, curlen, totlen;
    gchar *text, *curtext, *ptr, *cobbled;
    
    err = TRUE;
    cobbled = NULL;

    curtext = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    if (curtext == NULL)
        curtext = g_strdup("");

    text = (gchar *)arg1;
    if (g_utf8_validate(text, -1, NULL) == FALSE)
    {
        g_warning("g_utf8_validate failed in \"insert_text\"!");
        goto out;
    }

    position = *(gint *)arg3;

    curlen = g_utf8_strlen(curtext, -1);
    totlen = curlen + g_utf8_strlen(text, -1);
    if (totlen > 3)
        goto out;

    if (position == 0)
        cobbled = g_strconcat(text, curtext, NULL);
    else if (position >= curlen)
        cobbled = g_strconcat(curtext, text, NULL);
    else
    {
        /* should only be the case if curlen == 2 and position = 1 */
        g_assert((curlen == 2) && (position == 1));
        cobbled = g_strdup_printf("%.1s%s%.1s", curtext, text,
                                  g_utf8_find_next_char(curtext, NULL));
    }

    val = (gint)strtol(cobbled, &ptr, 10);
    if ((ptr != NULL) && (*ptr != '\0'))
        goto out;

    if ((val > 255) || (val < 0))
        goto out;

    err = FALSE;

out:
    g_free(curtext);
    g_free(cobbled);

    if (err != FALSE)
    {
        gdk_beep();
        g_signal_stop_emission_by_name(G_OBJECT(widget),
                                       "insert_text");
    }
}  /* do_ip_addr_insert_text() */


/*
 * static void do_ip_addr_delete_text(GtkWidget *widget,
 *                                    gpointer arg1, gpointer arg2,
 *                                    gpointer user_data)
 *
 * Handles the "delete_text" signal on ip_addr entries.  Doesn't
 * do anything right yet.
 */
static void do_ip_addr_delete_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer user_data)
{
    /* Does nothing so far */
}  /* do_ip_addr_delete_text() */
#endif  /* ENABLE_DEPRECATED */


/*
 * static void do_numeric_changed(GtkWidget *widget, gpointer user_data)
 *
 * Handler for the "changed" signal on numeric entries.
 * AFAIK, all the work is in insert/delete_text.
 */
static void do_numeric_changed(GtkWidget *widget, gpointer user_data)
{
    /* Do nothing so far */
}  /* do_numeric_changed() */


/* static void do_numeric_insert_text(GtkWidget *widget,
 *                                    gpointer arg1, gpointer arg2,
 *                                    gpointer arg3, gpointer user_data)
 *
 * Handler for the "insert_text" signal on numeric entries.  It
 * prevents users from entering text, et al.
 */
static void do_numeric_insert_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer arg3, gpointer user_data)
{
    gint position;
    gboolean err, done, have_sign, have_dp;
    gunichar c, dp, ns, ps;
    gchar *curtext, *text, *ptr;
    struct lconv *lc;
    
    err = TRUE;
    curtext = NULL;
    text = (gchar *)arg1;
    if (g_utf8_validate(text, -1, NULL) == FALSE)
    {
        g_warning("g_utf8_validate failed in \"insert_text\"!\n");
        goto out;
    }

    curtext = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    if (curtext == NULL)
        curtext = g_strdup("");
    if (g_utf8_validate(curtext, -1, NULL) == FALSE)
    {
        g_warning("g_utf8_validate failed on widget in  \"insert_text\"!\n");
        goto out;
    }

    position = *(gint *)arg3;

    lc = localeconv();
    if (lc->decimal_point)
    {
        ptr = g_locale_to_utf8(lc->decimal_point, -1, NULL, NULL, NULL);
        dp = g_utf8_get_char(ptr);
        g_free(ptr);
    }
    else
        dp = g_utf8_get_char(".");
   
    if (*(lc->negative_sign))
    {
        ptr = g_locale_to_utf8(lc->negative_sign, -1, NULL, NULL, NULL);
        ns = g_utf8_get_char(ptr);
        g_free(ptr);
    }
    else
        ns = g_utf8_get_char("-");
 
    if (*(lc->positive_sign))
    {
        ptr = g_locale_to_utf8(lc->positive_sign, -1, NULL, NULL, NULL);
        ps = g_utf8_get_char(ptr);
        g_free(ptr);
    }
    else
        ps = g_utf8_get_char("+");

    done = FALSE;
    have_sign = FALSE;
    have_dp = FALSE;
    ptr = curtext;
    while (done == FALSE)
    {
        if (*ptr != '\0')
        {
            c = g_utf8_get_char(ptr);
            if ((c == ns) || (c == ps))
                have_sign = TRUE;
            else if (c == dp)
                have_dp = TRUE;
            if (have_dp && have_sign)
                done = TRUE;
            ptr = g_utf8_next_char(ptr);
        }
        else
            done = TRUE;
    }

    done = FALSE;
    ptr = text;
    while (done == FALSE)
    {
        if (*ptr != '\0')
        {
            c = g_utf8_get_char(ptr);
            if (c == dp)
            {
                if (have_dp != FALSE)
                    goto out;
            }
            else if ((c == ns) || (c == ps))
            {
                if ((have_sign != FALSE) ||
                    (ptr != text) ||
                    (position != 0))
                    goto out;
            }
            else if (g_unichar_isdigit(c) == FALSE)
                goto out;
            ptr = g_utf8_next_char(ptr);
        }
        else
            done = TRUE;
    }

    err = FALSE;

out:
    g_free(curtext);

    if (err != FALSE)
    {
        gdk_beep();
        g_signal_stop_emission_by_name(G_OBJECT(widget),
                                       "insert_text");
    }
}  /* do_numeric_insert_text() */


/*
 * static void do_numeric_delete_text(GtkWidget *widget,
 *                                    gpointer arg1, gpointer arg2,
 *                                    gpointer user_data)
 *
 * Handles the "delete_text" signal on numeric entries.  Doesn't
 * do anything right yet.
 */
static void do_numeric_delete_text(GtkWidget *widget,
                                   gpointer arg1, gpointer arg2,
                                   gpointer user_data)
{
    /* Does nothing so far */
}  /* do_numeric_delete_text() */


/*
 * static void do_menu_help(GtkWidget *widget, gpointer user_data)
 *
 * Pops up a help menu
 */
static void do_menu_help(GtkWidget *widget, gpointer user_data)
{
    RastaMenuItem *item;
    GtkRastaContext *main_ctxt;
    gchar *help_text;

    g_assert(widget != NULL);
    g_assert(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;
    item = RASTA_MENU_ITEM(g_object_get_data(G_OBJECT(widget),
                                             "item"));
    g_assert(item != NULL);

    help_text = rasta_menu_item_get_help(item);
    pop_help(main_ctxt, help_text);
    g_free(help_text);
}  /* do_menu_help() */


/*
 * static void do_dialog_help(GtkWidget *widget, gpointer user_data)
 *
 * Pops up a help dialog
 */
static void do_dialog_help(GtkWidget *widget, gpointer user_data)
{
    RastaDialogField *field;
    GtkRastaContext *main_ctxt;
    gchar *help_text;

    g_assert(widget != NULL);
    g_assert(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;
    field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(widget),
                                                 "field"));
    g_assert(field != NULL);

    help_text = rasta_dialog_field_get_help(field);
    pop_help(main_ctxt, help_text);
    g_free(help_text);
}  /* do_dialog_help() */


/*
 * static void do_menu_item_toggled(GtkWidget *widget,
 *                                  gpointer user_data)
 *
 * Cheapo radio logic for toggle buttons
 */
static void do_menu_item_toggled(GtkWidget *widget, gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GtkWidget *tog_box, *tog_button;
    GtkWidget *menu, *screen_frame, *panes, *menu_pane;
    GList *buttons, *elem;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    menu_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "menu_pane"));
    g_assert(menu_pane != NULL);

    menu =
        GTK_WIDGET(g_object_get_data(G_OBJECT(menu_pane),
                                     "menu"));
    g_assert(menu != NULL);

    elem = buttons = gtk_container_get_children(GTK_CONTAINER(menu));
    while (elem != NULL)
    {
        tog_box = GTK_WIDGET(elem->data);
        g_assert(tog_box != NULL);
        tog_button = GTK_WIDGET(g_object_get_data(G_OBJECT(tog_box),
                                                  "button"));
        g_assert(tog_button != NULL);
        g_signal_handlers_block_by_func(G_OBJECT(tog_button),
                                        do_menu_item_toggled,
                                        main_ctxt);
        if (tog_button == widget)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tog_button),
                                         TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tog_button),
                                         FALSE);
        g_signal_handlers_unblock_by_func(G_OBJECT(tog_button),
                                          do_menu_item_toggled,
                                          main_ctxt);
        elem = g_list_next(elem);
    }
    g_list_free(buttons);
}  /* do_menu_item_toggled() */


/*
 * static void create_dialog_widget_readonly(GtkRastaContext *main_ctxt,
 *                                           GtkWidget *parent,
 *                                           RastaDialogField *field)
 *
 * Builds a widget for a dialog field
 */
static void create_dialog_widget_readonly(GtkRastaContext *main_ctxt,
                                          GtkWidget *parent,
                                          RastaDialogField *field)
                                 
{
    gchar *name, *value;
    GtkWidget *widget;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_ENTRY));
    value = rasta_symbol_lookup(main_ctxt->ctxt, name);
    g_free(name);
    widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget),
                       value ? value : "");
    g_free(value);
    gtk_editable_set_editable(GTK_EDITABLE(widget),
                              FALSE);
    gtk_widget_set_sensitive(widget, FALSE);
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);
}  /* create_dialog_widget_readonly() */


/*
 * static void create_dialog_widget_entry(GtkRastaContext *main_ctxt,
 *                                        GtkWidget *parent,
 *                                        RastaDialogField *field)
 *
 * Constructs a widget for entry fields
 */
static void create_dialog_widget_entry(GtkRastaContext *main_ctxt,
                                       GtkWidget *parent,
                                       RastaDialogField *field)
{
    guint length;
    gchar *name, *value;
    GtkWidget *widget;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_ENTRY));

    length = rasta_entry_dialog_field_get_length(field);
    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    widget = gtk_entry_new();
    if (rasta_entry_dialog_field_is_hidden(field) != FALSE)
        gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
    if (rasta_entry_dialog_field_is_numeric(field) != FALSE)
    {
        g_signal_connect(G_OBJECT(widget), "changed",
                         G_CALLBACK(do_numeric_changed),
                         main_ctxt);
        g_signal_connect(G_OBJECT(widget), "insert_text",
                         G_CALLBACK(do_numeric_insert_text),
                         main_ctxt);
        g_signal_connect(G_OBJECT(widget), "delete_text",
                         G_CALLBACK(do_numeric_delete_text),
                         main_ctxt);
    }
    else
    {
        length = rasta_entry_dialog_field_get_length(field);
        if (length > 0)
            gtk_entry_set_max_length(GTK_ENTRY(widget), length);
    }
    gtk_entry_set_text(GTK_ENTRY(widget), value ? value : "");
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);
}  /* create_dialog_widget_entry() */


/*
 * static void create_dialog_widget_entrylist(GtkRastaContext *main_ctxt,
 *                                            GtkWidget *parent,
 *                                            RastaDialogField *field)
 *
 * Creates a widget structure for entrylists.
 */
static void create_dialog_widget_entrylist(GtkRastaContext *main_ctxt,
                                           GtkWidget *parent,
                                           RastaDialogField *field)
{ 
    guint length;
    gchar *name, *value;
    GtkWidget *widget, *image;
    GtkTooltips *tips;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_ENTRYLIST));

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    widget = gtk_entry_new();
    if (rasta_entry_dialog_field_is_numeric(field) != FALSE)
    {
        g_signal_connect(G_OBJECT(widget), "changed",
                         G_CALLBACK(do_numeric_changed),
                         main_ctxt);
        g_signal_connect(G_OBJECT(widget), "insert_text",
                         G_CALLBACK(do_numeric_insert_text),
                         main_ctxt);
        g_signal_connect(G_OBJECT(widget), "delete_text",
                         G_CALLBACK(do_numeric_delete_text),
                         main_ctxt);
    }
    else
    {
        length = rasta_entry_dialog_field_get_length(field);
        if (length > 0)
            gtk_entry_set_max_length(GTK_ENTRY(widget), length);
    }
    gtk_entry_set_text(GTK_ENTRY(widget), value ? value : "");
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);

    widget = gtk_button_new();
    tips = 
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);
    gtk_tooltips_set_tip(tips, widget, "Display choices", NULL);
    image = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN,
                                     GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(widget), image);
    g_object_set_data(G_OBJECT(widget), "parent", parent);
    g_signal_connect(G_OBJECT(widget), "clicked",
                     G_CALLBACK(do_list), main_ctxt);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
}  /* create_dialog_widget_entrylist() */


/*
 * static void create_dialog_widget_file(GtkRastaContext *main_ctxt,
 *                                       GtkWidget *parent,
 *                                       RastaDialogField *field)
 *
 * Creates a widget for file searching, with a nice browse button
 */
static void create_dialog_widget_file(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field)
{
    gchar *name, *value;
    GtkWidget *widget, *image;
    GtkTooltips *tips;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                        GINT_TO_POINTER(RASTA_FIELD_FILE));

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget), value ? value : "");
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);

    widget = gtk_button_new();
    tips = 
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);
    gtk_tooltips_set_tip(tips, widget, "Browse files", NULL);
    image = gtk_image_new_from_stock(GTK_STOCK_FIND,
                                     GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(widget), image);
    g_object_set_data(G_OBJECT(widget), "parent", parent);
    g_signal_connect(G_OBJECT(widget), "clicked",
                     G_CALLBACK(do_browse), main_ctxt);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
}  /* create_dialog_widget_file() */


/* static void create_dialog_widget_list(GtkRastaContext *main_ctxt,
 *                                       GtkWidget *parent,
 *                                       RastaDialogField *field)
 *
 * Constructs a widget for list entries, with a pop up button 
 * to see the choices.
 */
static void create_dialog_widget_list(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field)
{
    gchar *name, *value;
    GtkWidget *widget, *image;
    GtkTooltips *tips;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_LIST));

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget), value ? value : "");
    gtk_editable_set_editable(GTK_EDITABLE(widget), FALSE);
    gtk_widget_set_sensitive(widget, FALSE);
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);

    widget = gtk_button_new();
    tips = 
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);
    gtk_tooltips_set_tip(tips, widget, "Display choices", NULL);
    image = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN,
                                     GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(widget), image);
    g_object_set_data(G_OBJECT(widget), "parent", parent);
    g_signal_connect(G_OBJECT(widget), "clicked",
                     G_CALLBACK(do_list), main_ctxt);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
}  /* create_dialog_widget_list() */


/*
 * static void create_dialog_widget_ring(GtkRastaContext *main_ctxt,
 *                                       GtkWidget *parent,
 *                                       RastaDialogField *field)
 *
 * Constructs a widget for ring entries, with a pop up button 
 * to see the choices.
 */
static void create_dialog_widget_ring(GtkRastaContext *main_ctxt,
                                      GtkWidget *parent,
                                      RastaDialogField *field)
{
    gchar *name, *value, *r_value, *text;
    GtkWidget *widget, *image;
    REnumeration *er;
    RastaRingValue *r_item;
    GtkTooltips *tips;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_RING));

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    widget = gtk_entry_new();
    
    if ((value != NULL) && (value[0] != '\0'))
    {
        text = NULL;
        er = rasta_dialog_field_enumerate_ring(field);
        while (r_enumeration_has_more(er))
        {
            r_item = RASTA_RING_VALUE(r_enumeration_get_next(er));
            g_assert(r_item != NULL);
            r_value = rasta_ring_value_get_value(r_item);
            if (strcmp(r_value, value) == 0)
            {
                g_free(r_value);
                text = rasta_ring_value_get_text(r_item);
                break;
            }
        }
        r_enumeration_free(er);
        gtk_entry_set_text(GTK_ENTRY(widget), text ? text : "");
    }
    else
        gtk_entry_set_text(GTK_ENTRY(widget), "");
    g_free(value);
    gtk_editable_set_editable(GTK_EDITABLE(widget), FALSE);
    gtk_widget_set_sensitive(widget, FALSE);
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "entry", widget);

    widget = gtk_button_new();
    tips = 
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);
    gtk_tooltips_set_tip(tips, widget, "Display choices", NULL);
    image = gtk_image_new_from_stock(GTK_STOCK_GO_DOWN,
                                     GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(widget), image);
    g_object_set_data(G_OBJECT(widget), "parent", parent);
    g_signal_connect(G_OBJECT(widget), "clicked",
                     G_CALLBACK(do_list), main_ctxt);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
}  /* create_dialog_widget_ring() */


#ifdef ENABLE_DEPRECATED
/*
 * static void attach_ip_handlers(GtkRastaContext *main_ctxt,
 *                                GtkWidget *widget)
 *
 * Simply a convenience to attach the handlers
 */
static void attach_ip_handlers(GtkRastaContext *main_ctxt,
                               GtkWidget *widget)
{
    g_signal_connect(G_OBJECT(widget), "changed",
                     G_CALLBACK(do_ip_addr_changed),
                     main_ctxt);
    g_signal_connect(G_OBJECT(widget), "insert_text",
                     G_CALLBACK(do_ip_addr_insert_text),
                     main_ctxt);
    g_signal_connect(G_OBJECT(widget), "delete_text",
                     G_CALLBACK(do_ip_addr_delete_text),
                     main_ctxt);
}  /* attach_ip_handlers() */


/*
 * static void create_dialog_widget_ip_addr(GtkRastaContext *main_ctxt,
 *                                          GtkWidget *parent,
 *                                          RastaDialogField *field)
 *
 * Creates a widget to enter IP addresses.
 * NOTE:  This is a NIC specific feature,a nd may be #ifdef'd out
 * in the future.  Though it *is* kinda useful...
 */
static void create_dialog_widget_ip_addr(GtkRastaContext *main_ctxt,
                                         GtkWidget *parent,
                                         RastaDialogField *field)
{
    gint i;
    GtkWidget *widget;
    gchar *name, *value;
    gchar **octets;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_IPADDR));

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);
    if (value != NULL)
    {
        /* FIXME: probably could use better input validation */
        octets = g_strsplit(value, ".", 0);
        g_free(value);
        for (i = 0; (octets[i] != NULL) && (i < 4); i++);
        if (i < 4)
        {
            /* Not enough octets */
            g_strfreev(octets);
            octets = NULL;
        }
    }
    else
        octets = NULL;

    widget = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(widget), 3);
    gtk_entry_set_width_chars(GTK_ENTRY(widget), 3);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
    g_object_set_data(G_OBJECT(parent), "ip0", widget);
    attach_ip_handlers(main_ctxt, widget);
    if (octets != NULL)
        gtk_entry_set_text(GTK_ENTRY(widget), octets[0]);

    widget = gtk_label_new(".");
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);

    widget = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(widget), 3);
    gtk_entry_set_max_length(GTK_ENTRY(widget), 3);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
    g_object_set_data(G_OBJECT(parent), "ip1", widget);
    if (octets != NULL)
        gtk_entry_set_text(GTK_ENTRY(widget), octets[1]);
    attach_ip_handlers(main_ctxt, widget);

    widget = gtk_label_new(".");
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);

    widget = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(widget), 3);
    gtk_entry_set_max_length(GTK_ENTRY(widget), 3);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
    g_object_set_data(G_OBJECT(parent), "ip2", widget);
    if (octets != NULL)
        gtk_entry_set_text(GTK_ENTRY(widget), octets[2]);
    attach_ip_handlers(main_ctxt, widget);

    widget = gtk_label_new(".");
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);

    widget = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(widget), 3);
    gtk_entry_set_max_length(GTK_ENTRY(widget), 3);
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
    g_object_set_data(G_OBJECT(parent), "ip3", widget);
    if (octets != NULL)
        gtk_entry_set_text(GTK_ENTRY(widget), octets[3]);
    attach_ip_handlers(main_ctxt, widget);
}  /* create_dialog_widget_ip_addr() */


/*
 * static void create_dialog_widget_volume(GtkRastaContext *main_ctxt,
 *                                         GtkWidget *parent,
 *                                         RastaDialogField *field)
 *
 * Creates a slider widget to select volume level.
 * NOTE:  This is a NIC specific feature, and will likely be #ifdef'd
 * out in the future.  A generic range might be valueable, but we
 * already have a ton of field types...
 */
static void create_dialog_widget_volume(GtkRastaContext *main_ctxt,
                                        GtkWidget *parent,
                                        RastaDialogField *field)
{
    gdouble curvol;
    gchar *name, *value, *ptr;
    GtkObject *adjustment;
    GtkWidget *widget;

    name = rasta_dialog_field_get_symbol_name(field);
    g_object_set_data(G_OBJECT(parent), "name", name);
    g_object_set_data(G_OBJECT(parent), "type",
                      GINT_TO_POINTER(RASTA_FIELD_VOLUME));

    /* TIMBOIZE - no, ignore it because it is deprecated */
    widget = gtk_label_new("min");
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);

    value = rasta_symbol_lookup(main_ctxt->ctxt, name);

    if (value == NULL)
        value = g_strdup("0");
    curvol = strtod(value, &ptr);
    if ((ptr == NULL) || (*ptr != '\0'))
    {
        /* FIXME: Error? If so, how? */
        curvol = 5;
    }
    g_free(value);

    adjustment = gtk_adjustment_new(curvol, 0, 11, 1, 1, 1);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
    gtk_scale_set_digits(GTK_SCALE(widget), 0);
    gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(widget),
                                GTK_UPDATE_DELAYED);
    gtk_box_pack_start(GTK_BOX(parent), widget, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(parent), "range", widget);

    /* TIMBOIZE - no, ignore it because it is deprecated */
    widget = gtk_label_new("max");
    gtk_box_pack_start(GTK_BOX(parent), widget, FALSE, FALSE, 2);
}  /* create_dialog_widget_volume() */
#endif  /* ENABLE_DEPRECATED */


/*
 * static gboolean build_action_screen(GtkRastaContext *main_ctxt,
 *                                     RastaScreen *screen)
 *
 * Builds an action screen and fires off the action command.
 * Returns false if there was a problem.
 */
static gboolean build_action_screen(GtkRastaContext *main_ctxt,
                                    RastaScreen *screen)
{
    gint page_num, rc;
    GtkWidget *screen_frame, *panes, *action_pane;
    GtkWidget *status, *stdo, *stde, *outp;
    GtkTextBuffer *buf;
    GtkTextIter start, end;

    g_assert(main_ctxt != NULL);
    g_assert(screen != NULL);

    if (rasta_action_screen_needs_confirm(screen) != FALSE)
    {
        if (pop_confirm(main_ctxt, NULL) == FALSE)
            return(FALSE);
    }

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    action_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "action_pane"));
    g_assert(action_pane != NULL);

    status =
        GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                     "status"));
    g_assert(status != NULL);
    /* TIMBOIZE */
    gtk_label_set_text(GTK_LABEL(status), "Status: Starting");

    stdo =
        GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                     "stdout"));
    g_assert(stdo != NULL);
    /* TIMBOIZE */
    gtk_label_set_text(GTK_LABEL(stdo), "Stdout: No");

    stde =
        GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                     "stderr"));
    g_assert(stde != NULL);
    /* TIMBOIZE */
    gtk_label_set_text(GTK_LABEL(stde), "Stderr: No");

    outp =
        GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                     "output"));
    g_assert(outp != NULL);
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(outp));
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_delete(buf, &start, &end);
    gtk_text_buffer_set_modified(buf, FALSE);

    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(panes), action_pane);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panes), page_num);
    gtk_widget_show_all(panes);

    /* Make the textarea senstive, but leave the busy indicator */
    gtk_widget_set_sensitive(screen_frame, TRUE);

    rc = ac_start(main_ctxt, screen);
    if (rc == -ECHILD)
    {
        /* TIMBOIZE */
        gtk_label_set_text(GTK_LABEL(status), "Status: Failed");
        finish_activity(main_ctxt);
        return(TRUE);
    }
    else if (rc == -ENOEXEC)
        return(FALSE);

    gtk_label_set_text(GTK_LABEL(status), "Status: Running");

    status = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                          "status"));
    gtk_label_set_text(GTK_LABEL(status), NULL);

    return(TRUE);
}  /* build_action_screen() */


/*
 * static gboolean build_dialog_screen(GtkRastaContext *main_ctxt,
 *                                     RastaScreen *screen)
 *
 * Builds a dialog screen.  Returns false if there is a problem.
 */
static gboolean build_dialog_screen(GtkRastaContext *main_ctxt,
                                    RastaScreen *screen)
{
    GList *children, *elem, *field_widgets;
    GtkWidget *screen_frame, *panes, *dialog_pane, *dialog, *ditem;
    GtkWidget *h_button, *h_image;
    GtkWidget *tbox, *tlabel;
    GtkWidget *wbox, *r_image;
    GtkTooltips *tips;
    gint page_num, row;
    REnumeration *en;
    RastaDialogField *field;
    RastaDialogFieldType type;
    gchar *text, *name;

    g_assert(main_ctxt != NULL);
    g_assert(screen != NULL);

    tips =
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    dialog_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "dialog_pane"));
    g_assert(dialog_pane != NULL);

    dialog =
        GTK_WIDGET(g_object_get_data(G_OBJECT(dialog_pane),
                                     "dialog"));
    g_assert(dialog != NULL);

    field_widgets = (GList *)
        g_object_get_data(G_OBJECT(dialog), "field_widgets");
    g_list_free(field_widgets);

    elem = children = gtk_container_get_children(GTK_CONTAINER(dialog));
    while (elem != NULL)
    {
        ditem = GTK_WIDGET(elem->data);
        g_assert(ditem != NULL);
        name = (gchar *)g_object_get_data(G_OBJECT(ditem), "name");
        g_free(name);
        gtk_container_remove(GTK_CONTAINER(dialog), ditem);
        elem = g_list_next(elem);
    }
    g_list_free(children);
    gtk_table_resize(GTK_TABLE(dialog), 1, 5);

    en = rasta_dialog_screen_enumerate_fields(main_ctxt->ctxt, screen);
    if (r_enumeration_has_more(en) == FALSE)
    {
        r_enumeration_free(en);
        /* TIMBOIZE */
        pop_error(main_ctxt, "There are no items of this type.");
        return(FALSE);
    }

    row = 0;
    field_widgets = NULL;
    while (r_enumeration_has_more(en))
    {
        field = RASTA_DIALOG_FIELD(r_enumeration_get_next(en));
        g_assert(field != NULL);

        text = rasta_dialog_field_get_text(field);
        tbox = gtk_hbox_new(FALSE, 2);
        tlabel = gtk_label_new(text);
        gtk_label_set_line_wrap(GTK_LABEL(tlabel), TRUE);
        g_free(text);
        gtk_label_set_justify(GTK_LABEL(tlabel), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start(GTK_BOX(tbox), tlabel, FALSE, FALSE, 2);

        type = rasta_dialog_field_get_type(field);
        if (type == RASTA_FIELD_DESCRIPTION)
        {
                gtk_table_attach(GTK_TABLE(dialog), tbox,
                                 1, 4, row, row + 1,
                                 GTK_FILL | GTK_EXPAND, 0, 5, 5);
        }
        else
        {
            wbox = gtk_hbox_new(FALSE, 2);
            g_object_set_data(G_OBJECT(wbox), "field", field);
            switch (type)
            {
                case RASTA_FIELD_DESCRIPTION:
                    g_assert_not_reached();
                    
                case RASTA_FIELD_READONLY:
                    create_dialog_widget_readonly(main_ctxt,
                                                  wbox,
                                                  field);
                    break;

                case RASTA_FIELD_ENTRY:
                    create_dialog_widget_entry(main_ctxt,
                                               wbox,
                                               field);
                    break;

                case RASTA_FIELD_FILE:
                    create_dialog_widget_file(main_ctxt,
                                              wbox,
                                              field);
                    break;

                case RASTA_FIELD_ENTRYLIST:
                    create_dialog_widget_entrylist(main_ctxt,
                                                   wbox,
                                                   field);
                    break;

                case RASTA_FIELD_RING:
                    create_dialog_widget_ring(main_ctxt,
                                              wbox,
                                              field);
                    break;

                case RASTA_FIELD_LIST:
                    create_dialog_widget_list(main_ctxt,
                                              wbox,
                                              field);
                    break;

#ifdef ENABLE_DEPRECATED
                case RASTA_FIELD_IPADDR:
                    create_dialog_widget_ip_addr(main_ctxt,
                                                 wbox,
                                                 field);
                    break;

                case RASTA_FIELD_VOLUME:
                    create_dialog_widget_volume(main_ctxt,
                                                wbox,
                                                field);
                    break;
#endif /* ENABLE_DEPRECATED */

                case RASTA_FIELD_NONE:
                default:
                    g_assert_not_reached();
                    break;
            }

            if (rasta_dialog_field_is_required(field))
            {
                r_image = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO,
                                                   GTK_ICON_SIZE_MENU);
                gtk_table_attach(GTK_TABLE(dialog), r_image,
                                 0, 1, row, row + 1,
                                 0, 0, 2, 5);
            }
            gtk_table_attach(GTK_TABLE(dialog), tbox,
                             1, 2, row, row + 1,
                             GTK_FILL | GTK_EXPAND, 0, 5, 5);
            gtk_table_attach(GTK_TABLE(dialog), wbox,
                             2, 3, row, row + 1,
                             GTK_FILL | GTK_EXPAND, 0, 5, 5);
            field_widgets = g_list_prepend(field_widgets, wbox);

            h_image = gtk_image_new_from_stock(GTK_STOCK_HELP,
                                               GTK_ICON_SIZE_MENU);
            h_button = gtk_button_new();
            gtk_tooltips_set_tip(tips, GTK_WIDGET(h_button),
                                 "What's this?", NULL);
            gtk_button_set_relief(GTK_BUTTON(h_button),
                                  GTK_RELIEF_NONE);
            gtk_container_add(GTK_CONTAINER(h_button), h_image);
            g_object_set_data(G_OBJECT(h_button), "field", field);
            g_signal_connect(G_OBJECT(h_button), "clicked",
                             G_CALLBACK(do_dialog_help),
                             main_ctxt);
            gtk_table_attach(GTK_TABLE(dialog),
                             gtk_vseparator_new(),
                             3, 4, row, row + 1,
                             0, GTK_FILL, 2, 5);
            gtk_table_attach(GTK_TABLE(dialog), h_button,
                             4, 5, row, row + 1,
                             0, 0, 2, 5);
        }
        row++;
    }

    field_widgets = g_list_reverse(field_widgets);
    g_object_set_data(G_OBJECT(dialog), "field_widgets",
                      field_widgets);

    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(panes), dialog_pane);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panes), page_num);
    gtk_widget_show_all(panes);
    finish_activity(main_ctxt);

    return(TRUE);
}  /* build_dialog_screen() */


/*
 * static gboolean build_menu_screen(GtkRastaContext *main_ctxt,
 *                                   RastaScreen *screen)
 *
 * Builds a menu screen.  Returns FALSE if there is a problem.
 */
static gboolean build_menu_screen(GtkRastaContext *main_ctxt,
                                  RastaScreen *screen)
{
    GtkWidget *tog_button, *menu, *screen_frame, *panes, *menu_pane;
    GtkWidget *tog_box, *h_button, *h_image;
    GtkTooltips *tips;
    REnumeration *en;
    RastaMenuItem *item;
    GList *buttons, *elem;
    gchar *item_text, *item_id;
    gint page_num;

    g_assert(main_ctxt != NULL);
    g_assert(screen != NULL);

    tips =
        GTK_TOOLTIPS(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                       "tips"));
    g_assert(tips != NULL);

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    menu_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "menu_pane"));
    g_assert(menu_pane != NULL);

    menu =
        GTK_WIDGET(g_object_get_data(G_OBJECT(menu_pane),
                                     "menu"));
    g_assert(menu != NULL);

    en = rasta_menu_screen_enumerate_items(main_ctxt->ctxt, screen);
    if (r_enumeration_has_more(en) == FALSE)
    {
        r_enumeration_free(en);
        /* TIMBOIZE */
        pop_error(main_ctxt, "There are no items of this type.");
        return(FALSE);
    }

    elem = buttons = gtk_container_get_children(GTK_CONTAINER(menu));
    while (r_enumeration_has_more(en))
    {
        item = RASTA_MENU_ITEM(r_enumeration_get_next(en));
        g_assert(item != NULL);
        item_text = rasta_menu_item_get_text(item);
        item_id = rasta_menu_item_get_id(item);
        g_assert((item_text != NULL) && (item_id != NULL));

        if (elem != NULL)
        {
            tog_box = GTK_WIDGET(elem->data);
            g_assert(tog_box != NULL);
            tog_button = GTK_WIDGET(g_object_get_data(G_OBJECT(tog_box), "button"));
            h_button = GTK_WIDGET(g_object_get_data(G_OBJECT(tog_box), "help"));
            gtk_label_set_text(GTK_LABEL(GTK_BIN(tog_button)->child),
                               item_text);
            g_object_set_data(G_OBJECT(h_button), "item", item);
            elem = g_list_next(elem);
        }
        else
        {
            tog_box = gtk_hbox_new(FALSE, 2);
            tog_button = gtk_toggle_button_new_with_label(item_text);
            gtk_button_set_relief(GTK_BUTTON(tog_button),
                                  GTK_RELIEF_NONE);
            gtk_label_set_justify(GTK_LABEL(GTK_BIN(tog_button)->child),
                                  GTK_JUSTIFY_LEFT);
            g_signal_connect(G_OBJECT(tog_button), "clicked",
                             G_CALLBACK(do_menu_item_toggled),
                             main_ctxt);
            gtk_box_pack_start(GTK_BOX(tog_box), tog_button,
                               TRUE, TRUE, 2);
            g_object_set_data(G_OBJECT(tog_box), "button",
                              tog_button);

            gtk_box_pack_start(GTK_BOX(tog_box),
                               gtk_vseparator_new(),
                               FALSE, FALSE, 2);

            h_image = gtk_image_new_from_stock(GTK_STOCK_HELP,
                                               GTK_ICON_SIZE_MENU);
            h_button = gtk_button_new();
            gtk_tooltips_set_tip(tips, GTK_WIDGET(h_button),
                                 "What's this?", NULL);
            gtk_button_set_relief(GTK_BUTTON(h_button),
                                  GTK_RELIEF_NONE);
            gtk_container_add(GTK_CONTAINER(h_button), h_image);
            g_object_set_data(G_OBJECT(h_button), "item", item);
            g_signal_connect(G_OBJECT(h_button), "clicked",
                             G_CALLBACK(do_menu_help),
                             main_ctxt);
            gtk_box_pack_start(GTK_BOX(tog_box), h_button,
                               FALSE, FALSE, 2);
            g_object_set_data(G_OBJECT(tog_box), "help",
                              h_button);

            gtk_box_pack_start(GTK_BOX(menu), tog_box,
                               FALSE, FALSE, 2);
        }
        g_free(item_text);
        g_object_set_data(G_OBJECT(tog_button), "next_id",
                          item_id);
        g_signal_handlers_block_by_func(G_OBJECT(tog_button),
                                        do_menu_item_toggled,
                                        main_ctxt);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tog_button),
                                     FALSE);
        g_signal_handlers_unblock_by_func(G_OBJECT(tog_button),
                                          do_menu_item_toggled,
                                          main_ctxt);
    }
    r_enumeration_free(en);

    /* Remove extra items */
    while (elem != NULL)
    {
        tog_button = GTK_WIDGET(elem->data);
        g_assert(tog_button != NULL);
        gtk_container_remove(GTK_CONTAINER(menu), tog_button);
        elem = g_list_next(elem);
    }
    g_list_free(buttons);

    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(panes),
                                     menu_pane);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panes), page_num);
    gtk_widget_show_all(panes);
    finish_activity(main_ctxt);

    return(TRUE);
}  /* build_menu_screen() */


/*
 * static void build_screen(GtkRastaContext *main_ctxt)
 *
 * This is the main screen handling function.  It builds all screens.
 * In the case of HIDDEN screens or DIALOG screens that require an
 * INITCOMMAND, it starts the INITCOMMAND off.
 */
static void build_screen(GtkRastaContext *main_ctxt)
{
    gchar *title;
    RastaScreen *screen;
    GtkWidget *screen_frame;
    gboolean done;

    g_assert(main_ctxt != NULL);
    g_assert(main_ctxt->ctxt != NULL);

    do
    {
        done = TRUE;
        screen = rasta_context_get_screen(main_ctxt->ctxt);
        if ((screen == NULL) ||
            (rasta_screen_get_type(screen) == RASTA_SCREEN_NONE))
        {
            /* TIMBOIZE */
            pop_error(main_ctxt, "There are no items of this type");
            return;
        }
    
        title = rasta_screen_get_title(screen);
        
        screen_frame =
            GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                         "screens"));
    
        switch (rasta_screen_get_type(screen))
        {
            case RASTA_SCREEN_MENU:
                gtk_frame_set_label(GTK_FRAME(screen_frame), title);
                if (build_menu_screen(main_ctxt, screen) == FALSE)
                {
                    done = FALSE;
                    rasta_screen_previous(main_ctxt->ctxt);
                }
                break;
    
            case RASTA_SCREEN_DIALOG:
            case RASTA_SCREEN_HIDDEN:
                if (rasta_initcommand_is_required(main_ctxt->ctxt, screen))
                {
                    ic_start(main_ctxt);
                    break;
                }
                if (rasta_screen_get_type(screen) == RASTA_SCREEN_DIALOG)
                {
                    gtk_frame_set_label(GTK_FRAME(screen_frame), title);
                    if (build_dialog_screen(main_ctxt, screen) == FALSE)
                    {
                        done = FALSE;
                        rasta_screen_previous(main_ctxt->ctxt);
                    }
                }
                else if (rasta_screen_get_type(screen) == RASTA_SCREEN_HIDDEN)
                {
                    rasta_hidden_screen_next(main_ctxt->ctxt);
                    done = FALSE;
                }
                else
                    g_assert_not_reached();
                break;
    
            case RASTA_SCREEN_ACTION:
                gtk_frame_set_label(GTK_FRAME(screen_frame), title);
                if (build_action_screen(main_ctxt, screen) == FALSE)
                {
                    done = FALSE;
                    rasta_screen_previous(main_ctxt->ctxt);
                }
                break;
    
            default:
                g_assert_not_reached();
        }

        g_free(title);
    }
    while (done == FALSE);
}  /* build_screen() */


/*
 * static GQuark gtk_rasta_error_quark()
 *
 * Returns the quark for this error domain
 */
static GQuark gtk_rasta_error_quark()
{
    static GQuark q = 0;
    if (q == 0)
        q = g_quark_from_static_string("gtk-rasta-error-quark");

    return(q);
}  /* gtk_rasta_error_quark() */


/*
 * static void print_usage(gint rc)
 *
 * Prints the usage statement, then exits with the given return code.
 * If rc is 0, it uses stdout, otherwise it uses stderr
 */
static void print_usage(gint rc)
{
    FILE *out;

    out = rc ? stderr : stdout;
    fprintf(out, "Usage: gtkrasta [--file <filename>] [<fastpath>]\n"
                 "       gtkrasta --help\n");
    exit(rc);
}  /* print_usage() */


/*
 * static gboolean init_start_func(gpointer user_data)
 *
 * Starts the processing of the XML template
 */
static gboolean init_start_func(gpointer user_data)
{
    GIOChannel *chan;
    GError *err;
    gchar *etext;
    GtkRastaContext *main_ctxt;

    g_assert(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;

    if (main_ctxt->filename == NULL)
        main_ctxt->filename = g_build_filename(_RASTA_DIR,
                                               RASTA_SYSTEM_FILE,
                                               NULL);

    g_assert(main_ctxt->filename != NULL);

    main_ctxt->ctxt = rasta_context_init_push(main_ctxt->filename,
                                              main_ctxt->fastpath);

    if (main_ctxt->ctxt != NULL)
    {
        err = NULL;
        chan = g_io_channel_new_file(main_ctxt->filename, "r", &err);
        if (chan != NULL)
        {
            g_io_channel_set_flags(chan, G_IO_FLAG_NONBLOCK, NULL);
            g_io_channel_set_encoding(chan, NULL, NULL);
            g_io_add_watch_full(chan, G_PRIORITY_LOW,
                                G_IO_IN | G_IO_ERR | G_IO_HUP, 
                                init_in_func, main_ctxt, NULL);
            g_io_channel_unref(chan);
            return(FALSE);
        }
    }

    /* TIMBOIZE */
    etext = g_strdup_printf("Unable to load screen descriptions from\n"
                            "\"%s\": %s",
                            main_ctxt->filename,
                            err->message);
    g_error_free(err);
    pop_error(main_ctxt, etext);
    g_free(etext);
    gtk_main_quit();

    return(FALSE);
}  /* init_start_func() */


/*
 * static gboolean init_in_func(GIOChannel *chan,
 *                              GIOCondition cond,
 *                              gpointer user_data)
 *
 * Callback to handle input data from the XML template
 */
static gboolean init_in_func(GIOChannel *chan,
                             GIOCondition cond,
                             gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gint ret;
    gsize bytes_read;
    gboolean cont;
    gchar *etext;
    GError *err;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            ret = rasta_context_parse_chunk(main_ctxt->ctxt, buffer,
                                            bytes_read);
            if (ret != 0)
            {
                /* TIMBOIZE */
                pop_error(main_ctxt,
                          "Error loading screen definitions!");
                exit(1);
                cont = FALSE;
            }
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            cont = FALSE;
            break;
    }

    if ((cont == FALSE) && (err == NULL))
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        ret = rasta_context_parse_chunk(main_ctxt->ctxt, NULL, 0);
        if (ret != 0)
        {
            /* TIMBOIZE */
            g_set_error(&err,
                        GTK_RASTA_ERROR,
                        GTK_RASTA_ERROR_LOAD,
                        "Context failed to initialize");
        }
        else 
            build_screen(main_ctxt);
    }

    if (err != NULL)
    {
        /* TIMBOIZE */
        etext = g_strdup_printf("Error loading screen data: %s",
                                err->message);
        g_error_free(err);
        pop_error(main_ctxt, etext);
        g_free(etext);
        gtk_main_quit();
    }

    return(cont);
}  /* init_in_func() */


/*
 * static void ic_start(GtkRastaContext *main_ctxt)
 *
 * Fires off an INITCOMMAND
 */
static void ic_start(GtkRastaContext *main_ctxt)
{
    gint outfd, errfd, rc;
    gchar *encoding;
    RastaScreen *screen;

    screen = rasta_context_get_screen(main_ctxt->ctxt);
    rc = rasta_initcommand_run(main_ctxt->ctxt, screen,
                               &(main_ctxt->child_pid),
                               &outfd, &errfd);
    if (rc != 0)
    {
        /* TIMBOIZE */
        pop_error(main_ctxt, "Unable to process the next screen.");
        rasta_initcommand_failed(main_ctxt->ctxt, screen);
        return;
    }

    encoding = rasta_initcommand_get_encoding(screen);

    main_ctxt->child_out_chan = g_io_channel_unix_new(outfd);
    g_io_channel_set_flags(main_ctxt->child_out_chan,
                           G_IO_FLAG_NONBLOCK,
                           NULL);
    g_io_channel_set_encoding(main_ctxt->child_out_chan, encoding, NULL);
    g_io_add_watch_full(main_ctxt->child_out_chan,
                        G_PRIORITY_LOW,
                        G_IO_IN | G_IO_HUP | G_IO_ERR,
                        ic_out_func,
                        main_ctxt,
                        NULL);
    g_io_channel_unref(main_ctxt->child_out_chan);

    main_ctxt->child_err_chan = g_io_channel_unix_new(errfd);
    g_io_channel_set_flags(main_ctxt->child_err_chan,
                           G_IO_FLAG_NONBLOCK,
                           NULL);
    g_io_channel_set_encoding(main_ctxt->child_err_chan, encoding, NULL);
    g_io_add_watch_full(main_ctxt->child_err_chan,
                        G_PRIORITY_LOW,
                        G_IO_IN | G_IO_HUP | G_IO_ERR,
                        ic_err_func,
                        main_ctxt,
                        NULL);
    g_io_channel_unref(main_ctxt->child_err_chan);
    
    g_free(encoding);

    g_timeout_add(100, ic_timeout, main_ctxt);
}  /* ic_start() */


/*
 * static gboolean ic_timeout(gpointer user_data)
 *
 * Timeout function to check whether the INITCOMMAND has finished
 */
static gboolean ic_timeout(gpointer user_data)
{
    gint rc;
    GtkRastaContext *main_ctxt;
    RastaScreen *screen;

    g_return_val_if_fail(user_data != NULL, FALSE);

    main_ctxt = (GtkRastaContext *)user_data;

    if (main_ctxt->child_pid > 0)
    {
        rc = waitpid(main_ctxt->child_pid,
                     &(main_ctxt->child_status),
                     WNOHANG);
        if (rc > 0)
            main_ctxt->child_pid = -1;
        else if ((rc < 0) && (errno == ECHILD))
            g_assert_not_reached();
    }

    if ((main_ctxt->child_pid < 0) &&
        (main_ctxt->child_out_chan == NULL) &&
        (main_ctxt->child_err_chan == NULL))
    {
        /* Clear our timeout before doing work */
        g_source_remove_by_user_data(user_data);

        screen = rasta_context_get_screen(main_ctxt->ctxt);

        if (WIFEXITED(main_ctxt->child_status) &&
            (WEXITSTATUS(main_ctxt->child_status) == 0))
        {
            if (main_ctxt->child_out_data == NULL)
                main_ctxt->child_out_data = g_strdup("");

            rasta_initcommand_complete(main_ctxt->ctxt,
                                       screen,
                                       main_ctxt->child_out_data);
        }
        else
        {
            pop_error(main_ctxt, main_ctxt->child_err_data);
            rasta_initcommand_failed(main_ctxt->ctxt, screen);
        }

        if (main_ctxt->child_out_data != NULL)
        {
            g_free(main_ctxt->child_out_data);
            main_ctxt->child_out_data = NULL;
        }
        if (main_ctxt->child_err_data != NULL)
        {
            g_free(main_ctxt->child_err_data);
            main_ctxt->child_err_data = NULL;
        }
        build_screen(main_ctxt);
        return(FALSE);
    }

    return(TRUE);
}  /* ic_timeout() */


/*
 * static gboolean ic_out_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Function to read data from an INITCOMMAND's STDOUT.
 */
static gboolean ic_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *out = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (out == NULL)
        out = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    d_print("IN = %s, HUP = %s, ERR = %s\n",
            cond & G_IO_IN ? "true" : "false",
            cond & G_IO_HUP ? "true" : "false",
            cond & G_IO_ERR ? "true" : "false");
    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    d_print("Calling read_chars\n");
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            d_print("Status EOF\n");
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            d_print("Status NORMAL\n");
            d_print("Bytes read = %d\n", bytes_read);
            g_assert(bytes_read > 0);
            g_string_append_len(out, buffer, bytes_read);
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            d_print("Status AGAIN\n");
            break;

        case G_IO_STATUS_ERROR:
            d_print("Status ERROR\n");
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;
    }

    d_print("Chan refcount = %d\n", chan->ref_count);
    if (cont == FALSE)
    {
        d_print("Calling shutdown\n");
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_out_chan = NULL;
        if (out->len > 0)
            main_ctxt->child_out_data = g_strdup(out->str);
        g_string_free(out, TRUE);
        out = NULL;
    }

    d_print("cont = %s\n", cont ? "true" : "false");
    return(cont);
}  /* ic_out_func() */


/*
 * static gboolean ic_err_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Function to read data from an INITCOMMAND's STDERR.
 */
static gboolean ic_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *ers = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (ers == NULL)
        ers = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            g_string_append_len(ers, buffer, bytes_read);
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;
    }

    if (cont == FALSE)
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_err_chan = NULL;
        if (ers->len > 0)
            main_ctxt->child_err_data = g_strdup(ers->str);
        g_string_free(ers, TRUE);
        ers = NULL;
    }
    return(cont);
}  /* ic_err_func() */


/*
 * static void lc_start(GtkRastaContext *main_ctxt,
 *                      RastaDialogField *field)
 *
 * Starts a LISTCOMMAND in the background.
 */
static void lc_start(GtkRastaContext *main_ctxt,
                     RastaDialogField *field)
{
    gint outfd, errfd, rc;
    gchar *encoding;

    g_assert(main_ctxt != NULL);
    g_assert(field != NULL);

    rc = rasta_listcommand_run(main_ctxt->ctxt, field,
                               &(main_ctxt->child_pid),
                               &outfd, &errfd);
    if (rc != 0)
    {
        /* TIMBOIZE */
        pop_error(main_ctxt, "Error generating list");
        finish_activity(main_ctxt);
        return;
    }

    encoding = rasta_listcommand_get_encoding(field);

    main_ctxt->child_out_chan = g_io_channel_unix_new(outfd);
    g_io_channel_set_flags(main_ctxt->child_out_chan,
                           G_IO_FLAG_NONBLOCK,
                           NULL);
    g_io_channel_set_encoding(main_ctxt->child_out_chan, encoding, NULL);
    g_io_add_watch_full(main_ctxt->child_out_chan,
                        G_PRIORITY_LOW,
                        G_IO_IN | G_IO_HUP | G_IO_ERR,
                        lc_out_func,
                        main_ctxt,
                        NULL);
    g_io_channel_unref(main_ctxt->child_out_chan);

    main_ctxt->child_err_chan = g_io_channel_unix_new(errfd);
    g_io_channel_set_flags(main_ctxt->child_err_chan,
                           G_IO_FLAG_NONBLOCK,
                           NULL);
    g_io_channel_set_encoding(main_ctxt->child_err_chan, encoding, NULL);
    g_io_add_watch_full(main_ctxt->child_err_chan,
                        G_PRIORITY_LOW,
                        G_IO_IN | G_IO_HUP | G_IO_ERR,
                        lc_err_func,
                        main_ctxt,
                        NULL);
    g_io_channel_unref(main_ctxt->child_err_chan);

    g_free(encoding);

    g_timeout_add(100, lc_timeout, main_ctxt);
}  /* lc_start() */


/*
 * static gboolean lc_timeout(gpointer user_data)
 *
 * Timeout to check for LISTCOMMAND completion.
 */
static gboolean lc_timeout(gpointer user_data)
{
    GtkWidget *dialog, *tv;
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    gint rc;
    RastaDialogField *field;
    GtkRastaContext *main_ctxt;

    g_return_val_if_fail(user_data != NULL, FALSE);

    main_ctxt = (GtkRastaContext *)user_data;
    g_assert(main_ctxt != NULL);

    if (main_ctxt->child_pid > 0)
    {
        rc = waitpid(main_ctxt->child_pid,
                     &(main_ctxt->child_status),
                     WNOHANG);
        if (rc > 0)
            main_ctxt->child_pid = -1;
        else if ((rc < 0) && (errno == ECHILD))
            g_assert_not_reached();
    }

    if ((main_ctxt->child_pid < 0) &&
        (main_ctxt->child_out_chan == NULL) &&
        (main_ctxt->child_err_chan == NULL))
    {
        /* Clear our timeout before doing work */
        g_source_remove_by_user_data(user_data);

        dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                              "dialog"));
        g_assert(dialog != NULL);

        if (WIFEXITED(main_ctxt->child_status) &&
            (WEXITSTATUS(main_ctxt->child_status) == 0))
        {
            tv = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog),
                                                      "treeview"));
            g_assert(tv != NULL);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
            g_assert(model != NULL);
            if (gtk_tree_model_get_iter_first(model, &iter) == FALSE)
            {
                pop_error(main_ctxt,
                          "There are no items in this list.");
                gtk_widget_destroy(dialog);
                finish_activity(main_ctxt);
            }
            else
            {
                field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                                             "field"));
                g_assert(field != NULL);
                if (rasta_listcommand_allow_multiple(field) == FALSE)
                {
                    sel =
                        gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
                    if (gtk_tree_selection_get_selected(sel,
                                                        &model,
                                                        &iter) != FALSE)
                    {
                        path = gtk_tree_model_get_path(model, &iter);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tv),
                                                     path, NULL,
                                                     FALSE, 0.0, 0.0);
                        gtk_tree_path_free(path);
                    }
                }
    
                g_signal_connect(G_OBJECT(dialog), "response",
                                 G_CALLBACK(do_list_response),
                                 main_ctxt);
                gtk_widget_set_sensitive(dialog, TRUE);
            }
        }
        else
        {
            pop_error(main_ctxt, main_ctxt->child_err_data);
            gtk_widget_destroy(dialog);
            finish_activity(main_ctxt);
        }

        if (main_ctxt->child_err_data != NULL)
        {
            g_free(main_ctxt->child_err_data);
            main_ctxt->child_err_data = NULL;
        }
        return(FALSE);
    }

    return(TRUE);
}  /* lc_timeout() */


/*
 * static void lc_maybe_add(GtkRastaContext *main_ctxt,
 *                          GString *data,
 *                          gboolean done)
 *
 * Checks data for any complete lines of text.  If there are any, it
 * adds them to the list dialog.  If done == TRUE, it adds
 * all data, disregarding the newline check.
 */
static void lc_maybe_add(GtkRastaContext *main_ctxt,
                         GString *data,
                         gboolean done)
{
    gunichar c;
    gint len;
    gchar *str, *ptr, *tmp;
    const gchar *cur;
    GtkWidget *entry;
    RastaDialogField *field;
    gboolean multiple;
    gboolean single;
    gboolean select;

    g_assert(main_ctxt != NULL);
    g_assert(data != NULL);
    g_assert(data->str != NULL);

    field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                                 "field"));
    g_assert(field != NULL);
    multiple = rasta_listcommand_allow_multiple(field);
    single = rasta_listcommand_is_single_column(field);

    entry = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                         "entry"));
    g_assert(entry != NULL);
    cur = gtk_entry_get_text(GTK_ENTRY(entry));

    c = g_utf8_get_char("\n");

    while (data->len > 0)
    {
        select = FALSE;
        ptr = g_utf8_strchr(data->str, -1, c);
        if (ptr != NULL)
        {
            len = ptr - data->str;
            str = g_new(gchar, len + 1);
            memmove(str, data->str, len);
            str[len] = '\0';

            /* Only select if we are single-selection */
            if (multiple == FALSE)
            {
                tmp = g_strdup(str);
                if (single != FALSE)
                {
                    for (ptr = tmp;
                         (*ptr != '\0') &&
                         (*ptr != '\t') &&
                         (*ptr != ' ');
                         ptr++);
                    *ptr = '\0';
                }
                if ((cur != NULL) && (strcmp(cur, tmp) == 0))
                    select = TRUE;
                g_free(tmp);
            }
            add_choice_item(main_ctxt, str, select);
            g_free(str);

            g_string_erase(data, 0, len + 1); /* + 1 to skip \n */
        }
        else if (done != FALSE)
        {
            if (data->str[data->len - 1] == '\n')
                g_string_erase(data, data->len - 1, 1);

            /* Only select if we are single-selection */
            if (multiple == FALSE)
            {
                tmp = g_strdup(data->str);
                if (single != FALSE)
                {
                    for (ptr = tmp;
                         (*ptr != '\0') &&
                         (*ptr != '\t') &&
                         (*ptr != ' ');
                         ptr++);
                    *ptr = '\0';
                }
                if ((cur != NULL) && (strcmp(cur, tmp) == 0))
                    select = TRUE;
                g_free(tmp);
            }
            add_choice_item(main_ctxt, data->str, select);
            g_string_truncate(data, 0);
        }
        else
            break;
    }

    d_print("Add run\n");
    if (done == TRUE)
    {
        d_print("Done is true\n");
    }
}  /* lc_maybe_add() */


/*
 * static gboolean lc_out_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Handles reading STDOUT from a LISTCOMMAND.
 */
static gboolean lc_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *out = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (out == NULL)
        out = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            g_string_append_len(out, buffer, bytes_read);
            lc_maybe_add(main_ctxt, out, FALSE);
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;
    }

    if (cont == FALSE)
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_out_chan = NULL;
        lc_maybe_add(main_ctxt, out, TRUE);
        g_string_free(out, TRUE);
        out = NULL;
    }
    return(cont);
}  /* lc_out_func() */


/*
 * static gboolean lc_err_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Handles reading STDERR from a LISTCOMMAND.
 */
static gboolean lc_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *ers = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (ers == NULL)
        ers = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            g_string_append_len(ers, buffer, bytes_read);
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;
    }

    if (cont == FALSE)
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_err_chan = NULL;
        if (ers->len > 0)
            main_ctxt->child_err_data = g_strdup(ers->str);
        g_string_free(ers, TRUE);
        ers = NULL;
    }
    return(cont);
}  /* lc_err_func() */


/*
 * static void pop_choice_foreach(GtkTreeModel *model,
 *                                GtkTreePath *path,
 *                                GtkTreeIter *iter,
 *                                gpointer res)
 *
 * Foreach function to retrieve selections.
 */
static void pop_choice_foreach(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer res)
{
    gchar *ptr;
    GList **elem;

    g_assert(res != NULL);
    elem = (GList **)res;

    ptr = NULL;
    gtk_tree_model_get(model, iter, 0, &ptr, -1);
    if (ptr != NULL)
        *elem = g_list_prepend(*elem, ptr);
}  /* pop_choice_foreach() */


/*
 * static void add_choice_item(GtkRastaContext *main_ctxt,
 *                             const gchar *item,
 *                             gboolean selected)
 *
 * Adds an item to the dialog.
 */
static void add_choice_item(GtkRastaContext *main_ctxt,
                            const gchar *item,
                            gboolean selected)
{
    GtkWidget *dialog, *tv;
    GtkListStore *list;
    GtkTreeIter iter;
    GtkTreeSelection *sel;
    GValue gval = {0, };

    g_assert(main_ctxt != NULL);
    g_assert(item != NULL);

    dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane), "dialog"));
    g_assert(dialog != NULL);

    tv = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog),
                                      "treeview"));
    g_assert(tv != NULL);

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
    list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)));

    gtk_list_store_append(list, &iter);
    g_value_init(&gval, G_TYPE_STRING);
    g_value_set_string(&gval, item);
    gtk_list_store_set_value(list, &iter, 0, &gval);
    if (selected != FALSE)
        gtk_tree_selection_select_iter(sel, &iter);
}  /* add_choice_item() */


/*
 * static void do_list_response(GtkWidget *widget,
 *                              gint response,
 *                              gpointer user_data)
 *
 */
static void do_list_response(GtkWidget *widget,
                             gint response,
                             gpointer user_data)
{
    gchar *ptr;
    gboolean single;
    GtkRastaContext *main_ctxt;
    RastaDialogField *field;
    GtkWidget *tv, *box, *entry;
    GtkTreeSelection *sel;
    GList *elem, *tmp;
    GString *resstr;

    main_ctxt = (GtkRastaContext *)user_data;
    g_assert(main_ctxt != NULL);

    elem = NULL;
    if (response == GTK_RESPONSE_ACCEPT)
    {
        tv = GTK_WIDGET(g_object_get_data(G_OBJECT(widget),
                                          "treeview"));
        g_assert(tv != NULL);

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
        gtk_tree_selection_selected_foreach(sel,
                                            pop_choice_foreach,
                                            &elem);
    }
    gtk_widget_destroy(widget);

    box = main_ctxt->main_pane;
    g_object_set_data(G_OBJECT(box), "dialog", NULL);

    elem = g_list_reverse(elem);

    if (elem != NULL)
    {
        field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(main_ctxt->child_data), "field"));
        single = rasta_listcommand_is_single_column(field);
        resstr = g_string_new("");
        for (tmp = elem; tmp != NULL; tmp = g_list_next(tmp))
        {
            g_assert(tmp->data != NULL);
            if (resstr->len != 0)
                g_string_append_c(resstr, ' ');
            if (single != FALSE)
            {
                for (ptr = tmp->data;
                     (*ptr != '\0') &&
                     (*ptr != '\t') &&
                     (*ptr != ' ');
                     ptr++);
                *ptr = '\0';
            }
            g_string_append(resstr, (gchar *)tmp->data);
            g_free(tmp->data);
        }
        g_list_free(elem);
        entry = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->child_data), "entry"));
        gtk_entry_set_text(GTK_ENTRY(entry), resstr->str);
        g_string_free(resstr, TRUE);
     }

     finish_activity(main_ctxt);
}  /* do_list_response() */


/*
 * static void do_ring_response(GtkWidget *widget,
 *                              gint response,
 *                              gpointer user_data)
 *
 */
static void do_ring_response(GtkWidget *widget,
                             gint response,
                             gpointer user_data)
{
    gchar *ptr;
    GtkRastaContext *main_ctxt;
    GtkWidget *tv, *box, *entry;
    GtkTreeSelection *sel;
    GList *elem, *tmp;

    main_ctxt = (GtkRastaContext *)user_data;
    g_assert(main_ctxt != NULL);

    elem = NULL;
    if (response == GTK_RESPONSE_ACCEPT)
    {
        tv = GTK_WIDGET(g_object_get_data(G_OBJECT(widget),
                                          "treeview"));
        g_assert(tv != NULL);

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
        gtk_tree_selection_selected_foreach(sel,
                                            pop_choice_foreach,
                                            &elem);
    }
    gtk_widget_destroy(widget);

    box = main_ctxt->main_pane;
    g_object_set_data(G_OBJECT(box), "dialog", NULL);

    elem = g_list_reverse(elem);
    if (elem != NULL)
    {
        ptr = (gchar *)elem->data;
        g_assert(ptr != NULL);
        entry = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                             "entry"));
        gtk_entry_set_text(GTK_ENTRY(entry), ptr);
    }
    for (tmp = elem; tmp != NULL; tmp = g_list_next(tmp))
        g_free(tmp->data);
    g_list_free(elem);

    finish_activity(main_ctxt);
}  /* do_ring_response() */


/*
 * static void pop_choice_dialog(GtkRastaContext *main_ctxt,
 *                               gboolean multiple)
 *
 * Pops up a dialog with a list of choices.  The chosen entry(ies)
 * are returned
 */
static void pop_choice_dialog(GtkRastaContext *main_ctxt,
                              gboolean multiple)
{
    GtkWidget *dialog, *tv, *scroll;
    GtkListStore *list;
    GtkTreeViewColumn *column;
    GtkTreeSelection *sel;
    GtkCellRenderer *cell_renderer;

    g_assert(main_ctxt != NULL);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width(GTK_CONTAINER(scroll), 2);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    list = gtk_list_store_new(1, G_TYPE_STRING);
    tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));
    g_object_unref(list);

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
    gtk_tree_selection_set_mode(sel,
                                multiple != FALSE ?
                                GTK_SELECTION_MULTIPLE :
                                GTK_SELECTION_SINGLE);

    cell_renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(cell_renderer), 1);
    column = gtk_tree_view_column_new_with_attributes(NULL,
                                                      cell_renderer,
                                                      "text", 0,
                                                      NULL);
    gtk_tree_view_column_set_sizing(column,
                                    GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll), tv);

    /* TIMBOIZE */
    dialog =
        gtk_dialog_new_with_buttons("Select From List",
                                    GTK_WINDOW(gtk_widget_get_toplevel(main_ctxt->main_pane)),
                                    GTK_DIALOG_MODAL |
#if 0  /* SEPARATORS ARE FUCKING UGLY */
                                    GTK_DIALOG_NO_SEPARATOR |
#endif
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_STOCK_CANCEL,
                                    GTK_RESPONSE_REJECT,
                                    GTK_STOCK_OK,
                                    GTK_RESPONSE_ACCEPT,
                                    NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scroll,
                       TRUE, TRUE, 2);

    g_object_set_data(G_OBJECT(dialog), "treeview", tv);

    g_object_set_data(G_OBJECT(main_ctxt->main_pane),
                      "dialog", dialog);

    gtk_widget_set_sensitive(dialog, FALSE);
    gtk_widget_show_all(dialog);
}  /* pop_choice_dialog() */


/*
 * static void pop_list_dialog(GtkRastaContext *main_ctxt,
 *                             GtkWidget *widget)
 *
 * Uses list data to create a choice dialog for list entries.
 */
static void pop_list_dialog(GtkRastaContext *main_ctxt,
                            GtkWidget *widget)
{
    RastaDialogField *field;

    g_assert(main_ctxt != NULL);
    g_assert(widget != NULL);

    field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(widget),
                                                 "field"));
    g_assert(field != NULL);

    main_ctxt->child_data = widget;
    pop_choice_dialog(main_ctxt,
                      rasta_listcommand_allow_multiple(field));
}  /* pop_list_dialog() */


/*
 * static gboolean ring_dialog_idle(gpointer user_data)
 *
 * Idle function to add ring items
 */
static gboolean ring_dialog_idle(gpointer user_data)
{
    GtkWidget *dialog, *entry, *tv;
    GtkRastaContext *main_ctxt;
    REnumeration *en;
    RastaRingValue *value;
    gchar *ptr;
    const gchar *cur;
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;

    main_ctxt = (GtkRastaContext *)user_data;
    g_assert(main_ctxt != NULL);
    g_assert(main_ctxt->child_data != NULL);

    en = (REnumeration *)g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                           "en");
    g_assert(en != NULL);

    if (r_enumeration_has_more(en))
    {
        entry = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->child_data),
                                             "entry"));
        cur = gtk_entry_get_text(GTK_ENTRY(entry));
        value = RASTA_RING_VALUE(r_enumeration_get_next(en));
        g_assert(value != NULL);
        ptr = rasta_ring_value_get_text(value);
        g_assert(ptr != NULL);
        add_choice_item(main_ctxt, ptr,
                        (cur != NULL) && (strcmp(cur, ptr) == 0));
        g_free(ptr);
    }
    else
    {
        dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                              "dialog"));
        g_assert(dialog != NULL);
        tv = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog),
                                          "treeview"));
        g_assert(tv != NULL);
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
        if (gtk_tree_selection_get_selected(sel, &model, &iter) != FALSE)
        {
            path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tv),
                                         path, NULL,
                                         FALSE, 0.0, 0.0);
            gtk_tree_path_free(path);
        }
        g_signal_connect(G_OBJECT(dialog), "response",
                         G_CALLBACK(do_ring_response), main_ctxt);
        gtk_widget_set_sensitive(dialog, TRUE);
        r_enumeration_free(en);
        return(FALSE);
    }
    
    return(TRUE);
}  /* ring_dialog_idle() */

 
/*
 * static void pop_ring_dialog(GtkRastaContext *main_ctxt,
 *                             GtkWidget *widget)
 *
 * Creates a choice dialog with the possible ring values.
 */
static void pop_ring_dialog(GtkRastaContext *main_ctxt,
                            GtkWidget *widget)
{
    RastaDialogField *field;
    REnumeration *en;

    g_assert(main_ctxt != NULL);
    g_assert(widget != NULL);

    field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(widget),
                                                 "field"));
    g_assert(field != NULL);

    en = rasta_dialog_field_enumerate_ring(field);
    if (r_enumeration_has_more(en) == FALSE)
    {
        r_enumeration_free(en);
        /* TIMBOIZE */
        pop_error(main_ctxt, "There are no items in this list.");
        finish_activity(main_ctxt);
        return;
    }

    g_object_set_data(G_OBJECT(widget), "en", en);

    pop_choice_dialog(main_ctxt, FALSE);
    main_ctxt->child_data = widget;

    g_idle_add(ring_dialog_idle, main_ctxt);
}  /* pop_ring_dialog() */


/*
 * static gint ac_start(GtkRastaContext *main_ctxt,
 *                          RastaScreen *screen)
 *
 * Starts an action command, returning nonzero if there was a problem.
 */
static gint ac_start(GtkRastaContext *main_ctxt,
                     RastaScreen *screen)
{
    RastaTTYType type;
    gint rc, outfd, errfd;
    /* Needs win32 cmd.exe #ifdef */
    gchar *args[] = {"/bin/sh", "-c", NULL, NULL};
    gchar *encoding, *utf8_command;

    utf8_command =
        rasta_action_screen_get_command(main_ctxt->ctxt, screen);
    if ((utf8_command== NULL) || (utf8_command[0] == '\0'))
    {
        g_free(utf8_command);
        /* TIMBOIZE */
        pop_error(main_ctxt, "There are no items of this type.");
        return(-ENOEXEC);
    }

    /* Shell needs current encoding */
    args[2] = g_locale_from_utf8(utf8_command, -1, NULL, NULL, NULL);
    g_free(utf8_command);
    if ((args[2]== NULL) || (args[2][0] == '\0'))
    {
        g_free(args[2]);
        /* TIMBOIZE */
        pop_error(main_ctxt,
                  "There was an error creating the action command.");
        return(-ENOEXEC);
    }

    type = rasta_action_screen_get_tty_type(screen);
    
    if (type == RASTA_TTY_SELF)
    {
        g_free(args[2]);
        /* TIMBOIZE - Ignored, we should eventually support 'SELF' */
        pop_error(main_ctxt, "Actions of type SELF are not supported yet.");
        return(-ENOEXEC);
    }
    else if (type == RASTA_TTY_NO)
    {
        main_ctxt->child_out_chan = NULL;
        main_ctxt->child_err_chan = NULL;

        rc = rasta_exec_command_v(&(main_ctxt->child_pid),
                                  NULL, NULL, NULL,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_NULL,
                                  args);

        g_free(args[2]);
        if (rc != 0)
            return(-ECHILD);
    }
    else if (type == RASTA_TTY_YES)
    {
        rc = rasta_exec_command_v(&(main_ctxt->child_pid),
                                  NULL, &outfd, &errfd,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_PIPE,
                                  RASTA_EXEC_FD_PIPE,
                                  args);
        g_free(args[2]);
        if (rc != 0)
            return(-ECHILD);

        encoding = rasta_action_screen_get_encoding(screen);

        main_ctxt->child_out_chan = g_io_channel_unix_new(outfd);
        g_io_channel_set_flags(main_ctxt->child_out_chan,
                               G_IO_FLAG_NONBLOCK,
                               NULL);
        g_io_channel_set_encoding(main_ctxt->child_out_chan, encoding, NULL);
        g_io_add_watch_full(main_ctxt->child_out_chan,
                            G_PRIORITY_LOW,
                            G_IO_IN | G_IO_HUP | G_IO_ERR,
                            ac_out_func,
                            main_ctxt,
                            NULL);
        g_io_channel_unref(main_ctxt->child_out_chan);

        main_ctxt->child_err_chan = g_io_channel_unix_new(errfd);
        g_io_channel_set_flags(main_ctxt->child_err_chan,
                               G_IO_FLAG_NONBLOCK,
                               NULL);
        g_io_channel_set_encoding(main_ctxt->child_err_chan, encoding, NULL);
        g_io_add_watch_full(main_ctxt->child_err_chan,
                            G_PRIORITY_LOW,
                            G_IO_IN | G_IO_HUP | G_IO_ERR,
                            ac_err_func,
                            main_ctxt,
                            NULL);
        g_io_channel_unref(main_ctxt->child_err_chan);

        g_free(encoding);
    }

    g_timeout_add(100, ac_timeout, main_ctxt);

    return(TRUE);
}  /* ac_start() */


/*
 * static void ac_tag_output(GtkRastaContext *main_ctxt,
 *                           const gchar *stream)
 *
 * Flags the action screen when output has occurred
 */
static void ac_tag_output(GtkRastaContext *main_ctxt,
                          const gchar *stream)
{
    GtkWidget *screen_frame, *panes, *action_pane, *label;
    gchar *tag;

    g_assert(main_ctxt != NULL);
    g_assert(stream != NULL);

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes = GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                         "panes"));
    g_assert(panes != NULL);

    action_pane = GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                               "action_pane"));
    g_assert(action_pane != NULL);

    label = GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                         stream));

    tag = (strcmp(stream, "stdout") == 0) ?
          "Stdout: Yes" :
          "Stderr: Yes";
    gtk_label_set_text(GTK_LABEL(label), tag);
}  /* ac_tag_output() */


/*
 * static void ac_maybe_output(GtkRastaContext *main_ctxt,
 *                             GString *data,
 *                             gboolean done)
 *
 * Checks data for any complete lines of text.  If there are any, it
 * outputs them to the output widget.  If done == TRUE, it outputs
 * all data, disregarding the newline check.
 */
static void ac_maybe_output(GtkRastaContext *main_ctxt,
                            GString *data,
                            gboolean done)
{
    GtkWidget *screen_frame, *panes, *action_pane, *output;
    GtkTextBuffer *buf;
    GtkTextIter iter;
    GtkTextMark *mark;
    gunichar c = '\n';
    gint len;
    gchar *str, *ptr;

    g_assert(main_ctxt != NULL);
    g_assert(data != NULL);
    g_assert(data->str != NULL);

    if (data->len == 0)
        return;

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes = GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                         "panes"));
    g_assert(panes != NULL);

    action_pane = GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                               "action_pane"));
    g_assert(action_pane != NULL);

    output = GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                          "output"));
    g_assert(output != NULL);

    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output));

    if (done == TRUE)
    {
        if (data->str[data->len - 1] == '\n')
            str = g_strdup(data->str);
        else
            str = g_strdup_printf("%s\n", data->str);
        g_string_truncate(data, 0);
    }
    else
    {
        str = NULL;
        ptr = g_utf8_strchr(data->str, -1, c);
        while (ptr != NULL)
        {
            str = ptr;
            ptr = g_utf8_strchr(str + 1, -1, c);
        }

        if (str != NULL)
        {
            len = str - data->str + 1;
            str = g_new(gchar, len + 1);
            memmove(str, data->str, len);
            str[len] = '\0';
            g_string_erase(data, 0, len);
        }
    }

    if (str == NULL)
        return;

    d_print("Adding text, len = %ld\n", g_utf8_strlen(str, -1));
    /* FIXME: text_view scroll is fixed, can we do this better? */
    mark = gtk_text_buffer_get_mark(buf, "end");
    if (gtk_text_buffer_get_modified(buf) == FALSE)
    {
        gtk_text_buffer_set_text(buf, str, -1);
        gtk_text_buffer_get_end_iter(buf, &iter);
        gtk_text_buffer_move_mark(buf, mark, &iter);
    }
    else
    {
        gtk_text_buffer_get_iter_at_mark(buf, &iter, mark); 
        gtk_text_buffer_insert(buf, &iter, str, -1);
    }
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(output), mark);
    g_free(str);
}  /* ac_maybe_output() */


/*
 * static gboolean ac_out_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Handles STDOUT output from an action command
 */
static gboolean ac_out_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *out = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (out == NULL)
        out = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    main_ctxt = (GtkRastaContext *)user_data;
    cont = TRUE;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            g_string_append_len(out, buffer, bytes_read);
            ac_maybe_output(main_ctxt, out, FALSE);
            ac_tag_output(main_ctxt, "stdout");
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            g_error_free(err);
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;

        default:
            g_assert_not_reached();
    }

    if (cont == FALSE)
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_out_chan = NULL;
        ac_maybe_output(main_ctxt, out, TRUE);
        g_string_free(out, TRUE);
        out = NULL;
    }
    return(cont);
}  /* ac_out_func() */


/*
 * static gboolean ac_err_func(GIOChannel *chan,
 *                             GIOCondition cond,
 *                             gpointer user_data)
 *
 * Handles STDERR output from an action command
 */
static gboolean ac_err_func(GIOChannel *chan,
                            GIOCondition cond,
                            gpointer user_data)
{
    GtkRastaContext *main_ctxt;
    GIOStatus rc;
    gsize bytes_read;
    gboolean cont;
    GError *err;
    static GString *ers = NULL;
    static gchar *buffer = NULL;
    static size_t page_size = 0;

    g_return_val_if_fail(user_data != NULL, FALSE);

    if (ers == NULL)
        ers = g_string_new(NULL);

    if (buffer == NULL)
    {
        page_size = getpagesize();
        buffer = g_new0(gchar, page_size * NUM_READ_PAGES);
    }

    cont = TRUE;
    main_ctxt = (GtkRastaContext *)user_data;
    err = NULL;
    rc = g_io_channel_read_chars(chan, buffer,
                                 page_size * NUM_READ_PAGES,
                                 &bytes_read, &err);
    switch (rc)
    {
        case G_IO_STATUS_EOF:
            cont = FALSE;
            if (bytes_read < 1)
                break;
            /* Fall through with read data */

        case G_IO_STATUS_NORMAL:
            g_assert(bytes_read > 0);
            g_string_append_len(ers, buffer, bytes_read);
            ac_maybe_output(main_ctxt, ers, FALSE);
            ac_tag_output(main_ctxt, "stderr");
            break;

        case G_IO_STATUS_AGAIN:
            /* Do nothing */
            break;

        case G_IO_STATUS_ERROR:
            cont = FALSE;
            /* FIXME: Flag an error? */
            break;
    }

    if (cont == FALSE)
    {
        g_io_channel_shutdown(chan, FALSE, NULL);
        g_free(buffer);
        buffer = NULL;
        main_ctxt->child_err_chan = NULL;
        ac_maybe_output(main_ctxt, ers, TRUE);
        g_string_free(ers, TRUE);
        ers = NULL;
    }
    return(cont);
}  /* ac_err_func() */


/*
 * static gboolean ac_timeout(gpointer user_data)
 *
 * Checks for action command completion
 */
static gboolean ac_timeout(gpointer user_data)
{
    gint rc;
    GtkRastaContext *main_ctxt;
    RastaScreen *screen;
    GtkWidget *screen_frame, *panes, *action_pane, *label;

    g_return_val_if_fail(user_data != NULL, FALSE);

    main_ctxt = (GtkRastaContext *)user_data;

    if (main_ctxt->child_pid > 0)
    {
        rc = waitpid(main_ctxt->child_pid,
                     &(main_ctxt->child_status),
                     WNOHANG);
        if (rc > 0)
            main_ctxt->child_pid = -1;
        else if ((rc < 0) && (errno == ECHILD))
            g_assert_not_reached();
    }

    if ((main_ctxt->child_pid < 0) &&
        (main_ctxt->child_out_chan == NULL) &&
        (main_ctxt->child_err_chan == NULL))
    {
        /* Clear our timeout before doing work */
        g_source_remove_by_user_data(user_data);

        screen = rasta_context_get_screen(main_ctxt->ctxt);

        screen_frame =
            GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                         "screens"));
        g_assert(screen_frame != NULL);
    
        panes = GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                             "panes"));
        g_assert(panes != NULL);
    
        action_pane = GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                                   "action_pane"));
        g_assert(action_pane != NULL);
    
        label = GTK_WIDGET(g_object_get_data(G_OBJECT(action_pane),
                                             "status"));

        if (WIFEXITED(main_ctxt->child_status) &&
            (WEXITSTATUS(main_ctxt->child_status) == 0))
        {
            /* TIMBOIZE */
            gtk_label_set_text(GTK_LABEL(label),
                               "Status: Success");
        }
        else
        {
            /* TIMBOIZE */
            gtk_label_set_text(GTK_LABEL(label),
                               "Status: Failed");
            if (WIFEXITED(main_ctxt->child_status))
            {
                d_print("Exit status: %d\n", WEXITSTATUS(main_ctxt->child_status));
            }
            else if (WIFSIGNALED(main_ctxt->child_status))
            {
                d_print("Exit signal: %d\n", WTERMSIG(main_ctxt->child_status));
            }
        }

        finish_activity(main_ctxt);
        return(FALSE);
    }

    return(TRUE);
}  /* ac_timeout() */


/*
 * static void create_menu_screen_panel(GtkWidget *nb)
 *
 * Builds the panel for the menu screens
 */
static void create_menu_screen_panel(GtkWidget *nb)
{
    GtkWidget *pane, *menu;

    g_return_if_fail(nb != NULL);

    pane = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(pane), 2);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    menu = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(menu), 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane),
                                          menu);
    g_object_set_data(G_OBJECT(pane), "menu", menu);

    gtk_widget_show_all(pane);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), pane, FALSE);
    g_object_set_data(G_OBJECT(nb), "menu_pane", pane);
}  /* create_menu_screen_panel() */


/*
 * static void create_dialog_screen_panel(GtkWidget *nb)
 *
 * Creates the panel for dialog screens
 */
static void create_dialog_screen_panel(GtkWidget *nb)
{
    GtkWidget *pane, *dialog;

    g_return_if_fail(nb != NULL);

    pane = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(pane), 2);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    dialog = gtk_table_new(0, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane),
                                          dialog);
    g_object_set_data(G_OBJECT(pane), "dialog", dialog);

    gtk_widget_show_all(pane);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), pane, FALSE);
    g_object_set_data(G_OBJECT(nb), "dialog_pane", pane);
}  /* create_dialog_screen_panel() */


/*
 * static void create_action_screen_panel(GtkWidget *nb)
 *
 * Creates the panel for action screens
 */
static void create_action_screen_panel(GtkWidget *nb)
{
    GtkWidget *pane, *label, *obox, *tv, *scroll;
    GtkTextBuffer *tb;
    GtkTextIter iter;

    g_return_if_fail(nb != NULL);

    pane = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(pane), 2);

    obox = gtk_hbox_new(TRUE, 2);

    label = gtk_label_new("Status: Running");
    gtk_box_pack_start(GTK_BOX(obox), label, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(pane), "status", label);

    label = gtk_label_new("Stdout: No");
    gtk_box_pack_start(GTK_BOX(obox), label, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(pane), "stdout", label);

    label = gtk_label_new("Stderr: No");
    gtk_box_pack_start(GTK_BOX(obox), label, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(pane), "stderr", label);

    gtk_box_pack_start(GTK_BOX(pane), obox, FALSE, FALSE, 2);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(pane), scroll, TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(pane), "output", tv);

    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_get_end_iter(tb, &iter);
    gtk_text_buffer_create_mark(tb, "end", &iter, FALSE);

    gtk_widget_show_all(pane);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), pane, NULL);
    g_object_set_data(G_OBJECT(nb), "action_pane", pane);
}  /* create_action_screen_panel() */


/*
 * static void create_screen_panel(GtkRastaContext *main_ctxt)
 *
 * Creates the main screen panel.  This panel will contain the panels
 * for the different screens.
 */
static void create_screen_panel(GtkRastaContext *main_ctxt)
{
    GtkWidget *screen_frame, *nb;

    g_return_if_fail(main_ctxt != NULL);
    g_return_if_fail(main_ctxt->main_pane != NULL);

    screen_frame = gtk_frame_new("");

    nb = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nb), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(nb), FALSE);

    create_menu_screen_panel(nb);
    create_dialog_screen_panel(nb);
    create_action_screen_panel(nb);

    gtk_container_add(GTK_CONTAINER(screen_frame), nb);
    g_object_set_data(G_OBJECT(screen_frame), "panes", nb);

    gtk_box_pack_start(GTK_BOX(main_ctxt->main_pane), screen_frame,
                       TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(main_ctxt->main_pane),
                      "screens", screen_frame);
}  /* create_screen_panel() */


/*
 * static void create_button_panel(GtkRastaContext *main_ctxt)
 *
 * Creates the panel for the navigation buttons.
 */
static void create_button_panel(GtkRastaContext *main_ctxt)
{
    GtkWidget *next, *previous, *quit, *cancel;
    GtkWidget *bbox, *pbox;

    g_return_if_fail(main_ctxt != NULL);
    g_return_if_fail(main_ctxt->main_pane != NULL);

    pbox = gtk_hbox_new(FALSE, 2);

    bbox = gtk_hbutton_box_new();
    gtk_box_set_spacing(GTK_BOX(bbox), 2);
    gtk_container_set_border_width(GTK_CONTAINER(bbox), 2);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox),
                              GTK_BUTTONBOX_END);

    quit = gtk_button_new_from_stock(GTK_STOCK_QUIT);
    g_signal_connect(G_OBJECT(quit), "clicked",
                     G_CALLBACK(do_quit), main_ctxt);
    gtk_container_add(GTK_CONTAINER(bbox), quit);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(G_OBJECT(cancel), "clicked",
                     G_CALLBACK(do_cancel), main_ctxt);
    gtk_container_add(GTK_CONTAINER(bbox), cancel);
    g_object_set_data(G_OBJECT(pbox), "cancel_button", cancel);

    gtk_box_pack_start(GTK_BOX(pbox), bbox,
                       FALSE, FALSE, 2);

    bbox = gtk_hbutton_box_new();
    gtk_box_set_spacing(GTK_BOX(bbox), 2);
    gtk_container_set_border_width(GTK_CONTAINER(bbox), 2);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox),
                              GTK_BUTTONBOX_END);

    previous = gtk_button_new_from_stock(GTK_STOCK_GO_BACK);
    g_signal_connect(G_OBJECT(previous), "clicked",
                     G_CALLBACK(do_previous), main_ctxt);
    gtk_container_add(GTK_CONTAINER(bbox), previous);
    g_object_set_data(G_OBJECT(pbox), "previous_button", previous);

    next = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    g_signal_connect(G_OBJECT(next), "clicked",
                     G_CALLBACK(do_next), main_ctxt);
    gtk_container_add(GTK_CONTAINER(bbox), next);
    g_object_set_data(G_OBJECT(pbox), "next_button", next);

    gtk_widget_show_all(bbox);

    gtk_box_pack_start(GTK_BOX(pbox), bbox,
                       TRUE, TRUE, 2);
    g_object_set_data(G_OBJECT(main_ctxt->main_pane),
                      "buttons", pbox);

    gtk_box_pack_start(GTK_BOX(main_ctxt->main_pane), pbox,
                       FALSE, FALSE, 0);
}  /* create_button_panel() */


/*
 * static void create_status_panel(GtkRastaContext *main_ctxt)
 *
 * Creates the panel for status information.
 */
static void create_status_panel(GtkRastaContext *main_ctxt)
{
    GtkWidget *frame, *sbox, *label, *bar;

    g_return_if_fail(main_ctxt != NULL);
    g_return_if_fail(main_ctxt->main_pane != NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    sbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new("Initializing...");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 5);
    g_object_set_data(G_OBJECT(main_ctxt->main_pane),
                      "status", label);

    bar = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(bar),
                                     GTK_PROGRESS_LEFT_TO_RIGHT);
    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(bar), 0.05);
    gtk_box_pack_end(GTK_BOX(sbox), bar, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(main_ctxt->main_pane),
                      "progress", bar);

    gtk_box_pack_start(GTK_BOX(main_ctxt->main_pane),
                       gtk_hseparator_new(),
                       FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(main_ctxt->main_pane), sbox,
                       FALSE, FALSE, 1);
}  /* create_status_panel() */


/*
 * static void finish_activity(GtkRastaContext *main_ctxt)
 *
 * Sets the pane sensitive again, clears the status bar too, and
 * stops the busy indicator.
 */
static void finish_activity(GtkRastaContext *main_ctxt)
{
    GtkWidget *screens, *buttons, *bar, *box, *status;
    GtkWidget *next, *previous, *cancel;
    RastaScreen *screen;
    guint timeout_id;

    box = GTK_WIDGET(main_ctxt->main_pane);
    screens = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                           "screens"));
    gtk_widget_set_sensitive(screens, TRUE);

    buttons = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                           "buttons"));

    next = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                        "next_button"));
    previous = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                            "previous_button"));
    cancel = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                          "cancel_button"));

    gtk_widget_set_sensitive(cancel, FALSE);

    screen = rasta_context_get_screen(main_ctxt->ctxt);
    gtk_widget_set_sensitive(next,
                             rasta_screen_get_type(screen) != RASTA_SCREEN_ACTION);

    gtk_widget_set_sensitive(previous,
                             rasta_context_is_initial_screen(main_ctxt->ctxt) == FALSE);

    bar = GTK_WIDGET(g_object_get_data(G_OBJECT(box), "progress"));
    timeout_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(bar),
                                                    "timeout_id"));
    gtk_timeout_remove(timeout_id);
    /*
     * FIXME (somehow): Broken GTK redraw leaves 'active'
     * bar sometimes
     */
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), 0.0);

    status = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                          "status"));
    gtk_label_set_text(GTK_LABEL(status), NULL);
}  /* finish_activity() */


/*
 * static void start_activity(GtkRastaContext *main_ctxt,
 *                            const gchar *info)
 *
 * Sets the pane insensitive, sets the status bar to <info>, and
 * starts the busy indicator.
 */
static void start_activity(GtkRastaContext *main_ctxt,
                           const gchar *info)
{
    GtkWidget *screens, *buttons, *bar, *box, *status;
    GtkWidget *next, *previous, *cancel;
    guint timeout_id;

    box = GTK_WIDGET(main_ctxt->main_pane);
    screens = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                           "screens"));
    gtk_widget_set_sensitive(screens, FALSE);

    buttons = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                           "buttons"));
    next = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                        "next_button"));
    previous = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                            "previous_button"));
    cancel = GTK_WIDGET(g_object_get_data(G_OBJECT(buttons),
                                          "cancel_button"));
    gtk_widget_set_sensitive(next, FALSE);
    gtk_widget_set_sensitive(previous, FALSE);
    gtk_widget_set_sensitive(cancel, TRUE);

    status = GTK_WIDGET(g_object_get_data(G_OBJECT(box),
                                          "status"));
    gtk_label_set_text(GTK_LABEL(status), info);

    bar = GTK_WIDGET(g_object_get_data(G_OBJECT(box), "progress"));
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(bar));
    timeout_id = g_timeout_add(100,
                               (GSourceFunc)gtk_progress_bar_pulse,
                               bar);
    g_object_set_data(G_OBJECT(bar), "timeout_id",
                      GUINT_TO_POINTER(timeout_id));
}  /* start_activity() */


/*
 * static void do_quit(GtkWidget *widget, gpointer user_data)
 *
 * Quits the application
 */
static void do_quit(GtkWidget *widget, gpointer user_data)
{
    gtk_main_quit();
}  /* do_quit() */


/*
 * static void do_previous(GtkWidget *widget, gpointer user_data)
 *
 * Backs up a screen
 */
static void do_previous(GtkWidget *widget, gpointer user_data)
{
    GtkRastaContext *main_ctxt;

    g_return_if_fail(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;

    start_activity(main_ctxt, "Retrieving screen...");
    rasta_screen_previous(main_ctxt->ctxt);
    build_screen(main_ctxt);
}  /* do_previous() */


/* 
 * static void do_next(GtkWidget *widget, gpointer user_data)
 *
 * Goes to the next screen
 */
static void do_next(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *screen_frame, *panes, *pane;
    GtkWidget *menu_pane, *dialog_pane, *action_pane;
    gint page_num;
    GtkRastaContext *main_ctxt;

    g_return_if_fail(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(panes));
    pane = gtk_notebook_get_nth_page(GTK_NOTEBOOK(panes), page_num);

    menu_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "menu_pane"));
    g_assert(menu_pane != NULL);
    dialog_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "dialog_pane"));
    g_assert(dialog_pane != NULL);
    action_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "action_pane"));
    g_assert(action_pane != NULL);

    if (pane == menu_pane)
        do_menu_screen_next(main_ctxt);
    else if (pane == dialog_pane)
        do_dialog_screen_next(main_ctxt);
    else
        g_assert_not_reached();
}  /* do_next() */


/*
 * static void do_cancel(GtkWidget *widget, gpointer user_data)
 *
 * Cancels the current operation
 */
static void do_cancel(GtkWidget *widget, gpointer user_data)
{
    GtkRastaContext *main_ctxt;

    g_return_if_fail(user_data != NULL);
    
    main_ctxt = (GtkRastaContext *)user_data;

    if (main_ctxt->child_pid > 0)
        kill(-(main_ctxt->child_pid), SIGTERM);

    /* FIXME: should 'finish_activity()' happen here? */
    /* FIXME: set some flag to the process to know that we failed */
    /* FIXME: should there be a timeout try -9 later? */
}  /* do_cancel() */


/*
 * static void do_file_selected(GtkWidget *widget, gpointer user_data)
 *
 * Stores the newly selected file
 */
static void do_file_selected(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *entry, *sel;
    const gchar *filename;

    entry = GTK_WIDGET(user_data);
    g_assert(entry != NULL);

    sel = GTK_WIDGET(g_object_get_data(G_OBJECT(widget),
                                       "selector"));
    g_assert(sel != NULL);

    filename =
        gtk_file_selection_get_filename(GTK_FILE_SELECTION(sel));

    gtk_entry_set_text(GTK_ENTRY(entry), filename);
}  /* do_file_selected() */


/*
 * static void do_browse(GtkWidget *widget, gpointer user_data)
 *
 * Browse the filesystem for a file.
 */
static void do_browse(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *parent, *entry, *sel;
    gchar *filename;

    parent = GTK_WIDGET(g_object_get_data(G_OBJECT(widget),
                                          "parent"));
    g_assert(parent != NULL);
    entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                         "entry"));
    g_assert(entry != NULL);
    
    filename = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
    if (filename == NULL)
        filename = g_strdup("");

    sel = gtk_file_selection_new("Please select a file.");
    g_object_set_data(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button),
                      "selector", sel);

    if (filename[0] != '\0')
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(sel),
                                        filename);
    g_free(filename);

    g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button), "clicked",
                     G_CALLBACK(do_file_selected), entry);
                                   
             
    g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button),
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             (gpointer) sel);

    g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->cancel_button),
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             (gpointer) sel);
                   
    gtk_widget_show (sel);
}  /* do_browse() */


/*
 * static void do_list(GtkWidget *widget, gpointer user_data)
 *
 * Callback for LIST/ENTRYLIST/RING entries.  Starts up a
 * LISTCOMMAND if required.
 */
static void do_list(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *parent;
    GtkRastaContext *main_ctxt;
    RastaDialogField *field;
    RastaDialogFieldType type;

    g_return_if_fail(user_data != NULL);

    main_ctxt = (GtkRastaContext *)user_data;

    /* TIMBOIZE */
    start_activity(main_ctxt, "Generating list...");
    
    parent = GTK_WIDGET(g_object_get_data(G_OBJECT(widget),
                                          "parent"));
    field = RASTA_DIALOG_FIELD(g_object_get_data(G_OBJECT(parent),
                                                 "field"));
    type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent),
                                             "type"));
    switch (type)
    {
        case RASTA_FIELD_RING:
            pop_ring_dialog(main_ctxt, parent);
            break;

        case RASTA_FIELD_LIST:
        case RASTA_FIELD_ENTRYLIST:
            pop_list_dialog(main_ctxt, parent);
            lc_start(main_ctxt, field);
            break;

        case RASTA_FIELD_NONE:
        case RASTA_FIELD_ENTRY:
        case RASTA_FIELD_FILE:
        case RASTA_FIELD_READONLY:
        case RASTA_FIELD_DESCRIPTION:
#ifdef ENABLE_DEPRECATED
        case RASTA_FIELD_IPADDR:
        case RASTA_FIELD_VOLUME:
#endif /* ENABLE_DEPRECATED */
        default:
            g_assert_not_reached();
    }

}  /* do_list() */


/*
 * static void do_dialog_screen_next(GtkRastaContext *main_ctxt)
 *
 * Moves to the next screen from this dialog screen
 */
static void do_dialog_screen_next(GtkRastaContext *main_ctxt)
{
    gint rc;
    GtkWidget *screen_frame, *panes, *dialog_pane, *dialog;
    GtkWidget *parent, *entry;
    GList *children, *elem;
    RastaDialogFieldType type;
    RastaDialogField *field;
    REnumeration *er;
    RastaRingValue *r_item;
    gchar *name, *error, *text, *format, *t_o, *t_n;
    gchar *orig_value = NULL;
    gchar *value = NULL;
#ifdef ENABLE_DEPRECATED
    gchar *ip0, *ip1, *ip2, *ip3;
#endif  /* ENABLE_DEPRECATED */

    g_return_if_fail(main_ctxt != NULL);

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    dialog_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "dialog_pane"));
    g_assert(dialog_pane != NULL);

    dialog =
        GTK_WIDGET(g_object_get_data(G_OBJECT(dialog_pane),
                                     "dialog"));
    g_assert(dialog != NULL);

    elem = children = (GList *)
        g_object_get_data(G_OBJECT(dialog), "field_widgets");
    while (elem != NULL)
    {
        parent = GTK_WIDGET(elem->data);
        g_assert(parent != NULL);

        name = (gchar *)g_object_get_data(G_OBJECT(parent), "name");
        g_assert(name != NULL);
        type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent),
                                                 "type"));
        if ((type == RASTA_FIELD_DESCRIPTION) ||
            (type == RASTA_FIELD_READONLY))
        {
            elem = g_list_next(elem);
            continue;
        }

        field = (RastaDialogField *)
            g_object_get_data(G_OBJECT(parent), "field");
        g_assert(field != NULL);

        switch (type)
        {
            case RASTA_FIELD_LIST:
            case RASTA_FIELD_FILE:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "entry"));
                value = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                               0, -1);
                break;

            case RASTA_FIELD_RING:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "entry"));
                value = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                               0, -1);
                er = rasta_dialog_field_enumerate_ring(field);
                while (r_enumeration_has_more(er))
                {
                    r_item =
                        RASTA_RING_VALUE(r_enumeration_get_next(er));
                    g_assert(r_item != NULL);
                    text = rasta_ring_value_get_text(r_item);
                    if (strcmp(text, value) == 0)
                    {
                        g_free(text);
                        g_free(value);
                        value = rasta_ring_value_get_value(r_item);
                        break;
                    }
                }
                r_enumeration_free(er);
                break;

            case RASTA_FIELD_ENTRY:
            case RASTA_FIELD_ENTRYLIST:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "entry"));
                value = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                               0, -1);
                if ((value != NULL) && (value[0] != '\0') &&
                    rasta_entry_dialog_field_is_numeric(field))
                {
                    /* NOTE: widget only enforces numeric */
                    format = rasta_entry_dialog_field_get_format(field);
                    rc = rasta_verify_number_format(format, value);
                    g_free(format);
                    if (rc != 0)
                    {
                        /* TIMBOIZE */
                        text = rasta_dialog_field_get_text(field);
                        switch (rc)
                        {
                            case -ERANGE:
                                error =
                                    g_strdup_printf("The value in the field \"%s\" does not match the required format.",
                                                    text);
                                                    
                                break;
                                
                            case -EINVAL:
                                error =
                                    g_strdup_printf("The number format for the field \"%s\" is invalid.",
                                                    text);
                                break;

                            default:
                                error =
                                    g_strdup_printf("The value in the field \"%s\" is invalid.",
                                                    text);
                                break;
                        }
                        g_free(text);
                        g_free(value);
                        pop_error(main_ctxt, error);
                        g_free(error);
                        return;
                    }
                }
                break;

#ifdef ENABLE_DEPRECATED
            case RASTA_FIELD_IPADDR:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "ip0"));
                ip0 = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                             0, -1);
                if (ip0[0] == '\0')
                {
                    g_free(ip0);
                    ip0 = NULL;
                }
                if (ip0 == NULL)
                    ip0 = g_strdup("0");

                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "ip1"));
                ip1 = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                             0, -1);
                if (ip1[0] == '\0')
                {
                    g_free(ip1);
                    ip1 = NULL;
                }
                if (ip1 == NULL)
                    ip1 = g_strdup("0");

                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "ip2"));
                ip2 = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                             0, -1);
                if (ip2[0] == '\0')
                {
                    g_free(ip2);
                    ip2 = NULL;
                }
                if (ip2 == NULL)
                    ip2 = g_strdup("0");

                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "ip3"));
                ip3 = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                             0, -1);
                if (ip3[0] == '\0')
                {
                    g_free(ip3);
                    ip3 = NULL;
                }
                if (ip3 == NULL)
                    ip3 = g_strdup("0");

                value = g_strdup_printf("%s.%s.%s.%s",
                                        ip0, ip1, ip2, ip3);
                g_free(ip0);
                g_free(ip1);
                g_free(ip2);
                g_free(ip3);
                break;

            case RASTA_FIELD_VOLUME:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "range"));
                value = g_strdup_printf("%.0f",
                                        gtk_adjustment_get_value(GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(entry)))));
                break;
#endif  /* ENABLE_DEPRECATED */

            case RASTA_FIELD_READONLY:
                entry = GTK_WIDGET(g_object_get_data(G_OBJECT(parent),
                                                     "entry"));
                value = gtk_editable_get_chars(GTK_EDITABLE(entry),
                                               0, -1);
                break;

            case RASTA_FIELD_DESCRIPTION:
            case RASTA_FIELD_NONE:
            default:
                g_assert_not_reached();
                break;
        }
        if (rasta_dialog_field_is_required(field) &&
            ((value == NULL) || (value[0] == '\0')))
        {
            g_free(value);
            text = rasta_dialog_field_get_text(field);
            /* TIMBOIZE */
            error = g_strdup_printf("Field \"%s\" is required.",
                                    text);
            g_free(text);
            pop_error(main_ctxt, error);
            g_free(error);
            return;
        }
        orig_value = rasta_symbol_lookup(main_ctxt->ctxt, name);
        t_o = orig_value ? orig_value : "";
        t_n = value ? value : "";
        if (strcmp(t_o, t_n))
            rasta_symbol_put(main_ctxt->ctxt, name, value);
        g_free(orig_value);
        g_free(value);
        elem = g_list_next(elem);
    }

    start_activity(main_ctxt, "Retrieving screen...");
    rasta_dialog_screen_next(main_ctxt->ctxt);
    build_screen(main_ctxt);
}  /* do_dialog_screen_next() */


/*
 * static void do_menu_screen_next(GtkRastaContext *main_ctxt)
 *
 * Moves to the next screen from this menu screen
 */
static void do_menu_screen_next(GtkRastaContext *main_ctxt)
{
    GtkWidget *screen_frame, *panes, *menu_pane, *menu;
    GtkWidget *tog_box, *tog_button;
    GList *buttons, *elem;
    gchar *next_id;

    g_return_if_fail(main_ctxt != NULL);

    screen_frame =
        GTK_WIDGET(g_object_get_data(G_OBJECT(main_ctxt->main_pane),
                                     "screens"));
    g_assert(screen_frame != NULL);

    panes =
        GTK_WIDGET(g_object_get_data(G_OBJECT(screen_frame),
                                     "panes"));
    g_assert(panes != NULL);

    menu_pane =
        GTK_WIDGET(g_object_get_data(G_OBJECT(panes),
                                     "menu_pane"));
    g_assert(menu_pane != NULL);

    menu =
        GTK_WIDGET(g_object_get_data(G_OBJECT(menu_pane),
                                     "menu"));
    g_assert(menu != NULL);

    next_id = NULL;
    elem = buttons = gtk_container_get_children(GTK_CONTAINER(menu));
    while (elem != NULL)
    {
        tog_box = GTK_WIDGET(elem->data);
        g_assert(tog_box != NULL);
        tog_button = GTK_WIDGET(g_object_get_data(G_OBJECT(tog_box),
                                                  "button"));
        g_assert(tog_button != NULL);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tog_button)))
        {
            next_id = g_object_get_data(G_OBJECT(tog_button),
                                        "next_id");
            g_assert(next_id != NULL);
            break;
        }
        elem = g_list_next(elem);
    }
    if (next_id != NULL)
    {
        start_activity(main_ctxt, "Retrieving screen...");
        rasta_menu_screen_next(main_ctxt->ctxt, next_id);
        build_screen(main_ctxt);
    }
}  /* do_menu_screen_next() */


/*
 * static gboolean load_options(gint argc, gchar *argv[],
 *                              GtkRastaContext *main_ctxt)
 *
 * Parses the command line options
 */
static gboolean load_options(gint argc, gchar *argv[],
                             GtkRastaContext *main_ctxt)
{
    gint i;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            break;
        if (strcmp(argv[i], "--") == 0)
        {
            i++;
            break;
        }
        if ((strcmp(argv[i], "--help") == 0) ||
            (strcmp(argv[i], "-h") == 0))
            print_usage(0);
        else if ((strcmp(argv[i], "--version") == 0) ||
            (strcmp(argv[i], "-v") == 0))
        {
            fprintf(stdout, "gtkrasta version " VERSION "\n");
            exit(0);
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            i++;
            if (argv[i][0] == '-')  /* FIXME: should we really error here? */
                return(FALSE);
            main_ctxt->filename = g_strdup(argv[i]);
        }
        else
        {
            fprintf(stderr, "Invalid option: \"%s\"\n", argv[i]);
            return(FALSE);
        }
    }

    if (i < argc)
        main_ctxt->fastpath = g_strdup(argv[i]);

    return(TRUE);
}  /* load_options() */



/*
 * Main program
 */

gint main(gint argc, gchar *argv[])
{
    GtkWidget *top, *box, *label;
    GtkRastaContext *main_ctxt;
    GtkTooltips *tips;

    main_ctxt = g_new0(GtkRastaContext, 1);

    gtk_set_locale();
    gtk_init(&argc, &argv);

    if (load_options(argc, argv, main_ctxt) == FALSE)
        print_usage(-EINVAL);

    top = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(top), 600, 400);
    gtk_window_set_title(GTK_WINDOW(top), "GtkRasta");
    gtk_container_set_border_width(GTK_CONTAINER(top), 2);

    box = gtk_vbox_new(FALSE, 2);
    main_ctxt->main_pane = box;
    gtk_container_add(GTK_CONTAINER(top), box);

    tips = gtk_tooltips_new();
    g_object_set_data(G_OBJECT(box), "tips", tips);

    label = gtk_label_new("GtkRasta");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
    g_object_set_data(G_OBJECT(box), "title", label);

    create_screen_panel(main_ctxt);

    create_button_panel(main_ctxt);

    create_status_panel(main_ctxt);

    g_signal_connect(G_OBJECT(top), "destroy",
                     G_CALLBACK(do_quit), NULL);
    g_signal_connect(G_OBJECT(top), "delete_event",
                     G_CALLBACK(do_quit), NULL);
                       
    gtk_widget_show_all(top);

    start_activity(main_ctxt, "Initializing...");
    g_idle_add(init_start_func, main_ctxt);
    gtk_main();

    return(0);
}  /* main() */

/*
 * Fin
 */
