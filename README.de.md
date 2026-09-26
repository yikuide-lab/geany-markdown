# Geany Markdown-Vorschau (Erweitert)

[简体中文](README.md) · [English](README.en.md) · **Deutsch** · [Français](README.fr.md) · [한국어](README.ko.md) · [日本語](README.ja.md)

Echtzeit-Markdown-Vorschau für den [Geany](https://geany.org)-Editor,
ein Fork des offiziellen `markdown`-Plugins aus
[geany-plugins](https://github.com/geany/geany-plugins) 2.1, deutlich
erweitert: **Offline-Darstellung von Mathematik-/Physik-/Chemie-Formeln**
(MathJax / KaTeX), **live Synchronisation des Geany-Farbschemas**,
**GFM-Pipetabellen**, **Zoom-Folgen des Editors** und **HTML-Export**
(optimiert für Handy/Tablet/WeChat-Browser).

![Screenshot](docs/plugin_small.png)

```markdown
## Wärmeleitungsgleichung
$$\frac{\partial u}{\partial t} = \alpha \nabla^2 u + \frac{1}{c}\dv{q}{t}$$

inline: $E = mc^2$; Chemie: $\ce{2H2 + O2 -> 2H2O}$
```

Alles wird beim Tippen gerendert — vollständig **offline**, ohne CDN.

> Vollständige Details (Architektur, Erweiterungsleitfaden): siehe
> [README.md (中文)](README.md) oder [README.en.md](README.en.md).

---

## Funktionen

| Funktion | Beschreibung |
|---|---|
| Echtzeit-Vorschau | Rendert Markdown-Dokumente automatisch, Aktualisierung beim Tippen (idle-entprellt) |
| Formeldarstellung | LaTeX-Syntax; Trennzeichen `$...$`, `$$...$$`, `\(...\)`, `\[...\]`; MathJax 3.2.2 / KaTeX 0.18.7 offline gebündelt |
| Physik | Vollständiges `physics`-Makropaket (MathJax); KaTeX-Shim für `\dv`, `\pdv`, `\qty`, `\abs`, … |
| Chemie | `mhchem` (`\ce{...}`) auf beiden Engines |
| GFM-Tabellen | Pipetabellen, Spaltenausrichtung (`:---` `:---:` `---:`), Inline-Markup und Formeln in Zellen, `\|`-Escaping |
| Farbschema-Sync | Vorschau folgt dem aktiven Geany-Schema (Text/Selektion/Code/Tabellen); Themenwechsel wirkt sofort |
| Zoom-Folgen | `Ctrl++` / `Ctrl+-` / `Ctrl+0` im Editor skaliert die Vorschau; Zoom überlebt Neuladungen |
| HTML-Export | Ein Klick im Menü „Extras“; responsives Markup für Handy, iPad und WeChat-Browser |
| Position | Vorschau in Seitenleiste oder Meldungsfenster, sofort umschaltbar |
| Templates | Eigene HTML-Templates mit `@@Platzhaltern@@`; alte Templates erhalten responsiv-/Tabellen-CSS automatisch |
| Oberflächensprachen | UI übersetzt in Chinesisch, Deutsch, Französisch, Koreanisch, Japanisch (Englisch = Quellsprache) |

## Installation

Abhängigkeiten: Geany ≥ 2.0, GTK3, `webkit2gtk-4.1`, C-Compiler,
`pkg-config`, make, gettext.

```bash
sudo apt install libgeany-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
                 pkg-config build-essential gettext
git clone https://github.com/yikuide-lab/geany-markdown.git
cd geany-markdown
make && make test && sudo make install
```

Installation nach: `markdown.so` → `$(geany libdir)/geany/`; Mathe-Ressourcen
→ `/usr/share/geany-plugins/markdown/math/`; Übersetzungen →
`/usr/share/locale/<Sprache>/LC_MESSAGES/geany-markdown.mo`.

Geany neu starten → **Extras → Plugin-Manager** → **Markdown** aktivieren.

## Formeln

| Syntax | Typ | Beispiel |
|---|---|---|
| `$...$` | inline | `$E = mc^2$` |
| `\(...\)` | inline (alternativ) | `\(a_i + b_i\)` |
| `$$...$$` | Block (mehrzeilig) | `$$\int_0^1 x\,dx$$` |
| `\[...\]` | Block (alternativ) | `\[\dv{y}{x}\]` |

Formeln werden **vor** dem Markdown-Parsen extrahiert — `_`, `*`, `&` und
`|` in Formeln werden nie verändert; `$|x|$` teilt keine Tabellenzelle.

Schutz vor Fehlinterpretation: Code-Blöcke (```-Zäune, 4 Leerzeichen/Einrückung,
Inline-Code) werden nie angefasst; die Suche nach Block-Trennern stoppt an
Code-Zäunen; Währungsbeträge wie `$5-$10` bleiben Text; `\$` ist ein
literaler Dollar; Platzhalter tragen ein zufälliges Nonce.

```markdown
Physik:  \(\dv{y}{x}\) \(\qty{9.81}{m/s^2}\) \(\abs{x}\)
Chemie:  \(\ce{2H2 + O2 -> 2H2O}\) \(\ce{CO2 + C ->[hohe Temp] 2CO}\)
```

Engine-Umschaltung: Einstellungen → *Formel-Engine* (MathJax = Standard,
vollständiges physics-Paket; KaTeX = schneller, mit Shim). Fehlende
Ressourcen → reinrassiger Text ohne Fehlermeldung.

Bekannte Grenzen: Blockformeln **innerhalb eines Listenelements** trennen
die Liste (Inline-Formeln in Listen bevorzugen); `$1+1$` (reine Ziffern)
wird nicht gerendert — `$1+1=2$` funktioniert.

## GFM-Tabellen

```markdown
| Objekt      |   Definition      | Modellierbar? |
|-------------|:-----------------:|--------------:|
| Preis $P_t$ | **nicht stationär** | ❌ |
| Renditen    | schwach stationär   | ✅ |
```

Kopf- und Trennzeile müssen gleich viele Zellen haben; Ausrichtung über
`:---` / `:---:` / `---:`; `\|` bleibt in der Zelle; Tabellen-CSS wird
alten Templates automatisch injiziert.

## Farbschema, Zoom, Export

- **Farbschema**: Standardmäßig folgt die Vorschau dem Geany-Schema
  (Ansicht → Farbschemas), inkl. Sofort-Aktualisierung beim Wechsel.
  Abwählen über *Aktuelles Geany-Farbschema verwenden*.
- **Zoom**: Editor-Zoom (Ctrl+/−/0) skaliert die Vorschau mit.
- **Export**: **Extras → Markdown als HTML exportieren …** schreibt eine
  eigenständige Datei mit UTF-8, Viewport und responsivem CSS.

## Konfiguration

Dialog (Plugin-Manager → Preferences) und
`~/.config/geany/plugins/markdown/markdown.conf` entsprechen sich:

| Schlüssel | Typ / Standard | Bedeutung |
|---|---|---|
| `[general] template` | Zeichenkette / leer | HTML-Template; leer = Standardvorlage |
| `[view] position` | 0 / 1 (Seitenleiste) | Position der Vorschau |
| `[view] font_name` / `code_font_name` | Serif / Monospace | Schriften |
| `[view] font_point_size` / `code_font_point_size` | 12 / 12 | Größen (pt) |
| `[view] bg_color` / `fg_color` | #fff / #000 | manuelle Farben (bei Schema-Sync ignoriert) |
| `[view] scheme_colors` | true | Geany-Farbschema verwenden |
| `[math] enabled` | true | Formeldarstellung |
| `[math] engine` | mathjax | `mathjax` oder `katex` |

Template-Wechsel wirkt sofort. Platzhalter: `@@markdown@@`,
`@@font_name@@`, `@@code_font_name@@`, `@@font_point_size@@`,
`@@code_font_point_size@@`, `@@bg_color@@`, `@@fg_color@@`,
`@@math_assets@@`.

## Sprachen

UI-Übersetzungen (zh_CN, de, fr, ko, ja) folgen der Systemsprache,
Fallback Englisch. Neue Sprache hinzufügen: `make pot`, `.po` anlegen und
übersetzen, in `po/LINGUAS` eintragen, `make && sudo make install`.

## Architektur (Kurzfassung)

```
Scintilla → plugin.c (Signale) → viewer.c
  ├─ math.c: math_extract()      ← Formeln vor dem Parsen extrahieren
  ├─ peg-markdown: Markdown → HTML
  ├─ viewer.c: template_replace()  ← Template + Schema-CSS + Engine-Skripte
  └─ math.c: math_restore()      ← Platzhalter → HTML-escaped TeX
→ WebKitGtk load_html() (Scrollposition und Zoom bleiben erhalten)
```

| Datei | Aufgabe |
|---|---|
| `src/plugin.c` | Plugin-Eintrag, Signale, Schema-Fingerprint, Zoom, Export |
| `src/viewer.c` | `MarkdownViewer`, Render-Orchestrierung |
| `src/math.c` | Formel-Extraktion/-Restauration — pures GLib, testbar |
| `src/conf.c` | Konfiguration + Einstellungsdialog |
| `src/test-math.c` | Unit-Tests (21 Fälle / 65 Assertionen) |
| `peg-markdown/` | PEG-Parser mit GFM-Tabellenerweiterung |
| `math/` | Offline-Ressourcen (MathJax/KaTeX, Lizenzdateien) |
| `po/` | Übersetzungen (eigene Domain `geany-markdown`) |

Erweiterungsleitfaden (Engine/Platzhalter/Konfiguration hinzufügen,
Parser ändern): siehe [README.en.md](README.en.md#extending-the-plugin).

## Unterschiede zu geany-plugins 2.1

Formeldarstellung mit Offline-Assets, Farbschema-Sync, Zoom-Folgen,
HTML-Export, GFM-Tabellen im Parser, UI-Übersetzungen sowie eine Runde
Review-Fixes — Details in [CHANGELOG.md](CHANGELOG.md). Upstream-Copyright
und GPL-2.0 bleiben vollständig erhalten.

## Lizenz

Plugin-Code **GPL-2.0-or-later** (Matthew Brush, geany-plugins) ·
MathJax Apache-2.0 · KaTeX MIT · peg-markdown MIT/GPL-kompatibel.
