"""Shared paths and Apple-tool execution for native packaging workflows."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
APP = BUILD / "Congregation.app"
IDENTIFIER = "com.iisacc.congregation.painter"


def run(*args):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed ({result.returncode}):\n{result.stdout}{result.stderr}")
    return result.stdout + result.stderr
