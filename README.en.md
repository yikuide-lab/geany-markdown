# Geany Markdown Preview (Enhanced)

[简体中文](README.md) · **English** · [Deutsch](README.de.md) · [Français](README.fr.md) · [한국어](README.ko.md) · [日本語](README.ja.md)

A real-time Markdown preview plugin for the [Geany](https://geany.org) editor,
forked from the official `markdown` plugin of
[geany-plugins](https://github.com/geany/geany-plugins) 2.1 and heavily
enhanced: **offline math / physics / chemistry formula rendering** (dual
MathJax / KaTeX engines), **live Geany color-scheme sync**, **GFM pipe
tables**, **editor zoom tracking**, and **HTML export** tuned for phone /
tablet / WeChat browsers.

![screenshot](docs/plugin_small.png)

```markdown
## Heat equation
$$\frac{\partial u}{\partial t} = \alpha \nabla^2 u + \frac{1}{c}\dv{q}{t}$$

inline: mass–energy equivalence $E = mc^2$; chemistry: $\ce{2H2 + O2 -> 2H2O}$
```

Everything renders while you type — formulas, tables, code — fully
**offline**, with no CDN dependency.

---

## Contents

- [Features](#features)
- [Build & install](#build--install)
- [Formula rendering](#formula-rendering)
- [GFM pipe tables](#gfm-pipe-tables)
- [Color-scheme sync](#color-scheme-sync)
- [Zoom tracking](#zoom-tracking)
- [HTML export](#html-export)
- [Configuration reference](#configuration-reference)
- [Template system](#template-system)
- [Languages](#languages)
- [Architecture (for developers)](#architecture-for-developers)
- [Extending the plugin](#extending-the-plugin)
- [Differences from upstream geany-plugins 2.1](#differences-from-upstream-geany-plugins-21)
- [Tests](#tests)
- [License & credits](#license--credits)

---

## Features

| Feature | Details |
|---|---|
| Real-time preview | Renders automatically for Markdown documents, updates as you type (idle-debounced) |
| Formula rendering | LaTeX syntax; `$...$` `$$...$$` `\(...\)` `\[...\]` delimiters; MathJax 3.2.2 / KaTeX 0.18.7 dual engines, bundled offline |
| Physics | Full `physics` macro package on MathJax; built-in shim of common macros on KaTeX (`\dv` `\pdv` `\qty` `\abs`, …) |
| Chemistry | `mhchem` (`\ce{...}`) on both engines |
| GFM tables | Pipe tables, column alignment (`:---` `:---:` `---:`), inline markup and formulas in cells, `\|` escaping |
| Color-scheme sync | Preview follows the active Geany scheme (body/selection/code/table chrome); switching themes refreshes instantly |
| Zoom tracking | Editor `Ctrl++` / `Ctrl+-` / `Ctrl+0` scales the preview; level survives re-renders |
| HTML export | One-click export from the Tools menu with responsive markup for phones, iPads and the WeChat browser |
| View position | Preview lives in the sidebar or the message window; changing the preference moves it immediately |
| Template system | Custom HTML templates with `@@placeholder@@` substitution; legacy templates get responsive/table CSS auto-injected |
| UI languages | Interface translated into Chinese, German, French, Korean and Japanese (English source) via gettext |

---

## Build & install

### Dependencies

- Geany ≥ 2.0 (developed and tested against Geany 2.1)
- GTK3, GLib, WebKitGTK 4.1 (`webkit2gtk-4.1` pkg-config module)
- A C compiler, `pkg-config`, GNU make, and gettext tools (`msgfmt`) for translations

Debian/Ubuntu:

```bash
sudo apt install libgeany-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
                 pkg-config build-essential gettext
```

### Standalone build (recommended)

```bash
git clone https://github.com/yikuide-lab/geany-markdown.git
cd geany-markdown
make            # builds markdown.so + translations + unit tests
make test       # runs the math-module test suite
sudo make install
```

Default install locations (overridable, see the header of the Makefile):

| Item | Location |
|---|---|
| `markdown.so` | `$(geany libdir)/geany/`, e.g. `/usr/lib/x86_64-linux-gnu/geany/` |
| Offline math assets | `/usr/share/geany-plugins/markdown/math/` |
| Help docs | `/usr/share/doc/geany-plugins/markdown/html/` |
| UI translations | `/usr/share/locale/<lang>/LC_MESSAGES/geany-markdown.mo` |

### Building inside the geany-plugins tree

The `src/`, `math/`, `peg-markdown/` and `docs/` directories stay compatible
with the geany-plugins 2.1 autotools layout: drop them into `markdown/` in
the official tree and run `./configure && make -C markdown && sudo make -C
markdown install`.

### Enabling

Restart Geany → **Tools → Plugin Manager** → check **Markdown**. Open a
`.md` file; the preview appears in the sidebar (default) or message window.

> The plugin uses `plugin_module_make_resident`, so **restart Geany** after
> upgrading to load the new version.

---

## Formula rendering

### Delimiters

| Syntax | Type | Example |
|---|---|---|
| `$...$` | inline | `$E = mc^2$` |
| `\(...\)` | inline (alternative) | `\(a_i + b_i\)` |
| `$$...$$` | display (may span lines) | `$$\int_0^1 x\,dx$$` |
| `\[...\]` | display (alternative) | `\[\dv{y}{x}\]` |

All formulas are extracted into opaque placeholders **before** Markdown
parsing (see [Architecture](#architecture-for-developers)), so `_`, `*`,
`&` and `|` inside math are never mangled — `$a*b_c$` renders correctly,
and `$|x|$` does not split a table cell.

### False-positive protection

The extractor (`src/math.c`) implements:

- **Code protection**: fenced blocks (```` ``` ```` / `~~~`), 4-space/tab
  indented code blocks and inline code spans are never touched;
- **Fence isolation**: display-delimiter scanning stops at code fences, so
  an unclosed `$$` cannot swallow the rest of the document;
- **Currency protection**: `$5 and $10` or `$5-$10` stay literal (content
  of `$...$` must not begin/end with whitespace and must contain a letter,
  `\`, `^`, `_`, `{`, `}` or `=` to count as math);
- **Escape protection**: `\$` is a literal dollar; `\\(` never opens math;
- **Collision protection**: placeholders carry a random per-extraction
  nonce, so a literal `mdmathI0` in your document never collides.

### Physics & chemistry

```markdown
Physics (full physics package on MathJax):
  \(\dv{y}{x}\) \(\pdv[2]{f}{x}\) \(\qty{9.81}{m/s^2}\) \(\abs{x}\)

Chemistry (mhchem on both engines):
  \(\ce{2H2 + O2 -> 2H2O}\)
  \(\ce{CO2 + C ->[high temp] 2CO}\)
```

KaTeX has no official physics package; the plugin ships a shim in
`math_build_assets()`: `\dv` `\pdv` `\pd` `\qty` `\unit` `\va` `\vb`
`\vu` `\abs` `\avg` `\comm`.

### Engines

| | MathJax (default) | KaTeX |
|---|---|---|
| Speed | slower | fast |
| physics package | full | built-in shim |
| mhchem chemistry | ✔ | ✔ |
| Switching | Preferences → *Math Engine* | same |

When assets are missing (e.g. installed with `NO_MATH=1`), formulas fall
back to plain text without errors.

### Known limitations

- Display math (`$$...$$`) **inside a list item** splits the list — an
  inherent trade-off of the placeholder approach; prefer inline math in
  lists;
- Pure-digit arithmetic like `$1+1$` is not rendered (currency rule);
  add a letter or `^`/`=` — `$1+1=2$` works.

---

## GFM pipe tables

```markdown
| Object    |  Definition  | Modelable? |
|-----------|:------------:|-----------:|
| Price $P_t$ | **non-stationary** | ❌ |
| Log returns | weakly stationary | ✅ |
```

- Header and delimiter rows must have the same number of cells;
- Alignment: `:---` left, `:---:` center, `---:` right (default left);
- `\|` stays inside the current cell;
- Formulas are extracted before table parsing, so `$|x|$` never splits a
  cell;
- Table styling (borders, zebra rows, header background) is injected into
  legacy templates automatically.

Table support is implemented in the extended peg-markdown parser
(`peg-markdown/markdown_parser.leg`).

## Color-scheme sync

On by default: body colors, selection highlight, code backgrounds and
table chrome all come from the active Geany scheme (**View → Color
Schemes**); switching re-renders without a restart. Geany has no
theme-switch signal, so the plugin computes a scheme fingerprint on
`SCN_PAINTED` notifications and refreshes when it changes.

Uncheck *Use current Geany color scheme* in the preferences to use the
manual *BG Color* / *FG Color* instead. The link color switches between a
darker and a lighter blue based on background luminance.

## Zoom tracking

`Ctrl++` / `Ctrl+-` / `Ctrl+0` (or Ctrl+wheel) in the editor scales the
preview by `(base font + zoom points) / base font`; WebKit resets zoom on
every reload, so the level is re-applied on `WEBKIT_LOAD_FINISHED`. Zoom
is per-document and follows document switches.

## HTML export

**Tools → Export Markdown as HTML...** saves the current preview —
formulas, tables and responsive markup included — as a standalone file
with:

- UTF-8 `charset` and mobile `viewport` meta tags;
- Adaptive CSS: images/videos scale to the viewport, wide tables scroll
  horizontally below 800 px, iOS/WeChat text-size inflation disabled.

Files read well on phones, iPads and in the WeChat browser. Formula
engine scripts reference the locally installed offline assets via
`file://` URIs — they render on **this machine**; when sending files to
others, distribute the assets too or switch to a CDN-based template.

---

## Configuration reference

The preferences dialog (Plugin Manager → Preferences) and
`~/.config/geany/plugins/markdown/markdown.conf` mirror each other:

| Key | Type / default | Meaning |
|---|---|---|
| `[general] template` | string / empty | HTML template path; empty uses `~/.config/geany/plugins/markdown/template.html` (auto-generated on first run) |
| `[view] position` | 0 / 1 (sidebar) | Preview location: sidebar or message window |
| `[view] font_name` | Serif | Body font |
| `[view] code_font_name` | Monospace | Code font |
| `[view] font_point_size` | 12 | Body size (pt) |
| `[view] code_font_point_size` | 12 | Code size (pt) |
| `[view] bg_color` / `fg_color` | #fff / #000 | Manual colors (ignored while color-scheme sync is on) |
| `[view] scheme_colors` | true | Follow the Geany color scheme |
| `[math] enabled` | true | Enable formula rendering |
| `[math] engine` | mathjax | Engine: `mathjax` or `katex` |

Changing the template path takes effect **immediately** (no restart).

## Template system

Placeholders are substituted on every render:

| Placeholder | Replaced by |
|---|---|
| `@@markdown@@` | rendered HTML body |
| `@@font_name@@` / `@@code_font_name@@` | font names (auto-quoted) |
| `@@font_point_size@@` / `@@code_font_point_size@@` | sizes |
| `@@bg_color@@` / `@@fg_color@@` | manual colors (empty while scheme sync is on) |
| `@@math_assets@@` | engine `<link>`/`<script>` block |

Legacy compatibility: templates without `@@math_assets@@` get the engine
scripts injected before `</head>`; missing viewport/charset/responsive/
table CSS is injected automatically.

---

## Languages

The plugin UI is translated via gettext and ships five complete catalogs
(zh_CN, de, fr, ko, ja — 22 strings each; English is the source). It
follows your system locale automatically and falls back to English.
This README is available in six languages — see the switcher at the top.

### Adding a language / fixing a translation

```bash
make pot                                   # 1. regenerate the template
cp po/geany-markdown.pot po/<lang>.po      # 2. create + translate
echo "<lang>" >> po/LINGUAS                # 3. register it
make && sudo make install                  # 4. build & install
```

---

## Architecture (for developers)

### Render pipeline

```
Scintilla editor
   │ SCN_MODIFIED / document switch / scheme fingerprint change / SCN_ZOOM
   ▼
plugin.c: update_markdown_viewer()
   │ fetch full text + document encoding
   ▼
viewer.c: markdown_viewer_get_html()
   │
   ├─ math.c: math_extract()        ← extract formulas before parsing
   │     $...$   → `mdmathI<idx><nonce>` (wrapped in a code span)
   │     $$...$$ → mdmathD<idx><nonce> (own paragraph)
   │
   ├─ peg-markdown: markdown_to_string()   ← Markdown → HTML
   │
   ├─ viewer.c: template_replace()  ← template + scheme CSS + engine assets
   │
   └─ math.c: math_restore()        ← placeholders → HTML-escaped TeX
   ▼
WebKitGtk: load_html() (scroll position and zoom preserved)
```

Formulas reach the page as `<span class="math-inline">\(...\)</span>` /
`<div class="math-display">\[...\]</div>` and are typeset by the engine
scripts (MathJax `tex-chtml-full.js` or KaTeX auto-render).

### Files

| File | Responsibility |
|---|---|
| `src/plugin.c` | plugin entry, Geany signal wiring, scheme fingerprint, zoom sync, export dialog |
| `src/viewer.c` | `MarkdownViewer` (WebKitWebView subclass), render orchestration, scroll/zoom preservation |
| `src/math.c` | formula extraction/restoration/engine assets — pure GLib, no GTK, independently testable |
| `src/conf.c` | `MarkdownConfig` (GKeyFile persistence), preferences GUI, scheme CSS generation |
| `src/markdown-gtk-compat.c` | GtkTable/GtkGrid compatibility shims |
| `src/test-math.c` | unit tests for `math.c` (21 cases / 65 assertions) |
| `peg-markdown/` | GPL-friendly Markdown parser (PEG grammar, GFM table extension) |
| `math/` | offline rendering assets (MathJax 3.2.2 / KaTeX 0.18.7, see `math/README`) |
| `po/` | gettext translations (own `geany-markdown` domain) |

---

## Extending the plugin

### Run the tests

```bash
make test
```

`math.c` is a pure module (string in → string + segment array out) with
no GTK/WebKit dependency — add cases in `src/test-math.c` and register
them in `main()`.

### Add a rendering engine

1. Add a branch in `math_build_assets()` (`src/math.c`): validate the
   assets (see `have_file()`), assemble the engine's `<script>`/`<style>`
   block;
2. The engine must handle `\(...\)` / `\[...\]` delimiters (the output
   format of `math_restore()`), or configure its native delimiters there;
3. Add the option in `src/conf.c` (`PROP_MATH_ENGINE` getter and the
   preferences combo box in `markdown_config_gui()`).

### Add a template placeholder

1. `src/conf.c`: read the key in `markdown_config_get_property()`;
2. `src/viewer.c`: fetch it in `template_replace()` and
   `replace_all(tmpl, "@@new_key@@", value)`;
3. Add it to the default template (`MARKDOWN_HTML_TEMPLATE`) and consider
   an injection fallback for legacy templates.

### Add a configuration key

Follow the existing pattern in `src/conf.c` (property enum +
`g_param_spec_*` + `set_property`/`get_property`), then add a row in
`markdown_config_gui()` and `on_dialog_response()`. Any config property
change re-renders the preview automatically (wired via `notify`).

### Modify the Markdown parser

The grammar lives in `peg-markdown/markdown_parser.leg`. Regenerating
`markdown_parser.c` requires the `leg` tool (upstream peg-0.1.9, not
bundled; the generated file is committed, so day-to-day changes work at
the C level).

### Debugging

`G_MESSAGES_DEBUG=Markdown geany 2>&1 | grep Markdown` shows asset and
template diagnostics.

---

## Differences from upstream geany-plugins 2.1

Upstream provides the basic real-time preview. This fork adds (see
[CHANGELOG.md](CHANGELOG.md)):

- `src/math.c` + offline `math/` assets: formula extraction/restoration
  with dual engines;
- color-scheme sync (fingerprint detection + CSS generation);
- zoom tracking and post-render zoom restoration;
- Tools-menu HTML export;
- responsive/table CSS auto-injection for legacy templates;
- peg-markdown extension: GFM pipe tables;
- UI translations (zh_CN/de/fr/ko/ja) under a dedicated gettext domain;
- a round of review fixes: fence-aware delimiter scanning, indented-code
  protection, placeholder nonces, currency heuristics, template hot
  reload, asset validation, small memory/reference fixes.

All upstream copyright and the GPL-2.0 license are preserved.

## Tests

```bash
make test
```

Coverage: four delimiters, code-block protection (fenced / indented /
tab / inline), fences not swallowed by `$$`, currency/range rejection,
escapes, chemistry, placeholder round-trip, collision protection, engine
asset generation and injection.

## License & credits

- Plugin code: **GPL-2.0-or-later**, based on the geany-plugins markdown
  plugin by Matthew Brush (see `AUTHORS`, `COPYING`);
- MathJax 3.2.2: Apache-2.0 (`math/mathjax/LICENSE`);
- KaTeX 0.18.7: MIT (`math/katex/LICENSE`);
- peg-markdown parser: MIT / GPL-compatible (`peg-markdown/README`).
