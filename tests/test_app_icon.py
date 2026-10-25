"""Verify the editable app-icon source, platform derivatives and actual macOS bundle."""
from pathlib import Path
import hashlib
import json
import plistlib
import struct
import sys
import unittest
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "resources/AppIcon"
CATALOG = ROOT / "packaging/macos/Congregation.xcassets/AppIcon.appiconset"
NS = "{http://www.w3.org/2000/svg}"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_header(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError("Not a PNG image")
    return struct.unpack(">IIBBBBB", data[16:29])


def png_scanlines(data):
    """Ignore PNG metadata and compression differences introduced by iconutil."""
    payloads, offset = [], 8
    while offset < len(data):
        count = struct.unpack(">I", data[offset:offset + 4])[0]
        if data[offset + 4:offset + 8] == b"IDAT":
            payloads.append(data[offset + 8:offset + 8 + count])
        offset += count + 12
    return zlib.decompress(b"".join(payloads))


class AppIconTests(unittest.TestCase):
    def test_master_is_opaque_1024_pixel_rgb(self):
        width, height, depth, color, *_ = png_header((ASSETS / "Congregation.png").read_bytes())
        self.assertEqual((width, height, depth, color), (1024, 1024, 8, 2))

    def test_svg_is_self_contained_vector_with_square_background(self):
        root = ET.parse(ASSETS / "Congregation.svg").getroot()
        self.assertEqual(root.get("viewBox"), "0 0 1024 1024")
        self.assertEqual((root.get("width"), root.get("height")), ("1024", "1024"))
        forbidden = {NS + name for name in ("image", "filter", "foreignObject", "text", "use")}
        self.assertFalse([node.tag for node in root.iter() if node.tag in forbidden])
        self.assertFalse(any("href" in key for node in root.iter() for key in node.attrib))
        background = next(node for node in root.iter() if node.get("id") == "Full-Bleed-Background")
        self.assertEqual(background.tag, NS + "rect")
        self.assertEqual(background.get("rx", "0"), "0")
        self.assertEqual(background.get("ry", "0"), "0")
        self.assertNotIn("stroke", background.attrib)

    def test_ai_is_native_illustrator_document_with_vector_data(self):
        data = (ASSETS / "Congregation.ai").read_bytes()
        self.assertTrue(data.startswith(b"%PDF-"))
        self.assertIn(b"Adobe Illustrator", data)
        self.assertIn(b"AIPrivateData", data)

    def test_all_asset_catalog_slots_have_correct_pixels(self):
        catalog = json.loads((CATALOG / "Contents.json").read_text())
        slots = set()
        for item in catalog["images"]:
            with self.subTest(slot=item):
                size = int(item["size"].split("x")[0])
                scale = int(item["scale"].removesuffix("x"))
                expected = size * scale
                pixels = png_header((CATALOG / item["filename"]).read_bytes())
                self.assertEqual(pixels[:2], (expected, expected))
                slots.add((size, scale))
        self.assertEqual(slots, {(size, scale) for size in (16, 32, 128, 256, 512) for scale in (1, 2)})
        self.assertEqual(digest(CATALOG / "AppIcon-1024.png"), digest(ASSETS / "Congregation.png"))
        self.assertEqual(digest(CATALOG / "icon_512x512@2x.png"), digest(ASSETS / "Congregation.png"))

    def test_icns_contains_1024_pixel_master(self):
        data = (ROOT / "resources/Appicon.icns").read_bytes()
        self.assertEqual(data[:4], b"icns")
        self.assertEqual(struct.unpack(">I", data[4:8])[0], len(data))
        chunks, offset = {}, 8
        while offset < len(data):
            kind, size = struct.unpack(">4sI", data[offset:offset + 8])
            self.assertGreaterEqual(size, 8)
            chunks[kind] = data[offset + 8:offset + size]
            offset += size
        self.assertEqual(offset, len(data))
        self.assertIn(b"ic10", chunks)
        self.assertEqual(png_header(chunks[b"ic10"])[:2], (1024, 1024))
        master = (ASSETS / "Congregation.png").read_bytes()
        self.assertEqual(png_header(chunks[b"ic10"]), png_header(master))
        self.assertEqual(png_scanlines(chunks[b"ic10"]), png_scanlines(master))

    def test_windows_ico_has_valid_small_and_large_images(self):
        data = (ROOT / "resources/Appicon.ico").read_bytes()
        reserved, kind, count = struct.unpack("<HHH", data[:6])
        self.assertEqual((reserved, kind, count), (0, 1, 7))
        sizes = set()
        for index in range(count):
            w, h, colors, reserved, planes, depth, size, offset = struct.unpack(
                "<BBBBHHII", data[6 + index * 16:22 + index * 16])
            width, height = w or 256, h or 256
            self.assertEqual((width, planes, depth), (height, 1, 32))
            self.assertLessEqual(offset + size, len(data))
            self.assertEqual(png_header(data[offset:offset + size])[:2], (width, height))
            sizes.add(width)
        self.assertEqual(sizes, {16, 24, 32, 48, 64, 128, 256})

    def test_manifest_rejects_stale_sources_or_derivatives(self):
        manifest = json.loads((ASSETS / "manifest.json").read_text())
        self.assertEqual(manifest["canvas"], {"width": 1024, "height": 1024, "opaque": True, "corner_radius": 0})
        for section in ("inputs", "outputs"):
            self.assertTrue(manifest[section])
            for name, expected in manifest[section].items():
                with self.subTest(path=name):
                    self.assertEqual(digest(ROOT / name), expected)

    @unittest.skipUnless(sys.platform == "darwin", "macOS bundle verification")
    def test_built_bundle_resolves_and_contains_current_icon(self):
        bundle = ROOT / "build/Congregation.app/Contents"
        if not (bundle / "Info.plist").exists():
            self.skipTest("Build the app to verify its packaged icon")
        with (bundle / "Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        self.assertEqual(info["CFBundleIconFile"], "Appicon.icns")
        self.assertEqual(digest(bundle / "Resources" / info["CFBundleIconFile"]),
                         digest(ROOT / "resources/Appicon.icns"))


if __name__ == "__main__":
    unittest.main()
