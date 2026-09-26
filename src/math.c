/*
 * math.c - Math/physics/chemistry formula extraction for the
 *          Geany Markdown plugin
 *
 * Copyright 2026 Geany Markdown plugin contributors
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

#include <string.h>
#include <glib.h>
#include "math.h"

/* Opaque placeholder tokens embedded in the Markdown source; alphanumeric
 * only so that no Markdown transformation can alter them.  The %08x tail
 * is a random per-extraction nonce: it makes tokens unpredictable so a
 * literal token string in the user's document can never collide with a
 * real placeholder during math_restore(). */
#define MATH_TOKEN_FMT_DISPLAY "mdmathD%u%08x"
#define MATH_TOKEN_FMT_INLINE  "mdmathI%u%08x"

static void
math_segment_free(gpointer data)
{
  MarkdownMathSegment *seg = data;
  if (seg) {
    g_free(seg->tex);
    g_free(seg->token);
    g_free(seg);
  }
}

void
math_segments_free(GPtrArray *segments)
{
  if (segments) {
    g_ptr_array_free(segments, TRUE);
  }
}

static void
add_segment(GString *out, GPtrArray *segments, gboolean display, guint nonce,
            const gchar *tex, gsize tex_len)
{
  MarkdownMathSegment *seg;
  guint idx = segments->len;

  seg = g_new(MarkdownMathSegment, 1);
  seg->display = display;
  seg->tex = g_strndup(tex, tex_len);
  seg->token = g_strdup_printf(display ? MATH_TOKEN_FMT_DISPLAY
                                       : MATH_TOKEN_FMT_INLINE, idx, nonce);
  g_ptr_array_add(segments, seg);

  if (display) {
    /* Isolate the placeholder in its own paragraph.  Note: this splits
     * the enclosing structure when the math sits inside a list item or
     * table cell; inherent to the pre-parse placeholder approach. */
    g_string_append_printf(out, "\n\n" MATH_TOKEN_FMT_DISPLAY "\n\n", idx, nonce);
  } else {
    /* Wrap in a code span so the parser cannot touch the token. */
    g_string_append_printf(out, "`" MATH_TOKEN_FMT_INLINE "`", idx, nonce);
  }
}

/* TRUE when the line starting at pos is a code fence marker line: up to
 * 3 leading spaces followed by a run of 3 or more ` or ~ characters. */
static gboolean
is_fence_line(const gchar *text, gsize len, gsize pos)
{
  gsize j = pos, spaces = 0, run = 0;

  while (j < len && spaces < 3 && text[j] == ' ') {
    j++;
    spaces++;
  }
  if (j >= len || (text[j] != '`' && text[j] != '~')) {
    return FALSE;
  }
  while (j + run < len && text[j + run] == text[j]) {
    run++;
  }
  return run >= 3;
}

/* Scan forward from start for a two-character closing delimiter, giving
 * up as soon as a code fence line is crossed: a math region must never
 * swallow a fence, or the rest of the document gets mangled.  Returns
 * the offset of the delimiter's first character or -1. */
static gssize
find_display_close(const gchar *text, gsize len, gsize start, gchar c1, gchar c2)
{
  gsize j;

  for (j = start; j + 1 < len; j++) {
    if ((j == 0 || text[j - 1] == '\n') && is_fence_line(text, len, j)) {
      return -1;
    }
    if (text[j] == c1 && text[j + 1] == c2) {
      return (gssize) j;
    }
  }
  return -1;
}

/* TRUE when the line ending right before pos (pos must be at a line
 * start) contains only whitespace.  The start of the text counts as
 * blank so a document beginning with an indented chunk is code. */
static gboolean
prev_line_blank(const gchar *text, gsize pos)
{
  gsize j = pos;

  /* Skip the newline that terminates the previous line, then check
   * that its content is all whitespace (or empty). */
  if (j > 0 && text[j - 1] == '\n') {
    j--;
  }
  while (j > 0) {
    gchar c = text[j - 1];
    if (c == '\n') {
      return TRUE;
    }
    if (c != ' ' && c != '\t' && c != '\r') {
      return FALSE;
    }
    j--;
  }
  return TRUE;
}

/* Number of leading indentation columns of the line at pos, counting a
 * tab as reaching the 4-column threshold.  An indented code block needs
 * 4 or more. */
static gsize
line_indent(const gchar *text, gsize line_end, gsize pos)
{
  gsize j = pos, cols = 0;

  while (j < line_end && cols < 4) {
    if (text[j] == ' ') {
      cols++;
    } else if (text[j] == '\t') {
      cols = 4;
    } else {
      break;
    }
    j++;
  }
  return cols;
}

/* TRUE when text[from..to) contains only whitespace. */
static gboolean
line_is_blank(const gchar *text, gsize from, gsize to)
{
  gsize j;

  for (j = from; j < to; j++) {
    if (text[j] != ' ' && text[j] != '\t' && text[j] != '\r') {
      return FALSE;
    }
  }
  return TRUE;
}

/* Heuristic that keeps currency amounts like "$5-$10" out of math: real
 * formula content almost always contains a letter, a TeX control
 * character or an equals sign, while prices and ranges are digits and
 * punctuation only. */
static gboolean
tex_looks_like_math(const gchar *s, gsize len)
{
  gsize i;

  for (i = 0; i < len; i++) {
    gchar c = s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\\' ||
        c == '^' || c == '_' || c == '{' || c == '}' || c == '=') {
      return TRUE;
    }
  }
  return FALSE;
}

gchar *
math_extract(const gchar *text, GPtrArray **segments)
{
  GString *out;
  GPtrArray *segs;
  gsize i, len;
  gboolean in_fence = FALSE;
  gboolean in_indented_code = FALSE;
  gchar fence_mark = 0;
  gsize fence_count = 0;
  guint nonce = g_random_int();

  g_return_val_if_fail(text != NULL, NULL);
  g_return_val_if_fail(segments != NULL, NULL);

  out = g_string_new(NULL);
  segs = g_ptr_array_new_with_free_func(math_segment_free);
  len = strlen(text);
  i = 0;

  while (i < len) {
    /* Fence and indented-code handling: only checked at line starts. */
    if (i == 0 || text[i - 1] == '\n') {
      if (!in_fence) {
        gsize e = i;
        while (e < len && text[e] != '\n') {
          e++;
        }
        if (in_indented_code) {
          /* The block continues while lines stay blank or indented. */
          if (line_is_blank(text, i, e) || line_indent(text, e, i) >= 4) {
            if (e < len) {
              e++; /* include the newline */
            }
            g_string_append_len(out, text + i, e - i);
            i = e;
            continue;
          }
          in_indented_code = FALSE; /* non-indented line ends the block */
        } else if (line_indent(text, e, i) >= 4 && prev_line_blank(text, i)) {
          /* Indented code block.  The blank-line requirement follows
           * CommonMark (indented code cannot interrupt a paragraph) and
           * keeps 4-space-indented list continuations treated as
           * markdown rather than code. */
          in_indented_code = TRUE;
          if (e < len) {
            e++; /* include the newline */
          }
          g_string_append_len(out, text + i, e - i);
          i = e;
          continue;
        }
      }
      gsize j = i, spaces = 0;
      while (j < len && spaces < 3 && text[j] == ' ') {
        j++;
        spaces++;
      }
      if (j < len && (text[j] == '`' || text[j] == '~')) {
        gchar c = text[j];
        gsize k = j;
        while (k < len && text[k] == c) {
          k++;
        }
        gsize run = k - j;
        if (!in_fence && run >= 3) {
          in_fence = TRUE;
          fence_mark = c;
          fence_count = run;
        } else if (in_fence && c == fence_mark && run >= fence_count) {
          gsize e = k;
          while (e < len && (text[e] == ' ' || text[e] == '\t' ||
                             text[e] == '\r')) {
            e++;
          }
          if (e >= len || text[e] == '\n') {
            /* Closing fence line: copy verbatim, then leave the fence. */
            if (e < len) {
              e++; /* include the newline */
            }
            g_string_append_len(out, text + i, e - i);
            i = e;
            in_fence = FALSE;
            continue;
          }
        }
      }
    }

    if (in_fence) {
      g_string_append_c(out, text[i]);
      i++;
      continue;
    }

    /* Inline code span: copy verbatim up to the matching run. */
    if (text[i] == '`') {
      gsize n = 0, j;
      gboolean found = FALSE;
      while (i + n < len && text[i + n] == '`') {
        n++;
      }
      j = i + n;
      while (j < len) {
        if (text[j] == '`') {
          gsize m = 0;
          while (j + m < len && text[j + m] == '`') {
            m++;
          }
          if (m == n) {
            found = TRUE;
            break;
          }
          j += m;
        } else {
          j++;
        }
      }
      if (found) {
        g_string_append_len(out, text + i, (j + n) - i);
        i = j + n;
        continue;
      }
      /* No closing run: treat the backtick as a literal character. */
      g_string_append_c(out, text[i]);
      i++;
      continue;
    }

    /* Backslash escapes and math openers. */
    if (text[i] == '\\' && i + 1 < len) {
      gchar nx = text[i + 1];
      if (nx == '\\' || nx == '$' || nx == '`') {
        /* Escaped backslash / dollar / backtick: copy the pair. */
        g_string_append_c(out, '\\');
        g_string_append_c(out, nx);
        i += 2;
        continue;
      }
      if (nx == '(') {
        gssize end = find_display_close(text, len, i + 2, '\\', ')');
        if (end > (gssize) (i + 2)) {
          add_segment(out, segs, FALSE, nonce, text + i + 2, end - (i + 2));
          i = end + 2;
          continue;
        }
      }
      if (nx == '[') {
        gssize end = find_display_close(text, len, i + 2, '\\', ']');
        if (end > (gssize) (i + 2)) {
          add_segment(out, segs, TRUE, nonce, text + i + 2, end - (i + 2));
          i = end + 2;
          continue;
        }
      }
      /* Any other escape: keep the backslash, let the next iteration
       * handle the following character. */
      g_string_append_c(out, text[i]);
      i++;
      continue;
    }

    /* Dollar delimiters. */
    if (text[i] == '$') {
      if (i + 1 < len && text[i + 1] == '$') {
        /* Display math: $$...$$ (may span lines, never across fences). */
        gssize end = find_display_close(text, len, i + 2, '$', '$');
        if (end > (gssize) (i + 2)) {
          add_segment(out, segs, TRUE, nonce, text + i + 2, end - (i + 2));
          i = end + 2;
          continue;
        }
        /* No proper closing pair: fall through and emit literally. */
      } else {
        /* Inline math: $...$ on the same line.  The content must be
         * non-empty and must not start or end with whitespace (avoids
         * matching currency amounts like "$5 and $6"). */
        gsize j = i + 1;
        gboolean found = FALSE;
        while (j < len && text[j] != '\n') {
          if (text[j] == '\\' && j + 1 < len && text[j + 1] == '$') {
            j += 2;
            continue;
          }
          if (text[j] == '$') {
            found = TRUE;
            break;
          }
          j++;
        }
        if (found) {
          gsize cs = i + 1, ce = j;
          if (ce > cs &&
              text[cs] != ' ' && text[cs] != '\t' &&
              text[ce - 1] != ' ' && text[ce - 1] != '\t' &&
              tex_looks_like_math(text + cs, ce - cs)) {
            add_segment(out, segs, FALSE, nonce, text + cs, ce - cs);
            i = j + 1;
            continue;
          }
        }
      }
    }

    g_string_append_c(out, text[i]);
    i++;
  }

  *segments = segs;
  return g_string_free(out, FALSE);
}

static gchar *
math_escape_html(const gchar *s)
{
  GString *out = g_string_new(NULL);

  for (; *s; s++) {
    switch (*s) {
      case '&': g_string_append(out, "&amp;"); break;
      case '<':  g_string_append(out, "&lt;");  break;
      case '>':  g_string_append(out, "&gt;");  break;
      default:   g_string_append_c(out, *s);     break;
    }
  }
  return g_string_free(out, FALSE);
}

/* Replace the first occurrence of needle in haystack with replacement. */
static void
replace_first(GString *haystack, const gchar *needle, const gchar *replacement)
{
  gchar *ptr = strstr(haystack->str, needle);
  if (ptr) {
    gsize offset = ptr - haystack->str;
    g_string_erase(haystack, offset, strlen(needle));
    g_string_insert(haystack, offset, replacement);
  }
}

gchar *
math_restore(const gchar *html, GPtrArray *segments)
{
  GString *out;
  guint i;

  g_return_val_if_fail(html != NULL, NULL);
  g_return_val_if_fail(segments != NULL, NULL);

  out = g_string_new(html);

  for (i = 0; i < segments->len; i++) {
    MarkdownMathSegment *seg = g_ptr_array_index(segments, i);
    gchar *esc = math_escape_html(seg->tex);
    gchar *content;
    gchar *wrapped;

    if (seg->display) {
      content = g_strdup_printf("<div class=\"math-display\">\\[%s\\]</div>", esc);
      wrapped = g_strdup_printf("<p>%s</p>", seg->token);
    } else {
      content = g_strdup_printf("<span class=\"math-inline\">\\(%s\\)</span>", esc);
      wrapped = g_strdup_printf("<code>%s</code>", seg->token);
    }
    if (strstr(out->str, wrapped)) {
      replace_first(out, wrapped, content);
    } else {
      replace_first(out, seg->token, content);
    }

    g_free(wrapped);
    g_free(content);
    g_free(esc);
  }

  return g_string_free(out, FALSE);
}

static gchar *
path_to_uri(const gchar *path)
{
  GError *error = NULL;
  gchar *uri = NULL;

  if (path && g_path_is_absolute(path)) {
    uri = g_filename_to_uri(path, NULL, &error);
  }
  if (!uri) {
    g_debug("math assets path '%s' is not a usable URI: %s",
            path ? path : "(null)", error ? error->message : "not absolute");
  }
  g_clear_error(&error);
  return uri;
}

static gboolean
have_file(const gchar *dir, const gchar *leaf)
{
  gchar *path = g_build_filename(dir, leaf, NULL);
  gboolean ok = g_file_test(path, G_FILE_TEST_IS_REGULAR);

  g_free(path);
  return ok;
}

gchar *
math_build_assets(const gchar *engine, const gchar *math_dir)
{
  gchar *dir_uri, *base;
  GString *s;

  if (!engine) {
    engine = "mathjax";
  }
  dir_uri = path_to_uri(math_dir);
  if (!dir_uri) {
    return g_strdup("");
  }
  while (dir_uri[0] && dir_uri[strlen(dir_uri) - 1] == '/') {
    dir_uri[strlen(dir_uri) - 1] = '\0';
  }

  s = g_string_new(NULL);

  if (g_strcmp0(engine, "katex") == 0) {
    gchar *js_path;

    base = g_strconcat(dir_uri, "/katex", NULL);
    js_path = g_filename_from_uri(base, NULL, NULL);
    if (js_path) {
      /* All three scripts are required for rendering to work; a partial
       * installation would 404 in the preview. */
      gboolean have = have_file(js_path, "katex.min.js") &&
                      have_file(js_path, "contrib/mhchem.min.js") &&
                      have_file(js_path, "contrib/auto-render.min.js");
      g_free(js_path);
      if (!have) {
        g_debug("KaTeX assets not found under '%s'", base);
        g_free(base);
        g_string_free(s, TRUE);
        g_free(dir_uri);
        return g_strdup("");
      }
    }

    /* Load order matters: katex -> mhchem -> auto-render; render on
     * DOMContentLoaded when all deferred scripts have executed.
     * Only \\(..\\) and \\[..\\] delimiters are enabled: math_extract()
     * already normalized every source delimiter to these, which avoids
     * false positives on currency symbols.  Common physics macros are
     * provided as a KaTeX shim (KaTeX has no physics package). */
    g_string_append_printf(s,
      "<link rel=\"stylesheet\" href=\"%s/katex.min.css\">\n"
      "<script defer src=\"%s/katex.min.js\"></script>\n"
      "<script defer src=\"%s/contrib/mhchem.min.js\"></script>\n"
      "<script defer src=\"%s/contrib/auto-render.min.js\"></script>\n"
      "<script>\n"
      "document.addEventListener(\"DOMContentLoaded\", function () {\n"
      "  if (!window.renderMathInElement) return;\n"
      "  renderMathInElement(document.body, {\n"
      "    delimiters: [\n"
      "      {left: \"\\\\(\", right: \"\\\\)\", display: false},\n"
      "      {left: \"\\\\[\", right: \"\\\\]\", display: true}\n"
      "    ],\n"
      "    throwOnError: false,\n"
      "    macros: {\n"
      "      \"\\\\dv\": \"\\\\frac{d#1}{d#2}\",\n"
      "      \"\\\\pdv\": \"\\\\frac{\\\\partial#1}{\\\\partial#2}\",\n"
      "      \"\\\\pd\": \"\\\\frac{\\\\partial#1}{\\\\partial#2}\",\n"
      "      \"\\\\qty\": \"#1\\\\,\\\\mathrm{#2}\",\n"
      "      \"\\\\unit\": \"\\\\mathrm{#1}\",\n"
      "      \"\\\\va\": \"\\\\vec{#1}\",\n"
      "      \"\\\\vb\": \"\\\\mathbf{#1}\",\n"
      "      \"\\\\vu\": \"\\\\hat{#1}\",\n"
      "      \"\\\\abs\": \"\\\\left|#1\\\\right|\",\n"
      "      \"\\\\avg\": \"\\\\langle#1\\\\rangle\",\n"
      "      \"\\\\comm\": \"\\\\left[#1,#2\\\\right]\"\n"
      "    }\n"
      "  });\n"
      "});\n"
      "</script>\n",
      base, base, base, base);
    g_free(base);
  } else {
    /* MathJax (default): tex-chtml-full.js bundles every TeX extension
     * including physics and mhchem; fonts are loaded from fontURL. */
    gchar *js_path;

    base = g_strconcat(dir_uri, "/mathjax", NULL);
    js_path = g_filename_from_uri(base, NULL, NULL);
    if (js_path) {
      gchar *fonts_dir = g_build_filename(js_path, "fonts", "woff-v2", NULL);
      gboolean have = have_file(js_path, "tex-chtml-full.js") &&
                      g_file_test(fonts_dir, G_FILE_TEST_IS_DIR);
      g_free(fonts_dir);
      g_free(js_path);
      if (!have) {
        g_debug("MathJax assets not found under '%s'", base);
        g_free(base);
        g_string_free(s, TRUE);
        g_free(dir_uri);
        return g_strdup("");
      }
    }

    g_string_append_printf(s,
      "<script>\n"
      "window.MathJax = {\n"
      "  tex: {\n"
      "    inlineMath: [['\\\\(', '\\\\)']],\n"
      "    displayMath: [['\\\\[', '\\\\]']],\n"
      "    packages: {'[+tex]': ['mhchem', 'physics', 'textmacros']}\n"
      "  },\n"
      "  chtml: { fontURL: '%s/fonts/woff-v2' },\n"
      "  startup: { typeset: true }\n"
      "};\n"
      "</script>\n"
      "<script defer src=\"%s/tex-chtml-full.js\"></script>\n",
      base, base);
    g_free(base);
  }

  g_free(dir_uri);
  return g_string_free(s, FALSE);
}

static const gchar *
find_ci(const gchar *haystack, const gchar *needle)
{
  gsize nlen = strlen(needle);
  const gchar *p = haystack;

  while (*p) {
    if (g_ascii_strncasecmp(p, needle, nlen) == 0) {
      return p;
    }
    p++;
  }
  return NULL;
}

gchar *
math_inject_assets(const gchar *html, const gchar *assets)
{
  const gchar *pos;

  g_return_val_if_fail(html != NULL, NULL);

  if (!assets || !assets[0]) {
    return g_strdup(html);
  }

  pos = find_ci(html, "</head>");
  if (!pos) {
    pos = find_ci(html, "</body>");
  }
  if (pos) {
    gsize offset = pos - html;
    return g_strdup_printf("%.*s%s%s", (int) offset, html, assets,
                           html + offset);
  }
  /* No head/body: append so the deferred scripts still run. */
  return g_strconcat(html, assets, NULL);
}
