"""The fine ripple of the lake, baked as a normal map: bin/textures/water/ripple.png.

    python scripts/water_ripple_texture.py

water.fs sums its waves one by one, which is right up close and impossible far off: past a certain distance a ripple is
smaller than a pixel, and summing it only makes streaks out of aliasing. Those ripples live here instead, where the
hardware's mip chain can average them properly. The map is a sum of small waves running every way, on wave numbers that
are whole numbers of the tile, so it repeats without a seam.

    RG  the slope of the surface, x and z, over [-kSlope, kSlope]
    B   1
    A   1
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'bin', 'textures', 'water')
SIZE = 512
# Metres a tile covers; water.fs must read it at this size.
SIDE_METRES = 4.0
# Ripples from a hand's breadth down to a centimetre or two, which is what is left when the drawn waves have faded.
LONGEST = 0.5
SHORTEST = 0.03
WAVES = 220
# The steepest slope the map can hold; the shader scales it back to what the waves it left out would have had.
SLOPE = 1.0


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = np.random.default_rng(6112026)
    t = np.arange(SIZE) / SIZE
    x = t[None, :] * SIDE_METRES
    z = t[:, None] * SIDE_METRES

    slopeX = np.zeros((SIZE, SIZE))
    slopeZ = np.zeros((SIZE, SIZE))
    for _ in range(WAVES):
        wavelength = np.exp(rng.uniform(np.log(SHORTEST), np.log(LONGEST)))
        # Whole waves to a tile, in both directions: anything else leaves a seam.
        cycles = max(1, round(SIDE_METRES / wavelength))
        angle = rng.uniform(0, 2 * np.pi)
        nx = round(cycles * np.cos(angle))
        nz = round(cycles * np.sin(angle))
        if nx == 0 and nz == 0:
            continue
        k = 2 * np.pi * np.array([nx, nz]) / SIDE_METRES
        length = np.hypot(*k)
        # Steepness the same at every size, so the slope each wave adds does not depend on how long it is.
        steepness = 0.05 * (0.6 + 0.8 * rng.random())
        amplitude = steepness / length
        phase = k[0] * x + k[1] * z + rng.uniform(0, 2 * np.pi)
        common = amplitude * np.cos(phase)
        slopeX += k[0] * common
        slopeZ += k[1] * common

    scale = SLOPE / max(float(np.percentile(np.abs(np.stack([slopeX, slopeZ])), 99.5)), 1e-6)
    encoded = np.stack([
        np.clip(slopeX * scale, -1, 1) * 0.5 + 0.5,
        np.clip(slopeZ * scale, -1, 1) * 0.5 + 0.5,
        np.ones((SIZE, SIZE)),
        np.ones((SIZE, SIZE)),
    ], axis=-1)
    Image.fromarray((encoded * 255.0 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'ripple.png'))
    print(f'ripple.png: {SIZE}x{SIZE}, one tile {SIDE_METRES} m, slopes up to {SLOPE}')


if __name__ == '__main__':
    main()
