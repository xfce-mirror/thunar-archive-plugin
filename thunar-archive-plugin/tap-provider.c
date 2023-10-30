/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General 
 * Public License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <thunar-archive-plugin/tap-backend.h>
#include <thunar-archive-plugin/tap-provider.h>

/* use g_access() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(filename, mode) access((filename), (mode))
#endif



static void   tap_provider_menu_provider_init   (ThunarxMenuProviderIface *iface);
static void   tap_provider_finalize             (GObject                  *object);
static GList *tap_provider_get_file_menu_items  (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 GList                    *files);
static GList *tap_provider_get_dnd_menu_items   (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 ThunarxFileInfo          *folder,
                                                 GList                    *files);
static void   tap_provider_execute              (TapProvider              *tap_provider,
                                                 GPid                    (*action) (const gchar *folder,
                                                                                    GList       *files,
                                                                                    GtkWidget   *window,
                                                                                    GError     **error),
                                                 GtkWidget                *window,
                                                 const gchar              *folder,
                                                 GList                    *files,
                                                 const gchar              *error_message);
static void   tap_provider_child_watch          (GPid                      pid,
                                                 gint                      status,
                                                 gpointer                  user_data);



struct _TapProviderClass
{
  GObjectClass __parent__;
};

struct _TapProvider
{
  GObject         __parent__;

#if !GTK_CHECK_VERSION(2,9,0)
  /* GTK+ 2.9.0 and above provide an icon-name property
   * for GtkActions, so we don't need the icon factory.
   */
  GtkIconFactory *icon_factory;
#endif
};



static const gchar TAP_MIME_TYPES[][34] = {
  "application/x-7z-compressed",
  "application/x-7z-compressed-tar",
  "application/x-ar",
  "application/x-arj",
  "application/x-bzip",
  "application/x-bzip-compressed-tar",
  "application/x-bzip2",
  "application/x-bzip2-compressed-tar",
  "application/x-bzip3",
  "application/x-bzip3-compressed-tar",
  "application/x-cd-image",
  "application/x-compress",
  "application/x-compressed-tar",
  "application/x-deb",
  "application/x-gtar",
  "application/x-gzip",
  "application/x-jar",
  "application/x-java-archive",
  "application/x-lha",
  "application/x-lhz",
  "application/x-lrzip",
  "application/x-lrzip-compressed-tar",
  "application/x-lz4",
  "application/x-lz4-compressed-tar",
  "application/x-lzip",
  "application/x-lzip-compressed-tar",
  "application/x-lzma",
  "application/x-lzma-compressed-tar",
  "application/x-lzop",
  "application/x-rar",
  "application/x-rar-compressed",
  "application/x-rpm",
  "application/x-tar",
  "application/x-xz",
  "application/x-xz-compressed-tar",
  "application/x-zip",
  "application/x-zip-compressed",
  "application/x-zoo",
  "application/x-zstd-compressed-tar",
  "application/zip",
  "application/zstd",
  "multipart/x-zip",
};

static GQuark tap_item_files_quark;
static GQuark tap_item_folder_quark;
static GQuark tap_item_provider_quark;



THUNARX_DEFINE_TYPE_WITH_CODE (TapProvider,
                               tap_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            tap_provider_menu_provider_init));


static void
tap_provider_class_init (TapProviderClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the "tap-item-files", "tap-item-folder" and "tap-item-provider" quarks */
  tap_item_files_quark = g_quark_from_string ("tap-item-files");
  tap_item_folder_quark = g_quark_from_string ("tap-item-folder");
  tap_item_provider_quark = g_quark_from_string ("tap-item-provider");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = tap_provider_finalize;
}



static void
tap_provider_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_menu_items = tap_provider_get_file_menu_items;
  iface->get_dnd_menu_items = tap_provider_get_dnd_menu_items;
}



static void
tap_provider_init (TapProvider *tap_provider)
{
}



static void
tap_provider_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (tap_provider_parent_class)->finalize) (object);
}



static gboolean
tap_is_archive (ThunarxFileInfo *file_info)
{
  guint n;

  for (n = 0; n < G_N_ELEMENTS (TAP_MIME_TYPES); ++n)
    if (thunarx_file_info_has_mime_type (file_info, TAP_MIME_TYPES[n]))
      return TRUE;

  return FALSE;
}



static gboolean
tap_is_parent_writable (ThunarxFileInfo *file_info)
{
  gboolean result = FALSE;
  gchar   *filename;
  gchar   *uri;

  /* determine the parent URI for the file info */
  uri = thunarx_file_info_get_parent_uri (file_info);
  if (G_LIKELY (uri != NULL))
    {
      /* determine the local filename for the URI */
      filename = g_filename_from_uri (uri, NULL, NULL);
      if (G_LIKELY (filename != NULL))
        {
          /* check if we can write to that folder */
          result = (g_access (filename, W_OK) == 0);

          /* release the filename */
          g_free (filename);
        }

      /* release the URI */
      g_free (uri);
    }

  return result;
}



static void
tap_extract_here (ThunarxMenuItem *item,
                  GtkWidget       *window)
{
  ThunarxFileInfo *folder;
  TapProvider     *tap_provider;
  GList           *files;
  gchar           *dirname;
  gchar           *uri;

  /* determine the files associated with the item */
  files = g_object_get_qdata (G_OBJECT (item), tap_item_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the item */
  tap_provider = g_object_get_qdata (G_OBJECT (item), tap_item_provider_quark);
  if (G_UNLIKELY (tap_provider == NULL))
    return;

  /* check if a folder was supplied (for the Drag'n'Drop item) */
  folder = g_object_get_qdata (G_OBJECT (item), tap_item_folder_quark);
  if (G_UNLIKELY (folder != NULL))
    {
      /* determine the URI of the supplied folder */
      uri = thunarx_file_info_get_uri (folder);
    }
  else
    {
      /* determine the parent URI of the first selected file */
      uri = thunarx_file_info_get_parent_uri (files->data);
    }

  /* verify that we have an URI */
  if (G_LIKELY (uri != NULL))
    {
      /* determine the directory of the first selected file */
      dirname = g_filename_from_uri (uri, NULL, NULL);

      /* verify that we were able to determine a local path */
      if (G_LIKELY (dirname != NULL))
        {
          /* execute the action associated with the menu item */
          tap_provider_execute (tap_provider, tap_backend_extract_here, window, dirname, files, _("Failed to extract files"));

          /* release the dirname */
          g_free (dirname);
        }

      /* release the URI */
      g_free (uri);
    }
}



static void
tap_extract_to (ThunarxMenuItem *item,
                GtkWidget       *window)
{
  TapProvider     *tap_provider;
  GList           *files;
  gchar           *dirname;
  gchar           *uri;

  /* determine the files associated with the item */
  files = g_object_get_qdata (G_OBJECT (item), tap_item_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the item */
  tap_provider = g_object_get_qdata (G_OBJECT (item), tap_item_provider_quark);
  if (G_UNLIKELY (tap_provider == NULL))
    {
      g_warning ("Failed to determine tap provider");
      return;
    }


  /* determine the parent URI of the first selected file */
  uri = thunarx_file_info_get_parent_uri (files->data);
  if (G_UNLIKELY (uri == NULL))
    {
      g_warning ("Failed to get parent URI");
      return;
    }

  /* determine the directory of the first selected file */
  dirname = g_filename_from_uri (uri, NULL, NULL);
  g_free (uri);

  /* verify that we were able to determine a local path */
  if (G_UNLIKELY (dirname == NULL))
    {
      g_warning ("Failed to determine local path");
      return;
    }

  /* execute the action */
  tap_provider_execute (tap_provider, tap_backend_extract_to, window, dirname, files, _("Failed to extract files"));

  /* cleanup */
  g_free (dirname);
}



static void
tap_create_archive (ThunarxMenuItem *item,
                    GtkWidget       *window)
{
  TapProvider *tap_provider;
  GList       *files;
  gchar       *dirname;
  gchar       *uri;

  /* determine the files associated with the item */
  files = g_object_get_qdata (G_OBJECT (item), tap_item_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the item */
  tap_provider = g_object_get_qdata (G_OBJECT (item), tap_item_provider_quark);
  if (G_UNLIKELY (tap_provider == NULL))
    return;

  /* determine the parent URI of the first selected file */
  uri = thunarx_file_info_get_parent_uri (files->data);
  if (G_UNLIKELY (uri == NULL))
    return;

  /* determine the directory of the first selected file */
  dirname = g_filename_from_uri (uri, NULL, NULL);
  g_free (uri);

  /* verify that we were able to determine a local path */
  if (G_UNLIKELY (dirname == NULL))
    return;

  /* execute the action associated with the menu item */
  tap_provider_execute (tap_provider, tap_backend_create_archive, window, dirname, files, _("Failed to create archive"));

  /* cleanup */
  g_free (dirname);
}



static GList*
tap_provider_get_file_menu_items (ThunarxMenuProvider *menu_provider,
                                  GtkWidget           *window,
                                  GList               *files)
{
  gchar              *scheme;
  TapProvider        *tap_provider = TAP_PROVIDER (menu_provider);
  ThunarxMenuItem    *item;
  GClosure           *closure;
  gboolean            all_archives = TRUE;
  gboolean            can_write = TRUE;
  GList              *items = NULL;
  GList              *lp;
  gint                n_files = 0;

  /* check all supplied files */
  for (lp = files; lp != NULL; lp = lp->next, ++n_files)
    {
      /* check if the file is a local file */
      scheme = thunarx_file_info_get_uri_scheme (lp->data);

      /* unable to handle non-local files */
      if (G_UNLIKELY (strcmp (scheme, "file")))
        {
          g_free (scheme);
          return NULL;
        }
      g_free (scheme);

      /* check if this file is a supported archive */
      if (all_archives && !tap_is_archive (lp->data))
        all_archives = FALSE;

      /* check if we can write to the parent folder */
      if (can_write && !tap_is_parent_writable (lp->data))
        can_write = FALSE;
    }

  /* check if all files are supported archives */
  if (all_archives)
    {
      /* check if we can write to the parent folders */
      if (G_LIKELY (can_write))
        {
          /* append the "Extract Here" menu item */
          item = thunarx_menu_item_new ("Tap::extract-here",
                                        _("Extract _Here"),
                                        dngettext (GETTEXT_PACKAGE,
                                                   "Extract the selected archive in the current folder",
                                                   "Extract the selected archives in the current folder",
                                                   n_files),
                                        "tap-extract");

          g_object_set_qdata_full (G_OBJECT (item), tap_item_files_quark,
                                   thunarx_file_info_list_copy (files),
                                   (GDestroyNotify) thunarx_file_info_list_free);
          g_object_set_qdata_full (G_OBJECT (item), tap_item_provider_quark,
                                   g_object_ref (G_OBJECT (tap_provider)),
                                   (GDestroyNotify) g_object_unref);
          closure = g_cclosure_new_object (G_CALLBACK (tap_extract_here), G_OBJECT (window));
          g_signal_connect_closure (G_OBJECT (item), "activate", closure, TRUE);
          items = g_list_append (items, item);
        }

      /* append the "Extract To..." menu item */
      item = thunarx_menu_item_new ("Tap::extract-to",
                                    _("_Extract To..."),
                                    dngettext (GETTEXT_PACKAGE,
                                               "Extract the selected archive",
                                               "Extract the selected archives",
                                               n_files),
                                    "tap-extract-to");

      g_object_set_qdata_full (G_OBJECT (item), tap_item_files_quark,
                               thunarx_file_info_list_copy (files),
                               (GDestroyNotify) thunarx_file_info_list_free);
      g_object_set_qdata_full (G_OBJECT (item), tap_item_provider_quark,
                               g_object_ref (G_OBJECT (tap_provider)),
                               (GDestroyNotify) g_object_unref);
      closure = g_cclosure_new_object (G_CALLBACK (tap_extract_to), G_OBJECT (window));
      g_signal_connect_closure (G_OBJECT (item), "activate", closure, TRUE);
      items = g_list_append (items, item);
    }

    /* append the "Create Archive..." menu item */
    item = thunarx_menu_item_new ("Tap::create-archive",
                                  _("Cr_eate Archive..."),
                                  dngettext (GETTEXT_PACKAGE,
                                              "Create an archive with the selected object",
                                              "Create an archive with the selected objects",
                                              n_files),
                                  "tap-create");

    g_object_set_qdata_full (G_OBJECT (item), tap_item_files_quark,
                              thunarx_file_info_list_copy (files),
                              (GDestroyNotify) thunarx_file_info_list_free);
    g_object_set_qdata_full (G_OBJECT (item), tap_item_provider_quark,
                              g_object_ref (G_OBJECT (tap_provider)),
                              (GDestroyNotify) g_object_unref);
    closure = g_cclosure_new_object (G_CALLBACK (tap_create_archive), G_OBJECT (window));
    g_signal_connect_closure (G_OBJECT (item), "activate", closure, TRUE);
    items = g_list_append (items, item);

  return items;
}



static GList*
tap_provider_get_dnd_menu_items (ThunarxMenuProvider *menu_provider,
                                 GtkWidget           *window,
                                 ThunarxFileInfo     *folder,
                                 GList               *files)
{
  gchar              *scheme;
  TapProvider        *tap_provider = TAP_PROVIDER (menu_provider);
  ThunarxMenuItem    *item;
  GClosure           *closure;
  GList              *lp;
  gint                n_files = 0;

  /* check if the folder is a local folder */
  scheme = thunarx_file_info_get_uri_scheme (folder);

  /* unable to extract to non-local folders */
  if (G_UNLIKELY (strcmp (scheme, "file")))
    {
      g_free (scheme);
      return NULL;
    }
  g_free (scheme);

  /* check all supplied files */
  for (lp = files; lp != NULL; lp = lp->next, ++n_files)
    {
      /* check if the file is a local file */
      scheme = thunarx_file_info_get_uri_scheme (lp->data);

      /* unable to handle non-local files */
      if (G_UNLIKELY (strcmp (scheme, "file")))
        {
          g_free (scheme);
          return NULL;
        }
      g_free (scheme);

      /* check if this file is a supported archive */
      if (G_LIKELY (!tap_is_archive (lp->data)))
        return NULL;
    }

  /* setup the "Extract here" menu item */
  item = thunarx_menu_item_new ("Tap::extract-here-dnd",
                                /* TRANSLATORS: This is the label of the Drag'n'Drop "Extract here" menu item */
                                _("_Extract here"),
                                dngettext (GETTEXT_PACKAGE,
                                           "Extract the selected archive here",
                                           "Extract the selected archives here",
                                           n_files),
                                "tap-extract");

  g_object_set_qdata_full (G_OBJECT (item), tap_item_files_quark,
                           thunarx_file_info_list_copy (files),
                           (GDestroyNotify) thunarx_file_info_list_free);
  g_object_set_qdata_full (G_OBJECT (item), tap_item_provider_quark,
                           g_object_ref (G_OBJECT (tap_provider)),
                           (GDestroyNotify) g_object_unref);
  g_object_set_qdata_full (G_OBJECT (item), tap_item_folder_quark,
                           g_object_ref (G_OBJECT (folder)),
                           (GDestroyNotify) g_object_unref);
  closure = g_cclosure_new_object (G_CALLBACK (tap_extract_here), G_OBJECT (window));
  g_signal_connect_closure (G_OBJECT (item), "activate", closure, TRUE);

  /* return a list with only the "Extract here" item */
  return g_list_prepend (NULL, item);
}



static void
tap_provider_execute (TapProvider *tap_provider,
                      GPid       (*action) (const gchar *folder,
                                            GList       *files,
                                            GtkWidget   *window,
                                            GError     **error),
                      GtkWidget   *window,
                      const gchar *folder,
                      GList       *files,
                      const gchar *error_message)
{
  GtkWidget *dialog;
  GError    *error = NULL;
  GPid       pid;

  /* try to execute the action */
  pid = (*action) (folder, files, window, &error);
  if (G_LIKELY (pid >= 0))
    {
      /* schedule the new child watch */
      g_child_watch_add_full (G_PRIORITY_LOW, pid, tap_provider_child_watch, NULL, NULL);
    }
  else if (error != NULL)
    {
      /* display an error dialog */
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "%s.", error_message);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
}



static void
tap_provider_child_watch (GPid     pid,
                          gint     status,
                          gpointer user_data)
{
  /* need to cleanup */
  g_spawn_close_pid (pid);
}



