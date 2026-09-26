# Geany Markdown プレビュー（拡張版）

[简体中文](README.md) · [English](README.en.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [한국어](README.ko.md) · **日本語**

[Geany](https://geany.org) エディタ用のリアルタイム Markdown プレビュー
プラグイン。[geany-plugins](https://github.com/geany/geany-plugins) 2.1
公式 `markdown` プラグインをフォークし、大幅に拡張したものです:
**オフラインでの数学・物理・化学の数式レンダリング**（MathJax /
KaTeX のデュアルエンジン）、**Geany 配色スキームのライブ同期**、
**GFM パイプテーブル**、**エディターズームの追従**、**HTML エクス
ポート**（スマホ／タブレット／WeChat ブラウザ対応）。

![スクリーンショット](docs/plugin_small.png)

```markdown
## 熱伝導方程式
$$\frac{\partial u}{\partial t} = \alpha \nabla^2 u + \frac{1}{c}\dv{q}{t}$$

インライン: $E = mc^2$; 化学: $\ce{2H2 + O2 -> 2H2O}$
```

入力と同時にすべてレンダリング — 数式・表・コードすべて完全
**オフライン**、CDN 依存なし。

> 完全な詳細（アーキテクチャ、拡張ガイド）:
> [README.md（中文）](README.md) または [README.en.md](README.en.md)。

---

## 機能

| 機能 | 説明 |
|---|---|
| リアルタイムプレビュー | Markdown ドキュメントを自動レンダリング、入力に追従して更新（アイドルデバウンス） |
| 数式レンダリング | LaTeX 構文; 区切り記号 `$...$` `$$...$$` `\(...\)` `\[...\]`; MathJax 3.2.2 / KaTeX 0.18.7 を同梱 |
| 物理 | MathJax では完全な `physics` マクロパッケージ; KaTeX では `\dv` `\pdv` `\qty` `\abs` などの shim を内蔵 |
| 化学 | 両エンジンで `mhchem`（`\ce{...}`）対応 |
| GFM テーブル | パイプテーブル、列揃え（`:---` `:---:` `---:`）、セル内インラインマークアップ・数式、`\|` エスケープ |
| 配色同期 | プレビューが現在の Geany スキーム（本文・選択・コード・表）に追従; テーマ切替で即時再描画 |
| ズーム追従 | エディタの `Ctrl++` / `Ctrl+-` / `Ctrl+0` がプレビューにも適用; 再レンダリング後も維持 |
| HTML エクスポート | ツールメニューからワンクリック、レスポンシブマークアップ付き |
| 表示位置 | サイドバーまたはメッセージウィンドウ、設定で即座に移動 |
| テンプレート | `@@プレースホルダ@@` 置換方式のカスタム HTML テンプレート; 旧テンプレートにはレスポンシブ／表 CSS を自動注入 |
| UI 多言語 | インターフェース翻訳: 中国語・ドイツ語・フランス語・韓国語・日本語（英語が基準言語） |

## インストール

依存: Geany ≥ 2.0、GTK3、`webkit2gtk-4.1`、C コンパイラ、
`pkg-config`、make、gettext。

```bash
sudo apt install libgeany-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
                 pkg-config build-essential gettext
git clone https://github.com/yikuide-lab/geany-markdown.git
cd geany-markdown
make && make test && sudo make install
```

インストール先: `markdown.so` → `$(geany libdir)/geany/`; 数式アセット →
`/usr/share/geany-plugins/markdown/math/`; 翻訳 →
`/usr/share/locale/<言語>/LC_MESSAGES/geany-markdown.mo`。

Geany を再起動 → **ツール → プラグインマネージャ** → **Markdown** を
有効化。

## 数式

| 構文 | 種類 | 例 |
|---|---|---|
| `$...$` | インライン | `$E = mc^2$` |
| `\(...\)` | インライン（別記法） | `\(a_i + b_i\)` |
| `$$...$$` | ブロック（複数行可） | `$$\int_0^1 x\,dx$$` |
| `\[...\]` | ブロック（別記法） | `\[\dv{y}{x}\]` |

数式は Markdown パースの**前に**抽出されるため、数式内の `_`、`*`、
`&`、`|` が崩れることは決してありません。`$|x|$` が表のセルを分割する
こともありません。

誤判定防止: コードブロック（``` フェンス、半角スペース 4 つ／タブ
インデント、インラインコード）は決して処理対象にしない; ブロック区
切り記号の探索はコードフェンスで停止; `$5-$10` のような金額はテキ
ストのまま; `\$` はリテラルのドル記号; プレースホルダにはランダムな
nonce を付与。

```markdown
物理:  \(\dv{y}{x}\) \(\qty{9.81}{m/s^2}\) \(\abs{x}\)
化学:  \(\ce{2H2 + O2 -> 2H2O}\) \(\ce{CO2 + C ->[高温] 2CO}\)
```

エンジン切替: 設定 → *数式エンジン*（MathJax がデフォルト、physics
完全対応; KaTeX は高速、shim 付き）。アセットがない場合はエラーなしで
プレーンテキストにフォールバックします。

既知の制限: リスト項目**内部の**ブロック数式はリストを分割します
（リスト内ではインライン数式を推奨）; `$1+1$`（数字のみ）は非レンダ
リング — `$1+1=2$` は動作します。

## GFM テーブル

```markdown
| オブジェクト |      定義       | モデル化可? |
|-------------|:--------------:|------------:|
| 価格 $P_t$  | **非定常**      |     ❌ |
| 対数収益    | 弱定常          |     ✅ |
```

ヘッダー行と区切り行のセル数は一致必須; 揃えは `:---` / `:---:` /
`---:`; `\|` はセル内に留まる; 表 CSS は旧テンプレートに自動注入
されます。

## 配色・ズーム・エクスポート

- **配色**: デフォルトでプレビューは Geany のスキーム（表示 → 配色
  スキーム）に追従し、切替時に即時再描画されます。*現在の Geany 配色
  スキームを使用* のチェックを外すと手動の背景色/前景色になります。
- **ズーム**: エディタのズームが同じ比率でプレビューに適用されます。
- **エクスポート**: **ツール → Markdown を HTML としてエクスポート...**
  が UTF-8・viewport・レスポンシブ CSS 入りの単独ファイルを書き出します。

## 設定

設定ダイアログと `~/.config/geany/plugins/markdown/markdown.conf` は
互いに対応します:

| キー | 型 / デフォルト | 意味 |
|---|---|---|
| `[general] template` | 文字列 / 空 | HTML テンプレート; 空なら標準テンプレート |
| `[view] position` | 0 / 1（サイドバー） | プレビュー位置 |
| `[view] font_name` / `code_font_name` | Serif / Monospace | フォント |
| `[view] font_point_size` / `code_font_point_size` | 12 / 12 | サイズ (pt) |
| `[view] bg_color` / `fg_color` | #fff / #000 | 手動色（同期有効時は無視） |
| `[view] scheme_colors` | true | Geany 配色スキームに追従 |
| `[math] enabled` | true | 数式レンダリング |
| `[math] engine` | mathjax | `mathjax` または `katex` |

テンプレート変更は即時反映。プレースホルダ: `@@markdown@@`,
`@@font_name@@`, `@@code_font_name@@`, `@@font_point_size@@`,
`@@code_font_point_size@@`, `@@bg_color@@`, `@@fg_color@@`,
`@@math_assets@@`。

## 言語

UI 翻訳（zh_CN, de, fr, ko, ja）はシステムロケールに追従し、なければ
英語にフォールバックします。言語追加: `make pot` → `.po` を作成・翻訳 →
`po/LINGUAS` に登録 → `make && sudo make install`。

## アーキテクチャ（概要）

```
Scintilla → plugin.c（シグナル）→ viewer.c
  ├─ math.c: math_extract()      ← パース前に数式を抽出
  ├─ peg-markdown: Markdown → HTML
  ├─ viewer.c: template_replace() ← テンプレート + スキーム CSS + エンジンスクリプト
  └─ math.c: math_restore()      ← プレースホルダ → HTML エスケープ済み TeX
→ WebKitGtk load_html()（スクロール位置とズームを維持）
```

| ファイル | 役割 |
|---|---|
| `src/plugin.c` | プラグインエントリ、シグナル、スキーム指紋、ズーム、エクスポート |
| `src/viewer.c` | `MarkdownViewer`、レンダリング統制 |
| `src/math.c` | 数式の抽出/復元 — 純粋 GLib、単体テスト可能 |
| `src/conf.c` | 設定 + 環境設定ダイアログ |
| `src/test-math.c` | 単体テスト（21 ケース / 65 アサーション） |
| `peg-markdown/` | GFM テーブル拡張付き PEG パーサ |
| `math/` | オフラインアセット（MathJax/KaTeX、ライセンス付き） |
| `po/` | 翻訳（専用ドメイン `geany-markdown`） |

拡張ガイド（エンジン/プレースホルダ/設定キーの追加、パーサ修正）:
[README.en.md](README.en.md#extending-the-plugin) 参照。

## geany-plugins 2.1 との違い

オフライン数式レンダリング、配色同期、ズーム追従、HTML エクスポート、
パーサの GFM テーブル対応、UI 多言語化、コードレビューによる修正 —
詳細は [CHANGELOG.md](CHANGELOG.md)。アップストリームの著作権と
GPL-2.0 は完全に保持されています。

## ライセンス

プラグインコード **GPL-2.0-or-later**（Matthew Brush, geany-plugins）·
MathJax Apache-2.0 · KaTeX MIT · peg-markdown MIT/GPL 互換。
