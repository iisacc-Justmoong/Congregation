"""Check the built bundle's version and build number before packaging."""
from pathlib import Path
import plistlib
import sys

bundle, version, build = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
with (bundle / 'Contents/Info.plist').open('rb') as stream:
    info = plistlib.load(stream)
if (info.get('CFBundleShortVersionString'), info.get('CFBundleVersion')) != (version, build):
    raise SystemExit('Application bundle version does not match the configured release')
print('BUNDLE_VERSION_OK', version, build)
