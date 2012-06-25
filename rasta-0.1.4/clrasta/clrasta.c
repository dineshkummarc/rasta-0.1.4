/*
 * clrasta.c
 *
 * A command line interface for Rasta
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com> and Manish Singh <manish.singh@oracle.com>
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
 * License along with this ; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif  /* HAVE_READLINE */

#include "rasta.h"



/*
 * Typedefs
 */
typedef struct _CLROptions      CLROptions;
typedef struct _CLRMenuItem     CLRMenuItem;



/*
 * Structures
 */
struct _CLROptions
{
    gboolean load_push;
    gchar *filename;
    gchar *state_filename;
    gchar *fastpath;
};

struct _CLRMenuItem
{
    gchar *text;
    gchar *next_id;
};



/*
 * Enumerations
 */
typedef enum _CLRQueryResult
{
    CLR_QUERY_SUCCESS = 0,      /* result contains the selected item */
    CLR_QUERY_BACK,             /* go back a screen */
    CLR_QUERY_HELP,             /* display help text */
    CLR_QUERY_NONE,             /* no result */
    CLR_QUERY_QUIT,             /* quit the application */
    CLR_QUERY_LIST,             /* choose from a list */
    CLR_QUERY_CHANGE,           /* change value */
    CLR_QUERY_MORE,             /* more choices */
    CLR_QUERY_CANCEL,           /* cancel list */
    CLR_QUERY_DONE,             /* done with list */
    CLR_QUERY_UNKNOWN           /* unknown response */
} CLRQueryResult;

typedef enum _CLRQueryFlags
{
    CLR_QUERY_FLAG_BACK = (1<<0),       /* user can go back a screen */
    CLR_QUERY_FLAG_QUIT = (1<<1),       /* user can quit */
    CLR_QUERY_FLAG_REQUIRED = (1<<2),   /* a response is required */
    CLR_QUERY_FLAG_LIST = (1<<3),       /* can choose from a list */
    CLR_QUERY_FLAG_CHANGE = (1<<4),     /* change value */
    CLR_QUERY_FLAG_MORE = (1<<5),       /* more choices */
    CLR_QUERY_FLAG_CANCEL = (1<<6),     /* cancel list */
    CLR_QUERY_FLAG_DONE = (1<<7),       /* done with list */
    CLR_QUERY_FLAG_CONT = (1<<8),       /* continue with default */
    CLR_QUERY_FLAG_HELP = (1<<9)        /* display help text */
} CLRQueryFlags;



/*
 * Prototypes
 */
static gboolean load_options(gint argc, gchar *argv[],
                             CLROptions *options);
static void print_usage();
static RastaContext *load_context(CLROptions *options);
static gint load_state(RastaContext *ctxt, CLROptions *options);
static void pretty_print_title(const gchar *title);
static gchar *clr_readline(const gchar *prompt);
static void pretty_print_choice(guint count, const gchar *text,
                                gboolean marked);
static void clr_print_field(const char *text, const gchar *dfault,
                            gboolean required);
static char *clr_build_prompt(CLRQueryFlags flags,
                              gint start_choice, gint end_choice);
static CLRQueryResult clr_query_response(const gchar *prompt,
                                         CLRQueryFlags flags,
                                         int *res_choice,
                                         gint start_choice,
                                         gint end_choice);
static CLRQueryResult clr_query_list(gchar *valid_choices[],
                                     guint num_choices,
                                     gboolean multiple,
                                     GList **result);
static GList *get_menu_items(RastaContext *ctxt, RastaScreen *screen);
static gchar *get_menu_item_help(RastaContext *ctxt,
                                 RastaScreen *screen,
                                 CLRMenuItem *item);
static void free_menu_items(GList *m_items);
static gboolean run_menu(RastaContext *ctxt, RastaScreen *screen);
static gboolean run_dialog(RastaContext *ctxt, RastaScreen *screen);
static gboolean run_hidden(RastaContext *ctxt, RastaScreen *screen);
static gboolean run_action(RastaContext *ctxt, RastaScreen *screen);
static gint run_initcommand(RastaContext *ctxt, RastaScreen *screen);
static CLRQueryResult run_dialog_field(RastaContext *ctxt,
                                       RastaScreen *screen,
                                       RastaDialogField *d_field);
static CLRQueryResult run_dialog_field_list(RastaContext *ctxt,
                                            RastaDialogField *field);
static CLRQueryResult run_dialog_field_ring(RastaContext *ctxt,
                                            RastaDialogField *field);
static
CLRQueryResult run_dialog_field_readonly(RastaContext *ctxt,
                                         RastaDialogField *field);
static
CLRQueryResult run_dialog_field_description(RastaContext *ctxt,
                                            RastaDialogField *field);
static CLRQueryResult run_dialog_field_entry(RastaContext *ctxt,
                                             RastaDialogField *field);
static
CLRQueryResult run_dialog_field_entrylist(RastaContext *ctxt,
                                          RastaDialogField *field);
static CLRQueryResult run_dialog_field_file(RastaContext *ctxt,
                                            RastaDialogField *field);
static gint run_listcommand(RastaContext *ctxt,
                            RastaDialogField *field,
                            gchar ***items,
                            gint *num_items);
static void ring_items(RastaContext *ctxt,
                       RastaDialogField *field,
                       gchar ***list_items,
                       gchar ***val_items,
                       gint *num_items,
                       gint *i_result);



/*
 * Functions
 */


/*
 * static CLRQueryResult run_dialog_field_list(RastaContext *ctxt,
 *                                             RastaDialogField *field)
 *
 * Handles the user interaction for a dialog field of type LIST.
 */
static CLRQueryResult run_dialog_field_list(RastaContext *ctxt,
                                            RastaDialogField *field)
{
    gboolean done;
    gchar *name, *result, *value, *tmp, *sym_name, *new_value;
    gchar *t_val, *t_new_val, *prompt;
    CLRQueryFlags flags;
    CLRQueryResult ret;
    gint i_result, i, num_items, len;
    gchar **list_items, **res_items;
    GList *l_result, *elem;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);

    flags = CLR_QUERY_FLAG_QUIT | CLR_QUERY_FLAG_BACK |
        CLR_QUERY_FLAG_LIST | CLR_QUERY_FLAG_CONT | CLR_QUERY_FLAG_HELP;

    if (rasta_dialog_field_is_required(field))
        flags |= CLR_QUERY_FLAG_REQUIRED;

    list_items = NULL;
    done = FALSE;
    new_value = g_strdup(value);
    l_result = NULL;
    while (!done)
    {
        clr_print_field(name, new_value ? new_value : "",
                        flags & CLR_QUERY_FLAG_REQUIRED);
        if (flags & CLR_QUERY_FLAG_REQUIRED)
        {
            if (!new_value || !*new_value)
                flags &= ~CLR_QUERY_FLAG_CONT;
            else
                flags |= CLR_QUERY_FLAG_CONT;
        }
        prompt = clr_build_prompt(flags, -1, -1);
        ret = clr_query_response(prompt, flags, NULL, -1, -1);
        g_free(prompt);

        if ((ret == CLR_QUERY_QUIT) || (ret == CLR_QUERY_BACK))
            done = TRUE;
        else if (ret == CLR_QUERY_NONE)
        {
            if (rasta_dialog_field_is_required(field) &&
                (!new_value || !*new_value))
                fprintf(stdout, "This field is required.  Please make a choice.\n");
            else
            {
                ret = CLR_QUERY_SUCCESS;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            tmp = rasta_dialog_field_get_help(field);
            if ((tmp != NULL) && (g_utf8_strlen(tmp, -1) > 0))
                fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                        name, tmp);
            else
                fprintf(stdout,
                        "\nThere is no help on this topic\n");
            g_free(tmp);
        }
        else if (ret == CLR_QUERY_LIST)
        {
            if (!list_items)
                i_result = run_listcommand(ctxt, field,
                                           &list_items, &num_items);
        
            if (!num_items || !list_items)
            {
                fprintf(stderr,
                        "clrasta: There are no items in this list.\n");
                continue;
            }

            /*
             * Only compute initial values from a string if we are
             * in single-select mode.  Handling multiple-select with
             * possibly entered/generated fields is Hard.
             */
            if (!l_result && new_value && *new_value &&
                !rasta_listcommand_allow_multiple(field))
            {
                for (i = 0; i < num_items; i++)
                {
                    if (rasta_listcommand_is_single_column(field))
                    {
                        len = strlen(new_value);
                        if (!strchr(new_value, ' ') &&
                            !strncmp(new_value, list_items[i], len) &&
                            (!list_items[i][len] ||
                             strchr(" \t\n", list_items[i][len])))
                            break;
                    }
                    else if (!strcmp(new_value, list_items[i]))
                        break;
                }
                if (i < num_items)
                    l_result = g_list_append(NULL, GUINT_TO_POINTER(i));
            }

            ret =
                clr_query_list(list_items,
                               num_items,
                               rasta_listcommand_allow_multiple(field),
                               &l_result);

            if (ret == CLR_QUERY_NONE)
                continue;
                                      
            if (l_result == NULL)
            {
                if (new_value)
                    g_free(new_value);
                new_value = g_strdup("");
                continue;
            }

            /* Shouldn't happen - keep only first element */
            if (!rasta_listcommand_allow_multiple(field) &&
                g_list_next(l_result))
            {
                elem = g_list_remove_link(l_result, l_result);
                g_list_free(elem);
            }

            /* Count length */
            i_result = 0;
            elem = l_result;
            while (elem)
            {
                i_result++;
                elem = g_list_next(elem);
            }

            res_items = g_new0(char *, i_result + 1);
            if (!res_items)
            {
                g_list_free(l_result);
                continue;
            }

            elem = l_result;
            i = 0;
            while (elem && (i < i_result))
            {
                if (GPOINTER_TO_UINT(elem->data) >= num_items)
                    continue;

                result = g_strdup(list_items[GPOINTER_TO_UINT(elem->data)]);
                if (result)
                {
                    if (rasta_listcommand_is_single_column(field))
                    {
                        for (tmp = result;
                             (*tmp != '\0') &&
                             (*tmp != '\t') &&
                             (*tmp != ' ');
                             tmp++);
                        *tmp = '\0';
                    }
                }
                res_items[i] = result;
                elem = g_list_next(elem);
                i++;
            }
            g_assert(i == i_result);
            res_items[i] = NULL;

            result = g_strjoinv(" ", res_items);
            g_free(res_items);

            if (!result)
                result = g_strdup("");
            if (new_value)
                g_free(new_value);
            new_value = result;

        }
    }

    t_val = value ? value : "";
    t_new_val = new_value ? new_value : "";
    if (strcmp(t_val, t_new_val))
        rasta_symbol_put(ctxt, sym_name, new_value);

    if (l_result)
        g_list_free(l_result);
    if (list_items)
        g_strfreev(list_items);
    if (new_value)
        g_free(new_value);
    if (value)
        g_free(value);

    return(ret);
}  /* run_dialog_field_list() */


/*
 * static
 * CLRQueryResult run_dialog_field_entrylist(RastaContext *ctxt,
 *                                           RastaDialogField *field)
 *
 * Handles the user interaction for a dialog field of type ENTRYLIST.
 */
static
CLRQueryResult run_dialog_field_entrylist(RastaContext *ctxt,
                                          RastaDialogField *field)
{
    gboolean done;
    gchar *name, *result, *value, *tmp, *sym_name, *new_value;
    gchar *format, *prompt, *conv;
    gchar *t_val, *t_new_val;
    CLRQueryFlags flags;
    CLRQueryResult ret;
    gint i_result, i, num_items, len;
    gchar **list_items, **res_items;
    GList *l_result, *elem;
    gssize br, bw;
    GError *error;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);

    if (rasta_entry_dialog_field_is_numeric(field))
        format = rasta_entry_dialog_field_get_format(field);
    else
        format = NULL;

    flags = CLR_QUERY_FLAG_QUIT | CLR_QUERY_FLAG_BACK |
        CLR_QUERY_FLAG_LIST | CLR_QUERY_FLAG_CHANGE |
        CLR_QUERY_FLAG_CONT | CLR_QUERY_FLAG_HELP;

    if (rasta_dialog_field_is_required(field))
        flags |= CLR_QUERY_FLAG_REQUIRED;

    list_items = NULL;
    l_result = NULL;
    done = FALSE;
    new_value = g_strdup(value);
    while (!done)
    {
        clr_print_field(name, new_value ? new_value : "",
                        flags & CLR_QUERY_FLAG_REQUIRED);
        if (flags & CLR_QUERY_FLAG_REQUIRED)
        {
            if (!new_value || !*new_value)
                flags &= ~CLR_QUERY_FLAG_CONT;
            else
                flags |= CLR_QUERY_FLAG_CONT;
        }
        prompt = clr_build_prompt(flags, -1, -1);
        ret = clr_query_response(prompt, flags, NULL, -1, -1);
        g_free(prompt);

        if ((ret == CLR_QUERY_QUIT) || (ret == CLR_QUERY_BACK))
            done = TRUE;
        else if (ret == CLR_QUERY_NONE)
        {
            if (rasta_dialog_field_is_required(field) &&
                (!new_value || !*new_value))
                fprintf(stdout, "This field is required.  Please make a choice.\n");
            else
            {
                ret = CLR_QUERY_SUCCESS;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            tmp = rasta_dialog_field_get_help(field);
            if ((tmp != NULL) && (g_utf8_strlen(tmp, -1) > 0))
                fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                        name, tmp);
            else
                fprintf(stdout,
                        "\nThere is no help on this topic\n");
            g_free(tmp);
        }
        else if (ret == CLR_QUERY_CHANGE)
        {
            /* FIXME: handle HIDDEN somehow */
            tmp =
                clr_readline(rasta_entry_dialog_field_is_numeric(field)
                             ?  "Enter number: " : "Enter text: ");

            if (rasta_entry_dialog_field_is_numeric(field))
            {
                if (format && tmp &&
                    rasta_verify_number_format(format, tmp))
                {
                    /* FIXME: better error needed */
                    fprintf(stderr,
                            "clrasta: %s is not a valid number\n",
                            tmp);
                    continue;
                }
            }

            error = NULL;
            conv = g_locale_to_utf8(tmp, -1, &br, &bw, &error);
            g_free(tmp);
            if (error)
            {
                fprintf(stderr,
                        "clrasta: Error with encoding conversion: %s\n",
                        error->message ?
                        error->message :
                        "Unknown error");
                if (conv)
                    g_free(conv);
                continue;
            }

            if (new_value)
                g_free(new_value);
            new_value = conv;
            if (l_result)
            {
                g_list_free(l_result);
                l_result = NULL;
            }
        }
        else if (ret == CLR_QUERY_LIST)
        {
            if (!list_items)
                i_result = run_listcommand(ctxt, field,
                                           &list_items, &num_items);

            if (!num_items || !list_items)
            {
                fprintf(stderr,
                        "clrasta: There are no items in this list.\n");
                continue;
            }

            /*
             * Only compute initial values from a string if we are
             * in single-select mode.  Handling multiple-select with
             * possibly entered/generated fields is Hard.
             */
            if (!l_result && new_value && *new_value &&
                !rasta_listcommand_allow_multiple(field))
            {
                for (i = 0; i < num_items; i++)
                {
                    if (rasta_listcommand_is_single_column(field))
                    {
                        len = strlen(new_value);
                        if (!strchr(new_value, ' ') &&
                            !strncmp(new_value, list_items[i], len) &&
                            (!list_items[i][len] ||
                             strchr(" \t\n", list_items[i][len])))
                            break;
                    }
                    else if (!strcmp(new_value, list_items[i]))
                        break;
                }
                if (i < num_items)
                    l_result = g_list_append(NULL, GUINT_TO_POINTER(i));
            }

            ret =
                clr_query_list(list_items,
                               num_items,
                               rasta_listcommand_allow_multiple(field),
                               &l_result);

            if (ret == CLR_QUERY_NONE)
                continue;
                                      
            if (l_result == NULL)
            {
                if (new_value)
                    g_free(new_value);
                new_value = g_strdup("");
                continue;
            }

            /* Shouldn't happen - keep only first element */
            if (!rasta_listcommand_allow_multiple(field) &&
                g_list_next(l_result))
            {
                elem = g_list_remove_link(l_result, l_result);
                g_list_free(elem);
            }

            /* Count length */
            i_result = 0;
            elem = l_result;
            while (elem)
            {
                i_result++;
                elem = g_list_next(elem);
            }

            res_items = g_new0(char *, i_result + 1);
            if (!res_items)
            {
                g_list_free(l_result);
                l_result = NULL;
                continue;
            }

            elem = l_result;
            i = 0;
            while (elem && (i < i_result))
            {
                if (GPOINTER_TO_UINT(elem->data) >= num_items)
                    continue;

                result = g_strdup(list_items[GPOINTER_TO_UINT(elem->data)]);
                if (result)
                {
                    if (rasta_listcommand_is_single_column(field))
                    {
                        for (tmp = result;
                             (*tmp != '\0') &&
                             (*tmp != '\t') &&
                             (*tmp != ' ');
                             tmp++);
                        *tmp = '\0';
                    }
                }
                res_items[i] = result;
                elem = g_list_next(elem);
                i++;
            }
            g_assert(i == i_result);
            res_items[i] = NULL;

            result = g_strjoinv(" ", res_items);
            g_free(res_items);

            if (!result)
                result = g_strdup("");
            if (new_value)
                g_free(new_value);
            new_value = result;
        }
    }

    t_val = value ? value : "";
    t_new_val = new_value ? new_value : "";
    if (strcmp(t_val, t_new_val))
        rasta_symbol_put(ctxt, sym_name, new_value);

    if (list_items)
        g_strfreev(list_items);
    if (format)
        g_free(format);
    if (new_value)
        g_free(new_value);
    if (value)
        g_free(value);

    return(ret);
}  /* run_dialog_field_entrylist() */


/*
 * static void ring_items(RastaContext *ctxt,
 *                        RastaDialogField *field,
 *                        gchar ***list_items,
 *                        gchar ***val_items
 *                        gint *num_items,
 *                        gint *i_result)
 *
 * Reads the RINGVALUEs from the dialog field and fills list_items
 * with the displayable text, val_items with the matching values,
 * num_items with the count of the items, and i_result with the
 * already selected item (if there is one).
 */
static void ring_items(RastaContext *ctxt,
                       RastaDialogField *field,
                       gchar ***list_items,
                       gchar ***val_items,
                       gint *num_items,
                       gint *i_result)
{
    REnumeration *ren;
    gint count, i;
    RastaRingValue *val;
    gchar *sym_name, *sym_val;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(field != NULL);
    g_return_if_fail(list_items != NULL);
    g_return_if_fail(val_items != NULL);
    g_return_if_fail(num_items != NULL);
    g_return_if_fail(i_result != NULL);

    count = 0;
    *num_items = 0;
    *list_items = NULL;
    *val_items = NULL;

    sym_name = rasta_dialog_field_get_symbol_name(field);
    if (!sym_name)
        g_assert_not_reached();

    sym_val = rasta_symbol_lookup(ctxt, sym_name);

    ren = rasta_dialog_field_enumerate_ring(field);
    if (!ren)
        return;

    while (r_enumeration_has_more(ren))
    {
        r_enumeration_get_next(ren);
        count++;
    }
    r_enumeration_free(ren);

    *list_items = g_new0(gchar *, count + 1);
    if (!*list_items)
        return;
    *val_items = g_new0(gchar *, count + 1);
    if (!*val_items)
    {
        g_free(list_items);
        return;
    }

    ren = rasta_dialog_field_enumerate_ring(field);
    if (!ren)
        return;

    i = 0;
    *i_result = -1;
    while (r_enumeration_has_more(ren) && (i < count))
    {
        val = RASTA_RING_VALUE(r_enumeration_get_next(ren));
        if (!val)
            continue;  /* Should we error? */

        (*list_items)[i] = rasta_ring_value_get_text(val);
        (*val_items)[i] = rasta_ring_value_get_value(val);

        if ((*val_items)[i] && sym_val &&
            !strcmp((*val_items)[i], sym_val))
            *i_result = i;
        i++;
    }
    r_enumeration_free(ren);

    *num_items = i;
}  /* ring_items() */

/*
 * static CLRQueryResult run_dialog_field_ring(RastaContext *ctxt,
 *                                             RastaDialogField *field)
 *
 * Handles user interaction for a dialog field of type RING.
 */
static CLRQueryResult run_dialog_field_ring(RastaContext *ctxt,
                                            RastaDialogField *field)
{
    gboolean done;
    gchar *name, *value, *sym_name, *new_value, *disp_val;
    gchar *t_val, *t_new_val, *prompt, *tmp;
    CLRQueryFlags flags;
    CLRQueryResult ret;
    gint i_result, num_items;
    gchar **list_items, **val_items;
    GList *l_result;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);

    flags = CLR_QUERY_FLAG_QUIT | CLR_QUERY_FLAG_BACK |
        CLR_QUERY_FLAG_LIST | CLR_QUERY_FLAG_CONT | CLR_QUERY_FLAG_HELP;

    if (rasta_dialog_field_is_required(field))
        flags |= CLR_QUERY_FLAG_REQUIRED;

    list_items = NULL;
    ring_items(ctxt, field,
               &list_items, &val_items, &num_items, &i_result);

    l_result = NULL;
    if (i_result > -1)
    {
        disp_val = list_items[i_result];
        l_result = g_list_append(NULL, GUINT_TO_POINTER(i_result));
    }
    else
        disp_val = NULL;

    done = FALSE;
    new_value = g_strdup(value);
    while (!done)
    {
        clr_print_field(name, disp_val ? disp_val : "",
                        flags & CLR_QUERY_FLAG_REQUIRED);
        if (flags & CLR_QUERY_FLAG_REQUIRED)
        {
            if (!new_value || !*new_value)
                flags &= ~CLR_QUERY_FLAG_CONT;
            else
                flags |= CLR_QUERY_FLAG_CONT;
        }
        prompt = clr_build_prompt(flags, -1, -1);
        ret = clr_query_response(prompt, flags, NULL, -1, -1);
        g_free(prompt);

        if ((ret == CLR_QUERY_QUIT) || (ret == CLR_QUERY_BACK))
            done = TRUE;
        else if (ret == CLR_QUERY_NONE)
        {
            if (rasta_dialog_field_is_required(field) &&
                (!new_value || !*new_value))
                fprintf(stdout, "This field is required.  Please make a choice.\n");
            else
            {
                ret = CLR_QUERY_SUCCESS;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            tmp = rasta_dialog_field_get_help(field);
            if ((tmp != NULL) && (g_utf8_strlen(tmp, -1) > 0))
                fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                        name, tmp);
            else
                fprintf(stdout,
                        "\nThere is no help on this topic\n");
            g_free(tmp);
        }
        else if (ret == CLR_QUERY_LIST)
        {
            if (!num_items || !list_items || !val_items)
            {
                fprintf(stderr,
                        "clrasta: There are no items in this list.\n");
                continue;
            }

            ret =
                clr_query_list(list_items, num_items, FALSE, &l_result);

            if (ret == CLR_QUERY_NONE)
                continue;
                                      
            if (l_result == NULL)
            {
                if (new_value)
                    g_free(new_value);
                new_value = g_strdup("");
                continue;
            }

            if (GPOINTER_TO_UINT(l_result->data) < num_items);
            {
                if (new_value)
                    g_free(new_value);
                new_value =
                    g_strdup(val_items[GPOINTER_TO_UINT(l_result->data)]);
                disp_val = list_items[GPOINTER_TO_UINT(l_result->data)];
            }
        }
    }

    t_val = value ? value : "";
    t_new_val = new_value ? new_value : "";
    if (strcmp(t_val, t_new_val))
        rasta_symbol_put(ctxt, sym_name, new_value);

    g_strfreev(list_items);
    g_strfreev(val_items);
    if (l_result)
        g_list_free(l_result);
    if (new_value)
        g_free(new_value);
    if (value)
        g_free(value);

    return(ret);
}  /* run_dialog_field_ring() */


/*
 * static
 * CLRQueryResult run_dialog_field_readonly(RastaContext *ctxt,
 *                                          RastaDialogField *field)
 *
 * Handles user interaction for a dialog field of type READONLY.
 */
static CLRQueryResult run_dialog_field_readonly(RastaContext *ctxt,
                                                RastaDialogField *field)
{
    gchar *name, *value, *sym_name;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);

    clr_print_field(name, value ? value : "", FALSE);

    return(CLR_QUERY_SUCCESS);
}  /* run_dialog_field_readonly() */


/*
 * static
 * CLRQueryResult run_dialog_field_description(RastaContext *ctxt,
 *                                             RastaDialogField *field)
 *
 * Handles user interaction for a dialog field of type DESCRIPTION.
 */
static
CLRQueryResult run_dialog_field_description(RastaContext *ctxt,
                                            RastaDialogField *field)
{
    gchar *name;

    name = rasta_dialog_field_get_text(field);

    clr_print_field(name, NULL, FALSE);

    return(CLR_QUERY_SUCCESS);
}  /* run_dialog_field_description() */


/*
 * static CLRQueryResult run_dialog_field_entry(RastaContext *ctxt,
 *                                              RastaDialogField *field)
 *
 * Handles user interaction for a dialog field of type ENTRY.
 */
static CLRQueryResult run_dialog_field_entry(RastaContext *ctxt,
                                             RastaDialogField *field)
{
    gboolean done;
    gchar *name, *value, *sym_name, *new_value, *format, *tmp;
    gchar *t_val, *t_new_val, *prompt, *conv;
    gsize br, bw;
    GError *error;
    CLRQueryFlags flags;
    CLRQueryResult ret;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);
    if (rasta_entry_dialog_field_is_numeric(field))
        format = rasta_entry_dialog_field_get_format(field);
    else
        format = NULL;

    flags = CLR_QUERY_FLAG_QUIT | CLR_QUERY_FLAG_BACK |
        CLR_QUERY_FLAG_CHANGE | CLR_QUERY_FLAG_CONT |
        CLR_QUERY_FLAG_HELP;

    if (rasta_dialog_field_is_required(field))
        flags |= CLR_QUERY_FLAG_REQUIRED;

    done = FALSE;
    new_value = g_strdup(value);
    while (!done)
    {
        clr_print_field(name, new_value ? new_value : "",
                        flags & CLR_QUERY_FLAG_REQUIRED);
        if (flags & CLR_QUERY_FLAG_REQUIRED)
        {
            if (!new_value || !*new_value)
                flags &= ~CLR_QUERY_FLAG_CONT;
            else
                flags |= CLR_QUERY_FLAG_CONT;
        }
        prompt = clr_build_prompt(flags, -1, -1);
        ret = clr_query_response(prompt, flags, NULL, -1, -1);
        g_free(prompt);

        if ((ret == CLR_QUERY_QUIT) || (ret == CLR_QUERY_BACK))
            done = TRUE;
        else if (ret == CLR_QUERY_NONE)
        {
            if (rasta_dialog_field_is_required(field) &&
                (!new_value || !*new_value))
                fprintf(stdout, "This field is required.  Please make a choice.\n");
            else
            {
                ret = CLR_QUERY_SUCCESS;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            tmp = rasta_dialog_field_get_help(field);
            if ((tmp != NULL) && (g_utf8_strlen(tmp, -1) > 0))
                fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                        name, tmp);
            else
                fprintf(stdout,
                        "\nThere is no help on this topic\n");
            g_free(tmp);
        }
        else if (ret == CLR_QUERY_CHANGE)
        {
            /* FIXME: handle HIDDEN somehow */
            tmp =
                clr_readline(rasta_entry_dialog_field_is_numeric(field)
                             ?  "Enter number: " : "Enter text: ");

            if (rasta_entry_dialog_field_is_numeric(field))
            {
                if (format && tmp &&
                    rasta_verify_number_format(format, tmp))
                {
                    /* FIXME: better error needed */
                    fprintf(stderr,
                            "clrasta: %s is not a valid number\n",
                            tmp);
                    continue;
                }
            }

            error = NULL;
            conv = g_locale_to_utf8(tmp, -1, &br, &bw, &error);
            g_free(tmp);
            if (error)
            {
                fprintf(stderr,
                        "clrasta: Error with encoding conversion: %s\n",
                        error->message ?
                        error->message :
                        "Unknown error");
                if (conv)
                    g_free(conv);
                continue;
            }

            if (new_value)
                g_free(new_value);
            new_value = conv;
        }
        else if (ret == CLR_QUERY_UNKNOWN)
            fprintf(stdout, "clrasta: Invalid choice\n");
    }

    t_val = value ? value : "";
    t_new_val = new_value ? new_value : "";
    if (strcmp(t_val, t_new_val))
        rasta_symbol_put(ctxt, sym_name, new_value);

    if (format)
        g_free(format);
    if (new_value)
        g_free(new_value);
    if (value)
        g_free(value);

    return(ret);
}  /* run_dialog_field_entry() */


/*
 * static CLRQueryResult run_dialog_field_file(RastaContext *ctxt,
 *                                             RastaDialogField *field)
 *
 * Handles user interaction for a dialog field of type FILE.
 */
static CLRQueryResult run_dialog_field_file(RastaContext *ctxt,
                                            RastaDialogField *field)
{
    gboolean done;
    gchar *name, *value, *sym_name, *new_value;
    gchar *t_val, *t_new_val, *prompt, *conv, *tmp;
    CLRQueryFlags flags;
    CLRQueryResult ret;
    gsize br, bw;
    GError *error;

    name = rasta_dialog_field_get_text(field);

    sym_name = rasta_dialog_field_get_symbol_name(field);
    value = rasta_symbol_lookup(ctxt, sym_name);

    flags = CLR_QUERY_FLAG_QUIT | CLR_QUERY_FLAG_BACK |
        CLR_QUERY_FLAG_CHANGE | CLR_QUERY_FLAG_CONT |
        CLR_QUERY_FLAG_HELP;

    if (rasta_dialog_field_is_required(field))
        flags |= CLR_QUERY_FLAG_REQUIRED;

    done = FALSE;
    new_value = g_strdup(value);
    while (!done)
    {
        clr_print_field(name, new_value ? new_value : "",
                        flags & CLR_QUERY_FLAG_REQUIRED);
        if (flags & CLR_QUERY_FLAG_REQUIRED)
        {
            if (!new_value || !*new_value)
                flags &= ~CLR_QUERY_FLAG_CONT;
            else
                flags |= CLR_QUERY_FLAG_CONT;
        }
        prompt = clr_build_prompt(flags, -1, -1);
        ret = clr_query_response(prompt, flags, NULL, -1, -1);
        g_free(prompt);

        if ((ret == CLR_QUERY_QUIT) || (ret == CLR_QUERY_BACK))
            done = TRUE;
        else if (ret == CLR_QUERY_NONE)
        {
            if (rasta_dialog_field_is_required(field) &&
                (!new_value || !*new_value))
                fprintf(stdout, "This field is required.  Please make a choice.\n");
            else
            {
                ret = CLR_QUERY_SUCCESS;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            tmp = rasta_dialog_field_get_help(field);
            if ((tmp != NULL) && (g_utf8_strlen(tmp, -1) > 0))
                fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                        name, tmp);
            else
                fprintf(stdout,
                        "\nThere is no help on this topic\n");
            g_free(tmp);
        }
        else if (ret == CLR_QUERY_CHANGE)
        {
            tmp = clr_readline("Enter filename: ");

            error = NULL;
            conv = g_locale_to_utf8(tmp, -1, &br, &bw, &error);
            g_free(tmp);
            if (error)
            {
                fprintf(stderr,
                        "clrasta: Error with encoding conversion: %s\n",
                        error->message ?
                        error->message :
                        "Unknown error");
                if (conv)
                    g_free(conv);
                continue;
            }

            if (new_value)
                g_free(new_value);
            new_value = conv;
        }
        else if (ret == CLR_QUERY_UNKNOWN)
            fprintf(stdout, "clrasta: Invalid choice\n");
    }

    t_val = value ? value : "";
    t_new_val = new_value ? new_value : "";
    if (strcmp(t_val, t_new_val))
        rasta_symbol_put(ctxt, sym_name, new_value);

    if (new_value)
        g_free(new_value);
    if (value)
        g_free(value);

    return(ret);
}  /* run_dialog_field_file() */


/*
 * static void print_usage()
 *
 * Prints a usage statement
 */
static void print_usage()
{
    fprintf(stderr,
            "Usage: clrasta [--load-push] [--file <filename>]\n"
            "               [<fastpath>]\n");
}  /* print_usage() */


/*
 * static gboolean load_options(gint argc, gchar *argv[],
 *                              CLROptions *options)
 *
 * Scans argv for options
 */
static gboolean load_options(gint argc, gchar *argv[],
                             CLROptions *options)
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
        if (strcmp(argv[i], "--load-push") == 0)
            options->load_push = TRUE;
        else if ((strcmp(argv[i], "--help") == 0) ||
                 (strcmp(argv[i], "-h") == 0))
        {
            print_usage();
            exit(0);
        }
        else if ((strcmp(argv[i], "--version") == 0) ||
                 (strcmp(argv[i], "-v") == 0))
        {
            fprintf(stdout, "clrasta version " VERSION "\n");
            exit(0);
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            i++;
            if (argv[i][0] == '-')
                return(FALSE);
            options->filename = argv[i];
        }
        else if (strcmp(argv[i], "--state") == 0)
        {
            i++;
            if (argv[i][0] == '-')
                return(FALSE);
            options->state_filename = argv[i];
        }
        else
            return(FALSE);
    }

    if (options->filename == NULL)
        options->filename = g_build_filename(_RASTA_DIR,
                                             RASTA_SYSTEM_FILE,
                                             NULL);
    if (i < argc)
    {
        if (options->state_filename == NULL)
            options->fastpath = argv[i];
        else
            return(FALSE);
    }

    return(TRUE);
}  /* load_options() */


/*
 * static RastaContext *load_context(CLROptions *options)
 *
 * Loads the RastaContext
 */
static RastaContext *load_context(CLROptions *options)
{
    gint fd, rc;
    gulong page_size;
    gchar *buffer;
    RastaContext *ctxt;

    page_size = getpagesize();


    if (options->load_push == FALSE)
        return(rasta_context_init(options->filename,
                                  options->fastpath));

    buffer = g_new(gchar, page_size);
    if (buffer == NULL)
        return(NULL);

    ctxt = rasta_context_init_push(options->filename,
                                   options->fastpath);
    if (ctxt == NULL)
        return(NULL);

    fd = open(options->filename, O_RDONLY);
    if (fd == -1)
    {
        rasta_context_destroy(ctxt);
        return(NULL);
    }

    while ((rc = read(fd, buffer, page_size)) != 0)
    {
        if ((rc == -1) && (errno != EINTR))
        {
            close(fd);
            rasta_context_destroy(ctxt);
            return(NULL);
        }
        rc = rasta_context_parse_chunk(ctxt,
                                       buffer,
                                       rc);
        if (rc != 0)
        {
            close(fd);
            rasta_context_destroy(ctxt);
            return(NULL);
        }
    }
    g_free(buffer);
    rc = rasta_context_parse_chunk(ctxt, NULL, 0);
    if (rc != 0)
    {
        close(fd);
        rasta_context_destroy(ctxt);
        return(NULL);
    }

    close(fd);
    return(ctxt);
}  /* load_context() */


/*
 * static gint load_state(RastaContext *ctxt, CLROptions *options)
 *
 * Loads the state for a RastaContext
 */
static gint load_state(RastaContext *ctxt, CLROptions *options)
{
    gint fd, rc;
    gulong page_size;
    gchar *buffer;

    page_size = getpagesize();

    if (options->load_push == FALSE)
        return(rasta_context_load_state(ctxt,
                                        options->state_filename));

    buffer = g_new(gchar, page_size);
    if (buffer == NULL)
        return(-ENOMEM);

    rc = rasta_context_load_state_push(ctxt,
                                       options->state_filename);

    if (rc != 0)
        return(rc);

    fd = open(options->filename, O_RDONLY);
    if (fd == -1)  /* FIXME: clean up push context? */
        return(-errno);

    while ((rc = read(fd, buffer, page_size)) != 0)
    {
        if ((rc == -1) && (errno != EINTR))
        {
            /* FIXME: clean up push context? */
            close(fd);
            return(-errno);
        }
        rc = rasta_context_parse_state_chunk(ctxt,
                                             buffer,
                                             rc);
        if (rc != 0)
        {
            close(fd);
            return(rc);
        }
    }
    g_free(buffer);
    rc = rasta_context_parse_state_chunk(ctxt, NULL, 0);

    close(fd);
    return(rc);
}  /* load_state() */


/*
 * static void pretty_print_title(const gchar *title)
 *
 * Print the title for a screen
 */
static void pretty_print_title(const gchar *title)
{
    gchar buf[(70 * 4) + 1];  /* (80 cols - 5 col border) * utf8 */
    const gchar *ptr;

    g_return_if_fail(title != NULL);

    fprintf(stdout, "\n\n   +------------------------------------------------------------------------+\n");

    /* FIXME: Not really i18n'd well */
    ptr = title;
    while (g_utf8_strlen(ptr, -1) > 70)
    {
        g_utf8_strncpy(buf, ptr, 70);
        fprintf(stdout, "   | %-70s |\n", buf);
        ptr = g_utf8_offset_to_pointer(ptr, 70);
    }
    fprintf(stdout, "   | %-70s |\n", ptr);

    fprintf(stdout, "   +------------------------------------------------------------------------+\n");
}  /* pretty_print_title() */


/*
 * static void pretty_print_choice(guint count, const gchar *text,
 *                                 gboolean marked)
 *
 * Prints a choice nicely
 */
static void pretty_print_choice(guint count, const gchar *text,
                                gboolean marked)
{
    gchar buf[(70 * 4) + 1];  /* 70 chars * 4 for utf8 */
    const gchar *ptr;
    gboolean initial;

    g_return_if_fail(text != NULL);

    /* FIXME: Not really i18n'd well */
    initial = TRUE;
    ptr = text;
    while (g_utf8_strlen(ptr, -1) > 70)
    {
        g_utf8_strncpy(buf, ptr, 70);
        if (initial != FALSE)
            fprintf(stdout, " %c % 2d) %s\n",
                    marked ? '*' : ' ', count, buf);
        else
            fprintf(stdout, "        %s\n", buf);
        if (initial == TRUE)
            initial = FALSE;
        ptr = g_utf8_offset_to_pointer(ptr, 70);
    }
    if (initial != FALSE)
        fprintf(stdout, " %c % 2d) %s\n",
                marked ? '*' : ' ', count, ptr);
    else
        fprintf(stdout, "        %s\n", ptr);
}  /* pretty_print_choice() */


/*
 * static gchar *clr_readline(const gchar *prompt)
 *
 * Simple wrapper around the readline() call
 */
static gchar *clr_readline(const gchar *prompt)
{
    gchar *r;

#ifdef HAVE_READLINE   
    static gboolean initialized = FALSE;
#else
    gchar buffer[2048];
#endif  /* HAVE_READLINE */

#ifdef HAVE_READLINE
    if (initialized == FALSE)
    {
        rl_bind_key('\t', rl_insert);  /* No completion */
        initialized = TRUE;
    }

    r = readline(prompt);
    if (r == NULL)
#else
    if (prompt != NULL)
        fprintf(stdout, prompt);
    r = fgets(buffer, 2048, stdin);

    if (r != NULL)
    {
        buffer[strlen(buffer) - 1] = '\0';
        r = g_strdup(buffer);
    }
    else
#endif /* HAVE_READLINE */
        fprintf(stdout, "\n");

    return(r);
}  /* clr_readline() */


/*
 * static CLRQueryResult clr_query_choice(gchar *valid_choices[],
 *                                        guint num_choices,
 *                                        gint *result,
 *                                        gint flags);
 *
 * Asks the user about valid choices.  Sets result to the response,
 * returns 0 on success, or a special response.
 */
static CLRQueryResult clr_query_choice(gchar *valid_choices[],
                                       guint num_choices,
                                       gint *result,
                                       gint flags)
{
    gboolean done;
    gint count, start, res_n, end_choice;
    CLRQueryResult ret = CLR_QUERY_QUIT;
    gboolean choose_help = FALSE;
    CLRQueryFlags newflags;
    char *prompt;

    g_return_val_if_fail(valid_choices != NULL, CLR_QUERY_QUIT);
    g_return_val_if_fail(num_choices > 0, CLR_QUERY_QUIT);

    start = 0;
    done = FALSE;
    fprintf(stdout, "\n");
    while (done == FALSE)
    {
        for (count = 0; (count < 10) &&
                        (count < (num_choices - start)); count++)
        {
            pretty_print_choice(count + 1,
                                valid_choices[start + count], FALSE);
        }
        fprintf(stdout, "\n");
        
        newflags = flags;
        if (num_choices > 10)
            newflags |= CLR_QUERY_FLAG_MORE;
        end_choice = (num_choices - start) > 10 ?
            10 : (num_choices - start);

        prompt = clr_build_prompt(newflags, 1, end_choice);
        ret = clr_query_response(prompt, newflags,
                                 &res_n, 1, end_choice);
        g_free(prompt);

        if (ret == CLR_QUERY_MORE)
        {
            if ((num_choices - start) > 10)
                start += 10;
            else
                start = 0;

            continue;
        }
        else if (ret == CLR_QUERY_SUCCESS)
        {
            res_n--; /* 0 based */
            if ((res_n > -1) &&
                (res_n < ((num_choices - start) > 10 ?
                          10 : (num_choices - start))))
            {
                *result = res_n + start;
                if (choose_help != FALSE)
                    ret = CLR_QUERY_HELP;
                done = TRUE;
            }
        }
        else if (ret == CLR_QUERY_HELP)
        {
            choose_help = TRUE;
            continue;
        }
        else if ((ret != CLR_QUERY_NONE) && (ret != CLR_QUERY_UNKNOWN))
            done = TRUE;

        if (done == FALSE)
            fprintf(stderr, "clrasta: Invalid choice\n\n");
    }

    return(ret);
}  /* clr_query_choice() */


/*
 * static void clr_print_field(const char *text, const gchar *dfault,
 *                             gboolean required)
 *
 *
 * Prints the field name, optionally the field's current value,
 * and a message to mark when a required field is currently empty.
 */
static void clr_print_field(const char *text, const gchar *dfault,
                            gboolean required)
{
    GString *str;
    gchar *res;
    gsize br, bw;
    GError *error;

    g_return_if_fail(text != NULL);

    str = g_string_new("");

    g_string_append_printf(str, "\n%s", text);
    if (dfault)
    {
        g_string_append_printf(str, " [%s]%s",
                               dfault,
                               (!*dfault && required) ?
                               " (value is required)" : "");
    }
    g_string_append_printf(str, "\n");

    error = NULL;
    res = g_locale_from_utf8(str->str, -1, &br, &bw, &error);

    g_string_free(str, TRUE);

    if (error)
    {
        fprintf(stderr, "clrasta: Error with encoding conversion: %s\n",
                error->message ? error->message : "Unknown error");
        g_clear_error(&error);
    }

    if (res && (bw > 0))
        fprintf(stdout, "%s", res);
}  /* clr_print_field() */


/*
 * static char *clr_build_prompt(CLRQueryFlags flags,
 *                               gint start_choice, gint end_choice)
 *
 * Generates a prompt string and returns it.
 */
static char *clr_build_prompt(CLRQueryFlags flags,
                              gint start_choice, gint end_choice)
{
    GString *str;
    char *ret;

    str = g_string_new("Enter choice (");

    if ((start_choice > -1) && (end_choice > -1))
        g_string_append_printf(str, "[%d-%d], ",
                               start_choice, end_choice);
    if (flags & CLR_QUERY_FLAG_MORE)
        g_string_append(str, "[m]ore, ");
    if (flags & CLR_QUERY_FLAG_DONE)
        g_string_append(str, "[d]one, ");
    if (flags & CLR_QUERY_FLAG_CANCEL)
        g_string_append(str, "[c]ancel, ");
    if (flags & CLR_QUERY_FLAG_CHANGE)
        g_string_append(str, "[c]hange, ");
    if (flags & CLR_QUERY_FLAG_LIST)
        g_string_append(str, "[l]ist, ");
    if (flags & CLR_QUERY_FLAG_BACK)
        g_string_append(str, "[b]ack, ");
    if (flags & CLR_QUERY_FLAG_QUIT)
        g_string_append(str, "[q]uit, ");
    if (flags & CLR_QUERY_FLAG_CONT)
        g_string_append(str, "<ENTER> to continue, ");
    if (flags & CLR_QUERY_FLAG_HELP)
        g_string_append(str, "[h]elp");
    else
        g_string_truncate(str, str->len - 2);
    g_string_append(str, "): ");

    ret = str->str;
    g_string_free(str, FALSE);

    return(ret);
}  /* clr_build_prompt */


/*
 * static CLRQueryResult clr_query_response(const gchar *prompt,
 *                                          CLRQueryFlags flags,
 *                                          int *res_choice,
 *                                          gint start_choice,
 *                                          gint end_choice)
 *
 * Given the prompt, flags, and possibly range of choices, asks the user
 * for a valid response and sets the proper return code.
 */
static CLRQueryResult clr_query_response(const gchar *prompt,
                                         CLRQueryFlags flags,
                                         int *res_choice,
                                         gint start_choice,
                                         gint end_choice)
{
    CLRQueryResult ret;
    gchar *r, *ptr;

    r = clr_readline(prompt);
    if (r == NULL)
        return(CLR_QUERY_QUIT);

    ret = CLR_QUERY_UNKNOWN;

    if (!r)
        return(CLR_QUERY_NONE);
    else if (!*r)
        ret = CLR_QUERY_NONE;
    else if ((flags & CLR_QUERY_FLAG_QUIT) && (strcmp(r, "q") == 0))
        ret = CLR_QUERY_QUIT;
    else if ((flags & CLR_QUERY_FLAG_LIST) && (strcmp(r, "l") == 0))
        ret = CLR_QUERY_LIST;
    else if ((flags & CLR_QUERY_FLAG_BACK) && (strcmp(r, "b") == 0))
        ret = CLR_QUERY_BACK;
    else if ((flags & CLR_QUERY_FLAG_CHANGE) && (strcmp(r, "c") == 0))
        ret = CLR_QUERY_CHANGE;
    else if ((flags & CLR_QUERY_FLAG_MORE) && (strcmp(r, "m") == 0))
        ret = CLR_QUERY_MORE;
    else if ((flags & CLR_QUERY_FLAG_DONE) && (strcmp(r, "d") == 0))
        ret = CLR_QUERY_DONE;
    else if ((flags & CLR_QUERY_FLAG_CANCEL) && (strcmp(r, "c") == 0))
        ret = CLR_QUERY_CANCEL;
    else if ((flags & CLR_QUERY_FLAG_HELP) && (strcmp(r, "h") == 0))
        ret = CLR_QUERY_HELP;
    else if (res_choice)
    {
        *res_choice = (gint)strtol(r, &ptr, 10);
        if (ptr && !*ptr)
            ret = CLR_QUERY_SUCCESS;
    }

    g_free(r);

    return(ret);
}  /* clr_query_response() */


/*
 * static CLRQueryResult clr_query_list(gchar *valid_choices[],
 *                                      guint num_choices,
 *                                      gboolean multiple,
 *                                      GList **result)
 *
 * Asks the user choices in a list.  Returns CLR_QUERY_SUCCESS or
 * CLR_QUERY_NONE.
 */
static CLRQueryResult clr_query_list(gchar *valid_choices[],
                                     guint num_choices,
                                     gboolean multiple,
                                     GList **result)
{
    gboolean done;
    gchar *prompt;
    gint count, start, res_n, end_choice, last_marked;
    CLRQueryResult ret;
    CLRQueryFlags newflags;
    gboolean *marked;
    GList *elem;

    g_return_val_if_fail(valid_choices != NULL, CLR_QUERY_NONE);
    g_return_val_if_fail(num_choices > 0, CLR_QUERY_NONE);
    g_return_val_if_fail(result != NULL, CLR_QUERY_NONE);

    marked = g_new0(gboolean, num_choices);
    elem = *result;
    last_marked = -1;
    while (elem)
    {
        last_marked = GPOINTER_TO_UINT(elem->data);
        marked[last_marked] = TRUE;
        if (!multiple)
            break;
        last_marked = -1;
        elem = g_list_next(elem);
    }
    if (*result)
    {
        g_list_free(*result);  /* In/Out param */
        *result = NULL;
    }

    start = 0;
    done = FALSE;
    while (done == FALSE)
    {
        fprintf(stdout, "\n");
        for (count = 0; (count < 10) &&
                        (count < (num_choices - start)); count++)
        {
            pretty_print_choice(count + 1,
                                valid_choices[start + count],
                                marked[start + count]);
        }
        fprintf(stdout, "\n");
        
        newflags = CLR_QUERY_FLAG_CANCEL | CLR_QUERY_FLAG_DONE;
        if (num_choices > 10)
            newflags |= CLR_QUERY_FLAG_MORE;
        end_choice = (num_choices - start) > 10 ?
            10 : (num_choices - start);
        prompt = clr_build_prompt(newflags, 1, end_choice);

        ret = clr_query_response(prompt, newflags,
                                 &res_n, 1, end_choice);
        g_free(prompt);

        if (ret == CLR_QUERY_MORE)
        {
            if ((num_choices - start) > 10)
                start += 10;
            else
                start = 0;
        }
        else if (ret == CLR_QUERY_SUCCESS)
        {
            res_n--; /* 0 based */
            if ((res_n > -1) &&
                (res_n < ((num_choices - start) > 10 ?
                          10 : (num_choices - start))))
            {
                marked[res_n + start] = !marked[res_n + start];
            }
            if (!multiple)
            {
                if ((res_n + start) == last_marked)
                    last_marked = -1;
                else
                {
                    if (last_marked  != -1)
                        marked[last_marked] = FALSE;
                    last_marked = res_n + start;
                }
            }
        }
        else if (ret == CLR_QUERY_CANCEL)
        {
            ret = CLR_QUERY_NONE;
            done = TRUE;
        }
        else if (ret == CLR_QUERY_DONE)
        {
            ret = CLR_QUERY_SUCCESS;
            done = TRUE;
        }
        else if (ret == CLR_QUERY_UNKNOWN)
            fprintf(stdout, "Invalid choice\n");
    }

    if (ret == CLR_QUERY_SUCCESS)
    {
        for (*result = NULL, count = 0; count < num_choices; count++)
        {
            if (marked[count])
                *result = g_list_prepend(*result,
                                         GUINT_TO_POINTER(count));
        }
    }

    return(ret);
}  /* clr_query_list() */



/*
 * static GList *get_menu_items(RastaContext *ctxt, RastaScreen *screen)
 *
 * Gets all the menu items into a local list
 */
static GList *get_menu_items(RastaContext *ctxt, RastaScreen *screen)
{
    GList *m_items;
    RastaMenuItem *cur_item;
    CLRMenuItem *item;
    REnumeration *enumer;

    g_return_val_if_fail(screen != NULL, NULL);

    enumer = rasta_menu_screen_enumerate_items(ctxt, screen);
    m_items = NULL;
    while (r_enumeration_has_more(enumer))
    {
        cur_item = RASTA_MENU_ITEM(r_enumeration_get_next(enumer));
        if (cur_item == NULL)
            continue;  /* should we error? */

        item = g_new0(CLRMenuItem, 1);
        if (item == NULL)
        {
            fprintf(stderr, "clrasta: Unable to allocate memory\n");
            return(NULL);
        }

        item->text = rasta_menu_item_get_text(cur_item);
        item->next_id = rasta_menu_item_get_id(cur_item);
        if ((item->text == NULL) || (item->next_id == NULL))
        {
            fprintf(stderr, "clrasta: Unable to allocate memory\n");
            r_enumeration_free(enumer);
            return(NULL);
        }

        m_items = g_list_append(m_items, item);
    }
    r_enumeration_free(enumer);
    
    return(m_items);
}  /* get_menu_items() */


/*
 * static gchar *get_menu_item_help(RastaContext *ctxt,
 *                                  RastaScreen *screen,
 *                                  CLRMenuItem *item)
 *
 * Gets the help text for the given menu item
 */
static gchar *get_menu_item_help(RastaContext *ctxt,
                                 RastaScreen *screen,
                                 CLRMenuItem *item)
{
    gchar *help_text, *cur_id;
    RastaMenuItem *cur_item;
    REnumeration *enumer;

    g_return_val_if_fail(screen != NULL, NULL);

    help_text = NULL;
    enumer = rasta_menu_screen_enumerate_items(ctxt, screen);
    while (r_enumeration_has_more(enumer))
    {
        cur_item = RASTA_MENU_ITEM(r_enumeration_get_next(enumer));
        if (cur_item == NULL)
            continue;  /* should we error? */

        cur_id = rasta_menu_item_get_id(cur_item);
        if (cur_id == NULL)
        {
            fprintf(stderr, "clrasta: Unable to allocate memory\n");
            r_enumeration_free(enumer);
            return(NULL);
        }
        if (strcmp(cur_id, item->next_id) == 0)
        {
            help_text = rasta_menu_item_get_help(cur_item);
            g_free(cur_id);
            break;
        }
        g_free(cur_id);
    }
    r_enumeration_free(enumer);
    
    return(help_text);
}  /* get_menu_item_help() */


/*
 * static void free_menu_items(GList *m_items)
 *
 * Frees the menu items
 */
static void free_menu_items(GList *m_items)
{
    GList *elem;
    CLRMenuItem *item;

    g_return_if_fail(m_items != NULL);

    elem = m_items;
    while (elem != NULL)
    {
        item = (CLRMenuItem *)elem->data;
        g_free(item->text);
        g_free(item->next_id);
        g_free(item);
        elem = g_list_next(elem);
    }
    g_list_free(m_items);
}  /* free_menu_items() */


/*
 * static gboolean void run_menu(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * Handles the running of a menu.
 */
static gboolean run_menu(RastaContext *ctxt, RastaScreen *screen)
{
    gint count, num_choices, ret_id, res;
    gchar *title, *help;
    GList *m_items, *elem;
    CLRMenuItem *item;
    gboolean done, retval;
    gchar **choices;

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(screen != NULL, FALSE);

    title = rasta_screen_get_title(screen);

    m_items = get_menu_items(ctxt, screen);
    if (m_items == NULL)
    {
        fprintf(stderr, "clrasta: There are no items of this type\n");
        return(FALSE);
    }

    num_choices = g_list_length(m_items);

    choices = g_new0(gchar *, num_choices);
    for (elem = m_items, count = 0;
         (count < num_choices) && (elem != NULL);
         elem = g_list_next(elem), count++)
    {
        item = (CLRMenuItem *)elem->data;
        choices[count] = item->text;
    }

    done = FALSE;
    retval = TRUE;
    while (done == FALSE)
    {
        pretty_print_title(title);
        
        res = clr_query_choice(choices, num_choices, &ret_id,
                               CLR_QUERY_FLAG_QUIT |
                               CLR_QUERY_FLAG_BACK |
                               CLR_QUERY_FLAG_REQUIRED |
                               CLR_QUERY_FLAG_HELP);
        switch (res)
        {
            case CLR_QUERY_QUIT:
                retval = FALSE;
                done = TRUE;
                break;

            case CLR_QUERY_BACK:
                rasta_screen_previous(ctxt);
                done = TRUE;
                break;
                
            case CLR_QUERY_HELP:
                elem = g_list_nth(m_items, ret_id);
                item = (CLRMenuItem *)elem->data;
                help = get_menu_item_help(ctxt, screen, item); 
                if ((help != NULL) && (g_utf8_strlen(help, -1) > 0))
                    fprintf(stdout, "\nHelp for \"%s\":\n%s\n",
                           item->text, help);
                else
                    fprintf(stdout,
                            "\nThere is no help on this topic\n");
                g_free(help);
                break;

            case CLR_QUERY_SUCCESS:
                elem = g_list_nth(m_items, ret_id);
                item = (CLRMenuItem *)elem->data;
                rasta_menu_screen_next(ctxt, item->next_id);
                done = TRUE;
                break;

            default:
                g_assert_not_reached();
                break;
        }
    }

    g_free(choices);
    free_menu_items(m_items);
    g_free(title);

    return(retval);
}  /* run_menu() */


/* static gint run_initcommand(RastaContext *ctxt, RastaScreen *screen)
 *
 * Runs the initcommand for a dialog.
 */
static gint run_initcommand(RastaContext *ctxt, RastaScreen *screen)
{
    pid_t pid;
    gint outfd, errfd, rc, wstat;
    GIOChannel *chan;
    GIOStatus status;
    GError *error;
    gsize out_len, err_len;
    char *out_data, *err_data, *encoding, *tmp;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(screen != NULL, -EINVAL);

    rc = rasta_initcommand_run(ctxt, screen,
                               &pid, &outfd, &errfd);
    if (rc != 0)
        g_error("Couldn't run init command\n");

    error = NULL;
    encoding = rasta_initcommand_get_encoding(screen);
    rc = -EIO;
    out_data = NULL;
    if (outfd > -1)
    {
        chan = g_io_channel_unix_new(outfd);
        if (!chan)
            g_error("Couldn't create GIOChannel\n");

        error = NULL;
        g_io_channel_set_encoding(chan, encoding, &error);
        if (error)
        {
            fprintf(stdout,
                    "clrasta: Error setting encoding for initcommand: %s\n",
                    error->message ? error->message : "Unknown error");
            g_clear_error(&error);
        }

        error = NULL;
        status = g_io_channel_read_to_end(chan, &out_data, &out_len,
                                          &error);
        if (status != G_IO_STATUS_NORMAL)
        {
            fprintf(stderr,
                    "clrasta: Unable to read initcommand data: %s\n",
                    (error && error->message) ?
                    error->message :
                    "Unknown error");
            if (error)
                g_clear_error(&error);
            goto out_encoding;
        }

        g_io_channel_unref(chan);
    }

    err_data = NULL;
    if (errfd > -1)
    {
        chan = g_io_channel_unix_new(errfd);
        if (!chan)
            g_error("Couldn't create GIOChannel\n");

        error = NULL;
        g_io_channel_set_encoding(chan, encoding, &error);
        if (error)
        {
            fprintf(stdout,
                    "clrasta: Error setting encoding for initcommand: %s\n",
                    error->message ? error->message : "Unknown error");
            g_clear_error(&error);
        }

        error = NULL;
        status = g_io_channel_read_to_end(chan, &err_data, &err_len,
                                          &error);
        if (status != G_IO_STATUS_NORMAL)
        {
            fprintf(stderr,
                    "clrasta: Unable to read initcommand data: %s\n",
                    (error && error->message) ?
                    error->message :
                    "Unknown error");
            if (error)
                g_clear_error(&error);
            goto out_stdout;
        }

        g_io_channel_unref(chan);
    }

    while (rc < 0)
    {
        rc = waitpid(pid, &wstat, 0);
        if ((rc == -1) && (errno != EINTR))
            break;
    }

    if (rc < 0)
    {
        fprintf(stderr,
                "clrasta: Error running initcommand: %s\n",
                g_strerror(errno));
        goto out_stderr;
    }

    if ((rc > 0) && WIFEXITED(wstat) && !WEXITSTATUS(wstat))
    {
        if (err_data)
            g_free(err_data);
        err_data = NULL;

        if (out_data)
        {
            rc = -ENOMEM;
            tmp = g_new(gchar, out_len + 1);
            if (!tmp)
                goto out_stdout;
            memmove(tmp, out_data, out_len);
            tmp[out_len] = '\0';
            g_free(out_data);
            out_data = tmp;

            rasta_initcommand_complete(ctxt, screen, out_data);
        }
        rc = 0;
    }
    else
    {
        if (rc > 0)
        {
            if (WIFEXITED(wstat))
                rc = WEXITSTATUS(wstat);
            else
                rc = -ECHILD;
        }
        else
            rc = -errno;

        if (err_data)
        {
            tmp = g_new(gchar, err_len + 1);
            if (!tmp)
                goto out_stderr;
            memmove(tmp, err_data, err_len);
            tmp[err_len] = '\0';
            g_free(err_data);
            err_data = tmp;
        }
        else
            err_data = g_strdup("Unknown error");

        fprintf(stderr,
                "clrasta: An error occurred running the initcommand:\n"
                "%s\n", err_data);

        rasta_initcommand_failed(ctxt, screen);
    }
    
out_stderr:
    g_free(err_data);
out_stdout:
    g_free(out_data);
out_encoding:
    if (encoding)
        g_free(encoding);
 
    return(rc);
}  /* run_initcommand() */


/*
 * static gint run_listcommand(RastaContext *ctxt,
 *                             RastaFialogField *field,
 *                             gchar ***items,
 *                             gchar *num_items)
 *
 * Runs the list_command of list/entrylist fields
 */
static gint run_listcommand(RastaContext *ctxt,
                            RastaDialogField *field,
                            gchar ***items,
                            gint *num_items)
{
    pid_t pid;
    gint rc, outfd, errfd, wstat;
    GIOChannel *chan;
    GIOStatus status;
    GError *error;
    gsize out_len, err_len;
    char *out_data, *err_data, *encoding, *tmp;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(field != NULL, -EINVAL);
    g_return_val_if_fail(items != NULL, -EINVAL);
    g_return_val_if_fail(num_items != NULL, -EINVAL);

    *num_items = 0;
    *items = NULL;

    rc = rasta_listcommand_run(ctxt, field,
                               &pid, &outfd, &errfd);
    /* FIXME: Fix the g_errors() */
    if (rc != 0)
        g_error("Couldn't run list command\n");

    error = NULL;
    encoding = rasta_listcommand_get_encoding(field);
    rc = -EIO;
    out_data = NULL;
    if (outfd > -1)
    {
        chan = g_io_channel_unix_new(outfd);
        if (!chan)
            g_error("Couldn't create GIOChannel\n");

        error = NULL;
        g_io_channel_set_encoding(chan, encoding, &error);
        if (error)
        {
            fprintf(stdout,
                    "clrasta: Error setting encoding for listcommand: %s\n",
                    error->message ? error->message : "Unknown error");
            g_clear_error(&error);
        }

        error = NULL;
        status = g_io_channel_read_to_end(chan, &out_data, &out_len,
                                          &error);
        if (status != G_IO_STATUS_NORMAL)
        {
            fprintf(stderr,
                    "clrasta: Unable to read listcommand data: %s\n",
                    (error && error->message) ?
                    error->message :
                    "Unknown error");
            if (error)
                g_clear_error(&error);
            goto out_encoding;
        }

        g_io_channel_unref(chan);
    }

    err_data = NULL;
    if (errfd > -1)
    {
        chan = g_io_channel_unix_new(errfd);
        if (!chan)
            g_error("Couldn't create GIOChannel\n");

        error = NULL;
        g_io_channel_set_encoding(chan, encoding, &error);
        if (error)
        {
            fprintf(stdout,
                    "clrasta: Error setting encoding for listcommand: %s\n",
                    error->message ? error->message : "Unknown error");
            g_clear_error(&error);
        }

        error = NULL;
        status = g_io_channel_read_to_end(chan, &err_data, &err_len,
                                          &error);
        if (status != G_IO_STATUS_NORMAL)
        {
            fprintf(stderr,
                    "clrasta: Unable to read listcommand data: %s\n",
                    (error && error->message) ?
                    error->message :
                    "Unknown error");
            if (error)
                g_clear_error(&error);
            goto out_stdout;
        }

        g_io_channel_unref(chan);
    }

    while (rc < 0)
    {
        rc = waitpid(pid, &wstat, 0);
        if ((rc == -1) && (errno != EINTR))
            break;
    }

    if (rc < 0)
    {
        fprintf(stderr,
                "clrasta: Error running listcommand: %s\n",
                g_strerror(errno));
        goto out_stderr;
    }

    if ((rc > 0) && WIFEXITED(wstat) && !WEXITSTATUS(wstat))
    {
        if (err_data)
            g_free(err_data);
        err_data = NULL;

        if (out_data && out_len > 0)
        {
            rc = -ENOMEM;
            tmp = g_new(gchar, out_len + 1);
            if (!tmp)
                goto out_stdout;
            memmove(tmp, out_data, out_len);
            tmp[out_len] = '\0';
            g_free(out_data);
            out_data = tmp;

            *items = g_strsplit(out_data, "\n", 0);
            if (*items == NULL)
                goto out_stdout;
    
            for (; (*items)[*num_items] != NULL; (*num_items)++);
    
            // Trailing "line"
            if ((*items)[*num_items - 1][0] == '\0')
            {
                g_free((*items)[*num_items - 1]);
                (*items)[*num_items - 1] = NULL;
                (*num_items)--;
            }
        }
        rc = 0;
    }
    else
    {
        if (rc > 0)
        {
            if (WIFEXITED(wstat))
                rc = WEXITSTATUS(wstat);
            else
                rc = -ECHILD;
        }
        else
            rc = -errno;

        if (err_data)
        {
            tmp = g_new(gchar, err_len + 1);
            if (!tmp)
                goto out_stderr;
            memmove(tmp, err_data, err_len);
            tmp[err_len] = '\0';
            g_free(err_data);
            err_data = tmp;
        }
        else
            err_data = g_strdup("Unknown error");

        fprintf(stderr,
                "clrasta: An error occurred running the listcommand:\n"
                "%s\n", err_data);
    }
    
out_stderr:
    g_free(err_data);
out_stdout:
    g_free(out_data);
out_encoding:
    if (encoding)
        g_free(encoding);

    return(rc);
}  /* run_listcommand() */


/*
 * static CLRQueryResult run_dialog_field(RastaContext *ctxt,
 *                                        RastaScreen *screen,
 *                                        RastaDialogField *d_field)
 *
 * Asks the user about the given dialog field
 */
static CLRQueryResult run_dialog_field(RastaContext *ctxt,
                                       RastaScreen *screen,
                                       RastaDialogField *d_field)
{
    CLRQueryResult ret_id;

    ret_id = CLR_QUERY_UNKNOWN;
    switch (rasta_dialog_field_get_type(d_field))
    {
        case RASTA_FIELD_LIST:
            ret_id = run_dialog_field_list(ctxt, d_field);
            break;

        case RASTA_FIELD_ENTRYLIST:
            ret_id = run_dialog_field_entrylist(ctxt, d_field);
            break;

        case RASTA_FIELD_ENTRY:
            ret_id = run_dialog_field_entry(ctxt, d_field);
            break;

        case RASTA_FIELD_FILE:
            ret_id = run_dialog_field_file(ctxt, d_field);
            break;

#ifdef ENABLE_DEPRECATED
        case RASTA_FIELD_IPADDR:
        case RASTA_FIELD_VOLUME:
            fprintf(stderr,
                    "clrasta: Special fields not supported\n");
            ret_id = CLR_QUERY_BACK;
            break;
#endif  /* ENABLE_DEPRECATED */

        case RASTA_FIELD_RING:
            ret_id = run_dialog_field_ring(ctxt, d_field);
            break;

        case RASTA_FIELD_READONLY:
            ret_id = run_dialog_field_readonly(ctxt, d_field);
            break;

        case RASTA_FIELD_DESCRIPTION:
            ret_id = run_dialog_field_description(ctxt, d_field);
            break;

        case RASTA_FIELD_NONE:
        default:
            ret_id = CLR_QUERY_QUIT;
            g_assert_not_reached();
            break;
    }

    return(ret_id);
}  /* run_dialog_field() */


/*
 * static gboolean void run_dialog(RastaContext *ctxt,
 *                                 RastaScreen *screen)
 *
 * Handles the running of a dialog.
 */
static gboolean run_dialog(RastaContext *ctxt, RastaScreen *screen)
{
    CLRQueryResult ret_id;
    gchar *title;
    gboolean retval, done;
    REnumeration *en;
    RastaDialogField *d_field;

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(screen != NULL, FALSE);

    if (rasta_initcommand_is_required(ctxt, screen) != FALSE)
    {
        if (run_initcommand(ctxt, screen) != 0)
            return(TRUE);
    }

    title = rasta_screen_get_title(screen);

    en = rasta_dialog_screen_enumerate_fields(ctxt, screen);
    if (en == NULL)
    {
        fprintf(stderr, "clrasta: An error occurred\n");
        return(FALSE);
    }

    if (!r_enumeration_has_more(en))
    {
        fprintf(stderr, "clrasta: There are no items of this type\n");
        return(FALSE);
    }

    pretty_print_title(title);
    retval = TRUE;
    done = FALSE;
    ret_id = CLR_QUERY_NONE;
    while (r_enumeration_has_more(en))
    {
        d_field = RASTA_DIALOG_FIELD(r_enumeration_get_next(en));
        ret_id = run_dialog_field(ctxt, screen, d_field);
        switch (ret_id)
        {
            case CLR_QUERY_UNKNOWN:
                fprintf(stderr, "clrasta: Unknown error\n");
                /* FALL THROUGH */

            case CLR_QUERY_QUIT:
                retval = FALSE;
                done = TRUE;
                break;

            case CLR_QUERY_BACK:
                rasta_screen_previous(ctxt);
                done = TRUE;
                break;
                
            case CLR_QUERY_HELP:
                /* FIXME: Handle help */
                break;

            case CLR_QUERY_SUCCESS:
                break;

            case CLR_QUERY_NONE: /* run_dialog_field handles REQUIRED */
                break;

            default:
                g_assert_not_reached();
                break;
        }

        if (done)
            break;
    }
    r_enumeration_free(en);

    g_free(title);

    if ((ret_id == CLR_QUERY_SUCCESS) || (ret_id == CLR_QUERY_NONE))
        rasta_dialog_screen_next(ctxt);

    return(retval);
}  /* run_dialog() */


/*
 * static gboolean void run_hidden(RastaContext *ctxt,
 *                                 RastaScreen *screen)
 *
 * Handles the running of a hidden screen.
 */
static gboolean run_hidden(RastaContext *ctxt, RastaScreen *screen)
{
#if DEBUG
    gchar *title;
#endif

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(screen != NULL, FALSE);

#if DEBUG
    title = rasta_screen_get_title(screen);
    g_print("Hidden screen \"%s\"\n", title);
    g_free(title);
#endif  /* DEBUG */

    if (rasta_initcommand_is_required(ctxt, screen) == FALSE)
    {
        rasta_hidden_screen_next(ctxt);
        return(TRUE);
    }

    run_initcommand(ctxt, screen);

    return(TRUE);
}  /* run_hidden() */


/*
 * static gboolean void run_action(RastaContext *ctxt,
 *                                 RastaScreen *screen)
 *
 * Handles the running of an action.
 */
static gboolean run_action(RastaContext *ctxt, RastaScreen *screen)
{
    gchar *title, *cmd, *prompt, *conv;
    gboolean retval, done;
    CLRQueryResult ret_id;
    RastaTTYType tty;
    pid_t pid;
    gint rc, wstat;
    gsize br, bw;
    GError *error;
    /* Needs win32 cmd.exe #ifdef */
    gchar *args[] = {"/bin/sh", "-c", NULL, NULL};


    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(screen != NULL, FALSE);

    title = rasta_screen_get_title(screen);

    pretty_print_title(title);
    g_free(title);

    fprintf(stdout, "\n");


    tty = rasta_action_screen_get_tty_type(screen);
    cmd = rasta_action_screen_get_command(ctxt, screen);
    
    if (!cmd || !*cmd)
    {
        g_free(cmd);
        fprintf(stderr,
                "clrasta: There are no items of this type\n");
        rasta_screen_previous(ctxt);
        return(TRUE);
    }

    error = NULL;
    conv = g_locale_from_utf8(cmd, -1, &br, &bw, &error);
    g_free(cmd);
    if (error)
    {
        g_free(conv);
        fprintf(stderr,
                "clrasta: Error with encoding conversion: %s\n",
                error->message ? error->message : "Unknown error");
        rasta_screen_previous(ctxt);
        return(TRUE);
    }

    args[2] = conv;

    rc = 0;
    pid = 0;
    if (tty == RASTA_TTY_NO)
    {
        rc = rasta_exec_command_v(&pid,
                                  NULL, NULL, NULL,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_NULL,
                                  args);
    }
    else if (tty == RASTA_TTY_SELF)
    {
        rc = rasta_exec_command_v(&pid,
                                  NULL, NULL, NULL,
                                  RASTA_EXEC_FD_INHERIT,
                                  RASTA_EXEC_FD_INHERIT,
                                  RASTA_EXEC_FD_INHERIT,
                                  args);
    }
    else  /* RASTA_TTY_YES and a default for broken */
    {
        rc = rasta_exec_command_v(&pid,
                                  NULL, NULL, NULL,
                                  RASTA_EXEC_FD_NULL,
                                  RASTA_EXEC_FD_INHERIT,
                                  RASTA_EXEC_FD_INHERIT,
                                  args);
    }

    if (rc != 0)
    {
        fprintf(stderr,
                "clrasta: Error starting command: %s\n",
                g_strerror(errno));
    }
    else if (pid > 1)
    {
        rc = -1;
        while (rc != pid)
        {
             rc = waitpid(pid, &wstat, 0);
             if ((rc < 0) && (errno != EINTR))
                 break;
        }
        if ((rc == pid) && WIFEXITED(wstat) && !WEXITSTATUS(wstat))
            fprintf(stdout, "clrasta: Action completed successfully\n");
        else
            fprintf(stdout, "clrasta: Action completed with error\n");
                
    }

    done = FALSE;
    retval = TRUE;
    while (done == FALSE)
    {
        prompt = clr_build_prompt(CLR_QUERY_FLAG_BACK |
                                  CLR_QUERY_FLAG_QUIT,
                                  -1, -1);
        ret_id = clr_query_response(prompt,
                                    CLR_QUERY_FLAG_BACK |
                                    CLR_QUERY_FLAG_QUIT,
                                    NULL, -1, -1);
        g_free(prompt);

        switch (ret_id)
        {
            case CLR_QUERY_BACK:
                rasta_screen_previous(ctxt);
                done = TRUE;
                break;

            case CLR_QUERY_QUIT:
                done = TRUE;
                retval = FALSE;
                break;

            case CLR_QUERY_HELP:
                /* FIXME */
                break;

            default:
                fprintf(stderr,
                        "clrasta: Invalid choice\n");
                break;
        }
    }

    return(retval);
}  /* run_action() */



/*
 * Main program
 */

gint main (gint argc, gchar *argv[])
{
    gint rc;
    gboolean done;
    RastaContext *ctxt;
    RastaScreen *screen;
    CLROptions options = {FALSE, NULL, NULL, NULL};

    if (load_options(argc, argv, &options) == FALSE)
    {
        print_usage();
        return(-EINVAL);
    }

    ctxt = load_context(&options);
    if (ctxt == NULL)
    {
        fprintf(stderr, "clrasta: Unable to load screen data\n");
        return(-ENOENT);
    }

    if (options.state_filename != NULL)
    {
        rc = load_state(ctxt, &options);
        if (rc != 0)
        {
            fprintf(stderr, "clrasta: Unable to load state\n");
            return(rc);
        }
    }

    done = FALSE;
    rc = 0;
    while (done == FALSE)
    {
        screen = rasta_context_get_screen(ctxt);
        if (screen == NULL)
        {
            fprintf(stderr,
                    "clrasta: There are no items of this type\n");
            rc = -ENOENT;
            done = TRUE;
        }
        else switch (rasta_screen_get_type(screen))
        {
            case RASTA_SCREEN_NONE:
                fprintf(stderr,
                        "clrasta: There are no items of this type\n");
                rc = -ENOENT;
                done = TRUE;
                break;

            case RASTA_SCREEN_MENU:
                if (run_menu(ctxt, screen) == FALSE)
                    done = TRUE;
                break;

            case RASTA_SCREEN_DIALOG:
                if (run_dialog(ctxt, screen) == FALSE)
                    done = TRUE;
                break;

            case RASTA_SCREEN_HIDDEN:
                if (run_hidden(ctxt, screen) == FALSE)
                    done = TRUE;
                break;

            case RASTA_SCREEN_ACTION:
                if (run_action(ctxt, screen) == FALSE)
                    done = TRUE;
                break;

            default:
                fprintf(stderr,
                        "clrasta: Invalid screen type\n");
                rc = -EINVAL;
                done = TRUE;
                break;
        }
    }

    rasta_context_destroy(ctxt);
    return(rc);
}  /* main() */
