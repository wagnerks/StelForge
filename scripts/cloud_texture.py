"""The cloud layer's texture, baked: bin/textures/sky/clouds.png.

    python scripts/cloud_texture.py

The sky shader used to work this out per pixel, a few octaves at a time, because everything it does costs a pixel of
sky. Baked here it can afford as many octaves as it likes, and the sky reads one texel instead: the shapes are richer
and the frame is cheaper. Two fields are kept apart rather than mixed, so the scene's cloud coverage still decides how
much of the sky is covered at runtime:

    R  where the clouds are: octaves of value noise, flattened so the value is the share of sky it covers
    G  the lumps they are piled from: three scales of worley, 0..1
    B  a finer curl over the lumps, for the edges
    A  1, unused

Seamless: every noise is built on a lattice that wraps, so the layer tiles over the sky. One tile is kSideMetres
across, which the shader's kSfeCloudTile must match.
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'bin', 'textures', 'sky')
SIZE = 2048
# Metres a tile covers. The sky reads the layer at 1600 m up, so a cloud a few kilometres across is what the eye sees
# as one cloud: at this size that is a few hundred texels of the tile.
SIDE_METRES = 27273.0


def value_noise(size, period, rng):
    """Value noise on a lattice of `period` cells that wraps, sampled over `size` texels; quintic, as the shader's."""
    lattice = rng.random((period, period))
    t = (np.arange(size) / size * period)
    cell = np.floor(t).astype(int)
    f = t - cell
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0)
    c0 = cell % period
    c1 = (cell + 1) % period
    # Rows first, then columns, so the whole grid comes out of two lerps.
    top = lattice[np.ix_(c0, c0)] * (1 - f)[None, :] + lattice[np.ix_(c0, c1)] * f[None, :]
    bottom = lattice[np.ix_(c1, c0)] * (1 - f)[None, :] + lattice[np.ix_(c1, c1)] * f[None, :]
    return top * (1 - f)[:, None] + bottom * f[:, None]


def fbm(size, period, octaves, rng):
    total = np.zeros((size, size))
    norm = 0.0
    amplitude = 0.5
    for octave in range(octaves):
        total += value_noise(size, period * 2 ** octave, rng) * amplitude
        norm += amplitude
        amplitude *= 0.5
    return total / norm


def worley(size, cells, rng):
    """One minus the distance to the nearest of a wrapping grid of jittered points: round lumps, as sfeCloudPuffs."""
    points = rng.random((cells, cells, 2))
    t = np.arange(size) / size * cells
    cell = np.floor(t).astype(int)
    f = t - cell
    nearest = np.full((size, size), 8.0)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            py = points[(cell[:, None] + dy) % cells, (cell[None, :] + dx) % cells, 0] + dy
            px = points[(cell[:, None] + dy) % cells, (cell[None, :] + dx) % cells, 1] + dx
            dyy = py - f[:, None]
            dxx = px - f[None, :]
            nearest = np.minimum(nearest, dyy * dyy + dxx * dxx)
    return 1.0 - np.clip(np.sqrt(nearest) * 1.35, 0.0, 1.0)


def bumps(size, cells, rng):
    """Round domes on a wrapping grid: one over each point, falling off to nothing between them. A cumulus is a heap of
    these, which is what the flat ridges of plain noise never look like."""
    return np.clip(worley(size, cells, rng), 0.0, 1.0) ** 0.65


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = np.random.default_rng(20260917)
    # Where the clouds are: the same frequency the shader used, with more octaves on top of it.
    where = fbm(SIZE, 4, 5, rng)
    # Flattened to a straight distribution: every value from 0 to 1 covers the same share of the tile, so the shader's
    # threshold reads directly as how much of the sky is under cloud. Noise left as it comes is a bell, and the same
    # threshold then covers far more of the sky than it says.
    flat = np.empty(where.size)
    flat[np.argsort(where, axis=None)] = np.arange(where.size) / (where.size - 1)
    where = flat.reshape(where.shape)
    # The heap itself: domes over domes, coarse to fine. This is what gives a cloud its cauliflower edge.
    lumps = bumps(SIZE, 14, rng) * 0.5 + bumps(SIZE, 32, rng) * 0.32 + bumps(SIZE, 68, rng) * 0.18
    lumps = (lumps - lumps.min()) / max(lumps.max() - lumps.min(), 1e-6)
    curl = bumps(SIZE, 120, rng) * 0.6 + bumps(SIZE, 210, rng) * 0.4

    # Left as they come out, not stretched to the full range: the shader's threshold for how much of the sky is
    # covered is written against these values, and stretching them would drown the sky in cloud.
    rgba = np.stack([np.clip(where, 0.0, 1.0), np.clip(lumps, 0.0, 1.0), np.clip(curl, 0.0, 1.0), np.ones_like(where)], axis=-1)
    image = Image.fromarray((rgba * 255.0 + 0.5).astype(np.uint8), 'RGBA')
    image.save(os.path.join(OUT, 'clouds.png'))
    print(f'clouds.png: {SIZE}x{SIZE}, one tile {SIDE_METRES:.0f} m')


if __name__ == '__main__':
    main()
