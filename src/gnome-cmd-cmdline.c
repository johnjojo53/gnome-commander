/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-style.h"
#include "utils.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"

static GtkHBoxClass *parent_class = NULL;

struct _GnomeCmdCmdlinePrivate
{
    GtkWidget *combo;
    GtkWidget *cwd;

    GList *history;
};


static void
update_history_combo (GnomeCmdCmdline *cmdline)
{
    GList *tmp = cmdline->priv->history;

    gnome_cmd_combo_clear (GNOME_CMD_COMBO (cmdline->priv->combo));

    for (; tmp; tmp = tmp->next)
    {
        gchar *command = (gchar*)tmp->data;
        gchar *text[2];

        text[0] = command;
        text[1] = NULL;

        gnome_cmd_combo_append (GNOME_CMD_COMBO (cmdline->priv->combo), text, command);
    }

    gtk_clist_select_row (GTK_CLIST (GNOME_CMD_COMBO (cmdline->priv->combo)->list), 0, 0);
}


static void
add_to_history (GnomeCmdCmdline *cmdline, const gchar *command)
{
    cmdline->priv->history = string_history_add (
        cmdline->priv->history,
        command,
        gnome_cmd_data_get_cmdline_history_length());

    update_history_combo (cmdline);
}


static void
on_exec (GnomeCmdCmdline *cmdline, gboolean term)
{
    const gchar *text;
    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_active_fs (main_win);

    text = gtk_entry_get_text (GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry));
    text = g_strstrip (g_strdup (text));

    if (strlen (text) > 3 && strncmp (text, "cd ", 3) == 0) {
        const gchar *dir = text + 3;
        dir = g_strstrip (g_strdup (dir));
        gnome_cmd_file_selector_goto_directory (fs, dir);
    }
    else if (strcmp (text, "cd") == 0) {
        gnome_cmd_file_selector_goto_directory (fs, "~");
    }
    else {
        if (gnome_cmd_con_is_local (gnome_cmd_file_selector_get_connection (fs))) {
            GnomeCmdDir *dir = gnome_cmd_file_selector_get_directory (fs);
            gchar *fpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (dir));

            run_command_indir (text, fpath, term);
            g_free (fpath);
        }
    }

    add_to_history (cmdline, text);
    gnome_cmd_cmdline_set_text (cmdline, "");
}


static gboolean
on_key_pressed (GtkWidget *entry,
                GdkEventKey *event,
                GnomeCmdCmdline *cmdline)
{
    switch (event->keyval) {
        case GDK_BackSpace:
        case GDK_space:
        case GDK_Home:
        case GDK_End:
        case GDK_Delete:
        case GDK_Left:
        case GDK_Right:
            return gnome_cmd_cmdline_keypressed (cmdline, event);
    }

    if (gnome_cmd_cmdline_keypressed (cmdline, event))
        return TRUE;
    else if (gnome_cmd_main_win_keypressed (main_win, event))
        return TRUE;

    return FALSE;
}


static void
on_combo_item_selected (GnomeCmdCombo *combo,
                        const gchar *command,
                        GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_COMBO (combo));
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));
    g_return_if_fail (command != NULL);

    gnome_cmd_cmdline_set_text (cmdline, command);
    gtk_widget_grab_focus (combo->entry);
}


static void
on_combo_popwin_hidden (GnomeCmdCombo *combo, GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_COMBO (combo));
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    gtk_widget_grab_focus (combo->entry);
}


static void
on_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs, GnomeCmdCmdline *cmdline)
{
    gchar *dpath;

    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    dpath = gnome_cmd_dir_get_display_path (gnome_cmd_file_selector_get_directory (fs));

    gnome_cmd_cmdline_set_dir (cmdline, dpath);
    g_free (dpath);
}


static void
on_fs_changed_dir (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, GnomeCmdCmdline *cmdline)
{
    gchar *dpath;

    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (dir != NULL);
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    if (fs != gnome_cmd_main_win_get_active_fs (main_win)) return;

    dpath = gnome_cmd_dir_get_display_path (dir);
    gnome_cmd_cmdline_set_dir (cmdline, dpath);
    g_free (dpath);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    gtk_signal_disconnect_by_func (
        GTK_OBJECT (main_win), GTK_SIGNAL_FUNC (on_switch_fs), object);
    gtk_signal_disconnect_by_func (
        GTK_OBJECT (gnome_cmd_main_win_get_fs (main_win, LEFT)),
        GTK_SIGNAL_FUNC (on_fs_changed_dir), object);
    gtk_signal_disconnect_by_func (
        GTK_OBJECT (gnome_cmd_main_win_get_fs (main_win, RIGHT)),
        GTK_SIGNAL_FUNC (on_fs_changed_dir), object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdCmdlineClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gtk_hbox_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdCmdline *cmdline)
{
    GtkWidget *label;

    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    cmdline->priv = g_new (GnomeCmdCmdlinePrivate, 1);
    cmdline->priv->history = NULL;

    cmdline->priv->cwd = gtk_label_new ("cwd");
    gtk_widget_ref (cmdline->priv->cwd);
    gtk_object_set_data_full (GTK_OBJECT (cmdline), "cwdlabel", cmdline->priv->cwd,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (cmdline->priv->cwd);
    gtk_box_pack_start (GTK_BOX (cmdline), cmdline->priv->cwd,
                        FALSE, TRUE, 2);
    gtk_label_set_selectable (GTK_LABEL (cmdline->priv->cwd), TRUE);

    label = gtk_label_new ("#");
    gtk_widget_ref (label);
    gtk_object_set_data_full (GTK_OBJECT (cmdline), "label", label,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (cmdline), label, FALSE, FALSE, 0);

    cmdline->priv->combo = gnome_cmd_combo_new (1, 0, NULL);
    gtk_widget_ref (cmdline->priv->combo);
    gtk_object_set_data_full (GTK_OBJECT (cmdline), "combo", cmdline->priv->combo,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_clist_set_column_width (
        GTK_CLIST (GNOME_CMD_COMBO (cmdline->priv->combo)->list), 0, 500);
    gtk_box_pack_start (GTK_BOX (cmdline), cmdline->priv->combo, TRUE, TRUE, 2);
    gtk_widget_show (cmdline->priv->combo);
    gtk_entry_set_editable (GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry), TRUE);
    GTK_WIDGET_UNSET_FLAGS (GNOME_CMD_COMBO (cmdline->priv->combo)->button, GTK_CAN_FOCUS);
    GTK_WIDGET_SET_FLAGS (GNOME_CMD_COMBO (cmdline->priv->combo)->entry, GTK_CAN_FOCUS);

    gtk_signal_connect (GTK_OBJECT (GNOME_CMD_COMBO (cmdline->priv->combo)->entry),
                        "key-press-event",
                        GTK_SIGNAL_FUNC (on_key_pressed), cmdline);
    gtk_signal_connect (GTK_OBJECT (cmdline->priv->combo), "item-selected",
                        GTK_SIGNAL_FUNC (on_combo_item_selected), cmdline);
    gtk_signal_connect (GTK_OBJECT (cmdline->priv->combo), "popwin-hidden",
                        GTK_SIGNAL_FUNC (on_combo_popwin_hidden), cmdline);
    gtk_signal_connect_after (GTK_OBJECT (main_win), "switch-fs",
                        GTK_SIGNAL_FUNC (on_switch_fs), cmdline);
    gtk_signal_connect (GTK_OBJECT (gnome_cmd_main_win_get_fs (main_win, LEFT)),
                        "changed-dir", GTK_SIGNAL_FUNC (on_fs_changed_dir), cmdline);
    gtk_signal_connect (GTK_OBJECT (gnome_cmd_main_win_get_fs (main_win, RIGHT)),
                        "changed-dir", GTK_SIGNAL_FUNC (on_fs_changed_dir), cmdline);

    gnome_cmd_cmdline_update_style (cmdline);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_cmdline_new ()
{
    GnomeCmdCmdline *cmdline = gtk_type_new (gnome_cmd_cmdline_get_type ());

    gnome_cmd_cmdline_set_history (cmdline, gnome_cmd_data_get_cmdline_history ());
    update_history_combo (cmdline);

    return GTK_WIDGET (cmdline);
}


GtkType
gnome_cmd_cmdline_get_type         (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdCmdline",
            sizeof (GnomeCmdCmdline),
            sizeof (GnomeCmdCmdlineClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gtk_hbox_get_type (), &dlg_info);
    }
    return dlg_type;
}


void
gnome_cmd_cmdline_set_dir (GnomeCmdCmdline *cmdline, const gchar *cwd)
{
    gchar *s;

    g_return_if_fail (cmdline != NULL);
    g_return_if_fail (cmdline->priv != NULL);

    s = get_utf8 (cwd);
    gtk_label_set_text (GTK_LABEL (cmdline->priv->cwd), s);
    g_free (s);
}


void
gnome_cmd_cmdline_append_text (GnomeCmdCmdline *cmdline, const gchar *text)
{
    const gchar *curtext;
    gint curpos;
    GtkEntry *entry;

    g_return_if_fail (cmdline != NULL);
    g_return_if_fail (cmdline->priv != NULL);
    g_return_if_fail (cmdline->priv->combo != NULL);

    entry = GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry);
    curtext = gtk_entry_get_text (entry);
    if (curtext[strlen(curtext)-1] != ' ' && strlen(curtext) > 0)
        gtk_entry_append_text (entry, " ");
    curpos = gtk_editable_get_position (GTK_EDITABLE (entry));
    gtk_entry_append_text (entry, text);
    gtk_editable_set_position (GTK_EDITABLE (entry), curpos + strlen (text));
}


void
gnome_cmd_cmdline_insert_text (GnomeCmdCmdline *cmdline, const gchar *text)
{
    gint curpos, tmp;
    GtkEntry *entry;

    g_return_if_fail (cmdline != NULL);
    g_return_if_fail (cmdline->priv != NULL);
    g_return_if_fail (cmdline->priv->combo != NULL);

    entry = GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry);
    curpos = gtk_editable_get_position (GTK_EDITABLE (entry));
    tmp = curpos;
    gtk_editable_insert_text (GTK_EDITABLE (entry), text, strlen (text), &tmp);
    gtk_editable_set_position (GTK_EDITABLE (entry), curpos + strlen (text));
}


void
gnome_cmd_cmdline_set_text    (GnomeCmdCmdline *cmdline, const gchar *text)
{
    g_return_if_fail (cmdline != NULL);
    g_return_if_fail (cmdline->priv != NULL);
    g_return_if_fail (cmdline->priv->combo != NULL);

    gtk_entry_set_text (GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry), text);
}


gboolean
gnome_cmd_cmdline_is_empty    (GnomeCmdCmdline *cmdline)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (GNOME_CMD_COMBO (cmdline->priv->combo)->entry));

    if (text == NULL || strcmp (text, ""))
        return TRUE;

    return FALSE;
}


void
gnome_cmd_cmdline_exec        (GnomeCmdCmdline *cmdline)
{
    on_exec (cmdline, FALSE);
}


void
gnome_cmd_cmdline_focus (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));
    g_return_if_fail (cmdline->priv->combo != NULL);

    gtk_widget_grab_focus (GTK_WIDGET (GNOME_CMD_COMBO (cmdline->priv->combo)->entry));
    gtk_editable_set_position (GTK_EDITABLE (GNOME_CMD_COMBO (cmdline->priv->combo)->entry), -1);
}


void
gnome_cmd_cmdline_update_style (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    gnome_cmd_combo_update_style (GNOME_CMD_COMBO (cmdline->priv->combo));
}


void
gnome_cmd_cmdline_show_history (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    gnome_cmd_combo_popup_list (GNOME_CMD_COMBO (cmdline->priv->combo));
}


GList *
gnome_cmd_cmdline_get_history (GnomeCmdCmdline *cmdline)
{
    g_return_val_if_fail (GNOME_CMD_IS_CMDLINE (cmdline), NULL);

    return cmdline->priv->history;
}


void
gnome_cmd_cmdline_set_history  (GnomeCmdCmdline *cmdline, GList *history)
{
    GList *tmp;

    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    // free the old history

    for (tmp = cmdline->priv->history; tmp; tmp = tmp->next)
        g_free (tmp->data);

    cmdline->priv->history = history;
}


GtkWidget *
gnome_cmd_cmdline_get_entry    (GnomeCmdCmdline *cmdline)
{
    g_return_val_if_fail (GNOME_CMD_IS_CMDLINE (cmdline), NULL);

    return GNOME_CMD_COMBO (cmdline->priv->combo)->entry;
}


gboolean
gnome_cmd_cmdline_keypressed   (GnomeCmdCmdline *cmdline,
                                GdkEventKey *event)
{
    g_return_val_if_fail (GNOME_CMD_IS_CMDLINE (cmdline), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (state_is_ctrl (event->state)) {
        switch (event->keyval)
        {
            case GDK_Down:
                gnome_cmd_cmdline_show_history (cmdline);
                return TRUE;

            case GDK_Return:
                event->string[0] = '\0';
                return FALSE;
        }
    }
    else if (state_is_shift (event->state)) {
        switch (event->keyval)
        {
            case GDK_Return:
                on_exec (cmdline, TRUE);
                event->keyval = GDK_Escape;
                return TRUE;
        }
    }
    else if (state_is_ctrl_shift (event->state)) {
        switch (event->keyval)
        {
            case GDK_Return:
                event->string[0] = '\0';
                return FALSE;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
                on_exec (cmdline, FALSE);
                event->keyval = GDK_Escape;
                return TRUE;

            case GDK_Escape:
                gnome_cmd_cmdline_set_text (cmdline, "");
                gnome_cmd_main_win_focus_file_lists (main_win);
                return TRUE;

            case GDK_Up:
            case GDK_Down:
            {
                gboolean ret;
                GdkEventKey event2;
                GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_active_fs (main_win);
                GtkWidget *file_list = fs->list_widget;

                gtk_widget_grab_focus (file_list);
                gnome_cmd_file_selector_set_active (fs, TRUE);

                memcpy (&event2, event, sizeof (GdkEventKey));
                gtk_signal_emit_by_name (
                    GTK_OBJECT (file_list), "key-press-event", &event2, &ret);
                event->keyval = 0;
            }
            break;
        }
    }

    return FALSE;
}
