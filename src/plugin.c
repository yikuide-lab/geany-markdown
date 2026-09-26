/*
 * plugin.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "config.h"
#include <geanyplugin.h>
#include "viewer.h"
#include "conf.h"

static GeanyPlugin *geany_plugin = NULL;

/* Cheap fingerprint of the active Geany color scheme (name + default/selection
 * styles). Geany has no "color scheme changed" signal; View→Color Schemes
 * re-styles every editor and paints, so SCN_PAINTED + this check catches it. */
static guint scheme_fingerprint(void)
{
  GeanyData *gd;
  const GeanyLexerStyle *st;
  guint h = 0;

  if (geany_plugin == NULL || geany_plugin->geany_data == NULL)
    return 0;
  gd = geany_plugin->geany_data;

  if (gd->editor_prefs && gd->editor_prefs->color_scheme)
    h = g_str_hash(gd->editor_prefs->color_scheme);

  st = highlighting_get_style(GEANY_FILETYPES_NONE, 0);
  if (st)
    h = h * 31u + (guint) (st->foreground ^ st->background);
  st = highlighting_get_style(GEANY_FILETYPES_NONE, 1);
  if (st)
    h = h * 31u + (guint) (st->foreground ^ st->background);

  return h;
}

/* Should be defined by build system, this is just a fallback */
#ifndef MARKDOWN_DOC_DIR
#  define MARKDOWN_DOC_DIR "/usr/local/share/doc/geany-plugins/markdown"
#endif
#ifndef MARKDOWN_HELP_FILE
#  define MARKDOWN_HELP_FILE MARKDOWN_DOC_DIR "/html/help.html"
#endif

#define MARKDOWN_PREVIEW_LABEL _("Markdown Preview")

/* Global data */
static MarkdownViewer *g_viewer = NULL;
static GtkWidget *g_scrolled_win = NULL;
static GtkWidget *g_export_html = NULL;
static guint g_scheme_fp = 0;

/* Forward declarations */
static void update_markdown_viewer(MarkdownViewer *viewer);
static void sync_zoom_from_editor(MarkdownViewer *viewer, ScintillaObject *sci);
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *notif, MarkdownViewer *viewer);
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownViewer *viewer);
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old, MarkdownViewer *viewer);
static void on_view_pos_notify(GObject *obj, GParamSpec *pspec, MarkdownViewer *viewer);
static void on_export_as_html_activate(GtkMenuItem *item, MarkdownViewer *viewer);

/* Plugin entry point on activation. */
static gboolean md_plugin_init(GeanyPlugin *plugin, gpointer data)
{
  gint page_num;
  gchar *conf_fn;
  MarkdownConfig *conf;
  MarkdownConfigViewPos view_pos;
  GtkWidget *viewer;
  GtkNotebook *nb;

  geany_plugin = plugin;

  /* Setup the config object which is needed by the view. */
  conf_fn = g_build_filename(geany_plugin->geany_data->app->configdir,
    "plugins", "markdown", "markdown.conf", NULL);
  conf = markdown_config_new(conf_fn);
  g_free(conf_fn);

  viewer = markdown_viewer_new(conf);
  /* store as global for plugin_cleanup() */
  g_viewer = MARKDOWN_VIEWER(viewer);
  view_pos = markdown_config_get_view_pos(conf);

  g_scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(g_scrolled_win), viewer);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_scrolled_win),
    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  if (view_pos == MARKDOWN_CONFIG_VIEW_POS_MSGWIN) {
    nb = GTK_NOTEBOOK(geany_plugin->geany_data->main_widgets->message_window_notebook);
    page_num = gtk_notebook_append_page(nb,
      g_scrolled_win, gtk_label_new(MARKDOWN_PREVIEW_LABEL));
  } else {
    nb = GTK_NOTEBOOK(geany_plugin->geany_data->main_widgets->sidebar_notebook);
    page_num = gtk_notebook_append_page(nb,
      g_scrolled_win, gtk_label_new(MARKDOWN_PREVIEW_LABEL));
  }

  gtk_widget_show_all(g_scrolled_win);
  gtk_notebook_set_current_page(nb, page_num);

  g_signal_connect(conf, "notify::view-pos", G_CALLBACK(on_view_pos_notify), viewer);

  g_scheme_fp = scheme_fingerprint();

  g_export_html = gtk_menu_item_new_with_label(_("Export Markdown as HTML..."));
  gtk_menu_shell_append(GTK_MENU_SHELL(geany_plugin->geany_data->main_widgets->tools_menu), g_export_html);
  g_signal_connect(g_export_html, "activate", G_CALLBACK(on_export_as_html_activate), viewer);
  gtk_widget_show(g_export_html);

#define MD_PSC(sig, cb) \
  plugin_signal_connect(geany_plugin, NULL, (sig), TRUE, G_CALLBACK(cb), viewer)
  /* Geany takes care of disconnecting these for us when the plugin is unloaded,
   * the macro is just to make the code smaller/clearer. */
  MD_PSC("editor-notify", on_editor_notify);
  MD_PSC("document-activate", on_document_signal);
  MD_PSC("document-filetype-set", on_document_filetype_set);
  MD_PSC("document-new", on_document_signal);
  MD_PSC("document-open", on_document_signal);
  MD_PSC("document-reload", on_document_signal);
#undef MD_PSC

  update_markdown_viewer(MARKDOWN_VIEWER(viewer));

  return TRUE;
}

/* Cleanup resources on plugin unload. */
static void md_plugin_cleanup(GeanyPlugin *plugin, gpointer data)
{
  gtk_widget_destroy(g_export_html);
  gtk_widget_destroy(g_scrolled_win);

  geany_plugin = NULL;
}

/* Called to show the preferences GUI. */
static GtkWidget *md_plugin_configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer data)
{
  MarkdownConfig *conf = NULL;
  GtkWidget *gui;

  g_object_get(g_viewer, "config", &conf, NULL);
  gui = markdown_config_gui(conf, dialog);
  g_object_unref(conf); /* g_object_get() added a reference */
  return gui;
}

/* Called to show the plugin's help */
static void md_plugin_help(GeanyPlugin *plugin, gpointer data)
{
#ifdef G_OS_WIN32
  gchar *prefix = g_win32_get_package_installation_directory_of_module(NULL);
#else
  gchar *prefix = NULL;
#endif
  gchar *uri = g_strconcat("file://", prefix ? prefix : "", MARKDOWN_HELP_FILE, NULL);

  utils_open_browser(uri);

  g_free(uri);
  g_free(prefix);
}

/* All of the various signal handlers call this function to update the
 * MarkdownViewer on specific events. This causes a bunch of memory
 * allocations, re-compiles the Markdown to HTML, reformats the HTML
 * template, copies the HTML into the webview and causes it to (eventually)
 * be redrawn. Only call it when really needed, like when the scintilla
 * editor's text contents change and not on other editor events.
 */
static void
update_markdown_viewer(MarkdownViewer *viewer)
{
  GeanyDocument *doc = document_get_current();

  if (DOC_VALID(doc) && g_strcmp0(doc->file_type->name, "Markdown") == 0) {
    gchar *text;
    text = (gchar*) scintilla_send_message(doc->editor->sci, SCI_GETCHARACTERPOINTER, 0, 0);
    markdown_viewer_set_markdown(viewer, text, doc->encoding);
    gtk_widget_set_sensitive(g_export_html, TRUE);
  } else {
    markdown_viewer_set_markdown(viewer,
      _("The current document does not have a Markdown filetype."), "UTF-8");
    gtk_widget_set_sensitive(g_export_html, FALSE);
  }

  sync_zoom_from_editor(viewer, NULL);
  markdown_viewer_queue_update(viewer);
}

/* Map the Scintilla editor zoom (points added to the font size) onto a
 * WebKit zoom level so Ctrl++/Ctrl+- (View -> Zoom In/Out) in the editor
 * also scales the preview.  Zoom is per-document in Scintilla, so this is
 * also re-run when the document changes. */
static void
sync_zoom_from_editor(MarkdownViewer *viewer, ScintillaObject *sci)
{
  MarkdownConfig *conf = NULL;
  guint base_pt = 12;
  gint zoom;
  gdouble level;

  if (sci == NULL) {
    GeanyDocument *doc = document_get_current();
    if (!DOC_VALID(doc)) {
      markdown_viewer_set_zoom(viewer, 1.0);
      return;
    }
    sci = doc->editor->sci;
  }

  zoom = (gint) scintilla_send_message(sci, SCI_GETZOOM, 0, 0);

  g_object_get(viewer, "config", &conf, NULL);
  if (conf) {
    g_object_get(conf, "font-point-size", &base_pt, NULL);
    g_object_unref(conf);
  }
  if (base_pt < 1)
    base_pt = 12;

  level = (gdouble) (base_pt + zoom) / (gdouble) base_pt;
  markdown_viewer_set_zoom(viewer, level);
}

/* Return TRUE if event is a buffer modification that inserts or deletes
 * text and which caused a text changed length greater than 0. */
#define IS_MOD_NOTIF(nt) (nt->nmhdr.code == SCN_MODIFIED && \
                          nt->length > 0 && ( \
                          (nt->modificationType & SC_MOD_INSERTTEXT) || \
                          (nt->modificationType & SC_MOD_DELETETEXT)))

/* Queue update of the markdown preview on editor text change.
 * Also watches for color scheme switches: Geany has no dedicated signal,
 * but switching schemes re-styles every editor and paints, so SCN_PAINTED
 * is a reliable cheap check of a scheme fingerprint.  SCN_ZOOM tracks
 * Ctrl++/Ctrl+- (View -> Zoom In/Out/Reset) so the preview follows. */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
  SCNotification *notif, MarkdownViewer *viewer)
{
  if (IS_MOD_NOTIF(notif)) {
    update_markdown_viewer(viewer);
  } else if (notif->nmhdr.code == SCN_PAINTED) {
    guint fp = scheme_fingerprint();
    if (fp != g_scheme_fp) {
      g_scheme_fp = fp;
      markdown_viewer_queue_update(viewer);
    }
  } else if (notif->nmhdr.code == SCN_ZOOM) {
    sync_zoom_from_editor(viewer, editor ? editor->sci : NULL);
  }
  return FALSE; /* Allow others to handle this event too */
}

/* Queue update of the markdown preview on document signals (new, open,
 * activate, etc.) */
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownViewer *viewer)
{
  update_markdown_viewer(viewer);
}

/* Queue update of the markdown preview when a document's filetype is set */
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old,
  MarkdownViewer *viewer)
{
  update_markdown_viewer(viewer);
}

/* Move the MarkdownViewer to the correct notebook when the view position
 * is changed. */
static void
on_view_pos_notify(GObject *obj, GParamSpec *pspec, MarkdownViewer *viewer)
{
  gint page_num;
  GtkNotebook *newnb;
  GtkNotebook *snb = GTK_NOTEBOOK(geany_plugin->geany_data->main_widgets->sidebar_notebook);
  GtkNotebook *mnb = GTK_NOTEBOOK(geany_plugin->geany_data->main_widgets->message_window_notebook);
  MarkdownConfigViewPos view_pos;

  g_object_ref(g_scrolled_win); /* Prevent it from being destroyed */

  /* Remove the tab from whichever notebook its in (sidebar or msgwin) */
  page_num = gtk_notebook_page_num(snb, g_scrolled_win);
  if (page_num >= 0) {
    gtk_notebook_remove_page(snb, page_num);
  } else {
    page_num = gtk_notebook_page_num(mnb, g_scrolled_win);
    if (page_num >= 0) {
      gtk_notebook_remove_page(mnb, page_num);
    } else {
      g_warning("Unable to relocate the Markdown preview tab: not found");
    }
  }

  /* Check the user preference to get the new notebook */
  view_pos = markdown_config_get_view_pos(MARKDOWN_CONFIG(obj));
  newnb = (view_pos == MARKDOWN_CONFIG_VIEW_POS_MSGWIN) ? mnb : snb;

  page_num = gtk_notebook_append_page(newnb, g_scrolled_win,
    gtk_label_new(MARKDOWN_PREVIEW_LABEL));

  gtk_notebook_set_current_page(newnb, page_num);

  g_object_unref(g_scrolled_win); /* The new notebook owns it now */

  update_markdown_viewer(viewer);
}

static gchar *replace_extension(const gchar *utf8_fn, const gchar *new_ext)
{
  gchar *fn_noext, *new_fn, *dot;
  fn_noext = g_filename_from_utf8(utf8_fn, -1, NULL, NULL, NULL);
  dot = strrchr(fn_noext, '.');
  if (dot != NULL) {
    *dot = '\0';
  }
  new_fn = g_strconcat(fn_noext, new_ext, NULL);
  g_free(fn_noext);
  return new_fn;
}

static void on_export_as_html_activate(GtkMenuItem *item, MarkdownViewer *viewer)
{
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gchar *fn;
  GeanyDocument *doc;
  gboolean saved = FALSE;

  doc = document_get_current();
  g_return_if_fail(DOC_VALID(doc));

  dialog = gtk_file_chooser_dialog_new(_("Save HTML File As"),
    GTK_WINDOW(geany_plugin->geany_data->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SAVE,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
    NULL);
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

  fn = replace_extension(DOC_FILENAME(doc), ".html");
  if (g_file_test(fn, G_FILE_TEST_EXISTS)) {
    /* If the file exists, GtkFileChooser will change to the correct
     * directory and show the base name as a suggestion. */
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), fn);
  } else {
    /* If the file doesn't exist, change the directory and give a suggested
     * name for the file, since GtkFileChooser won't do it. */
    gchar *dn = g_path_get_dirname(fn);
    gchar *bn = g_path_get_basename(fn);
    gchar *utf8_name;
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dn);
    g_free(dn);
    utf8_name = g_filename_to_utf8(bn, -1, NULL, NULL, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), utf8_name);
    g_free(bn);
    g_free(utf8_name);
  }
  g_free(fn);

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, _("HTML Files"));
  gtk_file_filter_add_mime_type(filter, "text/html");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, _("All Files"));
  gtk_file_filter_add_pattern(filter, "*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  while (!saved &&
         gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *html = markdown_viewer_get_html(viewer);
    GError *error = NULL;
    fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (! g_file_set_contents(fn, html, -1, &error)) {
      dialogs_show_msgbox(GTK_MESSAGE_ERROR,
        _("Failed to export Markdown HTML to file '%s': %s"),
        fn, error->message);
      g_error_free(error);
    } else {
      saved = TRUE;
    }
    g_free(fn);
    g_free(html);
  }

  gtk_widget_destroy(dialog);
}

/* Main plugin entry point on plugin load */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
  /* setup translation */
  main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

  /* metadata */
  plugin->info->name = "Markdown";
  plugin->info->description = _("Real-time Markdown preview");
  plugin->info->version = "0.01";
  plugin->info->author = "Matthew Brush <mbrush@codebrainz.ca>";

  /* entry points */
  plugin->funcs->init = md_plugin_init;
  plugin->funcs->cleanup = md_plugin_cleanup;
  plugin->funcs->configure = md_plugin_configure;
  plugin->funcs->help = md_plugin_help;

  /* Prevent segfault in plugin when it registers GTypes and gets unloaded
   * and when reloaded tries to re-register the GTypes.
   * It used to be only needed in init() (e.g. when the plugin actually called
   * some WebKit stuff), but we now see crashes with recent WebKit versions
   * even if the module has merely been loaded (e.g. for listing in the plugin
   * manager). */
  plugin_module_make_resident(plugin);

  /* register */
  GEANY_PLUGIN_REGISTER(plugin, 224);
}
