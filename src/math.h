/*
 * math.h - Math/physics/chemistry formula extraction for the
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

#ifndef MARKDOWN_MATH_H
#define MARKDOWN_MATH_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
  gboolean display;  /* TRUE for display/block math, FALSE for inline */
  gchar   *tex;      /* raw TeX source without delimiters */
  gchar   *token;    /* full placeholder token, incl. random nonce */
} MarkdownMathSegment;

/*
 * Extract math segments from Markdown source text before it is parsed by
 * the Markdown library.  Supported delimiters:
 *   display: $$...$$   \[...\]
 *   inline:  \(...\)   $...$   (same line, no leading/trailing space)
 * Fenced and indented code blocks, inline code spans and \$ escapes are
 * respected; display delimiters never match across a code fence.
 * Bare $...$ additionally requires the content to look like a formula
 * (a letter, TeX control character or equals sign) so currency amounts
 * like $5-$10 are left alone.
 *
 * Returns a newly allocated string with each math segment replaced by an
 * opaque placeholder which survives Markdown parsing.  *segments receives
 * a newly allocated GPtrArray of MarkdownMathSegment (free with
 * math_segments_free()); it is empty when no math was found.
 */
gchar   *math_extract(const gchar *text, GPtrArray **segments);

void     math_segments_free(GPtrArray *segments);

/*
 * Replace the placeholders left by math_extract() in the HTML produced by
 * the Markdown library with the HTML-escaped TeX wrapped in delimiters
 * understood by both rendering engines.
 */
gchar   *math_restore(const gchar *html, GPtrArray *segments);

/*
 * Build the HTML block (<link>/<script> tags) loading the offline assets
 * for the given engine ("mathjax" or "katex") from math_dir (absolute
 * filesystem path).  Returns an empty string when assets are unavailable.
 */
gchar   *math_build_assets(const gchar *engine, const gchar *math_dir);

/*
 * Return a copy of html with assets inserted before </head> (or </body> as
 * a fallback) when the template does not contain @@math_assets@@.
 */
gchar   *math_inject_assets(const gchar *html, const gchar *assets);

G_END_DECLS

#endif /* MARKDOWN_MATH_H */
