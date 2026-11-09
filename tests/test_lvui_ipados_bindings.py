"""Guard the app's bindings against obsolete LVUI native widget types."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / 'App' / 'Native'
old = re.compile(r'lvui::native::Native(?:Button|CheckBox|Slider|InputField|ComboBox|DropdownButton|Menu(?:Item)?|SegmentedControl|Popover|EventListener|Icon|Card|Material|SearchField|ColorSurface|ColorPickerButton)\b')
violations = [f'{p.name}:{n}' for p in root.glob('*') if p.suffix in ('.h', '.cpp') for n, line in enumerate(p.read_text().splitlines(), 1) if old.search(line)]
assert not violations, 'Obsolete LVUI native bindings: ' + ', '.join(violations)
assert (root / 'LVUIHostBindings.h').is_file()
cmake = (root.parents[1] / 'CMakeLists.txt').read_text()
assert 'examples/Integrations/App/ColorSelectionSurface.cpp' in cmake
assert 'iiColorManager::iiColorManager' in cmake
