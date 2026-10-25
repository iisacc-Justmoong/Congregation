"""Package the Illustrator-exported square master into Congregation's platform icons.

Generation uses native macOS image tools; --check is portable and has no dependencies.
All scratch files stay in the canonical build/ directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "resources/AppIcon"
CATALOG = ROOT / "packaging/macos/Congregation.xcassets/AppIcon.appiconset"
SCRATCH = ROOT / "build/app-icon"
SIGNATURE = b"\x89PNG\r\n\x1a\n"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_size(data):
    if data[:8] != SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError("Expected a PNG image")
    return struct.unpack(">IIBBBBB", data[16:29])


def rgba_png(data):
    """Add opaque alpha losslessly for Windows' 32-bit PNG icon entries."""
    width, height, depth, color, compression, filtering, interlace = png_size(data)
    if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
        raise ValueError("Export an opaque, noninterlaced, RGB8 master from Illustrator")
    compressed, at = bytearray(), 8
    while at < len(data):
        count = struct.unpack(">I", data[at:at + 4])[0]
        kind = data[at + 4:at + 8]
        if kind == b"IDAT":
            compressed.extend(data[at + 8:at + 8 + count])
        at += count + 12
    packed = zlib.decompress(compressed)
    stride = width * 3
    if len(packed) != (stride + 1) * height:
        raise ValueError("Unexpected PNG scanline length")
    rows, previous = bytearray(), bytearray(stride)
    for y in range(height):
        base = y * (stride + 1)
        method = packed[base]
        row = bytearray(packed[base + 1:base + 1 + stride])
        for x in range(stride):
            left = row[x - 3] if x >= 3 else 0
            above = previous[x]
            upper_left = previous[x - 3] if x >= 3 else 0
            if method == 0:
                predictor = 0
            elif method == 1:
                predictor = left
            elif method == 2:
                predictor = above
            elif method == 3:
                predictor = (left + above) // 2
            elif method == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError("Unsupported PNG scanline filter")
            row[x] = (row[x] + predictor) & 255
        rows.append(0)
        for x in range(0, stride, 3):
            rows.extend(row[x:x + 3])
            rows.append(255)
        previous = row

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))

    return (SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def check():
    manifest = json.loads((ASSETS / "manifest.json").read_text())
    stale = []
    for section in ("inputs", "outputs"):
        for name, expected in manifest[section].items():
            path = ROOT / name
            if not path.is_file() or sha256(path) != expected:
                stale.append(name)
    if stale:
        raise ValueError("Stale app icons; export in Illustrator and regenerate: " + ", ".join(stale))
    print("APP_ICON_ASSETS_OK master=1024x1024 opaque=true corner_radius=0 derivatives=" +
          str(len(manifest["outputs"])))


def generate():
    if sys.platform != "darwin":
        raise ValueError("Regenerate with Illustrator and macOS sips/iconutil; --check is portable")
    master = ASSETS / "Congregation.png"
    if png_size(master.read_bytes())[:4] != (1024, 1024, 8, 2):
        raise ValueError("The master must be a 1024x1024 opaque RGB8 PNG")
    for name in ("Congregation.svg", "Congregation.ai"):
        if not (ASSETS / name).is_file():
            raise ValueError("Missing editable master: " + name)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    iconset = SCRATCH / "Congregation.iconset"
    iconset.mkdir(exist_ok=True)
    pngs = {1024: master}
    for size in (16, 24, 32, 48, 64, 128, 256, 512):
        target = SCRATCH / ("icon-" + str(size) + ".png")
        subprocess.run(["sips", "-z", str(size), str(size), str(master), "--out", str(target)],
                       check=True, stdout=subprocess.DEVNULL)
        pngs[size] = target
    catalog = json.loads((CATALOG / "Contents.json").read_text())
    outputs = []
    for item in catalog["images"]:
        size = int(item["size"].split("x")[0]) * int(item["scale"].removesuffix("x"))
        target = CATALOG / item["filename"]
        shutil.copyfile(pngs[size], target)
        shutil.copyfile(target, iconset / item["filename"])
        outputs.append(target)
    canonical = CATALOG / "AppIcon-1024.png"
    shutil.copyfile(master, canonical)
    outputs.append(canonical)
    icns = ROOT / "resources/Appicon.icns"
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(SCRATCH / "Appicon.icns")], check=True)
    shutil.copyfile(SCRATCH / "Appicon.icns", icns)
    outputs.append(icns)

    sizes = (16, 24, 32, 48, 64, 128, 256)
    entries, images = bytearray(), bytearray()
    for size in sizes:
        data = rgba_png(pngs[size].read_bytes())
        offset = 6 + 16 * len(sizes) + len(images)
        entries.extend(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        images.extend(data)
    ico = ROOT / "resources/Appicon.ico"
    ico.write_bytes(struct.pack("<HHH", 0, 1, len(sizes)) + entries + images)
    outputs.append(ico)
    manifest = {
        "version": 1,
        "canvas": {"width": 1024, "height": 1024, "opaque": True, "corner_radius": 0},
        "generator": "Adobe Illustrator PNG24; macOS sips and iconutil; PNG-based ICO",
        "inputs": {str(path.relative_to(ROOT)): sha256(path) for path in
                   [ASSETS / "Congregation.svg", ASSETS / "Congregation.ai", master, CATALOG / "Contents.json"]},
        "outputs": {str(path.relative_to(ROOT)): sha256(path) for path in outputs},
    }
    (ASSETS / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    check()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify committed assets without regenerating")
    args = parser.parse_args()
    try:
        check() if args.check else generate()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")
