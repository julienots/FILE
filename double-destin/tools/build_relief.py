"""Génère le relief ombré de la carte EMPIRE MONDIAL.
Sources : tuiles d'altitude Terrarium (AWS Open Data / Mapzen, zoom 4), Natural Earth (domaine public).
Projection équirectangulaire : x = (lon+180)*10*K, y = (84-lat)*10*K  (lat 84 → -57).
Sorties : www/img/relief_{0..3}.jpg (2x2 tuiles), tools/.cache/elev.npy (grille d'altitude 0,1°)."""
import json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

GEO = sys.argv[1]           # dossier des données téléchargées
OUT = sys.argv[2]           # dossier www
K = 1.5                     # 1 unité carte = 0,1° ; image = 1,5 px par unité
W, H = int(3600 * K), int(1410 * K)

# --- altitude mercator z4 -------------------------------------------------
N = 16 * 256
merc = np.zeros((N, N), np.float32)
for x in range(16):
    for y in range(16):
        a = np.asarray(Image.open(f"{GEO}/tiles/{x}_{y}.png").convert("RGB"), np.float32)
        merc[y*256:(y+1)*256, x*256:(x+1)*256] = a[..., 0] * 256 + a[..., 1] + a[..., 2] / 256 - 32768

lon = (np.arange(W) + .5) / (10 * K) - 180
lat = 84 - (np.arange(H) + .5) / (10 * K)
px = (lon + 180) / 360 * N
lr = np.radians(lat)
py = (1 - np.log(np.tan(lr) + 1 / np.cos(lr)) / math.pi) / 2 * N
X, Y = np.meshgrid(px, py)
x0 = np.clip(np.floor(X).astype(int), 0, N - 2); y0 = np.clip(np.floor(Y).astype(int), 0, N - 2)
fx = X - x0; fy = Y - y0
elev = (merc[y0, x0] * (1-fx) * (1-fy) + merc[y0, x0+1] * fx * (1-fy) + merc[y0+1, x0] * (1-fx) * fy + merc[y0+1, x0+1] * fx * fy)

# --- masque terre (Natural Earth admin 0) --------------------------------
def proj(c):
    return ((c[0] + 180) * 10 * K, (84 - c[1]) * 10 * K)
mask = Image.new("L", (W, H), 0); md = ImageDraw.Draw(mask)
for f in json.load(open(f"{GEO}/ne_10m_admin_0_countries.geojson"))["features"]:
    if f["properties"].get("ADM0_A3") == "ATA": pass
    g = f["geometry"]; polys = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
    for p in polys:
        md.polygon([proj(c) for c in p[0]], fill=255)
        for hole in p[1:]: md.polygon([proj(c) for c in hole], fill=0)
lakes = Image.new("L", (W, H), 0); ld = ImageDraw.Draw(lakes)
for f in json.load(open(f"{GEO}/ne_50m_lakes.geojson"))["features"]:
    g = f["geometry"]; polys = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
    for p in polys: ld.polygon([proj(c) for c in p[0]], fill=255)
land = np.asarray(mask, np.float32) / 255
lake = np.asarray(lakes, np.float32) / 255

# --- ombrage -------------------------------------------------------------
e = np.where(land > .5, np.maximum(elev, 0), elev)
cell = 1000 * 111 / (10 * K) * 1.0         # m par pixel (approx.)
coslat = np.cos(np.radians(lat))[:, None]
dzdx = np.gradient(e, axis=1) / (cell * np.maximum(coslat, .2))
dzdy = np.gradient(e, axis=0) / cell
ex = 7.0
slope = np.arctan(ex * np.hypot(dzdx, dzdy))
aspect = np.arctan2(-dzdx, dzdy)
az, alt = math.radians(315), math.radians(42)
shade = np.sin(alt) * np.cos(slope) + np.cos(alt) * np.sin(slope) * np.cos(az - aspect)
shade = np.clip(shade, 0, 1)

def ramp(v, stops):
    xs = np.array([s[0] for s in stops], np.float32)
    out = np.zeros(v.shape + (3,), np.float32)
    for i in range(3):
        out[..., i] = np.interp(v, xs, np.array([s[1][i] for s in stops], np.float32))
    return out

landc = ramp(e, [(0, (118, 150, 92)), (200, (140, 163, 100)), (600, (176, 172, 118)), (1200, (172, 146, 104)),
                 (2200, (150, 124, 98)), (3500, (170, 160, 152)), (5000, (238, 238, 240))])
# zones arides : légère teinte sable sous 30° de latitude hors relief marqué
sea = ramp(-np.minimum(elev, 0), [(0, (58, 110, 150)), (200, (46, 92, 136)), (1500, (30, 66, 112)), (4000, (20, 46, 86)), (8000, (14, 32, 64))])
sh = (0.55 + 0.75 * shade)[..., None]
landc = np.clip(landc * sh, 0, 255)
seash = np.clip(sea * (0.85 + 0.25 * shade[..., None]), 0, 255)
L = land[..., None]
img = landc * L + seash * (1 - L)
lakec = np.array([62, 112, 156], np.float32)
img = img * (1 - lake[..., None]) + lakec * lake[..., None]
# liseré côtier
edge = np.asarray(mask.filter(ImageFilter.FIND_EDGES), np.float32)[..., None] / 255
img = img * (1 - .35 * edge)
im = Image.fromarray(img.astype(np.uint8), "RGB")
d = ImageDraw.Draw(im)
for f in json.load(open(f"{GEO}/ne_50m_rivers_lake_centerlines.geojson"))["features"]:
    g = f["geometry"]; lines = [g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]
    sr = f["properties"].get("scalerank", 5) or 5
    w = 2 if sr <= 3 else 1
    for ln in lines: d.line([proj(c) for c in ln], fill=(70, 120, 170), width=w)

os.makedirs(f"{OUT}/img", exist_ok=True)
hw, hh = W // 2, H // 2
for i, (x, y) in enumerate([(0, 0), (hw, 0), (0, hh), (hw, hh)]):
    im.crop((x, y, x + (W - hw if x else hw), y + (H - hh if y else hh))).save(f"{OUT}/img/relief_{i}.jpg", quality=82, optimize=True, progressive=True)
im.resize((900, int(900 * H / W)), Image.LANCZOS).save(f"{OUT}/img/relief_small.jpg", quality=85)
# grille d'altitude à 0,1° pour le jeu (terrain des provinces)
os.makedirs(os.path.join(os.path.dirname(__file__), ".cache"), exist_ok=True)
small = np.asarray(Image.fromarray(e.astype(np.float32)).resize((3600, 1410), Image.BILINEAR))
np.save(os.path.join(os.path.dirname(__file__), ".cache", "elev.npy"), small.astype(np.int16))
print("ok", W, H)
