# openOfficetab icon

图标以三张叠放的文档标签页为主体，蓝、绿、橙对应 Word、Excel、PowerPoint 三种宿主；不包含微软标志或字母。

- `openofficetab.png`：内置 image_gen 生成的原始透明 PNG，1254 × 1254。
- `openofficetab.ico`：由 PNG 等比缩放并封装的 Windows 图标，包含 16、24、32、48、64、128、256 像素七档，32 位 RGBA。

图标已用于 Tauri 管理中心的窗口与程序资源；Office 内的原生加载项保持独立，不嵌入 WebView2。

## Generation prompt

Built-in image_gen; transparent_background=true; generated 2026-10-27.

```text
Use case: logo-brand
Asset type: final Windows application icon for the independent open-source Office tab add-in "openOfficetab".
Primary request: Design one polished, distinctive, simple icon that communicates multiple document tabs in one window.
Subject: a compact unified mark composed of three overlapping upright document-tab shapes. Blue front document, green and warm orange tabs visible behind it, each with a clearly visible broad folder-tab-like top shoulder, arranged as one balanced continuous silhouette. Front blue document has two short clean white horizontal strokes suggesting document content. Keep shapes bold enough to remain legible at 32px.
Style/medium: crisp geometric vector-like bitmap, modern Windows desktop utility aesthetic, subtle tonal depth, precise smooth rounded corners. Professional and restrained.
Composition/framing: square 1024x1024 canvas, a single centered icon occupying about 82 percent of the canvas, equal transparent padding on all sides. View straight on, no perspective.
Color palette: saturated cobalt blue, deep emerald green, warm burnt orange; white content strokes.
Text: none.
Constraints: actual transparent background with alpha; transparent outside the mark; no surrounding tile, no background square, no external drop shadow, no checkerboard baked into pixels. No letters or text, no Microsoft logos, no existing Office app symbols, no mockup or presentation sheet, no multiple variants, no watermark. Deliver only the single finished icon.
```
