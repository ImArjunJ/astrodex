"""
Data loader for cached fused exoplanet JSON records.

Reads JSON files produced by ExoplanetData::toJson() from the .cache/fused/
directory and converts them into a pandas DataFrame for ML benchmarking.
"""

import json
import os
from pathlib import Path
from typing import List, Optional

import numpy as np
import pandas as pd


# All numerical columns extracted from ExoplanetData JSON
NUMERICAL_COLUMNS = [
    "mass_earth",
    "radius_earth",
    "density_gcc",
    "equilibrium_temp_k",
    "surface_gravity_g",
    "orbital_period_days",
    "semi_major_axis_au",
    "eccentricity",
    "surface_pressure_atm",
    "albedo",
    "ocean_coverage_fraction",
    "cloud_coverage_fraction",
    "ice_coverage_fraction",
    "star_temp_k",
    "star_radius_solar",
    "star_mass_solar",
    "star_metallicity",
]

# Categorical columns
CATEGORICAL_COLUMNS = [
    "planet_type",
    "discovery_method",
    "spectral_type",
]

# All columns in the DataFrame
ALL_COLUMNS = ["name"] + NUMERICAL_COLUMNS + CATEGORICAL_COLUMNS


def _parse_exoplanet_json(data: dict) -> dict:
    """Parse a single exoplanet JSON record into a flat dictionary.

    Handles both the extended JSON format (with atmosphere/rendering sections)
    and the simpler format (missing those sections), using .get() with defaults
    throughout.
    """
    physical = data.get("physical", {})
    orbital = data.get("orbital", {})
    atmosphere = data.get("atmosphere", {})
    classification = data.get("classification", {})
    host_star = data.get("host_star", {})

    return {
        "name": data.get("name", ""),
        # Physical parameters
        "mass_earth": physical.get("mass_earth", np.nan),
        "radius_earth": physical.get("radius_earth", np.nan),
        "density_gcc": physical.get("density_gcc", np.nan),
        "equilibrium_temp_k": physical.get("equilibrium_temp_k", np.nan),
        "surface_gravity_g": physical.get("surface_gravity_g", np.nan),
        # Orbital parameters
        "orbital_period_days": orbital.get("period_days", np.nan),
        "semi_major_axis_au": orbital.get("semi_major_axis_au", np.nan),
        "eccentricity": orbital.get("eccentricity", np.nan),
        # Atmosphere parameters
        "surface_pressure_atm": atmosphere.get("surface_pressure_atm", np.nan),
        "albedo": atmosphere.get("albedo", np.nan),
        "ocean_coverage_fraction": atmosphere.get(
            "ocean_coverage_fraction", np.nan
        ),
        "cloud_coverage_fraction": atmosphere.get(
            "cloud_coverage_fraction", np.nan
        ),
        "ice_coverage_fraction": atmosphere.get(
            "ice_coverage_fraction", np.nan
        ),
        # Classification
        "planet_type": classification.get("planet_type", ""),
        # Host star parameters
        "star_temp_k": host_star.get("effective_temp_k", np.nan),
        "star_radius_solar": host_star.get("radius_solar", np.nan),
        "star_mass_solar": host_star.get("mass_solar", np.nan),
        "star_metallicity": host_star.get("metallicity", np.nan),
        # Discovery info
        "discovery_method": data.get("discovery_method", ""),
        "spectral_type": host_star.get("spectral_type", ""),
    }


def load_cached_exoplanets(
    cache_dir: Optional[str] = None,
) -> pd.DataFrame:
    """Load cached fused exoplanet JSON records into a DataFrame.

    Args:
        cache_dir: Path to the fused cache directory. If None, checks
            ASTRODEX_CACHE_DIR environment variable, then falls back to
            ../../.cache/fused/ relative to this file.

    Returns:
        DataFrame with one row per exoplanet, columns for all numerical and
        categorical fields. Missing values are np.nan for numerical fields
        and empty string for categorical fields.

    Raises:
        FileNotFoundError: If the cache directory does not exist.
    """
    if cache_dir is None:
        cache_dir = os.environ.get("ASTRODEX_CACHE_DIR")
    if cache_dir is None:
        # Default: relative to this file's location
        cache_dir = str(Path(__file__).parent / ".." / ".." / ".cache" / "fused")

    cache_path = Path(cache_dir)

    if not cache_path.exists():
        raise FileNotFoundError(
            f"Cache directory not found: {cache_path.resolve()}"
        )

    records: List[dict] = []

    for json_file in sorted(cache_path.glob("*.json")):
        try:
            with open(json_file, "r") as f:
                data = json.load(f)
            records.append(_parse_exoplanet_json(data))
        except (json.JSONDecodeError, OSError) as e:
            print(f"Warning: Skipping {json_file.name}: {e}")

    df = pd.DataFrame(records, columns=ALL_COLUMNS)

    # Convert numerical columns to float (handles None/empty -> NaN)
    for col in NUMERICAL_COLUMNS:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    print(f"Loaded {len(df)} exoplanet records from {cache_path.resolve()}")
    return df


def filter_complete_records(
    df: pd.DataFrame, required_fields: List[str]
) -> pd.DataFrame:
    """Return only rows where ALL required fields are non-NaN.

    Args:
        df: Input DataFrame.
        required_fields: List of column names that must be non-NaN.

    Returns:
        Filtered DataFrame with only complete records.
    """
    mask = df[required_fields].notna().all(axis=1)
    filtered = df[mask].copy()
    print(
        f"Filtered to {len(filtered)}/{len(df)} records with complete "
        f"fields: {required_fields}"
    )
    return filtered


def generate_synthetic_data(n_records: int = 100, seed: int = 42) -> pd.DataFrame:
    """Generate synthetic exoplanet data for benchmarking when cache is empty.

    Uses physically plausible distributions based on known exoplanet
    population statistics. This is for testing the benchmark pipeline only --
    results from synthetic data are NOT scientifically meaningful.

    Args:
        n_records: Number of synthetic records to generate.
        seed: Random seed for reproducibility.

    Returns:
        DataFrame with synthetic exoplanet records.
    """
    rng = np.random.default_rng(seed)

    # Planet type probabilities (roughly matching Kepler survey)
    planet_types = ["Rocky", "Super-Earth", "Mini-Neptune", "Ice Giant", "Gas Giant"]
    type_probs = [0.15, 0.25, 0.30, 0.15, 0.15]
    types = rng.choice(planet_types, size=n_records, p=type_probs)

    records = []
    for i, ptype in enumerate(types):
        # Mass and radius correlated with type
        if ptype == "Rocky":
            mass = rng.uniform(0.1, 2.0)
            radius = rng.uniform(0.5, 1.2)
            temp = rng.uniform(200, 800)
        elif ptype == "Super-Earth":
            mass = rng.uniform(1.0, 10.0)
            radius = rng.uniform(1.0, 2.0)
            temp = rng.uniform(200, 600)
        elif ptype == "Mini-Neptune":
            mass = rng.uniform(5.0, 20.0)
            radius = rng.uniform(2.0, 4.0)
            temp = rng.uniform(100, 500)
        elif ptype == "Ice Giant":
            mass = rng.uniform(10.0, 50.0)
            radius = rng.uniform(3.5, 6.0)
            temp = rng.uniform(50, 200)
        else:  # Gas Giant
            mass = rng.uniform(50.0, 3000.0)
            radius = rng.uniform(6.0, 22.0)
            temp = rng.uniform(100, 2500)

        # Density from mass and radius (Earth units)
        density = mass / (radius ** 3) * 5.51  # Scale relative to Earth density

        # Orbital parameters (log-uniform for period)
        period = 10 ** rng.uniform(0, 3.5)  # 1 to ~3000 days
        sma = (period / 365.25) ** (2.0 / 3.0)  # Kepler's third law approx
        ecc = rng.beta(1.5, 8.0)  # Mostly circular

        # Surface gravity (Earth units)
        surface_g = mass / (radius ** 2)

        # Star parameters
        star_temp = rng.uniform(3000, 10000)
        star_radius = rng.uniform(0.3, 3.0)
        star_mass = rng.uniform(0.3, 2.5)
        star_metallicity = rng.normal(0.0, 0.3)

        # Atmosphere parameters (only for some planet types)
        if ptype in ("Rocky", "Super-Earth"):
            pressure = rng.lognormal(0, 1.5)
            albedo = rng.uniform(0.1, 0.8)
            ocean = rng.uniform(0, 1) if temp > 250 and temp < 400 else 0.0
            cloud = rng.uniform(0, 0.8)
            ice = rng.uniform(0, 0.5) if temp < 273 else 0.0
        else:
            pressure = np.nan
            albedo = rng.uniform(0.1, 0.5)
            ocean = np.nan
            cloud = np.nan
            ice = np.nan

        records.append({
            "name": f"Synthetic-{i+1:04d}",
            "mass_earth": mass,
            "radius_earth": radius,
            "density_gcc": density,
            "equilibrium_temp_k": temp,
            "surface_gravity_g": surface_g,
            "orbital_period_days": period,
            "semi_major_axis_au": sma,
            "eccentricity": ecc,
            "surface_pressure_atm": pressure,
            "albedo": albedo,
            "ocean_coverage_fraction": ocean,
            "cloud_coverage_fraction": cloud,
            "ice_coverage_fraction": ice,
            "planet_type": ptype,
            "star_temp_k": star_temp,
            "star_radius_solar": star_radius,
            "star_mass_solar": star_mass,
            "star_metallicity": star_metallicity,
            "discovery_method": rng.choice(
                ["Transit", "Radial Velocity", "Direct Imaging", "Microlensing"]
            ),
            "spectral_type": rng.choice(
                ["M", "K", "G", "F", "A"]
            ),
        })

    df = pd.DataFrame(records, columns=ALL_COLUMNS)
    print(f"Generated {len(df)} synthetic exoplanet records (WARNING: not real data)")
    return df
