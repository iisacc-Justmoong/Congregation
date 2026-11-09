"""Verify that iPad SDK consumers were built against the staged dependencies."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

STAGES = ('files', 'network', 'canvas', 'gui')
GROUPS = {
    # Runtime public layouts and linked archives define the ABI used by these stages.
    'base': ['include/iiSDKCore/Runtime', 'lib/libiiSDKCore.a', 'lib/libiiSDKCoreEmbedded.a',
             'lib/libiiSDKCoreObservation.a', 'lib/libiiJson.a', 'lib/libiiColorManager.a',
             'lib/libLVUI.a', 'lib/libLVUINative.a'],
    'files': ['lib/libiiFileProvider*.a'],
    'network': ['lib/libiiNetwork.a', 'lib/libiiAccountManagerNative.a', 'lib/libiiUpdateManagerNative.a'],
    'canvas': ['lib/libiiPaintEngine*.a', 'lib/libiiSharedCanvasNative.a',
               'lib/libiiGraphicAdjust.a', 'lib/libiiVisualCodec*.a'],
    'gui': ['lib/libiiGUI.a'],
}

def fingerprint(prefix: Path, group: str) -> str:
    digest = hashlib.sha256()
    paths = set()
    for pattern in GROUPS[group]:
        matched = list(prefix.glob(pattern))
        if not matched:
            raise RuntimeError(f'iPad SDK {group} artifact is missing: {pattern}')
        for path in matched:
            paths.update(p for p in path.rglob('*') if p.is_file()) if path.is_dir() else paths.add(path)
    for path in sorted(paths):
        name = path.relative_to(prefix).as_posix().encode()
        digest.update(len(name).to_bytes(8, 'big'))
        digest.update(name)
        with path.open('rb') as stream:
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
    return digest.hexdigest()

def invalidate(work: Path, stage: str) -> None:
    start = 0 if stage == 'base' else STAGES.index(stage)
    for name in STAGES[start:]:
        (work / 'generation' / (name + '.json')).unlink(missing_ok=True)

def record(prefix: Path, work: Path, stage: str) -> None:
    names = ('base',) + STAGES[:STAGES.index(stage) + 1]
    values = {name: fingerprint(prefix, name) for name in names}
    directory = work / 'generation'
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / (stage + '.json')
    temporary = target.with_suffix('.partial')
    temporary.write_text(json.dumps({'stage': stage, 'groups': values}, sort_keys=True) + '\n')
    temporary.replace(target)

def verify(prefix: Path, work: Path) -> None:
    current = {name: fingerprint(prefix, name) for name in ('base',) + STAGES}
    for stage in STAGES:
        path = work / 'generation' / (stage + '.json')
        try:
            document = json.loads(path.read_text())
        except (OSError, ValueError) as error:
            raise RuntimeError(f'iPad SDK {stage} generation record is missing or invalid') from error
        expected = ('base',) + STAGES[:STAGES.index(stage) + 1]
        for group in expected:
            if document.get('groups', {}).get(group) != current[group]:
                raise RuntimeError(f'iPad SDK {stage} was built against a different {group} generation')
