"""Check the native application source/build boundary without opening a window."""
from pathlib import Path
import re
import sys
from lvui_source_contract import component, components, contains_html_syntax

root = Path(sys.argv[1])
source = root / "App" / "Native"
patterns = (
    re.compile(r"(?:#\s*include\s*[<\"]LVRS/|\blvrs::|\bFRAMEWORK\s+LVRS\b)"),
    re.compile(r"#\s*include\s*[<\"](?:Qt|Q[A-Z][A-Za-z0-9_/]*)"),
    re.compile(r"(?:\.|->)\s*set_button_style\s*\("),
    re.compile(r"\b(?:ii::gui::)?ButtonStyle\s*\{"),
    re.compile(r"\b(?:ii::gui::)?InputStyle\s*\{"),
)
violations = []
for path in (root / "App").rglob("*"):
    if path.is_file() and path.suffix.lower() in {".qml", ".qmlc"}:
        violations.append(f"{path.relative_to(root)}: QML in active application source")
    if path.is_file() and path.suffix in {".h", ".cpp", ".mm"}:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if re.search(r"\b(?:QQml|QQuick|qmlRegister)[A-Za-z0-9_]*\b|[\"'][^\"']+\.qml(?:[\"'])", line):
                violations.append(f"{path.relative_to(root)}:{number}: QML runtime reference")
for path in source.rglob("*"):
    if path.is_file() and path.suffix in {".h", ".cpp", ".lvui"}:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if any(pattern.search(line) for pattern in patterns):
                violations.append(f"{path.relative_to(root)}:{number}")
for path in (source / "views").glob("*.lvui"):
    content = path.read_text(encoding="utf-8")
    if contains_html_syntax(content):
        violations.append(f"{path.relative_to(root)}: HTML-style markup is not LVUI view syntax")
layout = (source / "views/Layout.lvui").read_text()
for name in ("toolRailWidth", "size46", "timelineRowHeight", "size1369", "size1341", "size460", "iconBitmapSize", "size1", "size4", "size5", "size11", "size12", "size13", "iconSize", "iconRasterSize", "timelineHeight", "toolExpandedHeight", "toolExpandedScrollHeight", "toolDynamicsScrollOffset", "toolParameterHeight", "pressureGraphInset", "pressureGraphHeight", "pressureGridStep", "size1_5", "radius1_5", "size4_5", "size5_5", "size38", "size296", "size316", "size336", "size344", "size432"):
    if re.search(rf"\blet\s+{name}\s*:\s*Metric\b", layout):
        violations.append(f"views/Layout.lvui: product-owned {name}")
builder = (source / "OwnedWidgetStyle.cpp").read_text()
icon = re.search(r"ButtonHandle Builder::icon\(.*?(?=\nButtonHandle Builder::button\()", builder, re.S)
if (not icon or "lvui::icon_button_configuration()" not in icon.group() or
        re.search(r"configuration\.(?:icon_size|height|horizontal_padding)\s*=", icon.group())):
    violations.append("OwnedWidgetStyle.cpp: icon button overrides common geometry")
glyph = re.search(r"Widget Builder::glyph\(.*?(?=\nWidget Builder::choice\()", builder, re.S)
if not glyph or "ii::gui::IconView::create" not in glyph.group():
    violations.append("OwnedWidgetStyle.cpp: standalone icon is not owned by LVUI")
segments = re.search(r"Controls::icon_segmented_control\(.*?(?=\nstd::string Builder::id\()", builder, re.S)
if (not segments or "lvui::icon_segmented_configuration()" not in segments.group() or
        re.search(r"options\.(?:background|border|spacing|vertical_padding|horizontal_padding|corner_radius)\s*=", segments.group())):
    violations.append("OwnedWidgetStyle.cpp: segmented control overrides LVUI appearance")
preferences = (source / "views/Preferences.lvui").read_text()
main = (source / "views/Main.lvui").read_text()
setup = (source / "views/CanvasSetup.lvui").read_text()
for name, content, host_id in (("Main", main, "workspace-host"),
                               ("CanvasSetup", setup, "canvas-setup-host")):
    native = component(content, "NativeView", host_id)
    if not native or not re.search(r'\bsource\s*:\s*host\.source\b', native[0]):
        violations.append(f"{name}.lvui: native surface is not a child of the LVUI view")
placeholder = component(setup, "NativeView", "setup-lvui-scope")
if placeholder and re.search(r'\bvisible\s*:\s*false\b', placeholder[0]):
    violations.append("CanvasSetup.lvui: hidden placeholder instead of the active view")
setup_implementation = (source / "CanvasSetupWindow.cpp").read_text()
if re.search(r'CanvasSetupWindow::build_content\s*\(', setup_implementation):
    violations.append("CanvasSetupWindow.cpp: chooser bypasses the LVUI view")
sections = component(preferences, "LabelSegmentedControl", "preferences-sections")
if not sections:
    violations.append("Preferences.lvui: shared segmented control missing")
else:
    opening, body = sections
    tabs = [arguments for arguments, _ in components(body, "LabelButton")]
    if (re.search(r"\b(?:spacing|horizontalPadding|verticalPadding|backgroundColor|borderColor)\s*:", opening) or
            len(tabs) != 3 or any(not re.search(r"\bselectionStyle\s*:\s*\.segmented\b", tab) or
            re.search(r"\b(?:height|horizontalPadding|selectedColor|tone)\s*:", tab) for tab in tabs)):
        violations.append("Preferences.lvui: tabs override common LVUI appearance")
timeline = (source / "TimelineView.cpp").read_text()
if not re.search(r"row_height\s*=\s*lvui::icon_button_defaults::extent\b", timeline):
    violations.append("TimelineView.cpp: row height does not follow LVUI icon buttons")
cmake = (root / "CMakeLists.txt").read_text()
if re.search(r"find_package\s*\(\s*LVRS\b|\bLVRS::|FRAMEWORK\s+LVRS\b", cmake, re.I):
    violations.append("CMakeLists.txt: LVRS dependency")
if not re.search(r"find_package\s*\(\s*LVUI[^)]*COMPONENTS\s+Native\s+Color", cmake, re.I):
    violations.append("CMakeLists.txt: independent LVUI components are missing")
gui_versions = re.findall(r"find_package\s*\(\s*iiGUI\s+(\d+(?:\.\d+){1,2})", cmake, re.I)
if not gui_versions or any(tuple(int(part) for part in version.split(".")) < (0, 5) for version in gui_versions):
    violations.append("CMakeLists.txt: iiGUI lacks the required typography policy API")
if violations:
    raise SystemExit("Native LVUI boundary violations: " + ", ".join(violations))
print("LVUI_NATIVE_BOUNDARY_OK")
