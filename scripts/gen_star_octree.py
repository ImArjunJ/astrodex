#!/usr/bin/env python3
"""Build an octree binary from the Gaia DR3 star catalog.

Downloads ~176M stars from HuggingFace (samfatnassi/gaia-dr3),
builds a spatial octree with LOD aggregates, and writes a compact
binary file for the Star Explorer C++ runtime.

Usage:
    cd cooking
    source scripts/venv/bin/activate
    python scripts/gen_star_octree.py [--output assets/starmap/gaia_octree.bin]

Requires ~8 GB RAM. Takes 10-30 minutes depending on download speed.
"""

import argparse
import struct
import sys
import time
import numpy as np
from pathlib import Path

# ── Constants ────────────────────────────────────────────────────────────────

MAGIC = b"GAIA"
VERSION = 1
MAX_LEAF_STARS = 512
MAX_DEPTH = 20
ROOT_HALF_SIZE = 25000.0  # parsecs — covers the entire catalog

# Binary sizes
HEADER_SIZE = 64
STAR_SIZE = 20   # x,y,z (12) + mag (4) + rgb (3) + pad (1)
NODE_SIZE = 96

# ── B-V / BP-RP → RGB (Ballesteros approximation) ───────────────────────────

def bprp_to_bv(bp_rp):
    """Approximate B-V from Gaia BP-RP color index."""
    return bp_rp / 1.3 - 0.04

def bv_to_rgb(bv):
    """Ballesteros (2012) B-V color index to RGB, vectorized."""
    bv = np.clip(bv, -0.4, 2.0)
    t = 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62))

    # Planckian locus approximation
    x = np.where(t <= 4000,
        -0.2661239e9 / (t**3) - 0.2343589e6 / (t**2) + 0.8776956e3 / t + 0.179910,
        -3.0258469e9 / (t**3) + 2.1070379e6 / (t**2) + 0.2226347e3 / t + 0.240390)

    y = np.where(t <= 2222,
        -1.1063814 * x**3 - 1.34811020 * x**2 + 2.18555832 * x - 0.20219683,
        np.where(t <= 4000,
            -0.9549476 * x**3 - 1.37418593 * x**2 + 2.09137015 * x - 0.16748867,
             3.0817580 * x**3 - 5.87338670 * x**2 + 3.75112997 * x - 0.37001483))

    # CIE XYZ → sRGB
    Y = np.ones_like(t)
    X = (Y / y) * x
    Z = (Y / y) * (1.0 - x - y)

    r = np.maximum(0, 3.2406 * X - 1.5372 * Y - 0.4986 * Z)
    g = np.maximum(0, -0.9689 * X + 1.8758 * Y + 0.0415 * Z)
    b = np.maximum(0, 0.0557 * X - 0.2040 * Y + 1.0570 * Z)

    mx = np.maximum(np.maximum(np.maximum(r, g), b), 0.001)
    return (r / mx * 255).astype(np.uint8), (g / mx * 255).astype(np.uint8), (b / mx * 255).astype(np.uint8)

# ── Load Gaia DR3 ───────────────────────────────────────────────────────────

def load_gaia_data():
    """Download and extract star data from HuggingFace Gaia DR3."""
    from datasets import load_dataset
    import pyarrow as pa

    print("Loading Gaia DR3 from HuggingFace...")
    print("This will download ~15.7 GB on first run.")

    ds = load_dataset("samfatnassi/gaia-dr3", split="train")

    print(f"Dataset loaded: {len(ds)} stars")
    print("Extracting columns via Arrow (fast columnar access)...")

    t0 = time.time()

    # Access the underlying Arrow table — much faster than ds["col"]
    # which goes through Python iteration for large datasets
    table = ds.data.table if hasattr(ds.data, 'table') else ds.to_arrow()

    def arrow_to_numpy(col_name, dtype=np.float32):
        col = table.column(col_name)
        # Combine chunks then convert — avoids slow Python-level concat
        arr = col.combine_chunks().to_numpy(zero_copy_only=False).astype(dtype)
        print(f"  {col_name}: done ({time.time()-t0:.0f}s)")
        return arr

    x     = arrow_to_numpy("x")
    y     = arrow_to_numpy("y")
    z     = arrow_to_numpy("z")
    abs_m = arrow_to_numpy("abs_m")
    d_pc  = arrow_to_numpy("d_pc")
    bp_rp = arrow_to_numpy("bp_rp")

    # Free the Arrow table to reclaim memory
    del table, ds

    # Filter out NaN/invalid entries
    valid = (np.isfinite(x) & np.isfinite(y) & np.isfinite(z) &
             np.isfinite(abs_m) & np.isfinite(d_pc) & (d_pc > 0))
    n_invalid = (~valid).sum()
    if n_invalid > 0:
        print(f"  Filtered {n_invalid:,} invalid stars")
        x, y, z = x[valid], y[valid], z[valid]
        abs_m, d_pc, bp_rp = abs_m[valid], d_pc[valid], bp_rp[valid]

    # Compute apparent magnitude from absolute magnitude and distance
    # m = M + 5*log10(d) - 5
    app_mag = abs_m + 5.0 * np.log10(np.maximum(d_pc, 0.001)) - 5.0
    app_mag = np.clip(app_mag, -30, 25).astype(np.float32)

    # Handle NaN bp_rp — default to Sun-like (0.82)
    bp_rp = np.where(np.isfinite(bp_rp), bp_rp, 0.82)

    # Convert color
    bv = bprp_to_bv(bp_rp)
    r, g, b = bv_to_rgb(bv)

    print(f"Extracted {len(x):,} valid stars in {time.time()-t0:.0f}s")
    return x, y, z, app_mag, r, g, b

# ── Octree builder ───────────────────────────────────────────────────────────

class OctreeNode:
    __slots__ = ['cx', 'cy', 'cz', 'half_size', 'children',
                 'star_offset', 'star_count',
                 'agg_x', 'agg_y', 'agg_z', 'agg_flux',
                 'agg_r', 'agg_g', 'agg_b', 'total_descendants']

    def __init__(self, cx, cy, cz, half_size):
        self.cx, self.cy, self.cz = cx, cy, cz
        self.half_size = half_size
        self.children = [-1] * 8
        self.star_offset = 0
        self.star_count = 0
        self.agg_x = self.agg_y = self.agg_z = 0.0
        self.agg_flux = 0.0
        self.agg_r = self.agg_g = self.agg_b = 0.0
        self.total_descendants = 0


def build_octree(x, y, z, mag, r, g, b):
    """Build octree from star arrays. Returns (nodes, sorted_star_indices)."""
    sys.setrecursionlimit(50000)  # octree can be deep for clustered data
    n = len(x)
    print(f"Building octree for {n:,} stars...")

    # Pre-compute flux for aggregation
    flux = np.power(10.0, -0.4 * mag).astype(np.float32)

    nodes = []
    sorted_indices = []  # stars reordered to match leaf order

    def build(indices, cx, cy, cz, half_size, depth):
        node_idx = len(nodes)
        node = OctreeNode(cx, cy, cz, half_size)
        nodes.append(node)

        count = len(indices)
        node.total_descendants = count

        if count == 0:
            return node_idx

        # Compute aggregate (flux-weighted)
        f = flux[indices]
        total_flux = f.sum()
        if total_flux > 0:
            w = f / total_flux
            node.agg_x = float((x[indices] * w).sum())
            node.agg_y = float((y[indices] * w).sum())
            node.agg_z = float((z[indices] * w).sum())
            node.agg_flux = float(total_flux)
            # Flux-weighted color (work in float to avoid overflow)
            node.agg_r = float((r[indices].astype(np.float32) * w).sum())
            node.agg_g = float((g[indices].astype(np.float32) * w).sum())
            node.agg_b = float((b[indices].astype(np.float32) * w).sum())

        # Leaf condition
        if count <= MAX_LEAF_STARS or depth >= MAX_DEPTH:
            node.star_offset = len(sorted_indices)
            node.star_count = count
            sorted_indices.extend(indices.tolist())
            return node_idx

        # Split into 8 octants
        sx = x[indices]
        sy = y[indices]
        sz = z[indices]

        # Determine octant for each star: bit 0 = x, bit 1 = y, bit 2 = z
        octant = ((sx >= cx).astype(np.int32) |
                  ((sy >= cy).astype(np.int32) << 1) |
                  ((sz >= cz).astype(np.int32) << 2))

        hs = half_size * 0.5
        for o in range(8):
            mask = octant == o
            child_indices = indices[mask]
            if len(child_indices) == 0:
                continue

            # Child center
            ccx = cx + hs * (1 if (o & 1) else -1)
            ccy = cy + hs * (1 if (o & 2) else -1)
            ccz = cz + hs * (1 if (o & 4) else -1)

            child_idx = build(child_indices, ccx, ccy, ccz, hs, depth + 1)
            node.children[o] = child_idx

        return node_idx

    all_indices = np.arange(n, dtype=np.int64)
    build(all_indices, 0.0, 0.0, 0.0, ROOT_HALF_SIZE, 0)

    print(f"Octree built: {len(nodes):,} nodes, {len(sorted_indices):,} leaf stars")
    return nodes, sorted_indices


# ── Binary writer ────────────────────────────────────────────────────────────

def write_binary(path, nodes, sorted_indices, x, y, z, mag, r, g, b):
    """Write octree binary file."""
    num_stars = len(sorted_indices)
    num_nodes = len(nodes)

    total_size = HEADER_SIZE + num_stars * STAR_SIZE + num_nodes * NODE_SIZE
    print(f"Writing {path} ({total_size / 1e9:.2f} GB)")
    print(f"  Stars: {num_stars:,} × {STAR_SIZE}B = {num_stars * STAR_SIZE / 1e6:.0f} MB")
    print(f"  Nodes: {num_nodes:,} × {NODE_SIZE}B = {num_nodes * NODE_SIZE / 1e6:.0f} MB")

    with open(path, 'wb') as f:
        # Header (64 bytes)
        header = struct.pack('<4sIIIffff32s',
            MAGIC, VERSION, num_stars, num_nodes,
            ROOT_HALF_SIZE, 0.0, 0.0, 0.0,  # root center at origin
            b'\x00' * 32)
        f.write(header)

        # Star array — bulk numpy write (100x faster than per-star struct.pack)
        print("  Writing stars (bulk)...")
        idx = np.array(sorted_indices, dtype=np.int64)

        # Build a structured numpy array matching the 20-byte PackedStar layout
        star_dtype = np.dtype([
            ('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('mag', '<f4'),
            ('r', 'u1'), ('g', 'u1'), ('b', 'u1'), ('pad', 'u1')
        ])
        star_buf = np.empty(num_stars, dtype=star_dtype)
        star_buf['x']   = x[idx]
        star_buf['y']   = y[idx]
        star_buf['z']   = z[idx]
        star_buf['mag'] = mag[idx]
        star_buf['r']   = r[idx]
        star_buf['g']   = g[idx]
        star_buf['b']   = b[idx]
        star_buf['pad'] = 0
        star_buf.tofile(f)
        del star_buf, idx
        print(f"    {num_stars:,} stars written")

        # Node array — bulk numpy write
        print("  Writing nodes (bulk)...")
        node_dtype = np.dtype([
            ('cx', '<f4'), ('cy', '<f4'), ('cz', '<f4'), ('halfSize', '<f4'),
            ('c0', '<i4'), ('c1', '<i4'), ('c2', '<i4'), ('c3', '<i4'),
            ('c4', '<i4'), ('c5', '<i4'), ('c6', '<i4'), ('c7', '<i4'),
            ('starOffset', '<u4'), ('starCount', '<u4'),
            ('aggX', '<f4'), ('aggY', '<f4'), ('aggZ', '<f4'), ('aggFlux', '<f4'),
            ('aggR', '<f4'), ('aggG', '<f4'), ('aggB', '<f4'), ('padF', '<f4'),
            ('totalDesc', '<u4'), ('padI', '<u4'),
        ])
        node_buf = np.empty(num_nodes, dtype=node_dtype)
        for i, nd in enumerate(nodes):
            node_buf[i] = (
                nd.cx, nd.cy, nd.cz, nd.half_size,
                *nd.children,
                nd.star_offset, nd.star_count,
                nd.agg_x, nd.agg_y, nd.agg_z, nd.agg_flux,
                nd.agg_r, nd.agg_g, nd.agg_b, 0.0,
                nd.total_descendants, 0
            )
        node_buf.tofile(f)
        print(f"    {num_nodes:,} nodes written")

    print(f"Done! File size: {Path(path).stat().st_size / 1e9:.2f} GB")


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='Build Gaia DR3 octree binary')
    parser.add_argument('--output', type=str,
                        default='assets/starmap/gaia_octree.bin',
                        help='Output binary file path')
    args = parser.parse_args()

    t0 = time.time()

    # Load data
    x, y, z, mag, r, g, b = load_gaia_data()

    # Build octree
    nodes, sorted_indices = build_octree(x, y, z, mag, r, g, b)

    # Write binary
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    write_binary(args.output, nodes, sorted_indices, x, y, z, mag, r, g, b)

    print(f"\nTotal time: {time.time()-t0:.0f}s")

if __name__ == '__main__':
    main()
