"""Exercise the real settings file across two isolated test processes."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> None:
    executable = Path(sys.argv[1]).resolve(strict=True)
    resources = Path(sys.argv[2]).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="tool-preferences-restart-", dir=resources) as directory:
        subprocess.run([str(executable), "--write-restart-fixture", directory], check=True)
        saved = Path(directory) / "tool-preferences.bin"
        if not saved.is_file() or not saved.stat().st_size:
            raise RuntimeError("First process did not publish a settings file")
        subprocess.run([str(executable), "--read-restart-fixture", directory], check=True)
    print("TOOL_PREFERENCES_RESTART_OK")


if __name__ == "__main__":
    main()
