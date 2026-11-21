"""Encode the approved boot GIF as palette-based RGB565 frame deltas.

This build-time tool uses Pillow and NumPy. The generated .vpb.b64 file is
checked into the repository; the ESP-IDF build only needs Python's standard
library to decode it.
"""
from pathlib import Path
import base64
import hashlib
import struct
import sys

import numpy as np
from PIL import Image, ImageSequence


WIDTH = 360
HEIGHT = 360


def pack(indices):
    """Pack palette indices with small literal and repeated-value runs."""
    out = bytearray()
    i = 0
    while i < len(indices):
        run = 1
        while (i + run < len(indices) and indices[i + run] == indices[i]
               and run < 128):
            run += 1
        if run >= 3:
            out.extend((0x80 | (run - 1), int(indices[i])))
            i += run
            continue

        start = i
        i += run
        while i < len(indices) and i - start < 128:
            if (i + 2 < len(indices)
                    and indices[i] == indices[i + 1] == indices[i + 2]):
                break
            i += 1
        count = i - start
        out.append(count - 1)
        out.extend(indices[start:i])
    return out


def encode(source, destination):
    source = Path(source)
    destination = Path(destination)
    image = Image.open(source)

    frames = []
    delays = []
    for frame in ImageSequence.Iterator(image):
        frames.append(frame.convert("RGB").resize(
            (WIDTH, HEIGHT), Image.Resampling.LANCZOS))
        delays.append(int(frame.info.get("duration", image.info.get("duration", 80))))
    if not frames or len(frames) > 0xFFFF:
        raise SystemExit("Boot animation frame count is outside VPB limits")
    if len(set(delays)) != 1 or delays[0] <= 0 or delays[0] > 0xFFFF:
        raise SystemExit("VPB currently requires one fixed, nonzero frame delay")

    # Build a shared palette from widely spaced frames to retain important
    # highlights and dark detail throughout the animation.
    sample_ids = sorted({round(i * (len(frames) - 1) / 3) for i in range(4)})
    contact = Image.new("RGB", (WIDTH * len(sample_ids), HEIGHT))
    for column, frame_id in enumerate(sample_ids):
        contact.paste(frames[frame_id], (column * WIDTH, 0))
    palette_image = contact.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
    palette_rgb = palette_image.getpalette()
    palette_rgb += [0] * (768 - len(palette_rgb))
    palette = []
    for index in range(256):
        red, green, blue = palette_rgb[index * 3:index * 3 + 3]
        palette.append(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3))

    output = bytearray(struct.pack(
        "<4sHHHHI", b"VPB1", WIDTH, HEIGHT, len(frames), delays[0], 0))
    output.extend(struct.pack("<256H", *palette))
    previous = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)

    for frame in frames:
        indices = np.asarray(
            frame.quantize(palette=palette_image, dither=Image.Dither.NONE),
            dtype=np.uint8)
        body = bytearray()
        cursor = 0
        for y in range(HEIGHT):
            changed = np.flatnonzero(indices[y] != previous[y])
            if not len(changed):
                continue
            x0, x1 = int(changed[0]), int(changed[-1]) + 1
            position = y * WIDTH + x0
            skip = position - cursor
            while skip > 0xFFFF:
                body.extend(struct.pack("<HHH", 0xFFFF, 0, 0))
                skip -= 0xFFFF
            encoded = pack(indices[y, x0:x1])
            if len(encoded) > 0xFFFF:
                raise SystemExit("Encoded row exceeds VPB row limit")
            body.extend(struct.pack("<HHH", skip, x1 - x0, len(encoded)))
            body.extend(encoded)
            cursor = position + x1 - x0
        output.extend(struct.pack("<I", len(body)))
        output.extend(body)
        previous = indices

    destination.write_bytes(output)
    b64_path = destination.with_suffix(destination.suffix + ".b64")
    b64_path.write_bytes(base64.encodebytes(output))
    print(f"Encoded {len(frames)} frames at {delays[0]} ms; "
          f"{len(output)} bytes; SHA256 {hashlib.sha256(output).hexdigest()}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: encode_boot.py SOURCE.gif DESTINATION.vpb")
    encode(sys.argv[1], sys.argv[2])
