/*
 * viewer.c - Part of the Geany Markdown plugin
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
#include <string.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <geanyplugin.h>
#ifndef FULL_PRICE
# include <mkdio.h>
#else
# include "markdown_lib.h"
#endif
#include "viewer.h"
#include "conf.h"
#include "math.h"

#define MD_ENC_MAX 256

/* Installed location of the offline MathJax/KaTeX assets; the string
 * concatenation happens at compile time (PLUGINDATADIR is a string
 * literal provided by the build system). */
#ifndef MARKDOWN_MATH_DIR
# define MARKDOWN_MATH_DIR PLUGINDATADIR "/math"
#endif

enum
{
  PROP_0,
  PROP_CONFIG,
  PROP_TEXT,
  PROP_ENCODING,
  N_PROPERTIES
};

struct _MarkdownViewerPrivate
{
  MarkdownConfig *conf;
  gulong load_handle;
  guint update_handle;
  gulong prop_handle;
  GString *text;
  gchar enc[MD_ENC_MAX];
  gdouble vscroll_pos;
  gdouble hscroll_pos;
  gdouble zoom_level;
};

static void markdown_viewer_finalize (GObject *object);

static GParamSpec *viewer_props[N_PROPERTIES] = { NULL };

G_DEFINE_TYPE (MarkdownViewer, markdown_viewer, WEBKIT_TYPE_WEB_VIEW)

static GString *
update_internal_text(MarkdownViewer *self, const gchar *val)
{
  if (!self->priv->text) {
    self->priv->text = g_string_new(val);
  } else {
    gsize len = strlen(val);
    g_string_overwrite_len(self->priv->text, 0, val, len);
    g_string_truncate(self->priv->text, len);
  }
  /* TODO: queue re-draw */
  return self->priv->text;
}

static void
markdown_viewer_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  MarkdownViewer *self = MARKDOWN_VIEWER(obj);

  switch (prop_id) {
    case PROP_CONFIG:
      if (self->priv->conf) {
        g_object_unref(self->priv->conf);
      }
      self->priv->conf = MARKDOWN_CONFIG(g_value_get_object(value));
      break;
    case PROP_TEXT:
      update_internal_text(self, g_value_get_string(value));
      break;
    case PROP_ENCODING:
      strncpy(self->priv->enc, g_value_get_string(value), MD_ENC_MAX-1);
      self->priv->enc[MD_ENC_MAX-1] = '\0';  /* add 0 if MD_ENC_MAX exceeded */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void
markdown_viewer_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
  MarkdownViewer *self = MARKDOWN_VIEWER(obj);

  switch (prop_id) {
    case PROP_CONFIG:
      g_value_set_object(value, self->priv->conf);
      break;
    case PROP_TEXT:
      g_value_set_string(value, self->priv->text->str);
      break;
    case PROP_ENCODING:
      g_value_set_string(value, self->priv->enc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void
markdown_viewer_class_init(MarkdownViewerClass *klass)
{
  GObjectClass *g_object_class;
  guint i;

  g_object_class = G_OBJECT_CLASS(klass);
  g_object_class->set_property = markdown_viewer_set_property;
  g_object_class->get_property = markdown_viewer_get_property;
  g_object_class->finalize = markdown_viewer_finalize;
  g_type_class_add_private((gpointer)klass, sizeof(MarkdownViewerPrivate));

  viewer_props[PROP_CONFIG] = g_param_spec_object("config", "Config",
    "MarkdownConfig object", MARKDOWN_TYPE_CONFIG,
    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  viewer_props[PROP_TEXT] = g_param_spec_string("text", "MarkdownText",
    "The Markdown text to render", "", G_PARAM_READWRITE);
  viewer_props[PROP_ENCODING] = g_param_spec_string("encoding", "TextEncoding",
    "The encoding of the Markdown text", "UTF-8", G_PARAM_READWRITE);

  for (i = 1 /* skip PROP_0 */; i < N_PROPERTIES; i++) {
    g_object_class_install_property(g_object_class, i, viewer_props[i]);
  }
}

static void
markdown_viewer_finalize(GObject *object)
{
  MarkdownViewer *self;
  g_return_if_fail(MARKDOWN_IS_VIEWER(object));
  self = MARKDOWN_VIEWER(object);
  if (self->priv->conf) {
    g_signal_handler_disconnect(self->priv->conf, self->priv->prop_handle);
    g_object_unref(self->priv->conf);
  }
  if (self->priv->text) {
    g_string_free(self->priv->text, TRUE);
  }
  G_OBJECT_CLASS(markdown_viewer_parent_class)->finalize(object);
}

static void
markdown_viewer_init(MarkdownViewer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MARKDOWN_TYPE_VIEWER, MarkdownViewerPrivate);
  self->priv->zoom_level = 1.0;
}


GtkWidget *
markdown_viewer_new(MarkdownConfig *conf)
{
  MarkdownViewer *self;
  WebKitSettings *settings;

  self = g_object_new(MARKDOWN_TYPE_VIEWER, "config", conf, NULL);

  /* The math/physics/chemistry formula engines are JavaScript based;
   * make sure scripting is explicitly enabled. */
  settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(self));
  webkit_settings_set_enable_javascript(settings, TRUE);

  /* Cause the view to be updated whenever the config changes. */
  self->priv->prop_handle = g_signal_connect_swapped(self->priv->conf, "notify",
      G_CALLBACK(markdown_viewer_queue_update), self);

  return GTK_WIDGET(self);
}

static void
replace_all(MarkdownViewer *self,
            GString *haystack,
            const gchar *needle,
            const gchar *replacement)
{
  gchar *ptr;
  gsize needle_len = strlen(needle);
  gsize replacement_len = strlen(replacement);
  goffset offset = 0;

  /* For each occurrence of needle in haystack */
  while ((ptr = strstr(haystack->str + offset, needle)) != NULL) {
    offset = ptr - haystack->str;
    g_string_erase(haystack, offset, needle_len);
    g_string_insert(haystack, offset, replacement);
    offset += replacement_len;
  }
}

static gchar *
quote_font_family(const gchar *name)
{
  if (name == NULL || name[0] == '\0')
    return g_strdup("serif");
  if (name[0] == '"' || name[0] == '\'')
    return g_strdup(name);
  return g_strdup_printf("\"%s\"", name);
}

static gchar *
template_replace(MarkdownViewer *self, const gchar *html_text)
{
  MarkdownConfigViewPos view_pos;
  guint font_point_size = 0, code_font_point_size = 0;
  gchar *font_name = NULL, *code_font_name = NULL;
  gchar *bg_color = NULL, *fg_color = NULL;
  gboolean math_enabled = FALSE;
  gboolean scheme_colors = FALSE;
  gchar *math_engine = NULL;
  gchar *math_assets = NULL;
  gchar *scheme_css = NULL;
  gchar *font_css = NULL, *code_font_css = NULL;
  gchar font_pt_size[10] = { 0 };
  gchar code_font_pt_size[10] = { 0 };
  GString *tmpl;
  static const gchar table_css[] =
    "<style id=\"md-table-css\">"
    "table{border-collapse:collapse;margin:1em 0;}"
    "th,td{border:1px solid #888;padding:0.3em 0.7em;}"
    "th{background:rgba(127,127,127,0.15);}"
    "tr:nth-child(even){background:rgba(127,127,127,0.06);}"
    "</style>";

  { /* Read all the configuration settings into strings */
    g_object_get(self->priv->conf,
                 "view-pos", &view_pos,
                 "font-name", &font_name,
                 "code-font-name", &code_font_name,
                 "font-point-size", &font_point_size,
                 "code-font-point-size", &code_font_point_size,
                 "bg-color", &bg_color,
                 "fg-color", &fg_color,
                 "math-enabled", &math_enabled,
                 "math-engine", &math_engine,
                 "scheme-colors", &scheme_colors,
                 NULL);
    g_snprintf(font_pt_size, 10, "%d", font_point_size);
    g_snprintf(code_font_pt_size, 10, "%d", code_font_point_size);
  }

  /* Geany color scheme takes precedence over the static BG/FG prefs */
  if (scheme_colors) {
    scheme_css = markdown_build_scheme_css();
    if (scheme_css) {
      g_free(bg_color);
      g_free(fg_color);
      bg_color = NULL;
      fg_color = NULL;
    }
  }

  /* Load the template into a GString to be modified in place */
  tmpl = g_string_new(markdown_config_get_template_text(self->priv->conf));

  font_css = quote_font_family(font_name);
  code_font_css = quote_font_family(code_font_name);
  replace_all(self, tmpl, "@@font_name@@", font_css);
  replace_all(self, tmpl, "@@code_font_name@@", code_font_css);
  replace_all(self, tmpl, "@@font_point_size@@", font_pt_size);
  replace_all(self, tmpl, "@@code_font_point_size@@", code_font_pt_size);
  replace_all(self, tmpl, "@@bg_color@@", bg_color ? bg_color : "");
  replace_all(self, tmpl, "@@fg_color@@", fg_color ? fg_color : "");
  g_free(font_css);
  g_free(code_font_css);

  /* Mobile / tablet / WeChat browser adaptation for old templates */
  if (strstr(tmpl->str, "name=\"viewport\"") == NULL ||
      strstr(tmpl->str, "charset") == NULL ||
      strstr(tmpl->str, "md-responsive-css") == NULL)
  {
    GString *inj = g_string_new(NULL);
    if (strstr(tmpl->str, "charset") == NULL)
      g_string_append(inj, "<meta charset=\"UTF-8\">");
    if (strstr(tmpl->str, "name=\"viewport\"") == NULL)
      g_string_append(inj, "<meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1.0\">");
    if (strstr(tmpl->str, "md-responsive-css") == NULL)
      g_string_append(inj,
        "<style id=\"md-responsive-css\">"
        "img,video,canvas,svg{max-width:100%;height:auto;}"
        "pre{overflow-x:auto;}"
        "body{-webkit-text-size-adjust:100%;word-wrap:break-word;}"
        "@media(max-width:800px){"
        "table{display:block;overflow-x:auto;max-width:100%;}"
        "body{padding:0.75em;}"
        "}"
        "</style>");
    if (inj->len > 0) {
      gchar *out = math_inject_assets(tmpl->str, inj->str);
      g_string_assign(tmpl, out);
      g_free(out);
    }
    g_string_free(inj, TRUE);
  }

  /* Base table styles (old templates may lack them) */
  if (!strstr(tmpl->str, "md-table-css") && !strstr(tmpl->str, "border-collapse")) {
    gchar *injected = math_inject_assets(tmpl->str, table_css);
    g_string_assign(tmpl, injected);
    g_free(injected);
  }

  /* Current Geany color scheme (overrides body/table colors from template) */
  if (scheme_css) {
    gchar *injected;
    if (strstr(tmpl->str, "md-scheme-css")) {
      /* Re-render: replace previous scheme block if template hardcodes it */
      gchar *start = strstr(tmpl->str, "<style id=\"md-scheme-css\">");
      gchar *end = start ? strstr(start, "</style>") : NULL;
      if (start && end) {
        gsize off = start - tmpl->str;
        gsize len = (end + strlen("</style>")) - start;
        g_string_erase(tmpl, off, len);
      }
    }
    injected = math_inject_assets(tmpl->str, scheme_css);
    g_string_assign(tmpl, injected);
    g_free(injected);
    g_free(scheme_css);
    scheme_css = NULL;
  }

  /* Math/physics/chemistry rendering assets */
  if (math_enabled) {
    math_assets = math_build_assets(math_engine, MARKDOWN_MATH_DIR);
  } else {
    math_assets = g_strdup("");
  }
  if (strstr(tmpl->str, "@@math_assets@@")) {
    replace_all(self, tmpl, "@@math_assets@@", math_assets);
  } else if (math_assets[0]) {
    /* Templates created by older plugin versions have no placeholder;
     * inject the assets right before </head> (or </body>) instead. */
    gchar *injected = math_inject_assets(tmpl->str, math_assets);
    g_string_assign(tmpl, injected);
    g_free(injected);
  }
  g_free(math_assets);
  g_free(math_engine);

  replace_all(self, tmpl, "@@markdown@@", html_text);

  g_free(font_name);
  g_free(code_font_name);
  g_free(bg_color);
  g_free(fg_color);
  g_free(scheme_css);

  return g_string_free(tmpl, FALSE);
}

static gboolean
push_scroll_pos(MarkdownViewer *self)
{
  GtkWidget *parent;
  gboolean pushed = FALSE;

  parent = gtk_widget_get_parent(GTK_WIDGET(self));
  if (GTK_IS_SCROLLED_WINDOW(parent)) {
    GtkAdjustment *adj;
    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(parent));
    /* Another hack to try and keep scroll position from
     * resetting to top while typing, just don't store the new
     * scroll positions if they're 0. */
    if (gtk_adjustment_get_value(adj) != 0)
        self->priv->vscroll_pos = gtk_adjustment_get_value(adj);
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(parent));
    if (gtk_adjustment_get_value(adj) != 0)
        self->priv->hscroll_pos = gtk_adjustment_get_value(adj);
    pushed = TRUE;
  }

  return pushed;
}

static gboolean
pop_scroll_pos(MarkdownViewer *self)
{
  GtkWidget *parent;
  gboolean popped = FALSE;

  /* first process any pending events, like drawing of the webview */
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  parent = gtk_widget_get_parent(GTK_WIDGET(self));
  if (GTK_IS_SCROLLED_WINDOW(parent)) {
    GtkAdjustment *adj;
    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(parent));
    gtk_adjustment_set_value(adj, self->priv->vscroll_pos);
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(parent));
    gtk_adjustment_set_value(adj, self->priv->hscroll_pos);
    /* process any new events, like making sure the new scroll position
     * takes effect. */
    while (gtk_events_pending()) {
      gtk_main_iteration();
    }
    popped = TRUE;
  }

  return popped;
}

static void
on_webview_load_changed(MarkdownViewer  *self,
                        WebKitLoadEvent  load_event,
                        WebKitWebView   *web_view)
{
  /* When the webkit is done loading, reset the scroll position. */
  if (load_event == WEBKIT_LOAD_FINISHED) {
    pop_scroll_pos(self);
    /* load_html() may reset the zoom level; re-apply the last one */
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(self), self->priv->zoom_level);
  }
}

void
markdown_viewer_set_zoom(MarkdownViewer *self, gdouble level)
{
  g_return_if_fail(MARKDOWN_IS_VIEWER(self));
  if (level < 0.25)
    level = 0.25;
  else if (level > 5.0)
    level = 5.0;
  self->priv->zoom_level = level;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(self), level);
}

gdouble
markdown_viewer_get_zoom(MarkdownViewer *self)
{
  g_return_val_if_fail(MARKDOWN_IS_VIEWER(self), 1.0);
  return self->priv->zoom_level;
}

gchar *
markdown_viewer_get_html(MarkdownViewer *self)
{
  gchar *md_as_html = NULL, *html = NULL;
  gchar *extracted = NULL;
  GPtrArray *math_segments = NULL;
  const gchar *src;
  gboolean math_enabled = FALSE;

  /* Ensure the internal buffer is created */
  if (!self->priv->text) {
    update_internal_text(self, "");
  }

  /* Pull math/physics/chemistry segments out of the source before the
   * Markdown parser can mangle them (underscores, asterisks, entities). */
  g_object_get(self->priv->conf, "math-enabled", &math_enabled, NULL);
  if (math_enabled) {
    extracted = math_extract(self->priv->text->str, &math_segments);
    src = extracted;
  } else {
    src = self->priv->text->str;
  }

  {
#ifndef FULL_PRICE  /* this version using Discount markdown library
                     * is faster but may invoke endless discussions
                     * about the GPL and licenses similar to (but the
                     * same as) the old BSD 4-clause license being
                     * incompatible */
    MMIOT *doc;
    doc = mkd_string((char *) src, strlen(src), 0);
    mkd_compile(doc, 0);
    if (mkd_document(doc, &md_as_html) != EOF) {
      html = template_replace(self, md_as_html);
    }
    mkd_cleanup(doc);
#else /* this version is slower but is unquestionably GPL-friendly
       * and the lib also has much more readable/maintainable code */

    md_as_html = markdown_to_string((char *) src, 0, HTML_FORMAT);
    if (md_as_html) {
      html = template_replace(self, md_as_html);
      g_free(md_as_html); /* TODO: become 100% convinced this wasn't
                           * malloc()'d outside of GLIB functions with
                           * libc allocator (probably same anyway). */
    }
#endif
  }

  /* Replace the opaque placeholders with the actual formula HTML.
   * Placeholder tokens are globally unique, so restoring over the whole
   * document (fragment already substituted into the template) is safe. */
  if (html && math_segments && math_segments->len > 0) {
    gchar *restored = math_restore(html, math_segments);
    g_free(html);
    html = restored;
  }

  math_segments_free(math_segments);
  g_free(extracted);

  return html;
}

static gboolean
markdown_viewer_update_view(MarkdownViewer *self)
{
  gchar *html = markdown_viewer_get_html(self);

  push_scroll_pos(self);

  if (html) {
    gchar *base_path;
    gchar *base_uri; /* A file URI not a path URI; last component is stripped */
    GError *error = NULL;
    GeanyDocument *doc = document_get_current();

    /* If the current document has a known path (ie. is saved), use that,
     * substituting the file's basename for `index.html`. */
    if (DOC_VALID(doc) && doc->real_path != NULL) {
      gchar *base_dir = g_path_get_dirname(doc->real_path);
      base_path = g_build_filename(base_dir, "index.html", NULL);
      g_free(base_dir);
    }
    /* Otherwise assume use a file `index.html` in the current working directory. */
    else {
      gchar *cwd = g_get_current_dir();
      base_path = g_build_filename(cwd, "index.html", NULL);
      g_free(cwd);
    }

    base_uri = g_filename_to_uri(base_path, NULL, &error);
    if (base_uri == NULL) {
      g_warning("failed to encode path '%s' as URI: %s", base_path, error->message);
      g_error_free(error);
      base_uri = g_strdup("file://./index.html");
      g_debug("using phony base URI '%s', broken relative paths are likely", base_uri);
    }
    g_free(base_path);

    /* Connect a signal handler (only needed once) to restore the scroll
     * position once the webview is reloaded. */
    if (self->priv->load_handle == 0) {
      self->priv->load_handle =
        g_signal_connect_swapped(WEBKIT_WEB_VIEW(self), "load-changed",
          G_CALLBACK(on_webview_load_changed), self);
    }

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(self), html, base_uri);

    g_free(base_uri);
    g_free(html);
  }

  if (self->priv->update_handle != 0) {
    g_source_remove(self->priv->update_handle);
  }
  self->priv->update_handle = 0;

  return FALSE; /* When used as an idle handler, says to remove the source */
}

void
markdown_viewer_queue_update(MarkdownViewer *self)
{
  g_return_if_fail(MARKDOWN_IS_VIEWER(self));
  if (self->priv->update_handle == 0) {
    self->priv->update_handle = g_idle_add(
      (GSourceFunc) markdown_viewer_update_view, self);
  }
}

void
markdown_viewer_set_markdown(MarkdownViewer *self, const gchar *text, const gchar *encoding)
{
  g_return_if_fail(MARKDOWN_IS_VIEWER(self));
  g_object_set(self, "text", text, "encoding", encoding, NULL);
  markdown_viewer_queue_update(self);
}
