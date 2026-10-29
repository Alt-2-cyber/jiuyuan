import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RELAY_DIR = ROOT / "application/samples/products/sle_relay"
GROUND_UI_PATH = RELAY_DIR / "ground_ui.c"
GROUND_UI_HEADER_PATH = RELAY_DIR / "ground_ui.h"
RELAY_CMAKE_PATH = RELAY_DIR / "CMakeLists.txt"
RELAY_PATH = RELAY_DIR / "sle_relay.c"


def read_source(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"missing required source file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def macro_value(source: str, name: str) -> str:
    match = re.search(rf"^\s*#define\s+{re.escape(name)}\s+([^\s/]+)", source, re.MULTILINE)
    if match is None:
        raise AssertionError(f"missing macro: {name}")
    value = match.group(1).strip("()")
    return re.sub(r"[UuLl]+$", "", value)


def _blank(character: str) -> str:
    return character if character in "\r\n" else " "


def strip_c_comments_and_char_literals(source: str) -> str:
    output = list(source)
    index = 0
    state = "code"
    while index < len(source):
        character = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if character == '"':
                state = "string"
            elif character == "'":
                output[index] = " "
                state = "char"
            elif character == "/" and following == "/":
                output[index] = output[index + 1] = " "
                index += 1
                state = "line_comment"
            elif character == "/" and following == "*":
                output[index] = output[index + 1] = " "
                index += 1
                state = "block_comment"
        elif state == "string":
            if character == "\\" and following:
                index += 1
            elif character == '"':
                state = "code"
        elif state == "char":
            output[index] = _blank(character)
            if character == "\\" and following:
                index += 1
                output[index] = _blank(source[index])
            elif character == "'":
                state = "code"
        elif state == "line_comment":
            output[index] = _blank(character)
            if character in "\r\n":
                state = "code"
        else:
            output[index] = _blank(character)
            if character == "*" and following == "/":
                index += 1
                output[index] = " "
                state = "code"
        index += 1
    return "".join(output)


def c_executable_code(source: str) -> str:
    cleaned = strip_c_comments_and_char_literals(source)
    output = list(cleaned)
    index = 0
    in_string = False
    while index < len(cleaned):
        character = cleaned[index]
        following = cleaned[index + 1] if index + 1 < len(cleaned) else ""
        if not in_string and character == '"':
            output[index] = " "
            in_string = True
        elif in_string:
            output[index] = _blank(character)
            if character == "\\" and following:
                index += 1
                output[index] = _blank(cleaned[index])
            elif character == '"':
                in_string = False
        index += 1
    return "".join(output)


def _c_function_span(source: str, name: str) -> tuple[int, int]:
    code = c_executable_code(source)
    signature = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{",
        code,
        re.DOTALL,
    )
    if signature is None:
        raise AssertionError(f"missing function definition: {name}")
    opening = code.find("{", signature.start())
    depth = 0
    for index in range(opening, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return opening + 1, index
    raise AssertionError(f"unterminated function definition: {name}")


def extract_c_function_body(source: str, name: str) -> str:
    start, end = _c_function_span(source, name)
    return strip_c_comments_and_char_literals(source)[start:end]


def c_function_names(source: str) -> list[str]:
    code = c_executable_code(source)
    keywords = {"if", "for", "while", "switch"}
    names = re.findall(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", code, re.DOTALL)
    return [name for name in names if name not in keywords]


def c_function_bodies_executable(source: str) -> str:
    return "\n".join(
        c_executable_code(extract_c_function_body(source, name))
        for name in c_function_names(source)
    )


def has_exact_rssi_callback_assignment(source: str) -> bool:
    code = c_executable_code(source)
    return re.search(
        r"\bg_conn_cbks\s*\.\s*read_rssi_cb\s*=\s*&?\s*relay_read_rssi_cb\s*;",
        code,
    ) is not None


def explicit_root_regions(source: str) -> list[tuple[int, int]]:
    cleaned = strip_c_comments_and_char_literals(source)
    root = "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT"
    stack: list[dict[str, object]] = []
    regions: list[tuple[int, int]] = []
    offset = 0
    for line in cleaned.splitlines(keepends=True):
        text = line.rstrip("\r\n")
        opening = re.match(r"^\s*#\s*(if|ifdef|ifndef)\b(.*)$", text)
        if opening is not None:
            exact_root = re.fullmatch(
                rf"\s*#\s*if\s+defined\s*\(\s*{root}\s*\)\s*",
                text,
            ) is not None
            stack.append({"root": exact_root, "start": offset + len(line)})
        elif re.match(r"^\s*#\s*(elif|else)\b", text):
            if not stack:
                raise AssertionError("unmatched preprocessor branch")
            if stack[-1]["root"]:
                regions.append((int(stack[-1]["start"]), offset))
                stack[-1]["root"] = False
        elif re.match(r"^\s*#\s*endif\b", text):
            if not stack:
                raise AssertionError("unmatched #endif")
            frame = stack.pop()
            if frame["root"]:
                regions.append((int(frame["start"]), offset))
        offset += len(line)
    if stack:
        raise AssertionError("unterminated preprocessor condition")
    return regions


def strip_cmake_comments(source: str) -> str:
    output = list(source)
    in_string = False
    index = 0
    while index < len(source):
        character = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if character == '"':
            in_string = not in_string
        elif character == "\\" and in_string and following:
            index += 1
        elif character == "#" and not in_string:
            while index < len(source) and source[index] not in "\r\n":
                output[index] = " "
                index += 1
            continue
        index += 1
    return "".join(output)


def cmake_condition_is_root_only(condition: str) -> bool:
    root = r"CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT"
    accepted = (
        rf"\s*{root}\s*",
        rf"\s*DEFINED\s+{root}\s*",
        rf'\s*DEFINED\s+{root}\s+OR\s+"{root}\s*=\s*y"\s+'
        rf"IN_LIST\s+DEFINES\s*",
    )
    return any(re.fullmatch(pattern, condition, re.IGNORECASE) is not None for pattern in accepted)


def _cmake_control_commands(source: str) -> list[tuple[str, str, int, int]]:
    cleaned = strip_cmake_comments(source)
    syntax = list(cleaned)
    in_string = False
    index = 0
    while index < len(cleaned):
        character = cleaned[index]
        following = cleaned[index + 1] if index + 1 < len(cleaned) else ""
        if not in_string and character == '"':
            syntax[index] = " "
            in_string = True
        elif in_string:
            syntax[index] = _blank(character)
            if character == "\\" and following:
                index += 1
                syntax[index] = _blank(cleaned[index])
            elif character == '"':
                in_string = False
        index += 1
    code = "".join(syntax)

    commands: list[tuple[str, str, int, int]] = []
    cursor = 0
    command_pattern = re.compile(r"(?im)^[ \t]*(elseif|endif|else|if)\b")
    while True:
        match = command_pattern.search(code, cursor)
        if match is None:
            break
        name = match.group(1).lower()
        argument_start = match.end()
        while argument_start < len(code) and code[argument_start].isspace():
            argument_start += 1
        if argument_start < len(code) and code[argument_start] == "(":
            depth = 1
            end = argument_start + 1
            while end < len(code) and depth:
                if code[end] == "(":
                    depth += 1
                elif code[end] == ")":
                    depth -= 1
                end += 1
            if depth:
                raise AssertionError(f"unterminated CMake {name} condition")
            condition = cleaned[argument_start + 1 : end - 1]
        elif name in ("else", "endif"):
            newline = code.find("\n", match.end())
            end = len(code) if newline < 0 else newline + 1
            condition = ""
        else:
            raise AssertionError(f"CMake {name} must use parentheses")
        commands.append((name, condition, match.start(), end))
        cursor = end
    return commands


def cmake_root_blocks(source: str) -> tuple[list[str], str]:
    cleaned = strip_cmake_comments(source)
    stack: list[dict[str, object]] = []
    spans: list[tuple[int, int]] = []
    full_spans: list[tuple[int, int]] = []
    for name, condition, start, end in _cmake_control_commands(cleaned):
        if name == "if":
            is_root = cmake_condition_is_root_only(condition)
            stack.append(
                {
                    "root": is_root,
                    "branch_start": start,
                    "full_start": start,
                    "contains_root": is_root,
                }
            )
        elif name in ("else", "elseif"):
            if not stack:
                raise AssertionError(f"unmatched CMake {name}")
            frame = stack[-1]
            if frame["root"]:
                spans.append((int(frame["branch_start"]), start))
            next_is_root = name == "elseif" and cmake_condition_is_root_only(condition)
            frame["root"] = next_is_root
            frame["contains_root"] = bool(frame["contains_root"]) or next_is_root
            frame["branch_start"] = start
        else:
            if not stack:
                raise AssertionError("unmatched CMake endif")
            frame = stack.pop()
            if frame["root"]:
                spans.append((int(frame["branch_start"]), end))
            if frame["contains_root"]:
                full_spans.append((int(frame["full_start"]), end))
    if stack:
        raise AssertionError("unterminated CMake if")
    if not spans:
        raise AssertionError("missing CMake ROOT condition")
    spans.sort()
    full_spans.sort()
    merged: list[tuple[int, int]] = []
    for start, end in full_spans:
        if merged and start <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((start, end))
    blocks = [cleaned[start:end] for start, end in spans]
    common_parts: list[str] = []
    cursor = 0
    for start, end in merged:
        common_parts.append(cleaned[cursor:start])
        cursor = end
    common_parts.append(cleaned[cursor:])
    return blocks, "".join(common_parts)


def _cmake_source_commands(source: str) -> list[tuple[str, list[str], int, int]]:
    cleaned = strip_cmake_comments(source)
    syntax = list(cleaned)
    in_string = False
    index = 0
    while index < len(cleaned):
        character = cleaned[index]
        following = cleaned[index + 1] if index + 1 < len(cleaned) else ""
        if not in_string and character == '"':
            syntax[index] = " "
            in_string = True
        elif in_string:
            syntax[index] = _blank(character)
            if character == "\\" and following:
                index += 1
                syntax[index] = _blank(cleaned[index])
            elif character == '"':
                in_string = False
        index += 1
    code = "".join(syntax)

    commands: list[tuple[str, list[str], int, int]] = []
    cursor = 0
    command_pattern = re.compile(r"(?im)^[ \t]*(set|list)\b")
    while True:
        match = command_pattern.search(code, cursor)
        if match is None:
            break
        opening = match.end()
        while opening < len(code) and code[opening].isspace():
            opening += 1
        if opening >= len(code) or code[opening] != "(":
            raise AssertionError(f"CMake {match.group(1)} must use parentheses")
        depth = 1
        end = opening + 1
        while end < len(code) and depth:
            if code[end] == "(":
                depth += 1
            elif code[end] == ")":
                depth -= 1
            end += 1
        if depth:
            raise AssertionError(f"unterminated CMake {match.group(1)} command")
        arguments = cleaned[opening + 1 : end - 1]
        tokens = re.findall(r'"(?:\\.|[^"\\])*"|[^\s]+', arguments)
        commands.append(
            (
                match.group(1).lower(),
                [token.strip('"') for token in tokens],
                match.start(),
                end,
            )
        )
        cursor = end
    return commands


def _cmake_commands(source: str) -> list[tuple[str, list[str]]]:
    return [(name, tokens) for name, tokens, _start, _end in _cmake_source_commands(source)]


def cmake_variables(source: str) -> dict[str, list[str]]:
    variables: dict[str, list[str]] = {}
    for command, tokens in _cmake_commands(source):
        if command == "set" and tokens:
            variables[tokens[0]] = tokens[1:]
        elif command == "list" and len(tokens) >= 2 and tokens[0].upper() == "APPEND":
            variables.setdefault(tokens[1], []).extend(tokens[2:])
    variables.pop("SOURCES_LIST", None)
    return variables


def _expand_cmake_tokens(
    tokens: list[str], variables: dict[str, list[str]], seen: set[str] | None = None
) -> list[str]:
    expanded: list[str] = []
    active = set() if seen is None else set(seen)
    for token in tokens:
        reference = re.fullmatch(r"\$\{([A-Za-z_]\w*)\}", token)
        if reference is None or reference.group(1) in active or reference.group(1) not in variables:
            expanded.append(token)
            continue
        name = reference.group(1)
        expanded.extend(_expand_cmake_tokens(variables[name], variables, active | {name}))
    return expanded


def cmake_source_entries(source: str, initial: dict[str, list[str]] | None = None) -> list[str]:
    variables = {name: list(values) for name, values in (initial or {}).items()}
    entries: list[str] = []
    for command, tokens in _cmake_commands(source):
        if command == "set" and tokens:
            target = tokens[0]
            values = tokens[1:]
            variables[target] = values
            if target == "SOURCES_LIST":
                entries.extend(_expand_cmake_tokens(values, variables, {target}))
        elif command == "list" and len(tokens) >= 2 and tokens[0].upper() == "APPEND":
            target = tokens[1]
            values = tokens[2:]
            if target == "SOURCES_LIST":
                entries.extend(_expand_cmake_tokens(values, variables, {target}))
            else:
                variables.setdefault(target, []).extend(values)
    return [entry for entry in entries if re.search(r"(?i)\.c$", entry)]


def cmake_unconditional_source_entries(source: str) -> list[str]:
    cleaned = strip_cmake_comments(source)
    stack: list[int] = []
    conditional_spans: list[tuple[int, int]] = []
    for name, _condition, start, end in _cmake_control_commands(cleaned):
        if name == "if":
            stack.append(start)
        elif name in ("else", "elseif"):
            if not stack:
                raise AssertionError(f"unmatched CMake {name}")
        else:
            if not stack:
                raise AssertionError("unmatched CMake endif")
            opening = stack.pop()
            if not stack:
                conditional_spans.append((opening, end))
    if stack:
        raise AssertionError("unterminated CMake if")

    unconditional = list(cleaned)
    for start, end in conditional_spans:
        for index in range(start, end):
            unconditional[index] = _blank(unconditional[index])
    return cmake_source_entries("".join(unconditional))


def _merge_cmake_condition_frames(
    first: tuple[tuple[int, str], ...], second: tuple[tuple[int, str], ...]
) -> tuple[tuple[int, str], ...]:
    merged = {position: condition for position, condition in first}
    merged.update({position: condition for position, condition in second})
    return tuple(sorted(merged.items()))


def cmake_source_entries_with_conditions(source: str) -> list[tuple[str, tuple[str, ...]]]:
    controls = [
        (start, 0, (name, condition, end))
        for name, condition, start, end in _cmake_control_commands(source)
    ]
    source_commands = [
        (start, 1, (name, tokens, end))
        for name, tokens, start, end in _cmake_source_commands(source)
    ]
    events = sorted(controls + source_commands)
    condition_stack: list[tuple[int, str]] = []
    variables: dict[str, list[tuple[str, tuple[tuple[int, str], ...]]]] = {}
    entries: list[tuple[str, tuple[str, ...]]] = []

    def expand_tokens(
        tokens: list[str],
        active: tuple[tuple[int, str], ...],
        seen: set[str] | None = None,
    ) -> list[tuple[str, tuple[tuple[int, str], ...]]]:
        expanded: list[tuple[str, tuple[tuple[int, str], ...]]] = []
        resolving = set() if seen is None else set(seen)
        for token in tokens:
            reference = re.fullmatch(r"\$\{([A-Za-z_]\w*)\}", token)
            if reference is not None and reference.group(1) not in resolving:
                variable = reference.group(1)
                for filename, defined_under in variables.get(variable, []):
                    expanded.append(
                        (filename, _merge_cmake_condition_frames(defined_under, active))
                    )
            elif re.search(r"(?i)\.c$", token):
                expanded.append((token, active))
        return expanded

    for _position, event_kind, payload in events:
        if event_kind == 0:
            name, condition, _end = payload
            if name == "if":
                condition_stack.append((_position, condition.strip()))
            elif name == "elseif":
                if not condition_stack:
                    raise AssertionError("unmatched CMake elseif")
                condition_stack[-1] = (_position, f"ELSEIF ({condition.strip()})")
            elif name == "else":
                if not condition_stack:
                    raise AssertionError("unmatched CMake else")
                condition_stack[-1] = (_position, "ELSE")
            else:
                if not condition_stack:
                    raise AssertionError("unmatched CMake endif")
                condition_stack.pop()
            continue

        command, tokens, _end = payload
        active = tuple(condition_stack)
        if command == "set" and tokens:
            target = tokens[0]
            resolved = expand_tokens(tokens[1:], active, {target})
            variables[target] = resolved
            if target == "SOURCES_LIST":
                entries.extend(
                    (filename, tuple(condition for _position, condition in frames))
                    for filename, frames in resolved
                )
        elif command == "list" and len(tokens) >= 2 and tokens[0].upper() == "APPEND":
            target = tokens[1]
            resolved = expand_tokens(tokens[2:], active, {target})
            variables.setdefault(target, []).extend(resolved)
            if target == "SOURCES_LIST":
                entries.extend(
                    (filename, tuple(condition for _position, condition in frames))
                    for filename, frames in resolved
                )
    if condition_stack:
        raise AssertionError("unterminated CMake if")
    return entries


def _source_entry_matches(entry: str, filename: str) -> bool:
    return re.search(rf"(?i)(?:^|[/\\]){re.escape(filename)}$", entry) is not None


def cmake_entries_for_filename(
    entries: list[tuple[str, tuple[str, ...]]], filename: str
) -> list[tuple[str, tuple[str, ...]]]:
    return [entry for entry in entries if _source_entry_matches(entry[0], filename)]


def cmake_source_is_exactly_root_only(
    entries: list[tuple[str, tuple[str, ...]]], filename: str
) -> bool:
    matches = cmake_entries_for_filename(entries, filename)
    return (
        len(matches) == 1
        and len(matches[0][1]) == 1
        and cmake_condition_is_root_only(matches[0][1][0])
    )


def source_entries_contain(entries: list[str], filename: str) -> bool:
    return any(_source_entry_matches(entry, filename) for entry in entries)


class ContractHelperTest(unittest.TestCase):
    def test_c_sanitizer_rejects_comment_and_literal_decoys_and_extracts_body(self) -> None:
        source = r'''
/* static void target(void) { comment_only_call(); } */
static void target(void)
{
    const char *page = "VICTIM // {still text}";
    char closing_brace = '}';
    // comment_only_call();
    /* comment_only_call(); */
    real_call();
    if (ready()) { nested_call(); }
}
'''
        cleaned = strip_c_comments_and_char_literals(source)
        self.assertIn('"VICTIM // {still text}"', cleaned)
        body = extract_c_function_body(source, "target")
        executable = c_executable_code(body)
        self.assertNotRegex(executable, r"\bcomment_only_call\s*\(")
        self.assertNotIn("VICTIM", executable)
        self.assertRegex(executable, r"\breal_call\s*\(")
        self.assertRegex(executable, r"\bnested_call\s*\(")

    def test_explicit_root_regions_only_cover_the_exact_if_branch(self) -> None:
        source = '''
static void worker(void)
{
    unprotected_call();
#if defined(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    protected_call();
#else
    other_role_call();
#endif
}
'''
        regions = explicit_root_regions(source)
        code = c_executable_code(source)
        protected = re.search(r"\bprotected_call\b", code).start()
        unprotected = re.search(r"\bunprotected_call\b", code).start()
        other_role = re.search(r"\bother_role_call\b", code).start()
        self.assertTrue(any(start <= protected < end for start, end in regions))
        self.assertFalse(any(start <= unprotected < end for start, end in regions))
        self.assertFalse(any(start <= other_role < end for start, end in regions))

    def test_cmake_helpers_balance_bare_nested_and_multiple_root_blocks(self) -> None:
        accepted_conditions = (
            "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT",
            "defined CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT",
            'DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT OR '
            '"CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT=y" in_list defines',
        )
        rejected_conditions = (
            "NOT CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT",
            "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT OR CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM",
            '"prefix_CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT_suffix"',
        )
        for condition in accepted_conditions:
            with self.subTest(accepted=condition):
                self.assertTrue(cmake_condition_is_root_only(condition))
        for condition in rejected_conditions:
            with self.subTest(rejected=condition):
                self.assertFalse(cmake_condition_is_root_only(condition))

        source = '''
set(COMMON_FILES common.c rescue_route.c)
list(APPEND SOURCES_LIST ${COMMON_FILES})
# list(APPEND SOURCES_LIST commented.c)
if(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    if(ENABLE_DISPLAY)
        message(STATUS "message_only.c")
    endif
    list(APPEND SOURCES_LIST first_root.c)
else()
    list(APPEND SOURCES_LIST rescue_route.c)
endif
if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    set(ROOT_FILES second_root.c)
    list(APPEND SOURCES_LIST ${ROOT_FILES})
    list(REMOVE_ITEM SOURCES_LIST removed_only.c)
endif()
if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT OR "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT=y" IN_LIST DEFINES)
    list(APPEND SOURCES_LIST canonical_root.c)
endif()
if(
    CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT
)
    list(APPEND SOURCES_LIST multiline_root.c)
endif
if(NOT CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT)
    list(APPEND SOURCES_LIST negated_root.c rescue_route.c)
endif()
if(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT OR CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
    list(APPEND SOURCES_LIST mixed_role.c)
endif()
if(CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM)
    list(APPEND SOURCES_LIST victim_only.c rescue_route.c)
endif()
'''
        blocks, _ = cmake_root_blocks(source)
        variables = cmake_variables(source)
        root_entries = [
            entry
            for block in blocks
            for entry in cmake_source_entries(block, variables)
        ]
        unconditional_entries = cmake_unconditional_source_entries(source)
        self.assertEqual(len(blocks), 4)
        self.assertTrue(source_entries_contain(root_entries, "first_root.c"))
        self.assertTrue(source_entries_contain(root_entries, "second_root.c"))
        self.assertTrue(source_entries_contain(root_entries, "canonical_root.c"))
        self.assertTrue(source_entries_contain(root_entries, "multiline_root.c"))
        self.assertTrue(source_entries_contain(unconditional_entries, "common.c"))
        self.assertFalse(source_entries_contain(root_entries, "rescue_route.c"))
        self.assertEqual(
            sum(
                source_entries_contain([entry], "rescue_route.c")
                for entry in unconditional_entries
            ),
            1,
        )
        self.assertFalse(source_entries_contain(root_entries, "negated_root.c"))
        self.assertFalse(source_entries_contain(root_entries, "mixed_role.c"))
        self.assertFalse(source_entries_contain(unconditional_entries, "negated_root.c"))
        self.assertFalse(source_entries_contain(unconditional_entries, "victim_only.c"))
        for decoy in ("commented.c", "message_only.c", "removed_only.c"):
            with self.subTest(decoy=decoy):
                self.assertFalse(source_entries_contain(root_entries + unconditional_entries, decoy))


class GroundOledContractTest(unittest.TestCase):
    def assert_root_protected_call(self, source: str, function: str, call: str) -> None:
        function_start, function_end = _c_function_span(source, function)
        code = c_executable_code(source)
        matches = list(
            re.finditer(rf"\b{re.escape(call)}\s*\(", code[function_start:function_end])
        )
        self.assertTrue(matches, f"{function} must call {call}")
        regions = explicit_root_regions(source)
        self.assertTrue(
            any(
                start <= function_start + match.start() < end
                for match in matches
                for start, end in regions
            ),
            f"{function} call to {call} must be protected by an explicit ROOT #if",
        )

    def test_ground_ui_hardware_profile(self) -> None:
        source = strip_c_comments_and_char_literals(read_source(GROUND_UI_PATH))
        expected = {
            "GROUND_UI_I2C_BUS": "1",
            "GROUND_UI_I2C_SDA_PIN": "15",
            "GROUND_UI_I2C_SCL_PIN": "16",
            "GROUND_UI_I2C_BAUDRATE": "400000",
            "GROUND_UI_GAS_ADC_CHANNEL": "5",
            "GROUND_UI_KEY_ADC_CHANNEL": "1",
            "GROUND_UI_KEY_K1_MAX_MV": "750",
            "GROUND_UI_KEY_K2_MAX_MV": "1400",
            "GROUND_LINE_COUNT": "6",
            "GROUND_UI_Y_OFFSET": "3",
            "GROUND_UI_GAS_HIGH_THRESHOLD": "1800",
            "GROUND_UI_GAS_LOW_THRESHOLD": "1500",
            "GROUND_UI_TASK_STACK_SIZE": "0x3000",
            "GROUND_KEY_DIAGNOSTIC": "0",
            "GROUND_UI_LINE_STEP": "9",
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertEqual(macro_value(source, name), value)

    def test_ground_key_diagnostic_scans_all_adc_channels_and_candidate_gpio(self) -> None:
        source = strip_c_comments_and_char_literals(read_source(GROUND_UI_PATH))
        for text in ("KEY INPUT SCAN", "A0", "A1", "A2", "A3", "A4", "A5", "D13", "D14"):
            with self.subTest(text=text):
                self.assertRegex(source, rf'"[^"\r\n]*{re.escape(text)}[^"\r\n]*"')

        executable = c_executable_code(read_source(GROUND_UI_PATH))
        self.assertRegex(executable, r"\buapi_gpio_get_val\s*\(")
        self.assertRegex(executable, r"\bground_adc_scan\s*\(")

    def test_ground_ui_calls_required_peripherals(self) -> None:
        source = c_executable_code(read_source(GROUND_UI_PATH))
        required_calls = [
            r"\buapi_i2c_set_baudrate\s*\(",
            r"\buapi_adc_init\s*\(",
            r"\badc_port_read\s*\(",
            r"\bssd1306_Init\s*\(",
            r"\bssd1306_DrawString\s*\(",
            r"\bssd1306_UpdateScreen\s*\(",
        ]
        for call in required_calls:
            with self.subTest(call=call):
                self.assertRegex(source, call)
        self.assertRegex(
            source,
            r"\bssd1306_SetCursor\s*\(\s*0\s*,[^;]*GROUND_UI_Y_OFFSET",
        )
        self.assertRegex(source, r"(?i)\baht20_[a-z0-9_]+\s*\(")

    def test_ground_ui_contains_victim_and_environment_page_text(self) -> None:
        source = strip_c_comments_and_char_literals(read_source(GROUND_UI_PATH))
        for text in ("VICTIM", "SpO2", "HR", "BAT", "SOS", "ENV", "TEMP", "HUM", "GAS", "KEY"):
            with self.subTest(text=text):
                self.assertRegex(source, rf'"[^"\r\n]*{re.escape(text)}[^"\r\n]*"')

    def test_ground_ui_header_exposes_lifecycle_and_ingest_api(self) -> None:
        header = c_executable_code(read_source(GROUND_UI_HEADER_PATH))
        for function in ("ground_ui_init", "ground_ui_start", "ground_ui_ingest_report"):
            with self.subTest(function=function):
                self.assertRegex(header, rf"\b{function}\s*\(")

    def test_cmake_keeps_route_common_and_ground_hardware_root_only(self) -> None:
        cmake = read_source(RELAY_CMAKE_PATH)
        root_blocks, _ = cmake_root_blocks(cmake)
        variables = cmake_variables(cmake)
        root_entries = [
            entry
            for block in root_blocks
            for entry in cmake_source_entries(block, variables)
        ]
        unconditional_entries = cmake_unconditional_source_entries(cmake)
        self.assertTrue(
            source_entries_contain(unconditional_entries, "rescue_route.c"),
            "rescue_route.c must be in an unconditional source-list command",
        )
        self.assertFalse(source_entries_contain(root_entries, "rescue_route.c"))
        for source_name in (
            "ground_model.c",
            "ground_ui.c",
            "aht20.c",
            "ssd1306.c",
            "ssd1306_fonts.c",
        ):
            with self.subTest(source=source_name):
                self.assertTrue(
                    source_entries_contain(root_entries, source_name),
                    f"{source_name} must be appended by a ROOT source-list command",
                )
                self.assertFalse(source_entries_contain(unconditional_entries, source_name))

    def test_relay_root_connects_ui_rssi_and_shared_route_pipeline(self) -> None:
        relay = read_source(RELAY_PATH)
        cleaned = strip_c_comments_and_char_literals(relay)
        regions = explicit_root_regions(relay)
        includes = list(re.finditer(r'#\s*include\s*[<"]ground_ui\.h[>"]', cleaned))
        self.assertTrue(
            any(start <= match.start() < end for match in includes for start, end in regions),
            "ground_ui.h include must be protected by an explicit ROOT #if",
        )

        self.assert_root_protected_call(relay, "ssaps_write_cb", "ground_ui_ingest_report")
        self.assert_root_protected_call(relay, "relay_task", "ground_ui_init")
        self.assert_root_protected_call(relay, "relay_task", "ground_ui_start")

        callback_body = c_executable_code(
            extract_c_function_body(relay, "register_connection_callbacks")
        )
        self.assertRegex(callback_body, r"\.read_rssi_cb\s*=")

        rssi_tasks = [
            name
            for name in c_function_names(relay)
            if re.search(r"rssi", name, re.IGNORECASE) and re.search(r"task", name, re.IGNORECASE)
        ]
        self.assertTrue(rssi_tasks, "missing RSSI task function")
        self.assertTrue(
            any(
                re.search(
                    r"\bsle_read_remote_device_rssi\s*\(",
                    c_executable_code(extract_c_function_body(relay, name)),
                )
                for name in rssi_tasks
            ),
            "RSSI task must call sle_read_remote_device_rssi",
        )

        upstream_body = c_executable_code(extract_c_function_body(relay, "relay_send_upstream"))
        for function in ("rescue_route_decode", "rescue_route_add_hop", "rescue_route_encode"):
            with self.subTest(upstream_call=function):
                self.assertRegex(upstream_body, rf"\b{function}\s*\(")


if __name__ == "__main__":
    unittest.main()
