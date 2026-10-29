"""Draws tools/weather/run.sh days CSVs as an SVG timeline (no dependencies):
python3 tools/weather/chart.py out.svg "title" a.csv ["title" b.csv ...]"""
import csv
import sys

REG = {"High": "#f2c14e", "WarmFront": "#e07a5f", "WarmSector": "#f4a261", "ColdFront": "#3d5a80", "Showery": "#81b29a",
       "Occluded": "#9a8c98", "ArcticHigh": "#a8dadc", "ThunderyLow": "#9d0208"}
PREC = {"Snow": "#5e60ce", "SnowShowers": "#7400b8", "Sleet": "#48bfe3", "FreezingRain": "#d00000", "FreezingDrizzle": "#e85d04",
        "Thunderstorm": "#370617", "Hail": "#6a040f", "Graupel": "#9d4edd"}
W, PH, PAD = 1500, 370, 60


def panel(rows, title, y0):
    n = len(rows)
    sx = lambda i: PAD + (W - 2 * PAD) * i / max(n - 1, 1)
    out = [f'<text x="{PAD}" y="{y0 + 18}" class="h">{title}</text>']
    top = y0 + 30
    # regime strip
    for i, r in enumerate(rows):
        out.append(f'<rect x="{sx(i):.1f}" y="{top}" width="{(W - 2 * PAD) / n + 0.6:.2f}" height="14" fill="{REG.get(r["regime"], "#ccc")}"/>')
    # cloud band (grey under the temperature)
    ct = top + 20
    ch = 30
    pts = " ".join(f'{sx(i):.1f},{ct + ch - ch * float(r["cloud"]):.1f}' for i, r in enumerate(rows))
    out.append(f'<polygon points="{sx(0)},{ct + ch} {pts} {sx(n - 1)},{ct + ch}" fill="#8d99ae" opacity="0.55"/>')
    out.append(f'<text x="{PAD - 6}" y="{ct + 20}" class="s" text-anchor="end">cloud</text>')
    # temperature
    tt, th = ct + ch + 12, 120
    temps = [float(r["temp"]) for r in rows]
    lo, hi = min(min(temps), -5) // 5 * 5, max(max(temps), 10) // 5 * 5 + 5
    ty = lambda t: tt + th - th * (t - lo) / (hi - lo)
    for t in range(int(lo), int(hi) + 1, 5):
        out.append(f'<line x1="{PAD}" x2="{W - PAD}" y1="{ty(t):.1f}" y2="{ty(t):.1f}" stroke="{"#555" if t == 0 else "#ddd"}" stroke-width="{1.2 if t == 0 else 0.6}"/>')
        out.append(f'<text x="{PAD - 6}" y="{ty(t) + 4:.1f}" class="s" text-anchor="end">{t}°</text>')
    out.append('<polyline fill="none" stroke="#c1121f" stroke-width="1.4" points="' + " ".join(f'{sx(i):.1f},{ty(t):.1f}' for i, t in enumerate(temps)) + '"/>')
    # precipitation bars
    pt, phh = tt + th + 8, 60
    for i, r in enumerate(rows):
        rate = float(r["rate"])
        if rate > 0.03:
            h = min(phh, phh * rate / 6.0)
            col = PREC.get(r["precip"], "#0077b6")
            out.append(f'<rect x="{sx(i):.1f}" y="{pt + phh - h:.1f}" width="{(W - 2 * PAD) / n + 0.4:.2f}" height="{h:.1f}" fill="{col}"/>')
    out.append(f'<text x="{PAD - 6}" y="{pt + 40}" class="s" text-anchor="end">mm/h</text>')
    # snow depth and sea ice
    st = pt + phh + 6
    sh = 40
    if any(float(r["snow_cm"]) > 0.5 or float(r["ice_cm"]) > 0.5 for r in rows):
        mx = max(max(float(r["snow_cm"]) for r in rows), max(float(r["ice_cm"]) for r in rows), 10)
        out.append('<polyline fill="none" stroke="#5e60ce" stroke-width="1.6" points="' + " ".join(f'{sx(i):.1f},{st + sh - sh * float(r["snow_cm"]) / mx:.1f}' for i, r in enumerate(rows)) + '"/>')
        out.append('<polyline fill="none" stroke="#00b4d8" stroke-width="1.6" stroke-dasharray="4 2" points="' + " ".join(f'{sx(i):.1f},{st + sh - sh * float(r["ice_cm"]) / mx:.1f}' for i, r in enumerate(rows)) + '"/>')
        out.append(f'<text x="{PAD - 6}" y="{st + 14}" class="s" text-anchor="end">snow/ice</text><text x="{PAD - 6}" y="{st + 28}" class="s" text-anchor="end">0..{mx:.0f} cm</text>')
    # days
    for i in range(0, n, 24):
        out.append(f'<line x1="{sx(i):.1f}" x2="{sx(i):.1f}" y1="{top}" y2="{st + sh}" stroke="#eee" stroke-width="0.5"/>')
        if (i // 24) % (7 if n > 60 * 24 else 2) == 0:
            out.append(f'<text x="{sx(i) + 2:.1f}" y="{st + sh + 12}" class="s">{rows[i]["date"]}</text>')
    return out


def main():
    path = sys.argv[1]
    pairs = list(zip(sys.argv[2::3], sys.argv[3::3], sys.argv[4::3]))
    body = []
    y = 0
    for title, csvp, start in pairs:
        import datetime
        d0 = datetime.date.fromisoformat(start)
        rows = list(csv.DictReader(open(csvp)))
        for r in rows:
            r["date"] = (d0 + datetime.timedelta(hours=float(r["hours"]))).strftime("%d.%m")
        body += panel(rows, title, y)
        y += PH
    legend = [f'<rect x="{PAD + i * 120}" y="{y + 4}" width="12" height="12" fill="{c}"/><text x="{PAD + i * 120 + 16}" y="{y + 14}" class="s">{k}</text>' for i, (k, c) in enumerate(REG.items())]
    legend += [f'<rect x="{PAD + i * 120}" y="{y + 24}" width="12" height="12" fill="{c}"/><text x="{PAD + i * 120 + 16}" y="{y + 34}" class="s">{k}</text>' for i, (k, c) in enumerate([("Rain", "#0077b6")] + list(PREC.items()))]
    legend.append(f'<text x="{PAD}" y="{y + 56}" class="s">red line: air temperature; grey band: cloud cover; bars: precipitation (colour = kind); blue: level snow depth, dashed: sea ice</text>')
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{y + 66}" viewBox="0 0 {W} {y + 66}"><style>.h{{font:600 15px sans-serif}} .s{{font:11px sans-serif;fill:#333}}</style><rect width="100%" height="100%" fill="#fff"/>' + "".join(body + legend) + "</svg>"
    open(path, "w").write(svg)


main()
