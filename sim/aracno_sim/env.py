"""Ambiente gymnasium 2D da aranha (S05).

Cômodo retangular com paredes, obstáculos (caixas), 0–2 buracos (borda de mesa/degrau),
1–3 fontes de luz (mapa de lux com sombra), uma pessoa roteirizada (entra, para, sai a
0,5–1,5 m/s), bateria decrescente e sensores com ruído parametrizado por `noise`.

Observação = índice de estado discretizado (aracno_sim.discretize.state_index), espaço
Discrete(960). A `Obs` crua (espelho de types.h) e grandezas internas vão em `info`.
Ações = as 9 de types.h.

Unidades: mm, ms, graus internos em radianos. Passo de decisão DT_MS = 200 ms
[INFERÊNCIA: ordem de um ciclo de marcha do CSpider; o laço real é 20 ms (S03)].

Todos os números marcados [A MEDIR] são chutes a substituir pela caracterização de S03/S05
(docs/30-Medicoes/). Os marcados [INFERÊNCIA] vêm de datasheet/doc sem medição própria.
"""

import math
from typing import Any, Dict, Optional, Tuple

import gymnasium as gym
import numpy as np
from gymnasium import spaces

from .discretize import N_STATES, state_index
from .types import Action, N_ACTIONS, N_SENSORS, ALL_INSTALLED, Obs

DT_MS = 200
DT_S = DT_MS / 1000.0

# --- Dinâmica da aranha -------------------------------------------------------------------
SPIDER_RADIUS_MM = 150.0       # [INFERÊNCIA] raio do corpo com patas recolhidas (Spider ~ 30 cm)
HEAD_OFFSET_MM = 120.0         # [INFERÊNCIA] cabeça à frente do centro do corpo
FLOOR_LOOKAHEAD_MM = 150.0     # [INFERÊNCIA] ponto do chão visto pelo VL53L1X inferior
V_FORWARD_MM_S = 100.0         # [A MEDIR] velocidade da marcha tripé (S03)
TURN_RAD_S = math.radians(45)  # [A MEDIR] taxa de giro de CSpider::RotatelLeft/Right (S03)
LUNGE_MM = 300.0               # [A MEDIR] deslocamento de um bote (S03, frenagem)
LUNGE_VALID_MM = 600.0         # bote válido: pessoa a < 60 cm (S00/S05)
LUNGE_VALID_HALF_ANGLE = math.radians(30)  # [INFERÊNCIA] cone frontal do bote
HEAD_PAN_STEP = math.radians(20)           # types.h: HEAD_LEFT/RIGHT = ±20°
FLOOR_HEIGHT_MM = 80.0         # [A MEDIR] leitura do VL53L1X do chão com chão presente

# --- Pessoa -------------------------------------------------------------------------------
PERSON_RADIUS_MM = 200.0
PERSON_FOV_HALF = math.radians(60)   # [INFERÊNCIA] campo de visão simples
PERSON_SIGHT_MM = 3000.0             # [INFERÊNCIA] alcance em que nota a aranha
VISIBLE_LUX_STANDING = 20.0          # [INFERÊNCIA] aranha em pé é vista se lux no corpo >= 20
VISIBLE_LUX_CROUCHED = 80.0          # [INFERÊNCIA] abaixada só é vista com mais luz

# --- PIR HC-SR501 -------------------------------------------------------------------------
PIR_HALF_ANGLE = math.radians(55)    # doc do módulo HC-SR501: cone < 110° [INFERÊNCIA]
PIR_RANGE_MM = 4000.0                # doc do módulo: 3–7 m; usado 4 m [A MEDIR]

# --- VL53L1X ------------------------------------------------------------------------------
DIST_MAX_MM = 4000.0                 # ST DS12385: modo longo até 4 m [INFERÊNCIA]
DIST_FOV_HALF = math.radians(13.5)   # ST DS12385: campo de visão 27°
STIMULUS_DIST_MM = 1000              # contrato: objeto < 1 m à frente conta como estímulo

# --- Recompensa ---------------------------------------------------------------------------
DARK_LUX = 50.0                      # "escuro" = nível lux <= 1 (penumbra) [INFERÊNCIA]
R_DARK = 0.05                        # por passo no escuro
R_APPROACH_PER_MM = 0.002            # por mm de aproximação sem ser vista
R_SEEN = -0.05                       # por passo em que a pessoa vê a aranha
# Bote: o evento-objetivo do demo. Com +10/−2 a política ótima nunca dava o bote (com estes
# sensores só ~5–8 % das leituras "PIR e < 600 mm" são pessoa; o resto é parede) — ver
# relatório S05. +20/−1 mantém o bote positivo em expectativa para quem espreita bem.
R_LUNGE_OK = 20.0
R_LUNGE_FAR = -1.0
R_COLLISION = -1.0                   # ao encostar (início do contato)
R_COLLISION_HOLD = -0.2              # por passo continuando a empurrar/raspar
R_EDGE = -5.0
R_BATTERY_LOW = -0.02                # por passo com bateria < BATTERY_LOW_PCT
BATTERY_LOW_PCT = 20.0

DEFAULT_NOISE: Dict[str, float] = {
    # BH1750: ROHM, precisão ±20 %; ruído relativo entre amostras [A MEDIR]
    "lux_rel_sigma": 0.05,
    # VL53L1X: ST DS12385 dá erro típico ±20 mm (modo longo, 200 mm..4 m) [A MEDIR por superfície × lux]
    "dist_sigma_mm": 10.0,
    "dist_dropout_p": 0.02,          # leitura inválida (0) [A MEDIR]
    "floor_sigma_mm": 3.0,           # [A MEDIR]
    "pir_hold_ms": 2500.0,           # HC-SR501: tempo de retenção ajustável 0,3 s–5 min [A MEDIR]
    "pir_miss_p": 0.05,              # [A MEDIR]
    "pir_false_p": 0.002,            # [A MEDIR]
}

# Consumo por ação em % de bateria por passo [A MEDIR] (ensaio de autonomia, S00 item 2).
BATTERY_DRAIN_PCT = {
    Action.STOP: 0.005,
    Action.CROUCH: 0.005,
    Action.HEAD_LEFT: 0.005,
    Action.HEAD_RIGHT: 0.005,
    Action.FORWARD: 0.03,
    Action.BACKWARD: 0.03,
    Action.TURN_LEFT: 0.03,
    Action.TURN_RIGHT: 0.03,
    Action.LUNGE: 0.3,
}


def _wrap(a: float) -> float:
    while a > math.pi:
        a -= 2 * math.pi
    while a <= -math.pi:
        a += 2 * math.pi
    return a


def _seg_hits_rect(x0, y0, x1, y1, rect) -> bool:
    """Segmento (x0,y0)-(x1,y1) intersecta o retângulo (slab method)."""
    rx0, ry0, rx1, ry1 = rect
    dx, dy = x1 - x0, y1 - y0
    tmin, tmax = 0.0, 1.0
    for p, d, lo, hi in ((x0, dx, rx0, rx1), (y0, dy, ry0, ry1)):
        if abs(d) < 1e-9:
            if p < lo or p > hi:
                return False
        else:
            t1, t2 = (lo - p) / d, (hi - p) / d
            if t1 > t2:
                t1, t2 = t2, t1
            tmin = max(tmin, t1)
            tmax = min(tmax, t2)
            if tmin > tmax:
                return False
    return True


def _ray_rect(px, py, dx, dy, rect) -> float:
    """Distância do raio (origem fora do retângulo) até o retângulo; inf se não atinge."""
    rx0, ry0, rx1, ry1 = rect
    tmin, tmax = 0.0, math.inf
    for p, d, lo, hi in ((px, dx, rx0, rx1), (py, dy, ry0, ry1)):
        if abs(d) < 1e-9:
            if p < lo or p > hi:
                return math.inf
        else:
            t1, t2 = (lo - p) / d, (hi - p) / d
            if t1 > t2:
                t1, t2 = t2, t1
            tmin = max(tmin, t1)
            tmax = min(tmax, t2)
            if tmin > tmax:
                return math.inf
    return tmin


def _ray_circle(px, py, dx, dy, cx, cy, r) -> float:
    fx, fy = px - cx, py - cy
    b = 2 * (fx * dx + fy * dy)
    c = fx * fx + fy * fy - r * r
    disc = b * b - 4 * c
    if disc < 0:
        return math.inf
    t = (-b - math.sqrt(disc)) / 2
    return t if t >= 0 else math.inf


def _in_rect(x, y, rect) -> bool:
    return rect[0] <= x <= rect[2] and rect[1] <= y <= rect[3]


class AracnoEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, noise: Optional[Dict[str, float]] = None, max_steps: int = 400):
        self.noise = dict(DEFAULT_NOISE)
        if noise:
            self.noise.update(noise)
        self.max_steps = max_steps
        self.observation_space = spaces.Discrete(N_STATES)
        self.action_space = spaces.Discrete(N_ACTIONS)
        self._layout_ready = False

    # ---------------------------------------------------------------- layout -------------
    def _make_layout(self):
        rng = self.np_random
        self.W = float(rng.integers(3000, 5001))
        self.H = float(rng.integers(2500, 4001))
        self.ambient_lux = float(rng.uniform(1.0, 5.0))

        self.obstacles = []
        for _ in range(int(rng.integers(1, 5))):
            w = float(rng.integers(300, 901))
            h = float(rng.integers(300, 901))
            x = float(rng.uniform(0, self.W - w))
            y = float(rng.uniform(0, self.H - h))
            self.obstacles.append((x, y, x + w, y + h))

        self.holes = []
        for _ in range(int(rng.integers(0, 3))):
            w = float(rng.integers(300, 801))
            h = float(rng.integers(300, 801))
            x = float(rng.uniform(0, self.W - w))
            y = float(rng.uniform(0, self.H - h))
            self.holes.append((x, y, x + w, y + h))

        # Luzes: lux(p) = ambiente + k·Σ w_i/(1+(d_i/1000)^2), com w_i relativo e k escolhido
        # para que uma fração alvo U(0,3; 0,7) de uma grade do cômodo fique <= DARK_LUX
        # [INFERÊNCIA: cômodo com luz num canto; sem isso o cômodo fica ~todo claro ou ~todo
        # escuro e nem "esconder" nem "70 % do tempo no escuro" têm sentido].
        lights = [(float(rng.uniform(0, self.W)), float(rng.uniform(0, self.H)), float(rng.uniform(1.0, 3.0)))
                  for _ in range(int(rng.integers(1, 4)))]
        self.lights = lights
        dark_target = float(rng.uniform(0.3, 0.7))
        ks = []
        for gx in range(16):
            for gy in range(12):
                x = (gx + 0.5) * self.W / 16
                y = (gy + 0.5) * self.H / 12
                contrib = self._lux_at(x, y) - self.ambient_lux
                ks.append((DARK_LUX - self.ambient_lux) / contrib if contrib > 0 else math.inf)
        ks.sort()
        k = ks[int(dark_target * len(ks))]
        if not math.isfinite(k):
            k = 100.0
        self.lights = [(lx, ly, w * k) for lx, ly, w in lights]

        # Aranha: posição livre (fora de obstáculos/buracos)
        for _ in range(100):
            x = float(rng.uniform(SPIDER_RADIUS_MM, self.W - SPIDER_RADIUS_MM))
            y = float(rng.uniform(SPIDER_RADIUS_MM, self.H - SPIDER_RADIUS_MM))
            if not self._collides(x, y) and not self._in_hole(x, y):
                break
        self.sx, self.sy = x, y
        self.heading = float(rng.uniform(-math.pi, math.pi))
        self.pan = 0.0
        self.crouched = False

        # Pessoa: porta de entrada e saída na borda; para a 0,6–2 m da aranha (vem olhar a
        # aranha, como no roteiro de S00) [INFERÊNCIA]
        self.p_enter_ms = int(rng.integers(3000, 20001))
        self.p_speed = float(rng.uniform(500.0, 1500.0))  # mm/s (0,5–1,5 m/s)
        self.p_stop_ms = int(rng.integers(3000, 15001))
        self.p_door_in = self._door_point()
        self.p_door_out = self._door_point()
        for _ in range(20):
            ang = float(rng.uniform(-math.pi, math.pi))
            rad = float(rng.uniform(600.0, 2000.0))
            wp = (min(max(self.sx + rad * math.cos(ang), 300.0), self.W - 300.0),
                  min(max(self.sy + rad * math.sin(ang), 300.0), self.H - 300.0))
            if not any(_seg_hits_rect(*self.p_door_in, *wp, r) or _seg_hits_rect(*wp, *self.p_door_out, r)
                       for r in self.obstacles):
                break
        self.p_wp = wp
        self.battery_pct = float(rng.uniform(30.0, 100.0))

    def _door_point(self):
        rng = self.np_random
        side = int(rng.integers(0, 4))
        if side == 0:
            return (float(rng.uniform(300, self.W - 300)), 0.0)
        if side == 1:
            return (float(rng.uniform(300, self.W - 300)), self.H)
        if side == 2:
            return (0.0, float(rng.uniform(300, self.H - 300)))
        return (self.W, float(rng.uniform(300, self.H - 300)))

    # ---------------------------------------------------------------- geometria ----------
    def _collides(self, x, y) -> bool:
        r = SPIDER_RADIUS_MM
        if x < r or y < r or x > self.W - r or y > self.H - r:
            return True
        for ox0, oy0, ox1, oy1 in self.obstacles:
            if ox0 - r <= x <= ox1 + r and oy0 - r <= y <= oy1 + r:
                return True
        return False

    def _in_hole(self, x, y) -> bool:
        return any(_in_rect(x, y, h) for h in self.holes)

    def _los(self, x0, y0, x1, y1) -> bool:
        return not any(_seg_hits_rect(x0, y0, x1, y1, r) for r in self.obstacles)

    def _lux_at(self, x, y, nx=None, ny=None) -> float:
        """Lux em (x,y). Com normal (nx,ny) aplica resposta angular 0,3+0,7·cos [INFERÊNCIA,
        BH1750 tem campo amplo]; sem normal, lux "do ponto" (usado na recompensa)."""
        lux = self.ambient_lux
        for lx, ly, inten in self.lights:
            if not self._los(x, y, lx, ly):
                continue
            dx, dy = lx - x, ly - y
            d2 = dx * dx + dy * dy
            f = inten / (1.0 + d2 / 1.0e6)
            if nx is not None:
                d = math.sqrt(d2) or 1.0
                cos = (dx * nx + dy * ny) / d
                f *= 0.3 + 0.7 * max(0.0, cos)
            lux += f
        return lux

    def _ray_front(self) -> float:
        """Distância do VL53L1X frontal (cabeça) até parede/obstáculo/pessoa, em mm.
        O sensor tem campo de visão de 27° (ST DS12385) e reporta o alvo mais próximo:
        modelado como o mínimo de 3 raios em -13,5°, 0 e +13,5° [INFERÊNCIA]."""
        px = self.sx + HEAD_OFFSET_MM * math.cos(self.heading)
        py = self.sy + HEAD_OFFSET_MM * math.sin(self.heading)
        a0 = self.heading + self.pan
        best = math.inf
        for da in (-DIST_FOV_HALF, 0.0, DIST_FOV_HALF):
            best = min(best, self._ray(px, py, a0 + da))
        return max(best, 0.0)

    def _ray(self, px, py, a) -> float:
        dx, dy = math.cos(a), math.sin(a)
        best = math.inf
        # paredes
        if dx > 0:
            best = min(best, (self.W - px) / dx)
        elif dx < 0:
            best = min(best, (0 - px) / dx)
        if dy > 0:
            best = min(best, (self.H - py) / dy)
        elif dy < 0:
            best = min(best, (0 - py) / dy)
        for r in self.obstacles:
            best = min(best, _ray_rect(px, py, dx, dy, r))
        if self.p_present:
            best = min(best, _ray_circle(px, py, dx, dy, self.px, self.py, PERSON_RADIUS_MM))
        return max(best, 0.0)

    # ---------------------------------------------------------------- pessoa -------------
    def _update_person(self):
        t = self.t_ms
        self.p_moved = False
        if t < self.p_enter_ms:
            self.p_present = False
            return
        if not self.p_present:
            self.p_present = True
            self.px, self.py = self.p_door_in
            self.p_phase = 0  # 0 indo ao ponto, 1 parada, 2 saindo
            self.p_phase_ms = 0
            self.p_face = math.atan2(self.p_wp[1] - self.py, self.p_wp[0] - self.px)
        if self.p_phase == 1:
            self.p_phase_ms += DT_MS
            if self.p_phase_ms >= self.p_stop_ms:
                self.p_phase = 2
                self.p_face = math.atan2(self.p_door_out[1] - self.py, self.p_door_out[0] - self.px)
            return
        target = self.p_wp if self.p_phase == 0 else self.p_door_out
        dx, dy = target[0] - self.px, target[1] - self.py
        d = math.hypot(dx, dy)
        step = self.p_speed * DT_S
        if d <= step:
            self.px, self.py = target
            if self.p_phase == 0:
                self.p_phase = 1
            else:
                self.p_present = False
                self.p_left_ms = t
        else:
            self.px += dx / d * step
            self.py += dy / d * step
            self.p_face = math.atan2(dy, dx)
        self.p_moved = True

    def _person_sees_spider(self) -> bool:
        if not self.p_present:
            return False
        dx, dy = self.sx - self.px, self.sy - self.py
        d = math.hypot(dx, dy)
        if d > PERSON_SIGHT_MM:
            return False
        if abs(_wrap(math.atan2(dy, dx) - self.p_face)) > PERSON_FOV_HALF:
            return False
        if not self._los(self.px, self.py, self.sx, self.sy):
            return False
        thr = VISIBLE_LUX_CROUCHED if self.crouched else VISIBLE_LUX_STANDING
        return self._lux_at(self.sx, self.sy) >= thr

    def _pir_detects(self) -> bool:
        if not self.p_present or not self.p_moved:
            return False
        dx, dy = self.px - self.sx, self.py - self.sy
        d = math.hypot(dx, dy)
        if d > PIR_RANGE_MM:
            return False
        if abs(_wrap(math.atan2(dy, dx) - self.heading)) > PIR_HALF_ANGLE:
            return False
        return self._los(self.sx, self.sy, self.px, self.py)

    def _person_dist(self) -> float:
        return math.hypot(self.px - self.sx, self.py - self.sy) if self.p_present else math.inf

    # ---------------------------------------------------------------- sensores -----------
    def _sense(self) -> Obs:
        rng = self.np_random
        nz = self.noise
        a = self.heading + self.pan
        hx = self.sx + HEAD_OFFSET_MM * math.cos(self.heading)
        hy = self.sy + HEAD_OFFSET_MM * math.sin(self.heading)
        # BH1750 esq/dir: 40 mm de cada lado da cabeça, normais a ±30° do olhar [INFERÊNCIA]
        lux = []
        for sgn in (+1, -1):
            sxp = hx + 40.0 * math.cos(a + sgn * math.pi / 2)
            syp = hy + 40.0 * math.sin(a + sgn * math.pi / 2)
            n_ang = a + sgn * math.radians(30)
            v = self._lux_at(sxp, syp, math.cos(n_ang), math.sin(n_ang))
            v *= 1.0 + nz["lux_rel_sigma"] * float(rng.standard_normal())
            lux.append(int(min(65535, max(0, round(v)))))

        d = self._ray_front()
        if d > DIST_MAX_MM or float(rng.random()) < nz["dist_dropout_p"]:
            dist = 0
        else:
            dist = int(min(65535, max(1, round(d + nz["dist_sigma_mm"] * float(rng.standard_normal())))))

        fx = self.sx + FLOOR_LOOKAHEAD_MM * math.cos(self.heading)
        fy = self.sy + FLOOR_LOOKAHEAD_MM * math.sin(self.heading)
        if self._in_hole(fx, fy) or not (0 <= fx <= self.W and 0 <= fy <= self.H):
            floor = 0  # nada sob o sensor: sem retorno [INFERÊNCIA]
        else:
            floor = int(max(1, round(FLOOR_HEIGHT_MM + nz["floor_sigma_mm"] * float(rng.standard_normal()))))

        detected = self._pir_detects() and float(rng.random()) >= nz["pir_miss_p"]
        if detected or float(rng.random()) < nz["pir_false_p"]:
            self.pir_until_ms = self.t_ms + int(nz["pir_hold_ms"])
        pir = 1 if self.t_ms < self.pir_until_ms else 0

        if pir or (0 < dist < STIMULUS_DIST_MM):
            self.t_since_ms = 0
        return Obs(
            t_ms=self.t_ms, lux_l=lux[0], lux_r=lux[1], dist_front_mm=dist, dist_floor_mm=floor,
            pir=pir, estop=0, acc_x_mg=0, acc_y_mg=0, acc_z_mg=1000,
            t_since_stimulus_ms=self.t_since_ms, age_ms=[0] * N_SENSORS, installed=ALL_INSTALLED,
        )

    # ---------------------------------------------------------------- gym API ------------
    def reset(self, *, seed: Optional[int] = None, options: Optional[dict] = None) -> Tuple[int, dict]:
        super().reset(seed=seed)
        self._make_layout()
        self._layout_ready = True
        self.t_ms = 0
        self.steps = 0
        self.p_present = False
        self.p_moved = False
        self.p_left_ms = None
        self.pir_until_ms = 0
        self.t_since_ms = 20000  # sem estímulo há muito (nível 2)
        self.dark_steps = 0
        self.collided = False
        self.in_contact = False
        self.fell = False
        self.lunged_ok = False
        self._update_person()
        self.obs = self._sense()
        self.prev_person_dist = self._person_dist()
        return state_index(self.obs), self._info(0.0)

    def _info(self, reward: float) -> Dict[str, Any]:
        n = max(1, self.steps)
        return {
            "obs": self.obs,
            "x_mm": self.sx, "y_mm": self.sy, "heading_rad": self.heading, "pan_rad": self.pan,
            "crouched": self.crouched,
            "person_present": self.p_present, "person_dist_mm": self._person_dist(),
            "battery_pct": self.battery_pct,
            "dark_fraction": self.dark_steps / n,
            "collided": self.collided, "fell": self.fell, "lunged_ok": self.lunged_ok,
            "success": self.lunged_ok and not self.collided and not self.fell and self.dark_steps / n >= 0.7,
            "reward": reward,
        }

    def step(self, action):
        assert self._layout_ready, "chame reset() antes de step()"
        a = int(action)
        assert 0 <= a < N_ACTIONS
        self.steps += 1
        self.t_ms += DT_MS
        self.t_since_ms = min(self.t_since_ms + DT_MS, 0xFFFFFFFF)
        reward = 0.0
        terminated = False

        self._update_person()

        # Movimento
        nx, ny, nh = self.sx, self.sy, self.heading
        # Cabeça: S06 faz varredura/seguimento reativo por conta própria e o pan não está em
        # Obs. No simulador HEAD_LEFT/RIGHT são uma "olhada": o ToF é amostrado a ±20° só
        # neste passo e a cabeça volta ao centro [INFERÊNCIA].
        self.pan = 0.0
        if a in (Action.FORWARD, Action.BACKWARD):
            s = V_FORWARD_MM_S * DT_S * (1 if a == Action.FORWARD else -1)
            nx += s * math.cos(nh)
            ny += s * math.sin(nh)
            self.crouched = False
        elif a == Action.TURN_LEFT:
            nh = _wrap(nh + TURN_RAD_S * DT_S)
            self.crouched = False
        elif a == Action.TURN_RIGHT:
            nh = _wrap(nh - TURN_RAD_S * DT_S)
            self.crouched = False
        elif a == Action.CROUCH:
            self.crouched = True
        elif a == Action.LUNGE:
            self.crouched = False
            valid = False
            if self.p_present:
                dx, dy = self.px - self.sx, self.py - self.sy
                d = math.hypot(dx, dy) - PERSON_RADIUS_MM
                valid = d <= LUNGE_VALID_MM and abs(_wrap(math.atan2(dy, dx) - nh)) <= LUNGE_VALID_HALF_ANGLE
            if valid:
                reward += R_LUNGE_OK
                self.lunged_ok = True
                terminated = True
            else:
                reward += R_LUNGE_FAR
            nx += LUNGE_MM * math.cos(nh)
            ny += LUNGE_MM * math.sin(nh)
        elif a == Action.HEAD_LEFT:
            self.pan = HEAD_PAN_STEP
        elif a == Action.HEAD_RIGHT:
            self.pan = -HEAD_PAN_STEP

        self.heading = nh
        contact = False
        if (nx, ny) != (self.sx, self.sy):
            if self._collides(nx, ny):
                # Paredes e caixas são alinhadas aos eixos: desliza na componente livre
                # (patas raspando), ou fica parada se nenhuma for livre [INFERÊNCIA].
                contact = True
                if not self._collides(nx, self.sy):
                    ny = self.sy
                elif not self._collides(self.sx, ny):
                    nx = self.sx
                else:
                    nx, ny = self.sx, self.sy
                reward += R_COLLISION_HOLD if self.in_contact else R_COLLISION
                self.collided = True
            self.sx, self.sy = nx, ny
            if self._in_hole(nx, ny):
                reward += R_EDGE
                self.fell = True
                terminated = True
        self.in_contact = contact

        # Bateria
        self.battery_pct = max(0.0, self.battery_pct - BATTERY_DRAIN_PCT[Action(a)])
        if self.battery_pct < BATTERY_LOW_PCT:
            reward += R_BATTERY_LOW

        # Escuro / ser vista / aproximação
        if self._lux_at(self.sx, self.sy) <= DARK_LUX:
            self.dark_steps += 1
            reward += R_DARK
        seen = self._person_sees_spider()
        if seen:
            reward += R_SEEN
        d_now = self._person_dist()
        if self.p_present and not seen and math.isfinite(self.prev_person_dist) and math.isfinite(d_now):
            reward += R_APPROACH_PER_MM * (self.prev_person_dist - d_now)
        self.prev_person_dist = d_now

        self.obs = self._sense()
        truncated = (not terminated) and (
            self.steps >= self.max_steps
            or (self.p_left_ms is not None and self.t_ms - self.p_left_ms >= 10000)
        )
        return state_index(self.obs), float(reward), terminated, truncated, self._info(reward)
