#!/usr/bin/env python3
"""Download Gaia DR3 and write raw star data for C++ LOD processor.

Step 1 of 2: Python handles HuggingFace download + basic processing.
Step 2 of 2: C++ executable (star_lod_builder) does LOD assignment with OpenMP.

Usage:
    cd cooking
    source scripts/venv/bin/activate
    python scripts/gen_star_lod.py
    ./build/star_lod_builder assets/starmap/gaia_raw.bin assets/starmap/gaia_multilod.bin
"""

import struct
import sys
import time
import numpy as np
from pathlib import Path

# ── BP-RP → RGB (Ballesteros) ───────────────────────────────────────────────

def bv_to_rgb(bv):
    bv = np.clip(bv, -0.4, 2.0)
    t = 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62))
    x = np.where(t <= 4000,
        -0.2661239e9 / (t**3) - 0.2343589e6 / (t**2) + 0.8776956e3 / t + 0.179910,
        -3.0258469e9 / (t**3) + 2.1070379e6 / (t**2) + 0.2226347e3 / t + 0.240390)
    y = np.where(t <= 2222,
        -1.1063814 * x**3 - 1.34811020 * x**2 + 2.18555832 * x - 0.20219683,
        np.where(t <= 4000,
            -0.9549476 * x**3 - 1.37418593 * x**2 + 2.09137015 * x - 0.16748867,
             3.0817580 * x**3 - 5.87338670 * x**2 + 3.75112997 * x - 0.37001483))
    Y = np.ones_like(t)
    X = (Y / y) * x
    Z = (Y / y) * (1.0 - x - y)
    r = np.maximum(0, 3.2406 * X - 1.5372 * Y - 0.4986 * Z)
    g = np.maximum(0, -0.9689 * X + 1.8758 * Y + 0.0415 * Z)
    b = np.maximum(0, 0.0557 * X - 0.2040 * Y + 1.0570 * Z)
    mx = np.maximum(np.maximum(np.maximum(r, g), b), 0.001)
    return (r / mx * 255).astype(np.uint8), (g / mx * 255).astype(np.uint8), (b / mx * 255).astype(np.uint8)

# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "assets/starmap/gaia_raw.bin"
    t0 = time.time()

    from datasets import load_dataset
    print("Loading Gaia DR3 from HuggingFace...")
    ds = load_dataset("samfatnassi/gaia-dr3", split="train")
    print(f"Dataset loaded: {len(ds)} stars")
    print("Extracting columns via Arrow...")

    table = ds.data.table if hasattr(ds.data, 'table') else ds.to_arrow()

    def col(name, dtype=np.float32):
        arr = table.column(name).combine_chunks().to_numpy(zero_copy_only=False).astype(dtype)
        print(f"  {name}: done ({time.time()-t0:.0f}s)")
        return arr

    x     = col("x")
    y     = col("y")
    z     = col("z")
    abs_m = col("abs_m")
    d_pc  = col("d_pc")
    bp_rp = col("bp_rp")
    del table, ds

    # Filter invalid
    valid = (np.isfinite(x) & np.isfinite(y) & np.isfinite(z) &
             np.isfinite(abs_m) & np.isfinite(d_pc) & (d_pc > 0))
    n_invalid = (~valid).sum()
    if n_invalid > 0:
        print(f"  Filtered {n_invalid:,} invalid stars")
        x, y, z = x[valid], y[valid], z[valid]
        abs_m, d_pc, bp_rp = abs_m[valid], d_pc[valid], bp_rp[valid]

    # Compute apparent magnitude and color
    app_mag = (abs_m + 5.0 * np.log10(np.maximum(d_pc, 0.001)) - 5.0).clip(-30, 25).astype(np.float32)
    bp_rp = np.where(np.isfinite(bp_rp), bp_rp, 0.82)
    r, g, b = bv_to_rgb(bp_rp / 1.3 - 0.04)

    n = len(x)
    print(f"Writing {n:,} stars to {output}...")

    # Write raw binary: same PackedStar format (20 bytes each)
    # Header: just "GRAW" + uint32 count = 8 bytes
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    star_dtype = np.dtype([
        ('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('mag', '<f4'),
        ('r', 'u1'), ('g', 'u1'), ('b', 'u1'), ('pad', 'u1')
    ])
    star_buf = np.empty(n, dtype=star_dtype)
    star_buf['x']   = x
    star_buf['y']   = y
    star_buf['z']   = z
    star_buf['mag'] = app_mag
    star_buf['r']   = r
    star_buf['g']   = g
    star_buf['b']   = b
    star_buf['pad'] = 0

    with open(output, 'wb') as f:
        f.write(struct.pack('<4sI', b"GRAW", n))
        star_buf.tofile(f)

    size_gb = Path(output).stat().st_size / 1e9
    print(f"Done! {size_gb:.2f} GB in {time.time()-t0:.0f}s")
    print(f"\nNext: ./build/star_lod_builder {output} assets/starmap/gaia_multilod.bin")

if __name__ == '__main__':
    main()
