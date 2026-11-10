"""Guard authored UI styling; document pixels, model ranges and test oracles are data."""
from pathlib import Path
import re
import sys
import unittest
from lvui_source_contract import component, components, contains_html_syntax

ROOT = Path(__file__).resolve().parents[1]
NUMBER = r"-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?[fF]?"
STYLE_FIELDS = (r"width|height|minimum_width|minimum_height|maximum_width|maximum_height|"
                r"spacing|padding|horizontal_padding|vertical_padding|icon_size|font_size|"
                r"font_weight|line_height|radius|border_width|stroke_width|thumb_diameter|"
                r"track_height|range_inset|virtual_content_height|tint_opacity|intense_opacity|faint_opacity")


def split_arguments(text):
    """Split C++ call/aggregate arguments while preserving nested calls and strings."""
    result, start, stack, quote, escape = [], 0, [], None, False
    for i, c in enumerate(text):
        if quote:
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c in "({[":
            stack.append(c)
        elif c in ")}]":
            if not stack:
                result.append(text[start:i].strip())
                return result
            stack.pop()
        elif c == ',' and not stack:
            result.append(text[start:i].strip())
            start = i + 1
    result.append(text[start:].strip())
    return result


def literal(value):
    return bool(re.fullmatch(NUMBER, value))


def cpp_violations(source):
    found = []
    for match in re.finditer(r"\.(" + STYLE_FIELDS + r")\s*=\s*(" + NUMBER + r")\s*[,}]", source):
        # Full and zero opacity are structural on/off values, not palette entries.
        if match.group(1) in {'tint_opacity', 'intense_opacity', 'faint_opacity'} and float(match.group(2).rstrip('fF')) in (0., 1.):
            continue
        found.append((match.start(), 'numeric style field'))
    # A hexadecimal value displayed as label text is user-visible data, not
    # a color assigned to a UI style. Keep all other literals under the guard.
    label_colors = [m.span('value') for m in re.finditer(
        r'\bb\.label\(\s*[^,\n()]+,\s*"(?P<value>#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?)"', source)]
    for match in re.finditer(r'#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?\b', source):
        if not any(start <= match.start() and match.end() <= end for start, end in label_colors):
            found.append((match.start(), 'literal UI color'))
    for match in re.finditer(r'\.(?:color|fill|hover_fill|pressed_fill|surface_color)\s*=\s*\{\s*' + NUMBER, source):
        found.append((match.start(), 'literal color channels'))
    for match in re.finditer(r'\bInsets\{', source):
        if any(literal(value) for value in split_arguments(source[match.end():])):
            found.append((match.start(), 'numeric inset'))
    calls = {r'\b(?:b\.)?(?:row|column)\(': (2, 3, 4, 5), r'\bb\.label\(': (2, 4),
             r'\bb\.glyph\(': (2,), r'\btext\(': (3, 4)}
    for pattern, positions in calls.items():
        for match in re.finditer(pattern, source):
            args = split_arguments(source[match.end():])
            for i in positions:
                # Zero extent means automatic sizing; zero gaps/padding are tokens.
                if i < len(args) and literal(args[i]) and (float(args[i].rstrip('fF')) != 0 or i >= 4):
                    found.append((match.start(), 'numeric builder style argument'))
    for match in re.finditer(r'\.(?:panels|texts|images)\.push_back\(\{|\bRect\{', source):
        args = split_arguments(source[match.end():])
        if not args:
            continue
        geometry = split_arguments(args[0][1:]) if args[0].startswith('{') else args
        if any(literal(v) and float(v.rstrip('fF')) != 0 for v in geometry[:4]):
            found.append((match.start(), 'numeric drawing bounds'))
        if '.panels.' in match.group() and any(literal(v) and float(v.rstrip('fF')) != 0 for v in args[3:5]):
            found.append((match.start(), 'numeric drawing radius/stroke'))
    return [(source.count('\n', 0, at) + 1, reason) for at, reason in found]


def lvui_violations(source):
    geometry = (r'width|height|minimumWidth|minimumHeight|spacing|padding|fontSize|fontWeight|'
                r'cornerRadius|borderWidth|rowHeight|itemWidth|iconSize|selectionRadius|'
                r'horizontalPadding|verticalPadding|thumbDiameter|trackHeight|rangeInset')
    found = []
    for match in re.finditer(r'\b(?:' + geometry + r')\s*:\s*([^,\n)]+)', source):
        if 'Metric.' not in match[1]:
            found.append('geometry attribute: ' + match[0])
    for match in re.finditer(r'\b(?:color|backgroundColor|selectionColor|surfaceColor)\s*:\s*([^,\n)]+)', source):
        if 'Color.' not in match[1]:
            found.append('color attribute: ' + match[0])
    if re.search(r'#[0-9a-fA-F]{6}\b', source):
        found.append('literal LVUI color')
    return found


class UIStyleTokenTests(unittest.TestCase):
    def test_component_scan_accepts_omitted_empty_sections(self):
        source = '''Popover {
            id: "brush-popup", width: extent(max(1, 2)),
            VStack { Slider { size: .mini, id: "child" }\n Spacer }
        }'''
        popup = component(source, 'Popover', 'brush-popup')
        self.assertIsNotNone(popup)
        inputs, body = popup
        self.assertIn('extent(max(1, 2))', inputs)
        self.assertNotIn('child', inputs)
        sliders = list(components(body, 'Slider'))
        self.assertEqual(len(sliders), 1)
        self.assertRegex(sliders[0][0], r'\bsize\s*:\s*\.mini\b')
        self.assertEqual(len(list(components(body, 'Spacer'))), 1)

    def test_declarative_component_scan_keeps_nested_calls_and_quoted_delimiters(self):
        source = '''Popover(id: "brush-popup", width: extent(max(1, 2))) {
            VStack { Slider(size: .mini, onMoved: { log("})"); vm.apply(value) }) }
        }'''
        popup = component(source, 'Popover', 'brush-popup')
        self.assertIsNotNone(popup)
        opening, body = popup
        self.assertIn('extent(max(1, 2))', opening)
        self.assertEqual(len(list(components(body, 'Slider'))), 1)

    def test_html_guard_distinguishes_tags_from_text_and_script_variables(self):
        self.assertTrue(contains_html_syntax('<span style="color:red">value</span>'))
        self.assertFalse(contains_html_syntax('Label(text: "<span>") // <div>\nPushButton(onClick: { style = 1 })'))

    def test_description_labels_use_the_default_lvui_style(self):
        for name in ('Main.lvui', 'Preferences.lvui'):
            source = (ROOT / 'App/Native/views' / name).read_text(encoding='utf-8')
            self.assertNotRegex(source, r'\bfontSize\s*:\s*Metric\.textDescription\b')
            self.assertNotRegex(source, r'\bcolor\s*:\s*Color\.text\.description\b')

    def test_brush_popup_uses_lvui_mini_sliders_and_scroll_view(self):
        source = (ROOT / 'App/Native/views/Main.lvui').read_text(encoding='utf-8')
        popup = component(source, 'Popover', 'brush-popup')
        self.assertIsNotNone(popup)
        opening, body = popup
        self.assertNotRegex(opening, r'\b(?:surfaceColor|padding|cornerRadius|clip)\s*:')
        sliders = [arguments for arguments, _ in components(body, 'Slider')]
        self.assertEqual(len(sliders), 7)
        self.assertIsNotNone(component(body, 'ScrollView', 'brush-scroll'))
        self.assertNotRegex(body, r'\b(?:fontSize|fontWeight)\s*:')
        for slider in sliders:
            self.assertRegex(slider, r'\bsize\s*:\s*\.mini\b')
            self.assertNotRegex(slider, r'\b(?:height|thumbDiameter|trackHeight|rangeInset)\s*:')

    def test_native_slider_sizes_come_from_lvui(self):
        for name, expected in [('TimelineView.cpp', 2), ('WorkspaceView.cpp', 1)]:
            source = (ROOT / 'App/Native' / name).read_text(encoding='utf-8')
            self.assertEqual(source.count('.size = lvui::SliderSize::mini'), expected)
            self.assertNotRegex(source, r'\.(?:thumb_diameter|track_height|range_inset)\s*=')

    def test_native_icon_buttons_do_not_embed_a_product_palette(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        widgets = (ROOT / 'App/Native/OwnedWidgetStyle.cpp').read_text(encoding='utf-8')
        bindings = (ROOT / 'App/Native/LVUIHostBindings.h').read_text(encoding='utf-8')
        self.assertNotIn('EditorAssets.generated.h', cmake)
        self.assertNotIn('App/Native/EditorIcons.cpp', cmake)
        self.assertIn('lvui_host::Button::create', widgets)
        self.assertRegex(bindings, r'using Button\s*=\s*lvui::native::ButtonBinding<ii::gui::BasicButton,\s*ii::gui::BasicNativeButton')
        self.assertIn('options.icon_name=std::move(source)', widgets)
        self.assertIn('options.icon_source=lvui::design_icon_catalog()', widgets)

    def test_application_has_no_independent_color_palette(self):
        profile = (ROOT / 'App/Native/views/Layout.lvui').read_text(encoding='utf-8')
        self.assertNotRegex(profile, r'(?m)^\s*let\s+\w+\s*:\s*Color\b')
        for path in (ROOT / 'App/Native/views').glob('*.lvui'):
            if path.name != 'Layout.lvui':
                self.assertNotRegex(path.read_text(encoding='utf-8'), r'Color\.(?:popup|secondaryText|memberSelection|panelEdgeResolved)\b')
        palette = (ROOT / 'App/Native/EditorPalette.cpp').read_text(encoding='utf-8')
        self.assertIn('Color::canvas', palette)
        self.assertIn('Color::materialSurfaceDenseEdge', palette)

    def test_style_fields_builder_arguments_and_painting_reject_literals(self):
        for source in ['b.row(parent, "row", 0, 22, 4);', '.font_size = 13,', '.spacing = 0,',
                       '.fill = {1, 1, 1, .5}', 'd.panels.push_back({{0, 0, 32, 24}, ink, ink});',
                       'd.panels.push_back({{0, 0, w, h}, ink, ink, 6, 1});', 'Rect{0, 0, 22, 28}',
                       'Insets{0, Metric::space4, 0, Metric::space4}']:
            with self.subTest(source=source):
                self.assertTrue(cpp_violations(source))

    def test_tokens_and_dynamic_geometry_remain_valid(self):
        source = '''b.row(parent, "row", 0, Metric::size22, Metric::space4);
        .font_size = Metric::textBody, .spacing = Metric::space0,
        .fill = Color::window,
        d.panels.push_back({{0, 0, w, h}, ink, ink, Metric::radius6, Metric::stroke1});
        auto scale = std::clamp(value, 0., 1.);'''
        self.assertEqual(cpp_violations(source), [])

    def test_hex_label_text_is_data_but_style_literals_are_rejected(self):
        self.assertEqual(cpp_violations('b.label(color_row, "#000000", Metric::textDescription);'), [])
        self.assertTrue(cpp_violations('auto fill = "#000000";'))
        self.assertTrue(cpp_violations('b.label(color_row, "#000000", Metric::textDescription); auto fill = "#112233";'))

    def test_lvui_controls_reject_inline_style_values(self):
        self.assertTrue(lvui_violations('Window(width: 1999) { VStack(spacing: 8) {} }'))
        self.assertEqual(lvui_violations('Window(width: Metric.windowWidth) { Slider(from: 0, to: 1, value: 0.5) VStack(spacing: Metric.space8) {} }'), [])

    def test_active_native_views_use_shared_types(self):
        root = ROOT / 'App/Native'
        paths = set(root.glob('*View.cpp'))
        paths.update(root / name for name in ('MainWindow.cpp', 'WidgetStyle.cpp', 'EditorLayout.cpp', 'EditorPalette.cpp', 'ToolPanel.cpp', 'CanvasSetupWindow.cpp', 'Application.cpp', 'WidgetStyle.h', 'EditorPalette.h', 'EditorLayout.h'))
        violations = []
        for path in sorted(paths):
            source = path.read_text(encoding="utf-8")
            if path.name == 'CanvasView.cpp':
                # Remaining code rasterizes the user's document, including text/brush data.
                source = source[:source.index('void CanvasView::cancel(')]
            if path.name == 'Application.cpp':
                # Self-test geometry/color oracles remain independent of style tokens.
                source = source[:source.index('void capture(')]
            violations.extend(f'{path.name}:{line}: {why}' for line, why in cpp_violations(source))
        self.assertEqual(violations, [])

    def test_all_lvui_views_use_shared_profile(self):
        for path in (ROOT / 'App/Native/views').glob('*.lvui'):
            if path.name != 'Layout.lvui':
                with self.subTest(path=path.name):
                    self.assertEqual(lvui_violations(path.read_text(encoding="utf-8")), [])
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding="utf-8")
        calls = re.findall(r'lvui_generate\([^)]*\)', cmake, flags=re.DOTALL)
        views = {name for call in calls for name in re.findall(r'SOURCE App/Native/views/(Main|Preferences|CanvasSetup)\.lvui', call)}
        self.assertEqual(views, {'Main', 'Preferences', 'CanvasSetup'})
        self.assertTrue(all('STYLE_FILE "${_style_profile}"' in c for c in calls))
        self.assertIn('lvui_generate_styles(', cmake)
        style = (ROOT / 'App/Native/Style.h').read_text(encoding="utf-8")
        self.assertIn('CongregationStyle.generated.h', style)
        self.assertNotIn('<LVUI/', style)


if __name__ == '__main__':
    unittest.main()
