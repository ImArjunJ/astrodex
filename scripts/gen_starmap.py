#!/usr/bin/env python3
"""Generate a star cubemap from astronomical catalog data.

Uses the HYG database (~120k stars, full sky) as the primary source,
supplemented with Gaia data from Multimodal Universe when available.

Outputs 6 cubemap face PNGs to assets/starmap/.

Usage:
    python scripts/gen_starmap.py [--mag-limit 10.0] [--face-size 1024]
"""

import argparse
import csv
import io
import math
import sys
import urllib.request
import numpy as np
from pathlib import Path
from PIL import Image

# ── RA/Dec → unit vector ─────────────────────────────────────────────────────

def radec_to_xyz(ra_deg, dec_deg):
    """Convert RA/Dec (degrees) to unit vector (x, y, z)."""
    ra  = np.radians(ra_deg)
    dec = np.radians(dec_deg)
    x = np.cos(dec) * np.cos(ra)
    y = np.sin(dec)
    z = np.cos(dec) * np.sin(ra)
    return x, y, z

# ── BP-RP color index → RGB ──────────────────────────────────────────────────

def bprp_to_rgb(bp_rp):
    """Approximate star color from Gaia BP-RP color index.

    BP-RP < 0    → hot blue/white (O/B stars)
    BP-RP ~ 0.8  → white/yellow (G stars, Sun-like)
    BP-RP ~ 1.5  → orange (K stars)
    BP-RP > 2.0  → red (M stars)
    """
    t = np.clip(bp_rp, -0.5, 4.0)
    # Simple piecewise linear approximation
    r = np.clip(0.7 + 0.15 * t, 0.5, 1.0)
    g = np.clip(0.8 - 0.05 * t, 0.4, 0.95)
    b = np.clip(1.0 - 0.25 * t, 0.15, 1.0)
    return r, g, b

# ── Magnitude → brightness ───────────────────────────────────────────────────

def mag_to_brightness(mag, mag_limit):
    """Convert apparent magnitude to [0, 1] brightness.

    Brighter stars have lower magnitudes. We map:
      mag ≤ 0   → brightness = 1.0
      mag = mag_limit → brightness ≈ 0
    """
    # Astronomical flux ratio: 10^(-0.4 * mag)
    flux = 10.0 ** (-0.4 * mag)
    flux_min = 10.0 ** (-0.4 * mag_limit)
    flux_max = 10.0 ** (-0.4 * -1.5)  # Sirius-ish
    return np.clip((flux - flux_min) / (flux_max - flux_min), 0.0, 1.0)

# ── Cubemap face mapping ─────────────────────────────────────────────────────

FACE_NAMES = ['+X', '-X', '+Y', '-Y', '+Z', '-Z']

def xyz_to_cubemap(x, y, z):
    """Map a unit vector to (face_index, u, v) in [0, 1]."""
    ax, ay, az = abs(x), abs(y), abs(z)

    if ax >= ay and ax >= az:
        if x > 0:  # +X
            return 0, (-z / ax + 1) / 2, (-y / ax + 1) / 2
        else:       # -X
            return 1, (z / ax + 1) / 2, (-y / ax + 1) / 2
    elif ay >= ax and ay >= az:
        if y > 0:  # +Y (up)
            return 2, (x / ay + 1) / 2, (z / ay + 1) / 2
        else:       # -Y (down)
            return 3, (x / ay + 1) / 2, (-z / ay + 1) / 2
    else:
        if z > 0:  # +Z
            return 4, (x / az + 1) / 2, (-y / az + 1) / 2
        else:       # -Z
            return 5, (-x / az + 1) / 2, (-y / az + 1) / 2

# ── Main ──────────────────────────────────────────────────────────────────────

def bv_to_bprp(bv):
    """Approximate BP-RP from B-V color index (for HYG catalog)."""
    return bv * 1.3 + 0.05  # rough linear fit

def load_hyg_catalog(mag_limit):
    """Download and parse the HYG star database (full sky, ~120k stars)."""
    url = "https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv"
    cache = Path("assets/starmap/.hyg_cache.csv")

    if cache.exists():
        print("  Using cached HYG catalog...")
        data = cache.read_text()
    else:
        print(f"  Downloading HYG catalog...")
        data = urllib.request.urlopen(url).read().decode('utf-8')
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(data)

    reader = csv.DictReader(io.StringIO(data))
    stars_ra, stars_dec, stars_mag, stars_col = [], [], [], []

    for row in reader:
        try:
            mag = float(row['mag'])
            if mag > mag_limit:
                continue
            # HYG uses RA in hours (0-24), convert to degrees
            ra  = float(row['ra']) * 15.0
            dec = float(row['dec'])
            ci  = float(row['ci']) if row['ci'] else 0.65
            stars_ra.append(ra)
            stars_dec.append(dec)
            stars_mag.append(mag)
            stars_col.append(bv_to_bprp(ci))
        except (ValueError, KeyError):
            continue

    return stars_ra, stars_dec, stars_mag, stars_col


def main():
    parser = argparse.ArgumentParser(description='Generate star cubemap')
    parser.add_argument('--mag-limit', type=float, default=8.5,
                        help='Faintest magnitude to include (default: 8.5)')
    parser.add_argument('--face-size', type=int, default=1024,
                        help='Cubemap face resolution (default: 1024)')
    parser.add_argument('--output-dir', type=str, default='assets/starmap',
                        help='Output directory (default: assets/starmap)')
    parser.add_argument('--bloom', type=float, default=1.5,
                        help='Bloom radius in pixels (default: 1.5)')
    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    N = args.face_size

    # ── Load star catalog ─────────────────────────────────────────────────
    print(f"Loading stars (mag ≤ {args.mag_limit})...")
    stars_ra, stars_dec, stars_mag, stars_col = load_hyg_catalog(args.mag_limit)
    kept = len(stars_ra)
    print(f"  Collected {kept:,} stars")

    if kept == 0:
        print("No stars found! Try increasing --mag-limit or --max-rows")
        sys.exit(1)

    stars_ra  = np.array(stars_ra)
    stars_dec = np.array(stars_dec)
    stars_mag = np.array(stars_mag)
    stars_col = np.array(stars_col)

    # ── Build cubemap faces ───────────────────────────────────────────────
    print(f"Building {N}x{N} cubemap ({len(FACE_NAMES)} faces)...")
    faces = [np.zeros((N, N, 3), dtype=np.float32) for _ in range(6)]

    # Convert all stars to cubemap coordinates
    xs, ys, zs = radec_to_xyz(stars_ra, stars_dec)

    for i in range(kept):
        face, u, v = xyz_to_cubemap(xs[i], ys[i], zs[i])
        px = int(u * (N - 1))
        py = int(v * (N - 1))
        px = max(0, min(N - 1, px))
        py = max(0, min(N - 1, py))

        brightness = mag_to_brightness(stars_mag[i], args.mag_limit)
        r, g, b = bprp_to_rgb(stars_col[i])

        # Accumulate (handles overlapping stars)
        faces[face][py, px, 0] += brightness * r
        faces[face][py, px, 1] += brightness * g
        faces[face][py, px, 2] += brightness * b

    # ── Apply bloom (small Gaussian spread) ───────────────────────────────
    if args.bloom > 0:
        from scipy.ndimage import gaussian_filter
        print(f"Applying bloom (sigma={args.bloom})...")
        for i in range(6):
            for c in range(3):
                faces[i][:, :, c] = gaussian_filter(faces[i][:, :, c], sigma=args.bloom)

    # ── Save faces as PNG ─────────────────────────────────────────────────
    # Global normalization — preserve relative brightness across all faces
    global_peak = max(f.max() for f in faces)
    if global_peak <= 0:
        global_peak = 1.0

    face_filenames = ['posX', 'negX', 'posY', 'negY', 'posZ', 'negZ']
    for i, (face_data, name) in enumerate(zip(faces, face_filenames)):
        normalized = face_data / global_peak
        img = (normalized * 255).clip(0, 255).astype(np.uint8)
        path = out_dir / f"starmap_{name}.png"
        Image.fromarray(img).save(path)
        print(f"  Saved {path} (peak {face_data.max():.4f})")

    print(f"\nCubemap saved to {out_dir}/")
    print(f"Stars rendered: {kept:,}")

if __name__ == '__main__':
    main()
