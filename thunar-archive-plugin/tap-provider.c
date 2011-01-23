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
static GList *tap_provider_get_file_actions     (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 GList                    *files);
#if THUNARX_CHECK_VERSION(0,4,1)
static GList *tap_provider_get_dnd_actions      (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 ThunarxFileInfo          *folder,
                                                 GList                    *files);
#endif
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
static void   tap_provider_child_watch_destroy  (gpointer                  user_data);



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

  /* child watch support for the last spawn command, which allowed us to refresh 
   * the folder contents after the command had terminated with ThunarVFS (i.e. 
   * when the archive had been created). This no longer works with GIO but 
   * we still use the watch to close the PID properly.
   */
  gint            child_watch_id;
};



static const gchar TAP_MIME_TYPES[][34] = {
  "application/x-7z-compressed",
  "application/x-7z-compressed-tar",
  "application/x-ar",
  "application/x-arj",
  "application/x-bzip",
  "application/x-bzip-compressed-tar",
  "application/x-compress",
  "application/x-compressed-tar",
  "application/x-deb",
  "application/x-gtar",
  "application/x-gzip",
  "application/x-lha",
  "application/x-lhz",
  "application/x-lzma",
  "application/x-lzma-compressed-tar",
  "application/x-rar",
  "application/x-rar-compressed",
  "application/x-tar",
  "application/x-xz",
  "application/x-xz-compressed-tar",
  "application/x-zip",
  "application/x-zip-compressed",
  "application/zip",
  "multipart/x-zip",
  "application/x-rpm",
  "application/x-jar",
  "application/x-java-archive",
  "application/x-lzop",
  "application/x-zoo",
  "application/x-cd-image",
  "application/x-7z-compressed",
};

static GQuark tap_action_files_quark;
#if THUNARX_CHECK_VERSION(0,4,1)
static GQuark tap_action_folder_quark;
#endif
static GQuark tap_action_provider_quark;



THUNARX_DEFINE_TYPE_WITH_CODE (TapProvider,
                               tap_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            tap_provider_menu_provider_init));


static void
tap_provider_class_init (TapProviderClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the "tap-action-files", "tap-action-folder" and "tap-action-provider" quarks */
  tap_action_files_quark = g_quark_from_string ("tap-action-files");
#if THUNARX_CHECK_VERSION(0,4,1)
  tap_action_folder_quark = g_quark_from_string ("tap-action-folder");
#endif
  tap_action_provider_quark = g_quark_from_string ("tap-action-provider");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = tap_provider_finalize;
}



static void
tap_provider_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_actions = tap_provider_get_file_actions;
#if THUNARX_CHECK_VERSION(0,4,1)
  iface->get_dnd_actions = tap_provider_get_dnd_actions;
#endif
}



static void
tap_provider_init (TapProvider *tap_provider)
{
#if !GTK_CHECK_VERSION(2,9,0)
  GtkIconSource *icon_source;
  GtkIconSet    *icon_set;

  /* setup our icon factory */
  tap_provider->icon_factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (tap_provider->icon_factory);

  /* add the "tap-create" stock icon */
  icon_set = gtk_icon_set_new ();
  icon_source = gtk_icon_source_new ();
  gtk_icon_source_set_icon_name (icon_source, "tap-create");
  gtk_icon_set_add_source (icon_set, icon_source);
  gtk_icon_factory_add (tap_provider->icon_factory, "tap-create", icon_set);
  gtk_icon_source_free (icon_source);
  gtk_icon_set_unref (icon_set);

  /* add the "tap-extract" stock icon */
  icon_set = gtk_icon_set_new ();
  icon_source = gtk_icon_source_new ();
  gtk_icon_source_set_icon_name (icon_source, "tap-extract");
  gtk_icon_set_add_source (icon_set, icon_source);
  gtk_icon_factory_add (tap_provider->icon_factory, "tap-extract", icon_set);
  gtk_icon_source_free (icon_source);
  gtk_icon_set_unref (icon_set);

  /* add the "tap-extract-to" stock icon */
  icon_set = gtk_icon_set_new ();
  icon_source = gtk_icon_source_new ();
  gtk_icon_source_set_icon_name (icon_source, "tap-extract-to");
  gtk_icon_set_add_source (icon_set, icon_source);
  gtk_icon_factory_add (tap_provider->icon_factory, "tap-extract-to", icon_set);
  gtk_icon_source_free (icon_source);
  gtk_icon_set_unref (icon_set);
#endif /* !GTK_CHECK_VERSION(2,9,0) */
}



static void
tap_provider_finalize (GObject *object)
{
  TapProvider *tap_provider = TAP_PROVIDER (object);
  GSource     *source;

  /* give up maintaince of any pending child watch */
  if (G_UNLIKELY (tap_provider->child_watch_id != 0))
    {
      /* reset the callback function to g_spawn_close_pid() so the plugin can be
       * safely unloaded and the child will still not become a zombie afterwards.
       */
      source = g_main_context_find_source_by_id (NULL, tap_provider->child_watch_id);
      g_source_set_callback (source, (GSourceFunc) g_spawn_close_pid, NULL, NULL);
    }
  
#if !GTK_CHECK_VERSION(2,9,0)
  /* release our icon factory */
  gtk_icon_factory_remove_default (tap_provider->icon_factory);
  g_object_unref (G_OBJECT (tap_provider->icon_factory));
#endif

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
tap_extract_here (GtkAction *action,
                  GtkWidget *window)
{
#if THUNARX_CHECK_VERSION(0,4,1)
  ThunarxFileInfo *folder;
#endif
  TapProvider     *tap_provider;
  GList           *files;
  gchar           *dirname;
  gchar           *uri;

  /* determine the files associated with the action */
  files = g_object_get_qdata (G_OBJECT (action), tap_action_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the action */
  tap_provider = g_object_get_qdata (G_OBJECT (action), tap_action_provider_quark);
  if (G_UNLIKELY (tap_provider == NULL))
    return;

#if THUNARX_CHECK_VERSION(0,4,1)
  /* check if a folder was supplied (for the Drag'n'Drop action) */
  folder = g_object_get_qdata (G_OBJECT (action), tap_action_folder_quark);
  if (G_UNLIKELY (folder != NULL))
    {
      /* determine the URI of the supplied folder */
      uri = thunarx_file_info_get_uri (folder);
    }
  else
#endif
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
          /* execute the action */
          tap_provider_execute (tap_provider, tap_backend_extract_here, window, dirname, files, _("Failed to extract files"));

          /* release the dirname */
          g_free (dirname);
        }

      /* release the URI */
      g_free (uri);
    }
}



static void
tap_extract_to (GtkAction *action,
                GtkWidget *window)
{
  TapProvider *tap_provider;
  const gchar *default_dir;
  GList       *files;

  /* determine the files associated with the action */
  files = g_object_get_qdata (G_OBJECT (action), tap_action_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the action */
  tap_provider = g_object_get_qdata (G_OBJECT (action), tap_action_provider_quark);
  if (G_UNLIKELY (tap_provider == NULL))
    return;

  /* if $GTK_DEFAULT_FILECHOOSER_DIR is set, we use that as default
   * folder (i.e. Ubuntu), otherwise we just use $HOME.
   */
  default_dir = g_getenv ("GTK_DEFAULT_FILECHOOSER_DIR");
  if (G_LIKELY (default_dir == NULL))
    default_dir = g_get_home_dir ();

  /* execute the action */
  tap_provider_execute (tap_provider, tap_backend_extract_to, window, default_dir, files, _("Failed to extract files"));
}



static void
tap_create_archive (GtkAction *action,
                    GtkWidget *window)
{
  TapProvider *tap_provider;
  GList       *files;
  gchar       *dirname;
  gchar       *uri;

  /* determine the files associated with the action */
  files = g_object_get_qdata (G_OBJECT (action), tap_action_files_quark);
  if (G_UNLIKELY (files == NULL))
    return;

  /* determine the provider associated with the action */
  tap_provider = g_object_get_qdata (G_OBJECT (action), tap_action_provider_quark);
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

  /* execute the action */
  tap_provider_execute (tap_provider, tap_backend_create_archive, window, dirname, files, _("Failed to create archive"));

  /* cleanup */
  g_free (dirname);
}



static GList*
tap_provider_get_file_actions (ThunarxMenuProvider *menu_provider,
                               GtkWidget           *window,
                               GList               *files)
{
  gchar              *scheme;
  TapProvider        *tap_provider = TAP_PROVIDER (menu_provider);
  GtkAction          *action;
  GClosure           *closure;
  gboolean            all_archives = TRUE;
  gboolean            can_write = TRUE;
  GList              *actions = NULL;
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
          /* append the "Extract Here" action */
          action = g_object_new (GTK_TYPE_ACTION,
                                 "name", "Tap::extract-here",
                                 "label", _("Extract _Here"),
#if !GTK_CHECK_VERSION(2,9,0)
                                 "stock-id", "tap-extract",
#else
                                 "icon-name", "tap-extract",
#endif
                                 "tooltip", dngettext (GETTEXT_PACKAGE,
                                                       "Extract the selected archive in the current folder",
                                                       "Extract the selected archives in the current folder",
                                                       n_files),
                                 NULL);
          g_object_set_qdata_full (G_OBJECT (action), tap_action_files_quark,
                                   thunarx_file_info_list_copy (files),
                                   (GDestroyNotify) thunarx_file_info_list_free);
          g_object_set_qdata_full (G_OBJECT (action), tap_action_provider_quark,
                                   g_object_ref (G_OBJECT (tap_provider)),
                                   (GDestroyNotify) g_object_unref);
          closure = g_cclosure_new_object (G_CALLBACK (tap_extract_here), G_OBJECT (window));
          g_signal_connect_closure (G_OBJECT (action), "activate", closure, TRUE);
          actions = g_list_append (actions, action);
        }

      /* append the "Extract To..." action */
      action = g_object_new (GTK_TYPE_ACTION,
                             "label", _("_Extract To..."),
                             "name", "Tap::extract-to",
#if !GTK_CHECK_VERSION(2,9,0)
                             "stock-id", "tap-extract-to",
#else
                             "icon-name", "tap-extract-to",
#endif
                             "tooltip", dngettext (GETTEXT_PACKAGE,
                                                   "Extract the selected archive",
                                                   "Extract the selected archives",
                                                   n_files),
                             NULL);
      g_object_set_qdata_full (G_OBJECT (action), tap_action_files_quark,
                               thunarx_file_info_list_copy (files),
                               (GDestroyNotify) thunarx_file_info_list_free);
      g_object_set_qdata_full (G_OBJECT (action), tap_action_provider_quark,
                               g_object_ref (G_OBJECT (tap_provider)),
                               (GDestroyNotify) g_object_unref);
      closure = g_cclosure_new_object (G_CALLBACK (tap_extract_to), G_OBJECT (window));
      g_signal_connect_closure (G_OBJECT (action), "activate", closure, TRUE);
      actions = g_list_append (actions, action);
    }

  /* check if more than one files was given or the file is not an archive */
  if (G_LIKELY (n_files > 1 || !all_archives))
    {
      /* append the "Create Archive..." action */
      action = g_object_new (GTK_TYPE_ACTION,
                             "label", _("Cr_eate Archive..."),
                             "name", "Tap::create-archive",
#if !GTK_CHECK_VERSION(2,9,0)
                             "stock-id", "tap-create",
#else
                             "icon-name", "tap-create",
#endif
                             "tooltip", dngettext (GETTEXT_PACKAGE,
                                                   "Create an archive with the selected object",
                                                   "Create an archive with the selected objects",
                                                   n_files),
                             NULL);
      g_object_set_qdata_full (G_OBJECT (action), tap_action_files_quark,
                               thunarx_file_info_list_copy (files),
                               (GDestroyNotify) thunarx_file_info_list_free);
      g_object_set_qdata_full (G_OBJECT (action), tap_action_provider_quark,
                               g_object_ref (G_OBJECT (tap_provider)),
                               (GDestroyNotify) g_object_unref);
      closure = g_cclosure_new_object (G_CALLBACK (tap_create_archive), G_OBJECT (window));
      g_signal_connect_closure (G_OBJECT (action), "activate", closure, TRUE);
      actions = g_list_append (actions, action);
    }

  return actions;
}



#if THUNARX_CHECK_VERSION(0,4,1)
static GList*
tap_provider_get_dnd_actions (ThunarxMenuProvider *menu_provider,
                              GtkWidget           *window,
                              ThunarxFileInfo     *folder,
                              GList               *files)
{
  gchar              *scheme;
  TapProvider        *tap_provider = TAP_PROVIDER (menu_provider);
  GtkAction          *action;
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

  /* setup the "Extract here" action */
  action = g_object_new (GTK_TYPE_ACTION,
                         "name", "Tap::extract-here-dnd",
                         /* TRANSLATORS: This is the label of the Drag'n'Drop "Extract here" action */
                         "label", _("_Extract here"),
#if !GTK_CHECK_VERSION(2,9,0)
                         "stock-id", "tap-extract",
#else
                         "icon-name", "tap-extract",
#endif
                         "tooltip", dngettext (GETTEXT_PACKAGE,
                                               "Extract the selected archive here",
                                               "Extract the selected archives here",
                                               n_files),
                         NULL);
  g_object_set_qdata_full (G_OBJECT (action), tap_action_files_quark,
                           thunarx_file_info_list_copy (files),
                           (GDestroyNotify) thunarx_file_info_list_free);
  g_object_set_qdata_full (G_OBJECT (action), tap_action_provider_quark,
                           g_object_ref (G_OBJECT (tap_provider)),
                           (GDestroyNotify) g_object_unref);
  g_object_set_qdata_full (G_OBJECT (action), tap_action_folder_quark,
                           g_object_ref (G_OBJECT (folder)),
                           (GDestroyNotify) g_object_unref);
  closure = g_cclosure_new_object (G_CALLBACK (tap_extract_here), G_OBJECT (window));
  g_signal_connect_closure (G_OBJECT (action), "activate", closure, TRUE);

  /* return a list with only the "Extract here" action */
  return g_list_prepend (NULL, action);
}
#endif



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
  GSource   *source;
  GError    *error = NULL;
  GPid       pid;

  /* try to execute the action */
  pid = (*action) (folder, files, window, &error);
  if (G_LIKELY (pid >= 0))
    {
      /* check if we already have a child watch */
      if (G_UNLIKELY (tap_provider->child_watch_id != 0))
        {
          /* reset the callback function to g_spawn_close_pid() so the plugin can be
           * safely unloaded and the child will still not become a zombie afterwards.
           */
          source = g_main_context_find_source_by_id (NULL, tap_provider->child_watch_id);
          g_source_set_callback (source, (GSourceFunc) g_spawn_close_pid, NULL, NULL);
        }

      /* schedule the new child watch */
      tap_provider->child_watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid, tap_provider_child_watch,
                                                             tap_provider, tap_provider_child_watch_destroy);
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
  GDK_THREADS_ENTER ();

  /* need to cleanup */
  g_spawn_close_pid (pid);

  GDK_THREADS_LEAVE ();
}



static void
tap_provider_child_watch_destroy (gpointer user_data)
{
  TapProvider *tap_provider = TAP_PROVIDER (user_data);

  /* reset child watch id */
  tap_provider->child_watch_id = 0;
}



