"""Use Xcode's signed-in account to notarize a signed installer, preserving the app."""
from contextlib import contextmanager
import datetime
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import time
import zipfile

from installer_support import APP, BUILD, IDENTIFIER, run


def file_hashes(app):
    return {str(path.relative_to(app)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in app.rglob("*") if path.is_file() and not path.is_symlink()}


@contextmanager
def preserve_application(app, work):
    """Restore the signed source even when Xcode or the network fails."""
    backup = work / "source-application.zip"
    hashes = file_hashes(app)
    run("ditto", "-c", "-k", "--keepParent", app, backup)
    with zipfile.ZipFile(backup) as archive:
        if archive.testzip() is not None:
            raise ValueError("Invalid application backup")
    try:
        yield
    finally:
        if app.exists():
            shutil.rmtree(app)
        run("ditto", "-x", "-k", backup, app.parent)
        if file_hashes(app) != hashes:
            raise ValueError("Signed source changed after Xcode notarization")


def notarize(package, app=APP, work=None, app_identity=None, installer_identity=None):
    work = work or BUILD / "installer/xcode"
    work.mkdir(parents=True, exist_ok=True)
    app_identity = app_identity or "Developer ID Application: MUYEONG YUN (5U49ST9XZH)"
    installer_identity = installer_identity or "Developer ID Installer: MUYEONG YUN (5U49ST9XZH)"
    # Xcode's Organizer submits applications. Include the finished, signed PKG as
    # a nested container for that submission; ship neither this transport copy
    # nor its extra resource. Apple issues a separate ticket for the nested PKG.
    archive = work / "Congregation.xcarchive"
    products = archive / "Products/Applications"
    products.mkdir(parents=True, exist_ok=True)
    if (products / app.name).exists():
        raise ValueError("An earlier Xcode archive still contains an application")
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    if info.get("CFBundleIdentifier") != IDENTIFIER:
        raise ValueError("Only the canonical Congregation app may be submitted")
    properties = {"ApplicationPath": "Applications/" + app.name,
                  "Architectures": run("lipo", "-archs", app / "Contents/MacOS/Congregation").strip().split(),
                  "CFBundleIdentifier": info["CFBundleIdentifier"],
                  "CFBundleShortVersionString": info["CFBundleShortVersionString"],
                  "CFBundleVersion": info["CFBundleVersion"], "SigningIdentity": app_identity,
                  "Team": "5U49ST9XZH"}
    (archive / "Info.plist").write_bytes(plistlib.dumps({"ApplicationProperties": properties,
        "ArchiveVersion": 2, "CreationDate": datetime.datetime.utcnow(),
        "Name": "Congregation", "SchemeName": "Congregation"}))
    options = work / "ExportOptions.plist"
    options.write_bytes(plistlib.dumps({"method": "developer-id", "destination": "upload",
        "signingStyle": "manual", "teamID": "5U49ST9XZH", "signingCertificate": app_identity,
        "installerSigningCertificate": installer_identity, "manageAppVersionAndBuildNumber": False}))
    destination = products / app.name
    with preserve_application(app, work):
        try:
            payload = app / "Contents/SharedSupport/Congregation-installer.pkg"
            if payload.exists():
                raise ValueError("Temporary notarization resource already exists")
            payload.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(package, payload)
            run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", app_identity, app)
            app.rename(destination)
            print("Submitting installer through Xcode's signed-in account", flush=True)
            result = subprocess.run(["xcodebuild", "-exportArchive", "-archivePath", str(archive),
                "-exportPath", str(work / "export"), "-exportOptionsPlist", str(options),
                "-allowProvisioningUpdates"], capture_output=True, text=True, timeout=600)
            (work / "upload.log").write_text(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError("Xcode submission failed; see " + str(work / "upload.log"))
        finally:
            # Xcode creates a re-signed submission bundle itself. Preserve it as
            # ZIP evidence, then remove these temporary directories so build/
            # has one physical application when the source is restored.
            for submitted in (archive / "Submissions").glob("*/" + app.name):
                run("ditto", "-c", "-k", "--keepParent", submitted,
                    work / ("submission-" + submitted.parent.name + ".zip"))
                shutil.rmtree(submitted)
            if destination.exists():
                shutil.rmtree(destination)
    run("codesign", "--verify", "--deep", "--strict", app)
    print("Xcode upload complete; source app restored; waiting for installer ticket", flush=True)
    deadline = time.monotonic() + 600
    while True:
        result = subprocess.run(["xcrun", "stapler", "staple", str(package)], capture_output=True, text=True)
        (work / "staple.log").write_text(result.stdout + result.stderr)
        if result.returncode == 0:
            break
        if time.monotonic() >= deadline:
            raise RuntimeError("Installer ticket is not available; see " + str(work / "staple.log"))
        time.sleep(20)
    (work / "ticket-validation.log").write_text(run("xcrun", "stapler", "validate", package))
    (work / "gatekeeper.log").write_text(run("spctl", "--assess", "--type", "install", "--verbose=4", package))
    submitted = plistlib.loads((archive / "Info.plist").read_bytes()).get("Distributions", [])
    return {"status": "Accepted", "method": "Xcode Developer ID upload", "ticket_stapled": True,
            "gatekeeper": "accepted", "distribution_identifier": submitted[-1]["identifier"] if submitted else None,
            "logs": str(work)}


if __name__ == "__main__":
    package = BUILD / "Congregation.pkg"
    report_path = BUILD / "installer/verification.json"
    report = json.loads(report_path.read_text())
    if hashlib.sha256(package.read_bytes()).hexdigest() != report["sha256"]:
        raise ValueError("Installer differs from its verified package report")
    report["notarization"] = notarize(package)
    report["gatekeeper_ready"] = True
    report["sha256"] = hashlib.sha256(package.read_bytes()).hexdigest()
    report["bytes"] = package.stat().st_size
    (BUILD / "Congregation.pkg.sha256").write_text(report["sha256"] + "  Congregation.pkg\n")
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print("CONGREGATION_XCODE_NOTARIZATION_OK " + str(package), flush=True)
