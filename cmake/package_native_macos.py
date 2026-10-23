"""Close the native bundle's Mach-O dependency graph and sign its local result."""
import argparse
import json
import pathlib
import re
import shutil
import subprocess


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def dependencies(binary):
    return [line.strip().split(" (", 1)[0] for line in run("otool", "-L", str(binary)).splitlines()[1:]]


def rpaths(binary):
    output = run("otool", "-l", str(binary)).splitlines()
    found = []
    for index, line in enumerate(output):
        if line.strip() == "cmd LC_RPATH":
            found.append(output[index + 2].strip().split("path ", 1)[1].split(" (offset", 1)[0])
    return list(dict.fromkeys(found))


def system(name):
    return name.startswith(("/System/Library/", "/usr/lib/"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", type=pathlib.Path, required=True)
    parser.add_argument("--prefix", type=pathlib.Path, required=True)
    args = parser.parse_args()
    bundle, prefix = args.bundle.resolve(), args.prefix.resolve()
    executable = bundle / "Contents/MacOS/Congregation"
    frameworks = bundle / "Contents/Frameworks"
    frameworks.mkdir(parents=True, exist_ok=True)
    queue = [(executable, executable)]
    completed = set()
    report = []
    while queue:
        binary, source = queue.pop(0)
        if binary in completed:
            continue
        completed.add(binary)
        source_paths = rpaths(source)
        linked = dependencies(binary)
        for dependency in linked:
            if system(dependency):
                continue
            if "Qt" in dependency:
                raise RuntimeError("Qt dependency in native bundle: " + dependency)
            filename = pathlib.Path(dependency).name
            if binary != executable and filename == binary.name:
                continue  # dylib install id, not another dependency
            candidates = [prefix / "lib" / filename, frameworks / filename]
            if dependency.startswith("@loader_path/"):
                candidates.insert(0, source.parent / dependency.removeprefix("@loader_path/"))
            elif dependency.startswith("@executable_path/"):
                candidates.insert(0, executable.parent / dependency.removeprefix("@executable_path/"))
            elif dependency.startswith("@rpath/"):
                suffix = dependency.removeprefix("@rpath/")
                for entry in source_paths:
                    entry = entry.replace("@loader_path", str(source.parent)).replace("@executable_path", str(executable.parent))
                    candidates.append(pathlib.Path(entry) / suffix)
            elif dependency.startswith("/"):
                candidates.insert(0, pathlib.Path(dependency))
            located = next((path.resolve() for path in candidates if path.is_file()), None)
            if located is None:
                raise RuntimeError("Unresolved native dependency: " + dependency)
            if not located.is_relative_to(prefix) and not located.is_relative_to(frameworks):
                raise RuntimeError("Dependency escapes the installed native kit: " + str(located))
            destination = frameworks / filename
            if located != destination.resolve() or not destination.exists():
                shutil.copy2(located, destination)
            subprocess.run(["chmod", "u+w", str(destination)], check=True)
            if dependency != "@rpath/" + filename:
                subprocess.run(["install_name_tool", "-change", dependency, "@rpath/" + filename, str(binary)], check=True, capture_output=True)
            queue.append((destination, located))
        if binary != executable and (not linked or linked[0] != "@rpath/" + binary.name):
            subprocess.run(["install_name_tool", "-id", "@rpath/" + binary.name, str(binary)], check=True, capture_output=True)
        for entry in rpaths(binary):
            if entry.startswith("/"):
                subprocess.run(["install_name_tool", "-delete_rpath", entry, str(binary)], check=True, capture_output=True)
        required = "@executable_path/../Frameworks" if binary == executable else "@loader_path"
        if required not in rpaths(binary):
            subprocess.run(["install_name_tool", "-add_rpath", required, str(binary)], check=True, capture_output=True)
        versions = re.findall(r"minos\s+(\d+(?:\.\d+){1,2})", run("xcrun", "vtool", "-show-build", str(binary)))
        if any((tuple(map(int, version.split("."))) + (0,0))[:3] > (12, 0, 0) for version in versions):
            raise RuntimeError("A native dependency requires a newer OS: " + str(binary) + " " + str(versions))
        report.append({"binary": str(binary.relative_to(bundle)), "minos": versions, "dependencies": dependencies(binary), "rpaths": rpaths(binary)})
    notices = prefix / "share/licenses"
    if notices.is_dir():
        shutil.copytree(notices, bundle / "Contents/Resources/licenses", dirs_exist_ok=True)
    for binary in sorted(completed - {executable}):
        subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True, capture_output=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(bundle)], check=True, capture_output=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True, capture_output=True)
    output = bundle.parent / "native-bundle-dependencies.json"
    output.write_text(json.dumps({"qt": False, "minimum_macos": "12.0", "binaries": report}, indent=2) + "\n")
    print("NATIVE_BUNDLE_OK", len(report), "Mach-O images; closed dependency graph; ad-hoc signature verified")


if __name__ == "__main__":
    main()
