# Geany Markdown 미리보기 (강화판)

[简体中文](README.md) · [English](README.en.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · **한국어** · [日本語](README.ja.md)

[Geany](https://geany.org) 편집기의 실시간 Markdown 미리보기 플러그인.
[geany-plugins](https://github.com/geany/geany-plugins) 2.1의 공식
`markdown` 플러그인을 포크하여 대폭 강화했습니다: **오프라인 수학/물리/
화학 수식 렌더링**(MathJax / KaTeX 듀얼 엔진), **Geany 색 구성표 실시간
동기화**, **GFM 파이프 테이블**, **편집기 줌 추적**, **HTML 내보내기**
(휴대폰/태블릿/WeChat 브라우저에 최적화).

![스크린샷](docs/plugin_small.png)

```markdown
## 열방정식
$$\frac{\partial u}{\partial t} = \alpha \nabla^2 u + \frac{1}{c}\dv{q}{t}$$

인라인: $E = mc^2$; 화학: $\ce{2H2 + O2 -> 2H2O}$
```

입력하는 대로 모두 렌더링됩니다 — 수식, 표, 코드까지 전부
**오프라인**으로, CDN 의존 없이.

> 전체 세부사항(아키텍처, 확장 가이드): [README.md(中文)](README.md)
> 또는 [README.en.md](README.en.md) 참고.

---

## 기능

| 기능 | 설명 |
|---|---|
| 실시간 미리보기 | Markdown 문서를 자동 렌더링, 입력에 따라 갱신 (idle 디바운스) |
| 수식 렌더링 | LaTeX 문법; `$...$` `$$...$$` `\(...\)` `\[...\]` 구분자; MathJax 3.2.2 / KaTeX 0.18.7 오프라인 내장 |
| 물리 | MathJax에서 전체 `physics` 매크로 패키지; KaTeX는 `\dv` `\pdv` `\qty` `\abs` 등 shim 내장 |
| 화학 | 두 엔진 모두 `mhchem`(`\ce{...}`) 지원 |
| GFM 표 | 파이프 테이블, 열 정렬(`:---` `:---:` `---:`), 셀 내 인라인 마크업·수식, `\|` 이스케이프 |
| 색 구성표 동기화 | 미리보기가 현재 Geany 구성표(본문/선택/코드/표)를 따름; 테마 전환 즉시 반영 |
| 줌 추적 | 편집기 `Ctrl++` / `Ctrl+-` / `Ctrl+0`이 미리보기에 함께 적용; 다시 렌더링해도 유지 |
| HTML 내보내기 | 도구 메뉴에서 원클릭 내보내기, 반응형 마크업 포함 |
| 위치 | 사이드바 또는 메시지 창, 설정 즉시 이동 |
| 템플릿 | `@@플레이스홀더@@` 치환 방식의 사용자 정의 HTML 템플릿; 구형 템플릿에 반응형/표 CSS 자동 주입 |
| UI 다국어 | 인터페이스 번역: 중국어·독일어·프랑스어·한국어·일본어 (영어 = 원어) |

## 설치

의존성: Geany ≥ 2.0, GTK3, `webkit2gtk-4.1`, C 컴파일러, `pkg-config`,
make, gettext.

```bash
sudo apt install libgeany-dev libgtk-3-dev libwebkit2gtk-4.1-dev \
                 pkg-config build-essential gettext
git clone https://github.com/yikuide-lab/geany-markdown.git
cd geany-markdown
make && make test && sudo make install
```

설치 위치: `markdown.so` → `$(geany libdir)/geany/`; 수식 리소스 →
`/usr/share/geany-plugins/markdown/math/`; 번역 →
`/usr/share/locale/<언어>/LC_MESSAGES/geany-markdown.mo`.

Geany 재시작 → **도구 → 플러그인 관리자** → **Markdown** 활성화.

## 수식

| 문법 | 종류 | 예 |
|---|---|---|
| `$...$` | 인라인 | `$E = mc^2$` |
| `\(...\)` | 인라인 (대체 문법) | `\(a_i + b_i\)` |
| `$$...$$` | 블록 (여러 줄 가능) | `$$\int_0^1 x\,dx$$` |
| `\[...\]` | 블록 (대체 문법) | `\[\dv{y}{x}\]` |

수식은 Markdown 파싱 **이전에** 추출되므로 수식 안의 `_`, `*`, `&`,
`|`가 절대 깨지지 않습니다. `$|x|$`도 표 셀을 나누지 않습니다.

오탐 방지: 코드 블록(``` 울타리, 들여쓰기 4칸/탭, 인라인 코드)은
건드리지 않음; 블록 구분자 탐색은 코드 울타리에서 중단; `$5-$10` 같은
금액은 텍스트로 유지; `\$`는 리터럴 달러; 플레이스홀더에 무작위 nonce
포함.

```markdown
물리:  \(\dv{y}{x}\) \(\qty{9.81}{m/s^2}\) \(\abs{x}\)
화학:  \(\ce{2H2 + O2 -> 2H2O}\) \(\ce{CO2 + C ->[고온] 2CO}\)
```

엔진 전환: 기본 설정 → *수식 엔진* (MathJax 기본, physics 전체 지원;
KaTeX는 더 빠르고 shim 제공). 리소스가 없으면 오류 없이 일반 텍스트로
표시됩니다.

알려진 제한: 목록 항목 **안의** 블록 수식은 목록을 분할합니다(목록에서는
인라인 수식 사용 권장); `$1+1$`(숫자만)은 렌더링되지 않음 —
`$1+1=2$`는 정상 동작.

## GFM 표

```markdown
| 객체       |      정의        | 모델링 가능? |
|------------|:---------------:|-------------:|
| 가격 $P_t$ | **비정상**       |      ❌ |
| 로그수익륙 | 약한 정상성       |      ✅ |
```

머리행과 구분행의 셀 수가 같아야 함; 정렬은 `:---` / `:---:` / `---:`;
`\|`는 셀 안에 유지; 표 CSS는 구형 템플릿에 자동 주입됩니다.

## 색 구성표, 줌, 내보내기

- **색 구성표**: 기본적으로 미리보기가 Geany 구성표(보기 → 색 구성표)를
  따르며 전환 즉시 다시 렌더링됩니다. *현재 Geany 색 구성표 사용*을
  해제하면 수동 배경/글자색을 사용합니다.
- **줌**: 편집기 줌이 미리보기에 같은 비율로 적용됩니다.
- **내보내기**: **도구 → Markdown을 HTML로 내보내기...**가 UTF-8,
  viewport, 반응형 CSS를 담은 독립 파일을 생성합니다.

## 설정

설정 대화상자와 `~/.config/geany/plugins/markdown/markdown.conf`가
서로 대응합니다:

| 키 | 유형 / 기본값 | 의미 |
|---|---|---|
| `[general] template` | 문자열 / 비움 | HTML 템플릿; 비워두면 기본 템플릿 사용 |
| `[view] position` | 0 / 1 (사이드바) | 미리보기 위치 |
| `[view] font_name` / `code_font_name` | Serif / Monospace | 글꼴 |
| `[view] font_point_size` / `code_font_point_size` | 12 / 12 | 크기 (pt) |
| `[view] bg_color` / `fg_color` | #fff / #000 | 수동 색 (동기화 켜면 무시) |
| `[view] scheme_colors` | true | Geany 색 구성표 따르기 |
| `[math] enabled` | true | 수식 렌더링 |
| `[math] engine` | mathjax | `mathjax` 또는 `katex` |

템플릿 변경은 즉시 적용됩니다. 플레이스홀더: `@@markdown@@`,
`@@font_name@@`, `@@code_font_name@@`, `@@font_point_size@@`,
`@@code_font_point_size@@`, `@@bg_color@@`, `@@fg_color@@`,
`@@math_assets@@`.

## 언어

UI 번역(zh_CN, de, fr, ko, ja)은 시스템 언어를 따르며, 없으면 영어로
대체됩니다. 새 언어 추가: `make pot` → `.po` 작성·번역 → `po/LINGUAS`
등록 → `make && sudo make install`.

## 아키텍처 (요약)

```
Scintilla → plugin.c (시그널) → viewer.c
  ├─ math.c: math_extract()      ← 파싱 전에 수식 추출
  ├─ peg-markdown: Markdown → HTML
  ├─ viewer.c: template_replace() ← 템플릿 + 구성표 CSS + 엔진 스크립트
  └─ math.c: math_restore()      ← 플레이스홀더 → HTML 이스케이프된 TeX
→ WebKitGtk load_html() (스크롤 위치·줌 유지)
```

| 파일 | 역할 |
|---|---|
| `src/plugin.c` | 플러그인 진입점, 시그널, 구성표 지문, 줌, 내보내기 |
| `src/viewer.c` | `MarkdownViewer`, 렌더링 조율 |
| `src/math.c` | 수식 추출/복원 — 순수 GLib, 단독 테스트 가능 |
| `src/conf.c` | 설정 + 기본 설정 대화상자 |
| `src/test-math.c` | 유닛 테스트 (21 케이스 / 65 어설션) |
| `peg-markdown/` | GFM 표 확장을 포함한 PEG 파서 |
| `math/` | 오프라인 리소스 (MathJax/KaTeX, 라이선스 포함) |
| `po/` | 번역 (전용 도메인 `geany-markdown`) |

확장 가이드(엔진/플레이스홀더/설정 키 추가, 파서 수정):
[README.en.md](README.en.md#extending-the-plugin) 참고.

## geany-plugins 2.1 대비 차이점

오프라인 수식 렌더링, 색 구성표 동기화, 줌 추적, HTML 내보내기, 파서의
GFM 표 지원, UI 다국어화, 코드 리뷰 수정 한 라운드 — 자세한 내용은
[CHANGELOG.md](CHANGELOG.md). 업스트림 저작권과 GPL-2.0은 그대로
유지됩니다.

## 라이선스

플러그인 코드 **GPL-2.0-or-later** (Matthew Brush, geany-plugins) ·
MathJax Apache-2.0 · KaTeX MIT · peg-markdown MIT/GPL 호환.
