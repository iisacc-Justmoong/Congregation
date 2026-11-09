"""Shared name boundary for legacy UI dependencies in native artifacts."""
import re


_QT_COMPONENT = re.compile(r"(?:lib)?qt[a-z0-9_]*(?:[._-].*)?", re.IGNORECASE)
_LVRS_COMPONENT = re.compile(
    r"(?:lib)?lvrs(?:(?:core|native|plugin)[a-z0-9_]*)?(?:[._-].*)?", re.IGNORECASE)


def forbidden_ui_dependency(name):
    """Reject known Qt/LVRS names, not similarly named independent libraries."""
    text = str(name)
    # Preserve the existing case-sensitive Qt check, including an embedded
    # Qt marker, while also recognizing lower-case Windows and Unix names.
    if "Qt" in text:
        return True
    parts = text.replace("\\", "/").split("/")
    return any(_QT_COMPONENT.fullmatch(part) or _LVRS_COMPONENT.fullmatch(part) for part in parts)
