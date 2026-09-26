# Changelog

## 2026-09-26 — enhanced fork (基于 geany-plugins 2.1)

### 新增

- **多语言界面**（`po/`）：gettext 标准方案，独立翻译域
  `geany-markdown`（不干扰 geany-plugins 官方翻译目录）；内置
  简体中文、德语、法语、韩语、日语五种完整翻译（各 22 条界面
  字符串）；`make pot` 重新生成模板，`make` 自动编译并校验 `.po`，
  安装到 `<localedir>/<lang>/LC_MESSAGES/geany-markdown.mo`。
- **数学/物理/化学公式渲染**（`src/math.c`、`src/math.h`、`src/test-math.c`、`math/`）
  - 解析前提取公式为带随机 nonce 的不透明占位符，模板替换后还原，
    公式内容先做 HTML 转义（防注入）；
  - 四种定界符：`$...$`、`$$...$$`、`\(...\)`、`\[...\]`；
  - 双引擎：MathJax 3.2.2（默认，完整 physics 宏包）与
    KaTeX 0.18.7（内置 physics 宏 shim），均含 mhchem；
  - 离线资源随插件安装（`math/Makefile.am`），缺失时安静降级；
  - 偏好对话框新增 *Math* 开关与 *Math Engine* 引擎选择。
- **Geany 配色方案同步**（`scheme_colors` 配置项）：预览跟随当前
  主题（正文/选区/代码/表格），`SCN_PAINTED` 配色指纹检测实现
  无信号条件下的主题切换感知；链接色按背景亮度自适应。
- **缩放跟随**：`SCN_ZOOM` → WebKit 缩放级别换算，
  `WEBKIT_LOAD_FINISHED` 后恢复（修复重载重置缩放）。
- **导出 HTML**：工具菜单 *Export Markdown as HTML...*。
- **响应式/表格 CSS 自动注入**：旧模板缺 viewport/charset/表格
  样式时自动补齐（`md-responsive-css` / `md-table-css`）。
- **GFM 管道表格**：扩展 peg-markdown 解析器（`markdown_parser.leg`）
  支持管道表格、列对齐与单元格内行内标记。
- 默认模板升级：UTF-8、viewport、表格与响应式样式。
- `math.c` 单元测试套件（21 用例 / 65 断言）接入 automake `TESTS`。

### 修复（代码评审）

- 块级公式闭合定界符扫描改为围栏感知（`find_display_close()`），
  未闭合的 `$$`/`\[` 不再吞掉代码围栏及其后文档；
- 尊重 4 空格/tab 缩进代码块（`in_indented_code` 状态机，遵循
  CommonMark "缩进代码不能打断段落" 规则，嵌套列表不受影响）；
- 占位符加入随机 nonce（`mdmathD/I<idx><8hex>`），消除与用户
  文档字面 token 的碰撞；token 预存于 `MarkdownMathSegment.token`；
- `$...$` 内容须含字母/TeX 控制字符/`=`（`tex_looks_like_math()`），
  `$5-$10` 等价格文本不再误判；
- KaTeX 资源校验扩展到全部三个脚本，MathJax 增查字体目录，
  部分安装不再静默 404；
- `template-file` 属性写入时重新加载模板（换模板无需重启）；
  偏好对话框模板路径为空时回退默认，消除 GLib-CRITICAL；
- `md_plugin_configure()` 补 `g_object_unref(conf)`；
- 清除 `markdown_build_scheme_css()` 中的 luma 死代码，实现按背景
  亮度选择链接色。

### 基线

基于 geany-plugins 2.1 官方 markdown 插件（作者 Matthew Brush，
GPL-2.0-or-later），上游版权与许可证完整保留。
