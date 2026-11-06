#!/usr/bin/env python3
"""Converte a saída do r200_teste (cor.ppm, prof.pgm, ir.pgm) em PNGs e numa imagem lado a lado (r200.png)."""
import cv2, numpy as np
cor = cv2.imread("cor.ppm")                        # o OpenCV já devolve BGR
prof = cv2.imread("prof.pgm", 0)
ir = cv2.imread("ir.pgm", 0)
pc = cv2.applyColorMap(prof, cv2.COLORMAP_JET); pc[prof == 0] = 0   # perto = vermelho, longe = azul, preto = sem medida
cv2.imwrite("cor.png", cor); cv2.imwrite("prof.png", pc); cv2.imwrite("ir.png", ir)
h = 360
lado = [cv2.resize(x, (h * x.shape[1] // x.shape[0], h)) for x in (cor, pc, cv2.cvtColor(ir, cv2.COLOR_GRAY2BGR))]
cv2.imwrite("r200.png", np.hstack(lado))
print("gerados: cor.png prof.png ir.png r200.png")
