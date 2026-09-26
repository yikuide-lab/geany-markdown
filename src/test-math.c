/*
 * test-math.c - Unit tests for math.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <glib.h>
#include "math.h"

#ifndef TEST_MATH_DIR
# define TEST_MATH_DIR "."
#endif

static int failures = 0;

#define CHECK(cond, msg) \
  do { \
    if (cond) { \
      g_print("ok   - %s\n", msg); \
    } else { \
      g_printerr("FAIL - %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
      failures++; \
    } \
  } while (0)

static void
test_extract_inline_dollar(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("Euler: $e^{i\\pi} + 1 = 0$ !", &segs);

  CHECK(segs && segs->len == 1, "single $...$ extracted");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(!s->display, "$...$ is inline");
    CHECK(g_strcmp0(s->tex, "e^{i\\pi} + 1 = 0") == 0, "$...$ content");
  }
  CHECK(strstr(out, "mdmathI0") != NULL, "inline placeholder present");
  CHECK(strstr(out, "$e^") == NULL, "original delimiters removed");
  g_free(out);
  math_segments_free(segs);
}

static void
test_extract_display_dollars(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("before\n\n$$\n\\int_0^1 x\\,dx = \\frac12\n$$\n\nafter", &segs);

  CHECK(segs && segs->len == 1, "single $$...$$ extracted");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(s->display, "$$...$$ is display");
    CHECK(strstr(s->tex, "\\int_0^1") != NULL, "$$...$$ content");
  }
  CHECK(strstr(out, "mdmathD0") != NULL, "display placeholder present");
  g_free(out);
  math_segments_free(segs);
}

static void
test_extract_paren_delims(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("\\(a + b\\) and \\[c + d\\]", &segs);

  CHECK(segs && segs->len == 2, "\\(..\\) and \\[..\\] extracted");
  if (segs && segs->len == 2) {
    MarkdownMathSegment *s0 = g_ptr_array_index(segs, 0);
    MarkdownMathSegment *s1 = g_ptr_array_index(segs, 1);
    CHECK(!s0->display && g_strcmp0(s0->tex, "a + b") == 0, "\\(..\\) inline content");
    CHECK(s1->display && g_strcmp0(s1->tex, "c + d") == 0, "\\[..\\] display content");
  }
  CHECK(strstr(out, "mdmathI0") != NULL && strstr(out, "mdmathD1") != NULL,
        "both placeholders present");
  g_free(out);
  math_segments_free(segs);
}

static void
test_skip_fenced_code(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "text\n\n```\n$x$ and $$y$$\n```\n\nmore $z$";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 1, "math inside fence skipped, outside kept");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(g_strcmp0(s->tex, "z") == 0, "only outside math extracted");
  }
  CHECK(strstr(out, "$x$") != NULL, "fenced content untouched");
  g_free(out);
  math_segments_free(segs);
}

static void
test_skip_inline_code(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("use `$x$` but keep $y$", &segs);

  CHECK(segs && segs->len == 1, "math inside inline code skipped");
  CHECK(strstr(out, "`$x$`") != NULL, "inline code untouched");
  g_free(out);
  math_segments_free(segs);
}

static void
test_escaped_dollar(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("costs \\$5 and \\$6 total", &segs);

  CHECK(segs && segs->len == 0, "\\$ not treated as math");
  CHECK(strstr(out, "\\$5") != NULL, "escaped dollars preserved");
  g_free(out);
  math_segments_free(segs);
}

static void
test_currency_not_math(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("It costs $5 and $6 dollars.", &segs);

  CHECK(segs && segs->len == 0, "currency amounts not extracted");
  CHECK(strstr(out, "$5") != NULL && strstr(out, "$6") != NULL,
        "currency dollars preserved");
  g_free(out);
  math_segments_free(segs);
}

static void
test_escaped_backslash_paren(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("not math: \\\\(fake) here", &segs);

  CHECK(segs && segs->len == 0, "\\\\( not treated as math");
  CHECK(strstr(out, "\\\\(fake)") != NULL, "literal backslash-paren kept");
  g_free(out);
  math_segments_free(segs);
}

static void
test_chemistry_inside_math(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("\\(\\ce{2H2 + O2 -> 2H2O}\\)", &segs);

  CHECK(segs && segs->len == 1, "\\ce{} inside math extracted");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(strstr(s->tex, "\\ce{2H2 + O2 -> 2H2O}") != NULL,
          "chemistry TeX preserved");
  }
  g_free(out);
  math_segments_free(segs);
}

static void
test_restore_roundtrip(void)
{
  GPtrArray *segs = NULL;
  gchar *extracted = math_extract("inline $a<b&c$ and\n\n$$x<y$$\n", &segs);
  gchar *fake_html = NULL;
  gchar *restored;

  /* Simulate what a Markdown parser would emit for the placeholders:
   * segment 0 is the inline math, segment 1 the display math. */
  if (segs && segs->len == 2) {
    MarkdownMathSegment *s0 = g_ptr_array_index(segs, 0);
    MarkdownMathSegment *s1 = g_ptr_array_index(segs, 1);
    fake_html = g_strdup_printf("<p>inline <code>%s</code> and</p>\n<p>%s</p>\n",
                                s0->token, s1->token);
  }

  restored = math_restore(fake_html ? fake_html : "", segs);
  CHECK(strstr(restored, "\\(a&lt;b&amp;c\\)") != NULL,
        "inline TeX restored HTML-escaped");
  CHECK(strstr(restored, "\\[x&lt;y\\]") != NULL,
        "display TeX restored HTML-escaped");
  CHECK(strstr(restored, "mdmath") == NULL, "no leftover placeholders");
  CHECK(strstr(restored, "math-inline") != NULL && strstr(restored, "math-display") != NULL,
        "wrapper elements present");

  g_free(restored);
  g_free(fake_html);
  g_free(extracted);
  math_segments_free(segs);
}

static void
test_indented_code_respected(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "para\n\n    $x^2$ indented code\n\nafter $y$";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 1, "math inside indented code skipped");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(g_strcmp0(s->tex, "y") == 0, "only math outside indented code extracted");
  }
  CHECK(strstr(out, "$x^2$") != NULL, "indented code content untouched");
  g_free(out);
  math_segments_free(segs);
}

static void
test_indented_code_continuation(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "code:\n\n    $a$\n    $b$\n\ntext $c$";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 1, "whole indented block skipped");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(g_strcmp0(s->tex, "c") == 0, "only trailing text math extracted");
  }
  CHECK(strstr(out, "$a$") != NULL && strstr(out, "$b$") != NULL,
        "all indented lines verbatim");
  g_free(out);
  math_segments_free(segs);
}

static void
test_indented_list_not_code(void)
{
  GPtrArray *segs = NULL;
  /* 4-space indented nested list item directly below a non-blank line:
   * markdown (CommonMark: indented code can't interrupt a paragraph),
   * so its math must still be extracted. */
  const gchar *src = "- outer\n    - nested $x$";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 1, "nested list math still extracted");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(g_strcmp0(s->tex, "x") == 0, "nested list math content");
  }
  g_free(out);
  math_segments_free(segs);
}

static void
test_tab_indented_code(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "para\n\n\t$x$ tabbed code\n\n$y$";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 1, "tab-indented code skipped");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    CHECK(g_strcmp0(s->tex, "y") == 0, "only math after the code block");
  }
  CHECK(strstr(out, "$x$") != NULL, "tab-indented content untouched");
  g_free(out);
  math_segments_free(segs);
}

static void
test_dollar_display_not_cross_fence(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "$$\nx=1\n```\n$$\n```\nrest";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 0, "$$ does not swallow a code fence");
  CHECK(strstr(out, "$$") != NULL && strstr(out, "```") != NULL,
        "fences and delimiters preserved");
  g_free(out);
  math_segments_free(segs);
}

static void
test_bracket_display_not_cross_fence(void)
{
  GPtrArray *segs = NULL;
  const gchar *src = "\\[x=1\n```\n\\]\n```\nrest";
  gchar *out = math_extract(src, &segs);

  CHECK(segs && segs->len == 0, "\\[ does not swallow a code fence");
  CHECK(strstr(out, "\\[x=1") != NULL && strstr(out, "```") != NULL,
        "fences and delimiters preserved");
  g_free(out);
  math_segments_free(segs);
}

static void
test_price_range_not_math(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("It costs $5-$10 total.", &segs);

  CHECK(segs && segs->len == 0, "\"$5-$10\" not treated as math");
  CHECK(strstr(out, "$5-$10") != NULL, "price range preserved");
  g_free(out);
  math_segments_free(segs);
}

static void
test_digit_math_still_extracted(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("$2^2$ and $1+1=2$ and $a+b$", &segs);

  CHECK(segs && segs->len == 3, "digit math with TeX indicators kept");
  if (segs && segs->len == 3) {
    MarkdownMathSegment *s0 = g_ptr_array_index(segs, 0);
    MarkdownMathSegment *s1 = g_ptr_array_index(segs, 1);
    MarkdownMathSegment *s2 = g_ptr_array_index(segs, 2);
    CHECK(g_strcmp0(s0->tex, "2^2") == 0, "^ content kept");
    CHECK(g_strcmp0(s1->tex, "1+1=2") == 0, "= content kept");
    CHECK(g_strcmp0(s2->tex, "a+b") == 0, "letter content kept");
  }
  g_free(out);
  math_segments_free(segs);
}

static void
test_token_no_collision(void)
{
  GPtrArray *segs = NULL;
  gchar *out = math_extract("about `mdmathI0` and real $a$", &segs);
  gchar *html = NULL;
  gchar *restored;

  CHECK(segs && segs->len == 1, "math next to literal token text extracted");
  if (segs && segs->len == 1) {
    MarkdownMathSegment *s = g_ptr_array_index(segs, 0);
    /* user text with the old, predictable token comes FIRST */
    html = g_strdup_printf("<p>about <code>mdmathI0</code> and <code>%s</code></p>",
                           s->token);
    restored = math_restore(html, segs);
    CHECK(strstr(restored, "mdmathI0") != NULL && strstr(restored, s->token) == NULL,
          "literal mdmathI0 text kept, only real token replaced");
    CHECK(strstr(restored, "\\(a\\)") != NULL, "math restored at its own token");
    g_free(restored);
  }
  g_free(html);
  g_free(out);
  math_segments_free(segs);
}

static void
test_build_assets(void)
{
  gchar *mj = math_build_assets("mathjax", TEST_MATH_DIR);
  gchar *kx = math_build_assets("katex", TEST_MATH_DIR);
  gchar *none = math_build_assets("mathjax", "/nonexistent/path/xyz");

  CHECK(strstr(mj, "tex-chtml-full.js") != NULL, "mathjax script tag");
  CHECK(strstr(mj, "fontURL") != NULL, "mathjax fontURL configured");
  CHECK(strstr(mj, "physics") != NULL, "mathjax physics package");
  CHECK(strstr(kx, "katex.min.css") != NULL, "katex css tag");
  CHECK(strstr(kx, "mhchem.min.js") != NULL, "katex mhchem tag");
  CHECK(strstr(kx, "renderMathInElement") != NULL, "katex auto-render call");
  CHECK(strstr(kx, "\\\\dv") != NULL, "katex physics macro shim");
  CHECK(g_strcmp0(none, "") == 0, "missing assets yields empty string");

  g_free(mj);
  g_free(kx);
  g_free(none);
}

static void
test_inject_assets(void)
{
  const gchar *html = "<html><head><title>t</title></head><body>hi</body></html>";
  gchar *out = math_inject_assets(html, "<script src=\"x.js\"></script>");

  CHECK(strstr(out, "<script src=\"x.js\"></script></head>") != NULL,
        "assets injected before </head>");
  g_free(out);

  out = math_inject_assets(html, "");
  CHECK(g_strcmp0(out, html) == 0, "empty assets returns copy");
  g_free(out);

  out = math_inject_assets("<body>x</body>", "<b>a</b>");
  CHECK(strstr(out, "<b>a</b></body>") != NULL, "falls back before </body>");
  g_free(out);
}

int
main(int argc, char **argv)
{
  g_print("math.c unit tests (assets dir: %s)\n", TEST_MATH_DIR);
  test_extract_inline_dollar();
  test_extract_display_dollars();
  test_extract_paren_delims();
  test_skip_fenced_code();
  test_skip_inline_code();
  test_escaped_dollar();
  test_currency_not_math();
  test_escaped_backslash_paren();
  test_chemistry_inside_math();
  test_restore_roundtrip();
  test_build_assets();
  test_inject_assets();
  test_indented_code_respected();
  test_indented_code_continuation();
  test_indented_list_not_code();
  test_tab_indented_code();
  test_dollar_display_not_cross_fence();
  test_bracket_display_not_cross_fence();
  test_price_range_not_math();
  test_digit_math_still_extracted();
  test_token_no_collision();

  if (failures) {
    g_printerr("%d test(s) FAILED\n", failures);
    return 1;
  }
  g_print("all tests passed\n");
  return 0;
}
