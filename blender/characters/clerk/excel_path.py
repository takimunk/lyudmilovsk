"""The clerk's 8 s mouse path and click/blink timing, shared by the body loop (clerk_excel_anim.py)
and his mouse (build_clerk.py --desk), so hand and mouse move together."""
import math

LOOP = 8.0
# (seconds, dx, dy) in metres in character space: -x moves the cursor right, -y moves it up the screen.
MOUSE_KEYS = [(0.0, 0, 0), (0.30, -0.004, 0), (0.95, -0.004, 0), (1.25, -0.008, 0.0005), (1.85, -0.008, 0.0005),
              (2.35, 0.005, -0.022), (2.85, 0.005, -0.022), (3.05, 0.008, -0.0265), (3.55, 0.008, -0.0265),
              (4.05, -0.012, 0.002), (4.55, -0.012, 0.002), (4.85, -0.016, 0.002), (5.45, -0.016, 0.002),
              (5.75, -0.020, 0.003), (6.45, -0.020, 0.003), (7.35, 0.0, 0.0), (8.0, 0.0, 0.0)]
CLICKS = [0.62, 1.55, 2.6, 3.3, 4.3, 5.15, 6.02, 6.2]
BLINKS = [1.95, 5.6]


def ease(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def mouse_at(t):
    """((dx, dy, 0), how much it is moving 0..1, signed x step of the current move)."""
    t = t % LOOP
    for (t0, x0, y0), (t1, x1, y1) in zip(MOUSE_KEYS, MOUSE_KEYS[1:]):
        if t0 <= t <= t1:
            k = ease((t - t0) / max(t1 - t0, 1e-6))
            moving = (x0, y0) != (x1, y1)
            return (x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, 0.0), (math.sin(k * math.pi) if moving else 0.0), (x1 - x0)
    return (0.0, 0.0, 0.0), 0.0, 0.0


def pulse(t, times, up, hold, down):
    v = 0.0
    for c in times:
        dt = t - c
        if 0 <= dt < up:
            v = max(v, dt / up)
        elif up <= dt < up + hold:
            v = 1.0
        elif up + hold <= dt < up + hold + down:
            v = max(v, 1 - (dt - up - hold) / down)
    return ease(v)
