# Generates the Emperor Reborn "spice eye" emblem (the blue eyes of Ibad) and every
# derived asset (launcher .ico, favicon, logo, social card, banner) from one place.
# Run: python icon/generate.py   (needs Pillow)

import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = os.path.dirname(os.path.abspath(__file__))
FONTS = "C:/Windows/Fonts/"
S = 1024  # master working size

BG_TOP   = (26, 32, 56)
BG_BOT   = (10, 12, 22)
EYE_GLOW = (66, 150, 255)
SCLERA_T = (26, 56, 126)
SCLERA_B = (10, 26, 70)
IRIS_IN  = (170, 228, 255)
IRIS_OUT = (24, 74, 168)
PUPIL    = (6, 10, 24)
LID_GOLD = (230, 180, 98)
SAND_TEXT = (244, 206, 138)

def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))

def vgradient(size, top, bottom):
    img = Image.new("RGBA", (size, size))
    px = img.load()
    for y in range(size):
        c = lerp(top, bottom, y / (size - 1)) + (255,)
        for x in range(size):
            px[x, y] = c
    return img

def glow(size, cx, cy, r, color, max_a=150, steps=160):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(steps, 0, -1):
        t = i / steps
        rr = r * t
        a = int(max_a * (1 - t) ** 1.7)
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=color + (a,))
    return img.filter(ImageFilter.GaussianBlur(size / 90))

def disc(size, cx, cy, r, c_in, c_out, steps=320):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(steps, 0, -1):
        t = i / steps
        rr = r * t
        col = lerp(c_out, c_in, 1 - t) + (255,)
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=col)
    return img

def almond_pts(cx, cy, a, h, n=160):
    # horizontal lens (eye) shape, pointed at left/right; a=half width, h=half height
    Rc = (a * a / h + h) / 2.0
    d = (a * a / h - h) / 2.0
    top, bot = [], []
    for i in range(n + 1):
        xt = cx - a + (2 * a) * i / n
        top.append((xt, (cy + d) - math.sqrt(max(0.0, Rc * Rc - (xt - cx) ** 2))))
        xb = cx + a - (2 * a) * i / n
        bot.append((xb, (cy - d) + math.sqrt(max(0.0, Rc * Rc - (xb - cx) ** 2))))
    return top + bot

def emblem(shape="square"):
    cx = cy = S / 2
    a, h = S * 0.37, S * 0.225
    iris_r, pupil_r = S * 0.205, S * 0.088

    img = vgradient(S, BG_TOP, BG_BOT)
    img = Image.alpha_composite(img, glow(S, cx, cy, S * 0.42, EYE_GLOW, max_a=130))

    pts = almond_pts(cx, cy, a, h)
    amask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(amask).polygon(pts, fill=255)

    content = vgradient(S, SCLERA_T, SCLERA_B)
    content = Image.alpha_composite(content, disc(S, cx, cy, iris_r, IRIS_IN, IRIS_OUT))
    # iris fibres
    fib = Image.new("RGBA", (S, S), (0, 0, 0, 0)); fd = ImageDraw.Draw(fib)
    nf = 80
    for k in range(nf):
        ang = math.radians(k * 360.0 / nf + 6 * math.sin(k * 1.3))
        r0 = pupil_r * 1.04; r1 = iris_r * (0.93 + 0.05 * math.sin(k * 2.1))
        col = (210, 240, 255) if k % 2 == 0 else (18, 52, 130)
        fd.line([(cx + math.cos(ang) * r0, cy + math.sin(ang) * r0),
                 (cx + math.cos(ang) * r1, cy + math.sin(ang) * r1)],
                fill=col + (90,), width=max(2, int(S * 0.004)))
    content = Image.alpha_composite(content, fib.filter(ImageFilter.GaussianBlur(S / 220)))
    cd = ImageDraw.Draw(content)
    cd.ellipse([cx - iris_r, cy - iris_r, cx + iris_r, cy + iris_r], outline=(12, 36, 96, 180), width=int(S * 0.012))
    cd.ellipse([cx - pupil_r, cy - pupil_r, cx + pupil_r, cy + pupil_r], fill=PUPIL + (255,))
    hr = S * 0.032
    cd.ellipse([cx - pupil_r * 0.35 - hr, cy - pupil_r * 0.35 - hr,
                cx - pupil_r * 0.35 + hr, cy - pupil_r * 0.35 + hr], fill=(225, 242, 255, 255))

    eye = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    eye.paste(content, (0, 0), amask)
    img = Image.alpha_composite(img, eye)
    ImageDraw.Draw(img).line(pts + [pts[0]], fill=LID_GOLD + (255,), width=int(S * 0.018), joint="curve")

    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    if shape == "square":
        md.rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.16), fill=255)
    else:
        m = int(S * 0.03)
        md.ellipse([m, m, S - 1 - m, S - 1 - m], fill=255)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    if shape == "badge":
        m = int(S * 0.03)
        ImageDraw.Draw(out).ellipse([m, m, S - 1 - m, S - 1 - m], outline=LID_GOLD + (255,), width=int(S * 0.018))
    return out

def fit_font(path, text, max_w, start):
    s = start
    while s > 8:
        f = ImageFont.truetype(path, s)
        if f.getbbox(text)[2] <= max_w:
            return f
        s -= 2
    return ImageFont.truetype(path, 8)

def save_png(im, name):
    im.save(os.path.join(OUT, name)); print("wrote", name, im.size)

sq = emblem("square")
badge = emblem("badge")
save_png(sq.resize((512, 512), Image.LANCZOS), "icon-512.png")
save_png(sq.resize((256, 256), Image.LANCZOS), "icon-256.png")
save_png(badge.resize((512, 512), Image.LANCZOS), "logo.png")
save_png(sq.resize((180, 180), Image.LANCZOS), "apple-touch-icon.png")
save_png(sq.resize((32, 32), Image.LANCZOS), "favicon-32.png")

ico_master = sq.resize((256, 256), Image.LANCZOS)
ico_master.save(os.path.join(OUT, "EmperorReborn.ico"),
                sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("wrote EmperorReborn.ico")
ico_master.save(os.path.join(OUT, "favicon.ico"), sizes=[(16, 16), (32, 32), (48, 48)])
print("wrote favicon.ico")

def card(w, h, name, title_px):
    c = Image.new("RGBA", (w, h)); px = c.load()
    for y in range(h):
        col = lerp((16, 20, 40), (24, 18, 30), y / (h - 1)) + (255,)
        for x in range(w):
            px[x, y] = col
    g = glow(max(w, h), int(w * 0.16), int(h * 0.46), w * 0.26, EYE_GLOW, max_a=120).crop((0, 0, w, h))
    c = Image.alpha_composite(c, g)
    bsize = int(h * 0.62)
    b = badge.resize((bsize, bsize), Image.LANCZOS)
    bx, by = int(h * 0.19), (h - bsize) // 2
    c.alpha_composite(b, (bx, by))
    d = ImageDraw.Draw(c)
    tx = bx + bsize + int(w * 0.04); tw = w - tx - int(w * 0.05)
    f_title = fit_font(FONTS + "impact.ttf", "EMPEROR REBORN", tw, title_px)
    f_tag = fit_font(FONTS + "segoeui.ttf", "Emperor: Battle for Dune in true 16:9 widescreen", tw, int(title_px * 0.34))
    f_foot = ImageFont.truetype(FONTS + "segoeui.ttf", int(title_px * 0.26))
    ty = int(h * 0.30)
    d.text((tx + 3, ty + 3), "EMPEROR REBORN", font=f_title, fill=(0, 0, 0, 150))
    d.text((tx, ty), "EMPEROR REBORN", font=f_title, fill=SAND_TEXT + (255,))
    ty2 = ty + f_title.getbbox("EMPEROR REBORN")[3] + int(h * 0.03)
    d.text((tx, ty2), "Emperor: Battle for Dune in true 16:9 widescreen", font=f_tag, fill=(214, 210, 204, 255))
    d.text((tx, h - int(h * 0.13)), "azmawee.github.io/EmperorReborn", font=f_foot, fill=(150, 186, 240, 255))
    save_png(c, name)

card(1200, 630, "og-card.png", 132)
card(1280, 384, "banner.png", 116)

def launcher_header(w, h, name):
    # opaque 24-bit BMP header for the Win32 launcher (no alpha; shown via SS_BITMAP)
    c = Image.new("RGBA", (w, h)); px = c.load()
    for y in range(h):
        col = lerp((18, 22, 42), (26, 20, 32), y / (h - 1)) + (255,)
        for x in range(w):
            px[x, y] = col
    c = Image.alpha_composite(c, glow(max(w, h), int(h * 0.55), int(h * 0.5), h * 0.7, EYE_GLOW, max_a=110).crop((0, 0, w, h)))
    bsize = h - 16
    c.alpha_composite(badge.resize((bsize, bsize), Image.LANCZOS), (10, 8))
    d = ImageDraw.Draw(c)
    tx = 10 + bsize + 18; tw = w - tx - 14
    f_t = fit_font(FONTS + "impact.ttf", "EMPEROR REBORN", tw, int(h * 0.42))
    f_g = fit_font(FONTS + "segoeui.ttf", "Emperor: Battle for Dune in true 16:9 widescreen", tw, int(h * 0.165))
    ty = int(h * 0.20)
    d.text((tx + 2, ty + 2), "EMPEROR REBORN", font=f_t, fill=(0, 0, 0, 150))
    d.text((tx, ty), "EMPEROR REBORN", font=f_t, fill=SAND_TEXT + (255,))
    ty2 = ty + f_t.getbbox("EMPEROR REBORN")[3] + int(h * 0.06)
    d.text((tx, ty2), "Emperor: Battle for Dune in true 16:9 widescreen", font=f_g, fill=(206, 206, 212, 255))
    c.convert("RGB").save(os.path.join(OUT, name)); print("wrote", name, (w, h))

launcher_header(514, 118, "launcher-header.bmp")
print("done")
