"""Copy or verify the authored Dreamscapes catalogue without linking its UI runtime."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "Dreamscapes/src/App/Views/Home/Data/canvas-presets.json"
TARGET = ROOT / "resources/canvas-presets.json"
RECORD = ROOT / "resources/canvas-presets-source.json"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        raw = TARGET.read_bytes()
        record = json.loads(RECORD.read_text(encoding="utf-8"))
        if hashlib.sha256(raw).hexdigest() != record["sha256"]:
            raise SystemExit("Bundled canvas catalogue does not match its source record")
        if args.source.exists() and raw != args.source.read_bytes():
            raise SystemExit("Dreamscapes catalogue changed; review and sync the bundled snapshot")
    else:
        raw = args.source.read_bytes()
        data = json.loads(raw)
        counts = [sum(len(s["presets"]) for s in c["sections"]) for c in data["categories"]]
        if b')CATALOG"' in raw:
            raise SystemExit("Canvas catalogue contains a reserved build delimiter")
        TARGET.write_bytes(raw)
        record = {"source_product": "Dreamscapes", "source": "src/App/Views/Home/Data/canvas-presets.json",
                  "revision": data["revision"], "sha256": hashlib.sha256(raw).hexdigest(),
                  "categories": len(counts), "presets": sum(counts)}
        RECORD.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("CANVAS_CATALOGUE_SYNC_OK " + hashlib.sha256(raw).hexdigest())


if __name__ == "__main__":
    main()
