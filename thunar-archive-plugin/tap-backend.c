/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs.h>

#include <thunar-archive-plugin/tap-backend.h>



static ThunarVfsMimeApplication *tap_backend_mime_ask           (GList                    *mime_applications,
                                                                 GtkWidget                *parent);
static GList                    *tap_backend_mime_applications  (ThunarVfsMimeDatabase    *mime_database,
                                                                 GList                    *mime_infos);
static ThunarVfsMimeApplication *tap_backend_mime_application   (GList                    *mime_infos,
                                                                 GtkWidget                *window,
                                                                 GError                  **error);
static gchar                    *tap_backend_mime_wrapper       (ThunarVfsMimeApplication *mime_application) G_GNUC_MALLOC;
static GPid                      tap_backend_run                (const gchar              *action,
                                                                 const gchar              *folder,
                                                                 GList                    *files,
                                                                 GList                    *mime_infos,
                                                                 GtkWidget                *window,
                                                                 GError                  **error);



static ThunarVfsMimeApplication*
tap_backend_mime_ask (GList     *mime_applications,
                      GtkWidget *parent)
{
  ThunarVfsMimeApplication *mime_application = NULL;
  GtkIconTheme             *icon_theme;
  const gchar              *icon_name;
  GtkTooltips              *tooltips;
  GtkWidget                *button;
  GtkWidget                *dialog;
  GtkWidget                *image;
  GtkWidget                *label;
  GtkWidget                *table;
  GtkWidget                *bbox;
  GtkWidget                *hbox;
  GSList                   *buttons = NULL;
  GSList                   *bp;
  gchar                    *command;
  gchar                    *space;
  GList                    *mp;

  /* determine the icon theme for this screen */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (parent)));

  /* allocate tooltips */
  tooltips = gtk_tooltips_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (tooltips));

  /* prepare the dialog to query the preferred archiver for the user */
  dialog = gtk_dialog_new_with_buttons (_("Select an archive manager"),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_MODAL,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* add the main table */
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  /* add the header image */
  image = gtk_image_new_from_icon_name ("gnome-package", GTK_ICON_SIZE_DIALOG);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (image);

  /* add the header label */
  label = gtk_label_new (_("Please select your preferred archive manager\nfrom the list of available applications below:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (label);

  /* add the button box */
  bbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (bbox), 12);
  gtk_table_attach (GTK_TABLE (table), bbox, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (bbox);

  /* add the radio buttons */
  for (mp = mime_applications; mp != NULL; mp = mp->next)
    {
      /* add the radio button */
      button = gtk_radio_button_new (buttons);
      buttons = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
      g_object_set_data (G_OBJECT (button), "mime-application", mp->data);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      /* set the command as tooltip, as some archive manager's names are not very useful */
      command = g_strdup (thunar_vfs_mime_handler_get_command (THUNAR_VFS_MIME_HANDLER (mp->data)));
      space = strchr (command, ' ');
      if (G_LIKELY (space != NULL))
        *space = '\0';
      gtk_tooltips_set_tip (tooltips, button, command, NULL);
      g_free (command);

      /* add the hbox */
      hbox = gtk_hbox_new (FALSE, 2);
      gtk_container_add (GTK_CONTAINER (button), hbox);
      gtk_widget_show (hbox);

      /* check if we have an icon for the application */
      icon_name = thunar_vfs_mime_handler_lookup_icon_name (THUNAR_VFS_MIME_HANDLER (mp->data), icon_theme);
      if (G_LIKELY (icon_name != NULL))
        {
          /* add an image */
          image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
          gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
          gtk_widget_show (image);
        }

      /* add the label for the application */
      label = gtk_label_new (thunar_vfs_mime_application_get_name (mp->data));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
    }

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      /* determine the selected application */
      for (bp = buttons; bp != NULL; bp = bp->next)
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bp->data)))
          {
            mime_application = g_object_get_data (G_OBJECT (bp->data), "mime-application");
            g_object_ref (G_OBJECT (mime_application));
            break;
          }
    }

  /* cleanup */
  g_object_unref (G_OBJECT (tooltips));
  gtk_widget_destroy (dialog);

  return mime_application;
}



static GList*
tap_backend_mime_applications (ThunarVfsMimeDatabase *mime_database,
                               GList                 *mime_infos)
{
  GList *mime_applications = NULL;
  GList *list;
  GList *next;
  GList *ap;
  GList *lp;
  gchar *s;

  /* determine the set of applications that can handle all mime types */
  for (lp = mime_infos; lp != NULL; lp = lp->next)
    {
      /* no need to check anything if this is the same mime type as the previous one */
      if (lp->prev != NULL && lp->prev->data == lp->data)
        continue;

      /* determine the list of applications that can handle this mime type */
      list = thunar_vfs_mime_database_get_applications (mime_database, lp->data);
      if (G_UNLIKELY (mime_applications == NULL))
        {
          /* first file, so just use the applications list */
          mime_applications = list;
        }
      else
        {
          /* keep only the applications that are also present in list */
          for (ap = mime_applications; ap != NULL; ap = next)
            {
              /* grab a pointer on the next application */
              next = ap->next;

              /* check if the application is present in list */
              if (g_list_find (list, ap->data) == NULL)
                {
                  /* drop our reference on the application */
                  g_object_unref (G_OBJECT (ap->data));

                  /* drop this application from the list */
                  mime_applications = g_list_delete_link (mime_applications, ap);
                }
            }

          /* release the list of applications for this mime type */
          g_list_foreach (list, (GFunc) g_object_unref, NULL);
          g_list_free (list);
        }

      /* check if the set is still not empty */
      if (G_LIKELY (mime_applications == NULL))
        break;
    }

  /* filter out any unsupported applications */
  for (ap = mime_applications; ap != NULL; ap = next)
    {
      /* determine the pointer to the next item */
      next = ap->next;

      /* check if we have a wrapper for this application */
      s = tap_backend_mime_wrapper (ap->data);
      if (G_UNLIKELY (s == NULL))
        {
          /* drop our reference on the application */
          g_object_unref (G_OBJECT (ap->data));

          /* drop the application from the list */
          mime_applications = g_list_delete_link (mime_applications, ap);
        }
      g_free (s);
    }

  return mime_applications;
}



static ThunarVfsMimeApplication*
tap_backend_mime_application (GList     *mime_infos,
                              GtkWidget *window,
                              GError   **error)
{
  ThunarVfsMimeApplication *mime_application = NULL;
  ThunarVfsMimeDatabase    *mime_database;
  const gchar * const      *mime_types;
  ThunarVfsMimeInfo        *mime_info;
  GError                   *err = NULL;
  GList                    *mime_applications;
  guint                     n;

  /* grab a reference on the mime database */
  mime_database = thunar_vfs_mime_database_get_default ();

  /* determine the mime applications that can handle the mime types */
  mime_applications = tap_backend_mime_applications (mime_database, mime_infos);
  if (G_UNLIKELY (mime_applications == NULL))
    {
      /* tell the user that we cannot handle the specified mime types */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("No suitable archive manager found"));
    }
  else if (mime_applications->next == NULL)
    {
      /* only a single supported archive manager available, use that */
      mime_application = mime_applications->data;
      g_list_free (mime_applications);
    }
  else
    {
      /* more than one supported archive manager, check if the first
       * available is the default for all its supported mime types.
       */
      mime_types = thunar_vfs_mime_application_get_mime_types (mime_applications->data);
      for (n = 0; mime_types[n] != NULL; ++n)
        {
          /* determine the default application for this mime type */
          mime_info = thunar_vfs_mime_database_get_info (mime_database, mime_types[n]);
          mime_application = thunar_vfs_mime_database_get_default_application (mime_database, mime_info);
          thunar_vfs_mime_info_unref (mime_info);

          /* check if our expected default application is also the default here */
          if (mime_applications->data != mime_application)
            {
              /* no, have to ask the user */
              g_object_unref (G_OBJECT (mime_application));
              break;
            }

          /* yep, next one please... */
          g_object_unref (G_OBJECT (mime_application));
        }

      /* check if we have found a suitable one */
      if (G_LIKELY (mime_types[n] == NULL))
        {
          /* use the first available archive manager */
          mime_application = g_object_ref (G_OBJECT (mime_applications->data));
        }
      else
        {
          /* ask the user to specify the default archive manager */
          mime_application = tap_backend_mime_ask (mime_applications, window);
          if (G_LIKELY (mime_application != NULL))
            {
              /* make the selected application the default for all its
               * supported mime types, so we don't need to ask once again.
               */
              mime_types = thunar_vfs_mime_application_get_mime_types (mime_application);
              for (n = 0; mime_types[n] != NULL; ++n)
                {
                  /* set the default application */
                  mime_info = thunar_vfs_mime_database_get_info (mime_database, mime_types[n]);
                  if (!thunar_vfs_mime_database_set_default_application (mime_database, mime_info, mime_application, &err))
                    {
                      /* not critical, still we should tell the user that we failed */
                      g_warning ("Failed to make \"%s\" the default application for %s: %s",
                                 thunar_vfs_mime_application_get_name (mime_application),
                                 thunar_vfs_mime_info_get_name (mime_info), err->message);
                      g_clear_error (&err);
                    }
                  thunar_vfs_mime_info_unref (mime_info);
                }
            }
        }

      /* cleanup */
      g_list_foreach (mime_applications, (GFunc) g_object_unref, NULL);
      g_list_free (mime_applications);
    }

  /* release our reference on the mime database */
  g_object_unref (G_OBJECT (mime_database));

  return mime_application;
}



static gchar*
tap_backend_mime_wrapper (ThunarVfsMimeApplication *mime_application)
{
  const gchar *desktop_id;
  gchar       *basename;
  gchar       *filename;
  gchar       *dot;

  /* determine the basename of the .desktop file */
  desktop_id = thunar_vfs_mime_application_get_desktop_id (mime_application);
  basename = g_path_get_basename (desktop_id);
  dot = strrchr (basename, '.');
  if (G_LIKELY (dot != NULL))
    *dot = '\0';

  /* generate the filename for the .tap wrapper script */
  filename = g_strdup_printf (LIBEXECDIR G_DIR_SEPARATOR_S "thunar-archive-plugin" G_DIR_SEPARATOR_S "%s.tap", basename);

  /* check if the wrapper script exists */
  if (!g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE))
    {
      /* no wrapper then */
      g_free (filename);
      filename = NULL;
    }

  /* cleanup */
  g_free (basename);

  return filename;
}



static GPid
tap_backend_run (const gchar *action,
                 const gchar *folder,
                 GList       *files,
                 GList       *mime_infos,
                 GtkWidget   *window,
                 GError     **error)
{
  ThunarVfsMimeApplication *mime_application;
  ThunarVfsInfo            *info;
  GdkScreen                *screen;
  gchar                    *wrapper;
  gchar                   **argv;
  gchar                    *uri;
  GList                    *lp;
  GPid                      pid = -1;
  gint                      n;

  /* determine the mime infos on-demand */
  if (G_LIKELY (mime_infos == NULL))
    {
      /* determine the mime infos from the files */
      for (lp = files; lp != NULL; lp = lp->next)
        {
          info = thunarx_file_info_get_vfs_info (THUNARX_FILE_INFO (lp->data));
          mime_infos = g_list_append (mime_infos, thunar_vfs_mime_info_ref (info->mime_info));
          thunar_vfs_info_unref (info);
        }
    }

  /* determine the mime application to use */
  mime_application = tap_backend_mime_application (mime_infos, window, error);
  if (G_LIKELY (mime_application != NULL))
    {
      /* determine the wrapper script for the application */
      wrapper = tap_backend_mime_wrapper (mime_application);
      if (G_UNLIKELY (wrapper == NULL))
        {
          /* tell the user that we cannot handle the specified mime types */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("No suitable archive manager found"));
        }
      else
        {
          /* generate the command to run the wrapper */
          argv = g_new0 (gchar *, 4 + g_list_length (files));
          argv[0] = wrapper;
          argv[1] = g_strdup (action);
          argv[2] = g_strdup (folder);

          /* append the file paths */
          for (lp = files, n = 3; lp != NULL; lp = lp->next, ++n)
            {
              uri = thunarx_file_info_get_uri (THUNARX_FILE_INFO (lp->data));
              argv[n] = g_filename_from_uri (uri, NULL, NULL);
              g_free (uri);
            }

          /* determine the screen for this window */
          screen = gtk_widget_get_screen (window);

          /* try to run the command */
          if (!gdk_spawn_on_screen (screen, folder, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, error))
            pid = -1;

          /* cleanup */
          g_strfreev (argv);
        }

      /* cleanup */
      g_object_unref (G_OBJECT (mime_application));
    }

  /* cleanup */
  thunar_vfs_mime_info_list_free (mime_infos);

  return pid;
}



/**
 * tap_backend_create_archive:
 * @folder : the path to the folder in which to create the archive.
 * @files  : a #GList of #ThunarxFileInfo<!---->s that refer to the
 *           files that should be added to the new archive.
 * @window : a #GtkWindow, used to popup dialogs.
 * @error  : return location for errors or %NULL.
 *
 * Spawns a command to create a new archive in @folder with the
 * specified @files, using the default archive manager.
 *
 * Note that %-1 will also be returned when the user cancels this
 * operation, but @error will not be set then.
 *
 * Return value: the process id of the spawned command, or %-1
 *               on error.
 **/
GPid
tap_backend_create_archive (const gchar *folder,
                            GList       *files,
                            GtkWidget   *window,
                            GError     **error)
{
  ThunarVfsMimeDatabase *mime_database;
  GList                 *mime_infos = NULL;

  g_return_val_if_fail (files != NULL, -1);
  g_return_val_if_fail (GTK_IS_WINDOW (window), -1);
  g_return_val_if_fail (g_path_is_absolute (folder), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  /* determine the mime infos for zip and tar files (all supported archives must be able to handle them) */
  mime_database = thunar_vfs_mime_database_get_default ();
  mime_infos = g_list_append (mime_infos, thunar_vfs_mime_database_get_info (mime_database, "application/x-compressed-tar"));
  mime_infos = g_list_append (mime_infos, thunar_vfs_mime_database_get_info (mime_database, "application/x-tar"));
  mime_infos = g_list_append (mime_infos, thunar_vfs_mime_database_get_info (mime_database, "application/x-zip"));
  mime_infos = g_list_append (mime_infos, thunar_vfs_mime_database_get_info (mime_database, "application/zip"));
  g_object_unref (G_OBJECT (mime_database));

  /* run the action, the mime infos will be freed by the _run() method */
  return tap_backend_run ("create", folder, files, mime_infos, window, error);
}



/**
 * tap_backend_extract_here:
 * @folder : the path to the folder in which to extract the @files.
 * @files  : a #GList of #ThunarxFileInfo<!---->s that refer to the
 *           archive files that should be extracted.
 * @window : a #GtkWindow, used to popup dialogs.
 * @error  : return location for errors or %NULL.
 *
 * Spawns a command to extract the set of archive @files in the
 * specified @folder, using the default archive manager. The
 * user will not be prompted to specify a destination folder.
 *
 * Note that %-1 will also be returned when the user cancels this
 * operation, but @error will not be set then.
 *
 * Return value: the process id of the spawned command, or %-1
 *               on error.
 **/
GPid
tap_backend_extract_here (const gchar *folder,
                          GList       *files,
                          GtkWidget   *window,
                          GError     **error)
{
  g_return_val_if_fail (files != NULL, -1);
  g_return_val_if_fail (GTK_IS_WINDOW (window), -1);
  g_return_val_if_fail (g_path_is_absolute (folder), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  /* run the action */
  return tap_backend_run ("extract-here", folder, files, NULL, window, error);
}



/**
 * tap_backend_extract_here:
 * @folder : the path to the folder, which is suggested to the
 *           user as destination folder.
 * @files  : a #GList of #ThunarxFileInfo<!---->s that refer to the
 *           archive files that should be extracted.
 * @window : a #GtkWindow, used to popup dialogs.
 * @error  : return location for errors or %NULL.
 *
 * Spawns a command to extract the set of archive @files  using
 * the default archive manager. The user will be prompted to
 * specify a destination folder, and the @folder will be suggested
 * as default destination.
 *
 * Note that %-1 will also be returned when the user cancels this
 * operation, but @error will not be set then.
 *
 * Return value: the process id of the spawned command, or %-1
 *               on error.
 **/
GPid
tap_backend_extract_to (const gchar *folder,
                        GList       *files,
                        GtkWidget   *window,
                        GError     **error)
{
  g_return_val_if_fail (files != NULL, -1);
  g_return_val_if_fail (GTK_IS_WINDOW (window), -1);
  g_return_val_if_fail (g_path_is_absolute (folder), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  /* run the action */
  return tap_backend_run ("extract-to", folder, files, NULL, window, error);
}

