from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
RENDER_DIR = ROOT / "tmp" / "pdfs" / "render"
pages = sorted(RENDER_DIR.glob("page-*.png"))

thumb_w = 300
thumb_h = 424
label_h = 24
gap = 16
cols = 4
rows = 2
sheet_w = (cols * thumb_w) + ((cols + 1) * gap)
sheet_h = (rows * (thumb_h + label_h)) + ((rows + 1) * gap)

font_path = Path(r"C:\Windows\Fonts\malgunbd.ttf")
font = ImageFont.truetype(str(font_path), 15)

for group_idx in range(0, len(pages), cols * rows):
    group = pages[group_idx : group_idx + (cols * rows)]
    sheet = Image.new("RGB", (sheet_w, sheet_h), "#D8DEE3")
    draw = ImageDraw.Draw(sheet)
    for idx, page_path in enumerate(group):
        row = idx // cols
        col = idx % cols
        x = gap + col * (thumb_w + gap)
        y = gap + row * (thumb_h + label_h + gap)
        with Image.open(page_path) as img:
            rgb = img.convert("RGB")
            rgb.thumbnail((thumb_w, thumb_h), Image.Resampling.LANCZOS)
            px = x + (thumb_w - rgb.width) // 2
            py = y + (thumb_h - rgb.height) // 2
            sheet.paste(rgb, (px, py))
        label = f"Page {group_idx + idx + 1}"
        bbox = draw.textbbox((0, 0), label, font=font)
        tx = x + (thumb_w - (bbox[2] - bbox[0])) // 2
        draw.text((tx, y + thumb_h + 3), label, fill="#18324A", font=font)

    out = RENDER_DIR / f"contact-{(group_idx // (cols * rows)) + 1}.png"
    sheet.save(out, "PNG")
    print(out)
