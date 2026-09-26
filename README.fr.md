# Aperçu Markdown pour Geany (version enrichie)

[简体中文](README.md) · [English](README.en.md) · [Deutsch](README.de.md) · **Français** · [한국어](README.ko.md) · [日本語](README.ja.md)

Aperçu Markdown en temps réel pour l'éditeur [Geany](https://geany.org),
fork du plugin officiel `markdown` de
[geany-plugins](https://github.com/geany/geany-plugins) 2.1, fortement
enrichi : **rendu hors ligne des formules math/physique/chimie**
(MathJax / KaTeX), **synchronisation live du jeu de couleurs Geany**,
**tableaux GFM**, **suivi du zoom de l'éditeur** et **export HTML**
(adapté aux mobiles, tablettes et navigateur WeChat).

![capture d'écran](docs/plugin_small.png)

```markdown
## Équation de la chaleur
$$\frac{\partial u}{\partial t} = \alpha \nabla^2 u + \frac{1}{c}\dv{q}{t}$$

en ligne : $E = mc^2$ ; chimie : $\ce{2H2 + O2 -> 2H2O}$
```

Tout est rendu pendant la frappe — entièrement **hors ligne**, sans CDN.

> Détails complets (architecture, guide d'extension) : voir
> [README.md (中文)](README.md) ou [README.en.md](README.en.md).

---

## Fonctionnalités

| Fonctionnalité | Description |
|---|---|
| Aperçu temps réel | Rendu automatique des documents Markdown, mise à jour à la frappe (avec anti-rebond) |
| Formules | Syntaxe LaTeX ; délimiteurs `$...$`, `$$...$$`, `\(...\)`, `\[...\]` ; MathJax 3.2.2 / KaTeX 0.18.7 fournis hors ligne |
| Physique | Package `physics` complet sur MathJax ; shim intégré sur KaTeX (`\dv`, `\pdv`, `\qty`, `\abs`, …) |
| Chimie | `mhchem` (`\ce{...}`) sur les deux moteurs |
| Tableaux GFM | Tableaux à tubes, alignement des colonnes (`:---` `:---:` `---:`), balisage inline et formules dans les cellules, échappement `\|` |
| Sync des couleurs | L'aperçu suit le jeu de couleurs Geany actif (texte, sélection, code, tableaux) ; le changement de thème est immédiat |
| Suivi du zoom | `Ctrl++` / `Ctrl+-` / `Ctrl+0` de l'éditeur met l'aperçu à l'échelle ; le niveau survit aux rechargements |
| Export HTML | Un clic dans le menu Outils ; balisage responsive pour mobiles, iPad et WeChat |
| Position | Aperçu dans la barre latérale ou la fenêtre de messages, bascule immédiate |
| Modèles | Modèles HTML personnalisés avec substitution `@@placeholder@@` ; CSS responsive/tableaux injecté dans les anciens modèles |
| Langues de l'interface | UI traduite en chinois, allemand, français, coréen et japonais (anglais = langue source) |

## Installation

Dépendances : Geany ≥ 2.0, GTK3, `webkit2gtk-4.1`, compilateur C,
`pkg-config`, make, gettext.

```bash
sudo apt install libgeany-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
                 pkg-config build-essential gettext
git clone https://github.com/yikuide-lab/geany-markdown.git
cd geany-markdown
make && make test && sudo make install
```

Installation : `markdown.so` → `$(libdir geany)/geany/` ; ressources math
→ `/usr/share/geany-plugins/markdown/math/` ; traductions →
`/usr/share/locale/<langue>/LC_MESSAGES/geany-markdown.mo`.

Redémarrez Geany → **Outils → Gestionnaire de plugins** → activez
**Markdown**.

## Formules

| Syntaxe | Type | Exemple |
|---|---|---|
| `$...$` | en ligne | `$E = mc^2$` |
| `\(...\)` | en ligne (variante) | `\(a_i + b_i\)` |
| `$$...$$` | en bloc (multiligne) | `$$\int_0^1 x\,dx$$` |
| `\[...\]` | en bloc (variante) | `\[\dv{y}{x}\]` |

Les formules sont extraites **avant** l'analyse Markdown — les `_`, `*`,
`&` et `|` ne sont jamais altérés ; `$|x|$` ne coupe pas une cellule de
tableau.

Protection contre les faux positifs : les blocs de code (clôtures ```,
indentation 4 espaces/tabulation, code en ligne) ne sont jamais touchés ;
la recherche du délimiteur de fermeture s'arrête aux clôtures de code ;
les montants comme `$5-$10` restent du texte ; `\$` est un dollar
littéral ; les marqueurs comportent un nonce aléatoire.

```markdown
Physique :  \(\dv{y}{x}\) \(\qty{9.81}{m/s^2}\) \(\abs{x}\)
Chimie :    \(\ce{2H2 + O2 -> 2H2O}\) \(\ce{CO2 + C ->[haute temp] 2CO}\)
```

Changement de moteur : Préférences → *Moteur de formules* (MathJax par
défaut, physics complet ; KaTeX plus rapide, avec shim). Ressources
absentes → repli en texte brut, sans erreur.

Limites connues : une formule de bloc **dans un élément de liste** coupe
la liste (préférez le mode en ligne dans les listes) ; `$1+1$` (chiffres
seuls) n'est pas rendu — `$1+1=2$` fonctionne.

## Tableaux GFM

```markdown
| Objet       |     Définition      | Modélisable ? |
|-------------|:-------------------:|--------------:|
| Prix $P_t$  | **non stationnaire** |      ❌ |
| Log-rendts  | faiblement stationnaire |  ✅ |
```

Lignes d'en-tête et de séparation avec autant de cellules ; alignement
`:---` / `:---:` / `---:` ; `\|` reste dans la cellule ; le CSS des
tableaux est injecté automatiquement dans les anciens modèles.

## Couleurs, zoom, export

- **Couleurs** : par défaut l'aperçu suit le jeu de couleurs Geany
  (Affichage → Jeux de couleurs), avec rafraîchissement instantané.
  Désactivez *Utiliser le jeu de couleurs Geany actuel* pour les couleurs
  manuelles.
- **Zoom** : le zoom de l'éditeur met l'aperçu à l'échelle.
- **Export** : **Outils → Exporter Markdown en HTML…** écrit un fichier
  autonome (UTF-8, viewport, CSS adaptatif).

## Configuration

La boîte de préférences et `~/.config/geany/plugins/markdown/markdown.conf`
se correspondent :

| Clé | Type / défaut | Signification |
|---|---|---|
| `[general] template` | chaîne / vide | modèle HTML ; vide = modèle par défaut |
| `[view] position` | 0 / 1 (barre latérale) | position de l'aperçu |
| `[view] font_name` / `code_font_name` | Serif / Monospace | polices |
| `[view] font_point_size` / `code_font_point_size` | 12 / 12 | tailles (pt) |
| `[view] bg_color` / `fg_color` | #fff / #000 | couleurs manuelles (ignorées en sync) |
| `[view] scheme_colors` | true | suivre le jeu de couleurs Geany |
| `[math] enabled` | true | rendu des formules |
| `[math] engine` | mathjax | `mathjax` ou `katex` |

Le changement de modèle est immédiat. Placeholders : `@@markdown@@`,
`@@font_name@@`, `@@code_font_name@@`, `@@font_point_size@@`,
`@@code_font_point_size@@`, `@@bg_color@@`, `@@fg_color@@`,
`@@math_assets@@`.

## Langues

Les traductions de l'interface (zh_CN, de, fr, ko, ja) suivent la langue
du système, avec repli en anglais. Ajouter une langue : `make pot`,
créer et traduire le `.po`, l'inscrire dans `po/LINGUAS`,
puis `make && sudo make install`.

## Architecture (résumé)

```
Scintilla → plugin.c (signaux) → viewer.c
  ├─ math.c : math_extract()      ← extraire les formules avant l'analyse
  ├─ peg-markdown : Markdown → HTML
  ├─ viewer.c : template_replace() ← modèle + CSS couleurs + scripts moteur
  └─ math.c : math_restore()      ← marqueurs → TeX échappé en HTML
→ WebKitGtk load_html() (défilement et zoom conservés)
```

| Fichier | Rôle |
|---|---|
| `src/plugin.c` | entrée du plugin, signaux, empreinte du jeu de couleurs, zoom, export |
| `src/viewer.c` | `MarkdownViewer`, orchestration du rendu |
| `src/math.c` | extraction/restauration des formules — GLib pur, testable |
| `src/conf.c` | configuration + boîte de préférences |
| `src/test-math.c` | tests unitaires (21 cas / 65 assertions) |
| `peg-markdown/` | analyseur PEG avec extension tableaux GFM |
| `math/` | ressources hors ligne (MathJax/KaTeX, licences) |
| `po/` | traductions (domaine dédié `geany-markdown`) |

Guide d'extension (moteur, placeholder, clé de config, analyseur) :
voir [README.en.md](README.en.md#extending-the-plugin).

## Différences avec geany-plugins 2.1

Rendu de formules hors ligne, sync des couleurs, suivi du zoom, export
HTML, tableaux GFM dans l'analyseur, traductions UI et une série de
corrections de revue — détails dans [CHANGELOG.md](CHANGELOG.md). Le
copyright amont et la GPL-2.0 sont intégralement conservés.

## Licence

Code du plugin **GPL-2.0-or-later** (Matthew Brush, geany-plugins) ·
MathJax Apache-2.0 · KaTeX MIT · peg-markdown MIT/compatible GPL.
