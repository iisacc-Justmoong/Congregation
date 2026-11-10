"""Inspect declarative LVUI calls for source-boundary regression tests."""
import re


def _mask_literals_and_comments(source):
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda match: re.sub(r'[^\n]', ' ', match.group()), source)


def contains_html_syntax(source):
    # Local LVUI scripts may assign to a variable named "style".
    code = _mask_literals_and_comments(source)
    return bool(re.search(r'</?[A-Za-z][A-Za-z0-9_:-]*(?=[\s/>])', code))


def matching(source, start, opening, closing):
    if start >= len(source) or source[start] != opening:
        raise ValueError(f"Expected {opening} at {start}")
    depth = 0
    quote = None
    escaped = False
    for index in range(start, len(source)):
        char = source[index]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char in "\"'":
            quote = char
        elif char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"Unclosed {opening} at {start}")


def _body_inputs(source):
    """Collect only this View's inputs, excluding nested View inputs."""
    code = _mask_literals_and_comments(source)
    pairs = {'(': ')', '[': ']', '{': '}'}
    stack = []
    inputs = []
    position = 0
    while position < len(code):
        if not stack and (code[position].isalpha() or code[position] == '_'):
            field = re.match(r'[A-Za-z_][A-Za-z0-9_]*[ \t]*:', code[position:])
            if field:
                start = position
                position += field.end()
                nested = []
                while position < len(code):
                    char = code[position]
                    if char in pairs:
                        nested.append(pairs[char])
                    elif nested and char == nested[-1]:
                        nested.pop()
                    elif not nested and char in ',\n':
                        break
                    position += 1
                inputs.append(source[start:position])
                continue
        char = code[position]
        if char in pairs:
            stack.append(pairs[char])
        elif stack and char == stack[-1]:
            stack.pop()
        position += 1
    return '\n'.join(inputs)


def components(source, name):
    code = _mask_literals_and_comments(source)
    for match in re.finditer(rf"(?<![A-Za-z0-9_.]){re.escape(name)}\b", code):
        position = match.end()
        while position < len(code) and code[position].isspace():
            position += 1
        arguments = ""
        if position < len(code) and code[position] == "(":
            end = matching(code, position, "(", ")")
            arguments = source[position + 1:end]
            position = end + 1
            while position < len(code) and code[position].isspace():
                position += 1
        body = ""
        if position < len(code) and code[position] == "{":
            body_end = matching(code, position, "{", "}")
            body = source[position + 1:body_end]
            arguments += '\n' + _body_inputs(body)
        yield arguments, body


def component(source, name, identifier):
    for arguments, body in components(source, name):
        if re.search(rf'\bid\s*:\s*"{re.escape(identifier)}"', arguments):
            return arguments, body
    return None
