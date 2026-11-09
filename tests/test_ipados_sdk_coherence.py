"""A staged SDK must reject runtime or dependency changes after compilation."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ipados_sdk_coherence', ROOT / 'tools/ipados_sdk_coherence.py')
coherence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coherence)

class Coherence(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'resources')
        self.root = Path(self.temp.name)
        self.prefix = self.root / 'prefix'
        self.work = self.root / 'build'
        self.groups = {name: [f'lib/{name}.a'] for name in ('base', 'files', 'network', 'canvas', 'gui')}
        for paths in self.groups.values():
            path = self.prefix / paths[0]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(path.name.encode())
        self.groups_patch = patch.object(coherence, 'GROUPS', self.groups)
        self.groups_patch.start()
    def tearDown(self):
        self.groups_patch.stop()
        self.temp.cleanup()
    def stage_all(self):
        for stage in ('files', 'network', 'canvas', 'gui'):
            coherence.record(self.prefix, self.work, stage)
    def test_rejects_changed_runtime_or_dependency(self):
        self.stage_all()
        coherence.verify(self.prefix, self.work)
        (self.prefix / 'lib/base.a').write_bytes(b'new core')
        with self.assertRaisesRegex(RuntimeError, 'base'):
            coherence.verify(self.prefix, self.work)
    def test_rejects_missing_changed_or_interrupted_stage(self):
        self.stage_all()
        (self.prefix / 'lib/network.a').write_bytes(b'new network')
        with self.assertRaisesRegex(RuntimeError, 'network'):
            coherence.verify(self.prefix, self.work)
        self.stage_all()
        coherence.invalidate(self.work, 'network')
        with self.assertRaisesRegex(RuntimeError, 'network'):
            coherence.verify(self.prefix, self.work)

if __name__ == '__main__':
    unittest.main()
