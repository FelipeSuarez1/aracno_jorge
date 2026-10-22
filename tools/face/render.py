#!/usr/bin/env python3
"""Gera sprites do rosto, fonte bitmap e referências de teste (S04).

Sem argumentos: escreve
  firmware/src/face/sprites.h   7 expressões × 2 quadros × 5 camadas, RGB565
  firmware/src/face/font16.h    fonte 8×16, ASCII 32–126 + acentos pt-BR
  tests/ref/*.ppm               composições de referência (P6, RGB888 ← RGB565)
  tools/face/samples/*.png      amostra de cada expressão
  tools/face/HASHES.txt         sha256 de tudo que foi gerado

--compose EXPR QUADRO INTENSIDADE "frase" saida.ppm
  replica Canvas/Face (firmware/src/face/) em Python e grava um PPM.

Determinismo: fonte TrueType fixada em tools/face/fonts/ (Liberation Mono,
SIL OFL), Pillow pinado em requirements.txt; o desenho usa só inteiros e
redução por média (Image.reduce) — mesma entrada → mesmos bytes.

Expressões: variáveis do espaço de desenho de Kalegina et al., "Characterizing
the Design Space of Rendered Robot Faces", HRI 2018 (docs/40-Fontes/
fontes-externas.md): tamanho/altura dos olhos, presença e ângulo das
sobrancelhas, forma da boca. O mapeamento variável → emoção abaixo é
[INFERÊNCIA] (o artigo caracteriza o espaço, não prescreve emoções); o estudo
de reconhecimento [LAB] da S04 é quem valida.
"""
import argparse
import hashlib
import os
import sys

from PIL import Image, ImageDraw, ImageFont, features

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FONT_PATH = os.path.join(HERE, "fonts", "LiberationMono-Regular.ttf")
SPRITES_H = os.path.join(ROOT, "firmware", "src", "face", "sprites.h")
FONT16_H = os.path.join(ROOT, "firmware", "src", "face", "font16.h")
REF_DIR = os.path.join(ROOT, "tests", "ref")
SAMPLES_DIR = os.path.join(HERE, "samples")
HASHES_TXT = os.path.join(HERE, "HASHES.txt")

# ---- constantes espelhadas de canvas.h / face.h ----------------------------
W, H, FACE_H = 320, 240, 160
GLYPH_W, GLYPH_H = 8, 16
BALLOON_Y, BALLOON_H = FACE_H, H - FACE_H
BALLOON_COLS, BALLOON_LINES = 38, 3
BALLOON_TEXT_X, BALLOON_TEXT_Y, BALLOON_LINE_PX = 8, FACE_H + 8, 20

EXPRS = ["neutro", "alegria", "medo", "raiva", "surpresa", "tristeza", "curiosidade"]
LAYERS = ["eye_l", "eye_r", "brow_l", "brow_r", "mouth"]

# ---- geometria do rosto (coordenadas do canvas, 1×) -------------------------
SS = 4                      # supersample para anti-aliasing
EYE_CX = (105, 215)
EYE_CY = 66
BROW_CY = 32
MOUTH_CX, MOUTH_CY = 160, 124
EYE_STROKE, PUPIL_MIN = 4, 3
BROW_STROKE = 6
MOUTH_STROKE = 5
BLINK_STROKE = 5

# eye=(largura, altura) · pupil=raio · brow=(comprimento, dy_interno (+ = ponta
# interna mais alta), elevação) ou (esq, dir) ou None · mouth=(tipo, ...):
#   ('line', largura) · ('arc', largura, curvatura; + = sorriso) ·
#   ('open', largura, altura)
PARAMS = {
    "neutro":      dict(color=(255, 255, 255), eye=(40, 28), pupil=8, brow=None,
                        mouth=("line", 60)),
    "alegria":     dict(color=(255, 220, 0),   eye=(40, 22), pupil=8, brow=None,
                        mouth=("arc", 96, 26)),
    "medo":        dict(color=(190, 80, 255),  eye=(44, 38), pupil=5, brow=(40, 12, 10),
                        mouth=("open", 30, 22)),
    "raiva":       dict(color=(255, 40, 40),   eye=(40, 18), pupil=8, brow=(44, -14, -4),
                        mouth=("arc", 70, -14)),
    "surpresa":    dict(color=(0, 230, 255),   eye=(46, 46), pupil=9, brow=(44, 0, 14),
                        mouth=("open", 36, 36)),
    "tristeza":    dict(color=(60, 120, 255),  eye=(40, 26), pupil=8, brow=(40, 10, 2),
                        mouth=("arc", 70, -18)),
    "curiosidade": dict(color=(60, 255, 120),  eye=(40, 30), pupil=8,
                        brow=((40, 0, 16), (40, -6, 0)), mouth=("arc", 50, 10)),
}

# ---- fonte: ASCII 32–126 + acentos pt-BR -----------------------------------
EXTRA_CHARS = "áàâãéêíóôõúçÁÉÊÍÓÚÇ"
FONT_PX = 13          # tamanho do TrueType que cabe no avanço de 8 px (0,6 em)
FONT_BASELINE = 12    # linha de base dentro da célula de 16 px


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rgb888(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


# ============================================================================
# Sprites
# ============================================================================

def _bezier(p0, p1, p2, n=32):
    pts = []
    for i in range(n + 1):
        t = i / n
        x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]
        y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]
        pts.append((round(x * SS), round(y * SS)))
    return pts


def _layer_image():
    return Image.new("L", (W * SS, FACE_H * SS), 0)


def draw_eye(p, side, blink):
    img = _layer_image()
    d = ImageDraw.Draw(img)
    cx, cy = EYE_CX[side], EYE_CY
    ew, eh = p["eye"]
    if blink:
        d.line([((cx - ew // 2) * SS, cy * SS), ((cx + ew // 2) * SS, cy * SS)],
               fill=255, width=BLINK_STROKE * SS)
        return img
    box = [(cx - ew // 2) * SS, (cy - eh // 2) * SS, (cx + ew // 2) * SS, (cy + eh // 2) * SS]
    d.ellipse(box, outline=255, width=EYE_STROKE * SS)
    r = max(p["pupil"], PUPIL_MIN)
    d.ellipse([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], fill=255)
    return img


def draw_brow(p, side):
    spec = p["brow"]
    if spec is None:
        return None
    if isinstance(spec[0], tuple):
        spec = spec[side]
    length, inner_dy, raise_ = spec
    cx = EYE_CX[side]
    y_in = BROW_CY - raise_ - inner_dy // 2
    y_out = BROW_CY - raise_ + inner_dy // 2
    if side == 0:
        x_out, x_in = cx - length // 2, cx + length // 2
    else:
        x_out, x_in = cx + length // 2, cx - length // 2
    img = _layer_image()
    d = ImageDraw.Draw(img)
    d.line([(x_out * SS, y_out * SS), (x_in * SS, y_in * SS)],
           fill=255, width=BROW_STROKE * SS)
    return img


def draw_mouth(p):
    img = _layer_image()
    d = ImageDraw.Draw(img)
    m = p["mouth"]
    cx, cy = MOUTH_CX, MOUTH_CY
    if m[0] == "line":
        w = m[1]
        d.line([((cx - w // 2) * SS, cy * SS), ((cx + w // 2) * SS, cy * SS)],
               fill=255, width=MOUTH_STROKE * SS)
    elif m[0] == "arc":
        w, b = m[1], m[2]
        p0 = (cx - w / 2, cy - b / 2)
        p2 = (cx + w / 2, cy - b / 2)
        p1 = (cx, cy + 3 * b / 2)
        d.line(_bezier(p0, p1, p2), fill=255, width=MOUTH_STROKE * SS, joint="curve")
    elif m[0] == "open":
        w, h = m[1], m[2]
        box = [(cx - w // 2) * SS, (cy - h // 2) * SS, (cx + w // 2) * SS, (cy + h // 2) * SS]
        d.ellipse(box, fill=80, outline=255, width=EYE_STROKE * SS)
    return img


def layer_to_sprite(img, color):
    """Reduz SS×, colore, recorta ao bbox. → (x, y, w, h, [rgb565]) ou None."""
    if img is None:
        return None
    small = img.reduce(SS)
    bbox = small.getbbox()
    if bbox is None:
        return None
    x0, y0, x1, y1 = bbox
    crop = small.crop(bbox)
    data = []
    cr, cg, cb = color
    for a in crop.tobytes():
        data.append(rgb565((cr * a + 127) // 255, (cg * a + 127) // 255, (cb * a + 127) // 255))
    return (x0, y0, x1 - x0, y1 - y0, data)


def build_sprites():
    """→ sprites[expr][frame][layer] = (x,y,w,h,data) | None."""
    out = {}
    for e in EXPRS:
        p = PARAMS[e]
        frames = []
        for frame in (0, 1):
            layers = [
                layer_to_sprite(draw_eye(p, 0, frame == 1), p["color"]),
                layer_to_sprite(draw_eye(p, 1, frame == 1), p["color"]),
                layer_to_sprite(draw_brow(p, 0), p["color"]),
                layer_to_sprite(draw_brow(p, 1), p["color"]),
                layer_to_sprite(draw_mouth(p), p["color"]),
            ]
            frames.append(layers)
        out[e] = frames
    return out


def write_sprites_h(sprites):
    uniq = {}        # (w,h,tuple(data)) -> nome
    arrays = []
    lines = []
    lines.append("// Gerado por tools/face/render.py — NÃO EDITAR. Ver tools/face/README.md.")
    lines.append("// Sprites RGB565 (uint16_t nativo) em .rodata; 7 expressões × 2 quadros")
    lines.append("// (0 = olhos abertos, 1 = piscando) × 5 camadas. Incluir só em face.cpp.")
    lines.append("#pragma once")
    lines.append('#include "face/canvas.h"')
    lines.append("")
    lines.append("namespace aracno {")
    lines.append("namespace face {")
    lines.append("")
    colors = ", ".join("0x%04X" % rgb565(*PARAMS[e]["color"]) for e in EXPRS)
    lines.append("// cor do traço por humor: " + ", ".join(EXPRS))
    lines.append("static const uint16_t EXPR_COLOR[%d] = { %s };" % (len(EXPRS), colors))
    lines.append("")
    table = {}
    for e in EXPRS:
        for f in (0, 1):
            for li, spr in enumerate(sprites[e][f]):
                if spr is None:
                    table[(e, f, li)] = None
                    continue
                x, y, w, h, data = spr
                key = (w, h, tuple(data))
                if key not in uniq:
                    name = "SPR_%d" % len(arrays)
                    uniq[key] = name
                    arrays.append((name, w, h, data))
                table[(e, f, li)] = (x, y, w, h, uniq[key])
    total = 0
    for name, w, h, data in arrays:
        total += len(data) * 2
        lines.append("static const uint16_t %s[%d] = {  // %dx%d" % (name, len(data), w, h))
        for i in range(0, len(data), w):
            row = data[i:i + w]
            lines.append("    " + ",".join("%d" % v for v in row) + ",")
        lines.append("};")
    lines.append("")
    lines.append("// %d bytes de pixels em %d arrays únicos" % (total, len(arrays)))
    lines.append("static const Sprite SPRITES[%d][2][%d] = {" % (len(EXPRS), len(LAYERS)))
    for e in EXPRS:
        lines.append("    {  // " + e)
        for f in (0, 1):
            lines.append("        {  // quadro %d" % f)
            for li, lname in enumerate(LAYERS):
                t = table[(e, f, li)]
                if t is None:
                    lines.append("            { 0, 0, 0, 0, 0 },  // %s ausente" % lname)
                else:
                    x, y, w, h, name = t
                    lines.append("            { %d, %d, %d, %d, %s },  // %s" % (x, y, w, h, name, lname))
            lines.append("        },")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace face")
    lines.append("}  // namespace aracno")
    with open(SPRITES_H, "w", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")
    return total, len(arrays)


# ============================================================================
# Fonte
# ============================================================================

def build_font():
    """→ lista ordenada de (codepoint, [16 bytes])."""
    font = ImageFont.truetype(FONT_PATH, FONT_PX)
    cps = list(range(32, 127)) + sorted(ord(c) for c in EXTRA_CHARS)
    glyphs = []
    for cp in cps:
        img = Image.new("L", (GLYPH_W, GLYPH_H), 0)
        d = ImageDraw.Draw(img)
        d.text((0, FONT_BASELINE), chr(cp), font=font, fill=255, anchor="ls")
        rows = []
        for y in range(GLYPH_H):
            byte = 0
            for x in range(GLYPH_W):
                if img.getpixel((x, y)) > 127:
                    byte |= 0x80 >> x
            rows.append(byte)
        glyphs.append((cp, rows))
    return glyphs


def write_font16_h(glyphs):
    lines = []
    lines.append("// Gerado por tools/face/render.py — NÃO EDITAR. Ver tools/face/README.md.")
    lines.append("// Fonte bitmap 8×16 monoespaçada (Liberation Mono %d px, SIL OFL)," % FONT_PX)
    lines.append("// 1 bit/pixel, MSB = coluna da esquerda. Índices 0..94 = ASCII 32..126;")
    lines.append("// depois os acentos pt-BR (FONT16_CP). Incluir só em canvas.cpp.")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("namespace aracno {")
    lines.append("namespace face {")
    lines.append("")
    lines.append("static const int FONT16_N = %d;" % len(glyphs))
    lines.append("static const int FONT16_ASCII_N = 95;")
    lines.append("static const uint16_t FONT16_CP[FONT16_N] = {")
    cps = [cp for cp, _ in glyphs]
    for i in range(0, len(cps), 16):
        lines.append("    " + ",".join("0x%04X" % c for c in cps[i:i + 16]) + ",")
    lines.append("};")
    lines.append("static const uint8_t FONT16_ROWS[FONT16_N][16] = {")
    for cp, rows in glyphs:
        label = chr(cp) if cp != 92 else "\\\\"   # evita '\' encerrando o comentário
        lines.append("    {" + ",".join("0x%02X" % b for b in rows) + "},  // U+%04X '%s'" % (cp, label))
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace face")
    lines.append("}  // namespace aracno")
    with open(FONT16_H, "w", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")


# ============================================================================
# Espelho de Canvas / Face (canvas.cpp, face.cpp)
# ============================================================================

class PyCanvas:
    def __init__(self, glyphs):
        self.px = [0] * (W * H)
        self.glyph = {cp: rows for cp, rows in glyphs}

    def clear(self, color):
        self.px = [color] * (W * H)

    def blit(self, spr, x, y):
        sx, sy, w, h, data = spr
        for r in range(h):
            dy = y + r
            if dy < 0 or dy >= H:
                continue
            for c in range(w):
                dx = x + c
                if dx < 0 or dx >= W:
                    continue
                self.px[dy * W + dx] = data[r * w + c]

    def blit_scaled(self, spr, x, y, scale_pct):
        if scale_pct >= 100:
            return self.blit(spr, x, y)
        scale_pct = max(scale_pct, 1)
        sx, sy, w, h, data = spr
        dw = max((w * scale_pct) // 100, 1)
        dh = max((h * scale_pct) // 100, 1)
        x0 = x + (w - dw) // 2
        y0 = y + (h - dh) // 2
        for r in range(dh):
            dy = y0 + r
            if dy < 0 or dy >= H:
                continue
            srow = ((r * h) // dh) * w
            for c in range(dw):
                dx = x0 + c
                if dx < 0 or dx >= W:
                    continue
                self.px[dy * W + dx] = data[srow + (c * w) // dw]

    def text(self, x, y, s, color):
        for ch in s:
            rows = self.glyph.get(ord(ch)) or self.glyph[ord("?")]
            for r in range(GLYPH_H):
                dy = y + r
                if dy < 0 or dy >= H:
                    continue
                bits = rows[r]
                for c in range(GLYPH_W):
                    if not bits & (0x80 >> c):
                        continue
                    dx = x + c
                    if dx < 0 or dx >= W:
                        continue
                    self.px[dy * W + dx] = color
            x += GLYPH_W

    def rect(self, x, y, w, h, color, fill):
        if w <= 0 or h <= 0:
            return
        for r in range(h):
            dy = y + r
            if dy < 0 or dy >= H:
                continue
            edge_row = r == 0 or r == h - 1
            for c in range(w):
                dx = x + c
                if dx < 0 or dx >= W:
                    continue
                if fill or edge_row or c == 0 or c == w - 1:
                    self.px[dy * W + dx] = color

    def to_ppm(self):
        body = bytearray()
        for v in self.px:
            body.extend(rgb888(v))
        return b"P6\n%d %d\n255\n" % (W, H) + bytes(body)

    def to_image(self):
        img = Image.new("RGB", (W, H))
        img.putdata([rgb888(v) for v in self.px])
        return img


def wrap_lines(text):
    """Espelho de face.cpp wrap_lines (colunas = code points)."""
    out = []
    cur = ""

    def flush():
        if len(out) < BALLOON_LINES:
            out.append(cur)

    for word in text.split(" "):
        if len(out) >= BALLOON_LINES:
            break
        if not word:
            continue
        if len(word) > BALLOON_COLS:
            if cur:
                flush()
                cur = ""
            while word and len(out) < BALLOON_LINES:
                chunk, word = word[:BALLOON_COLS], word[BALLOON_COLS:]
                cur = chunk
                if word:
                    flush()
                    cur = ""
            continue
        if not cur:
            cur = word
        elif len(cur) + 1 + len(word) <= BALLOON_COLS:
            cur += " " + word
        else:
            flush()
            cur = word
    if cur and len(out) < BALLOON_LINES:
        out.append(cur)
    return out


def intensity_scale(intensity):
    return 50 + min(intensity, 100) // 2


def compose(sprites, glyphs, expr, frame, intensity, phrase):
    """Espelho de Face::draw_face + Face::draw_balloon."""
    cv = PyCanvas(glyphs)
    cv.rect(0, 0, W, FACE_H, 0, True)
    scale = intensity_scale(intensity)
    for li, spr in enumerate(sprites[expr][frame]):
        if spr is None:
            continue
        if LAYERS[li].startswith("brow"):
            cv.blit(spr, spr[0], spr[1])
        else:
            cv.blit_scaled(spr, spr[0], spr[1], scale)
    cv.rect(0, BALLOON_Y, W, BALLOON_H, 0, True)
    lines = wrap_lines(phrase) if phrase else []
    if lines:
        color = rgb565(*PARAMS[expr]["color"])
        cv.rect(0, BALLOON_Y, W, BALLOON_H, color, False)
        for i, ln in enumerate(lines):
            cv.text(BALLOON_TEXT_X, BALLOON_TEXT_Y + i * BALLOON_LINE_PX, ln, color)
    return cv


# ============================================================================
# Referências e amostras
# ============================================================================

REF_PHRASES = [
    ("neutro", "Olá, eu sou a Jorge"),
    ("medo", "Está escuro aqui... vou me esconder e esperar você chegar perto!"),
    ("raiva", "Pneumoultramicroscopicossilicovulcanoconiótico! Não cabe."),
]
SAMPLE_PHRASES = {
    "neutro": "Olá, eu sou a Jorge",
    "alegria": "Que bom te ver!",
    "medo": "Quem está aí? Vou me esconder...",
    "raiva": "Sai daqui!",
    "surpresa": "Ué, apareceu alguém!",
    "tristeza": "Ninguém vem brincar comigo",
    "curiosidade": "O que é isso ali?",
}


def ref_cases():
    cases = []
    for e in EXPRS:
        for f in (0, 1):
            cases.append(("face_%s_%d.ppm" % (e, f), e, f, 100, ""))
    cases.append(("face_alegria_i50_0.ppm", "alegria", 0, 50, ""))
    for i, (e, phrase) in enumerate(REF_PHRASES, 1):
        cases.append(("balao_%d.ppm" % i, e, 0, 100, phrase))
    return cases


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        h.update(fh.read())
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--compose", nargs=5, metavar=("EXPR", "QUADRO", "INTENSIDADE", "FRASE", "SAIDA"))
    args = ap.parse_args()

    sprites = build_sprites()
    glyphs = build_font()

    if args.compose:
        expr, frame, inten, phrase, out = args.compose
        cv = compose(sprites, glyphs, expr, int(frame), int(inten), phrase)
        with open(out, "wb") as fh:
            fh.write(cv.to_ppm())
        print("escrito", out)
        return 0

    total, n_arrays = write_sprites_h(sprites)
    write_font16_h(glyphs)
    os.makedirs(REF_DIR, exist_ok=True)
    os.makedirs(SAMPLES_DIR, exist_ok=True)
    written = [SPRITES_H, FONT16_H]
    for name, e, f, inten, phrase in ref_cases():
        path = os.path.join(REF_DIR, name)
        with open(path, "wb") as fh:
            fh.write(compose(sprites, glyphs, e, f, inten, phrase).to_ppm())
        written.append(path)
    for e in EXPRS:
        path = os.path.join(SAMPLES_DIR, e + ".png")
        compose(sprites, glyphs, e, 0, 100, SAMPLE_PHRASES[e]).to_image().save(path, optimize=True)
        written.append(path)
    with open(HASHES_TXT, "w", newline="\n") as fh:
        fh.write("# sha256 dos artefatos gerados por tools/face/render.py\n")
        fh.write("# Pillow %s, freetype %s, fonte %s\n" % (
            Image.__version__, features.version("freetype2"), sha256(FONT_PATH)))
        for p in written:
            fh.write("%s  %s\n" % (sha256(p), os.path.relpath(p, ROOT)))
    print("sprites: %d bytes de pixels em %d arrays únicos; sprites.h %d B; font16.h %d B" % (
        total, n_arrays, os.path.getsize(SPRITES_H), os.path.getsize(FONT16_H)))
    print("refs: %d em %s; amostras em %s" % (len(ref_cases()), REF_DIR, SAMPLES_DIR))
    return 0


if __name__ == "__main__":
    sys.exit(main())
