import argparse
import re
import sys
from pathlib import Path


def find_top_level_object_spans(text: str):
    spans = []
    arr_depth = 0
    obj_depth = 0
    obj_start = None
    i = 0
    n = len(text)
    in_string = False
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if in_line_comment:
            if c == "\n":
                in_line_comment = False
            i += 1
            continue

        if in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 2
            else:
                i += 1
            continue

        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue

        if c == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue

        if c == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue

        if c == '"':
            in_string = True
            i += 1
            continue

        if c == "[":
            arr_depth += 1
        elif c == "]":
            arr_depth -= 1
        elif c == "{":
            if arr_depth == 1 and obj_depth == 0:
                obj_start = i
            obj_depth += 1
        elif c == "}":
            obj_depth -= 1
            if arr_depth == 1 and obj_depth == 0 and obj_start is not None:
                spans.append((obj_start, i + 1))
                obj_start = None

        i += 1

    return spans


def ensure_shadow_256_in_light_object(obj_text: str):
    if not re.search(r'"type"\s*:\s*"LIGHT"', obj_text):
        return obj_text, False

    if re.search(r'"shadow"\s*:', obj_text):
        updated = re.sub(r'("shadow"\s*:\s*)([^,\n\r}]+)', r"\g<1>256", obj_text, count=1)
        return updated, updated != obj_text

    close_idx = obj_text.rfind("}")
    if close_idx == -1:
        return obj_text, False

    before_close = obj_text[:close_idx]
    after_close = obj_text[close_idx + 1 :]

    close_indent_match = re.search(r"\n([ \t]*)\}\s*$", obj_text)
    close_indent = close_indent_match.group(1) if close_indent_match else ""

    prop_indent_match = re.search(r"\n([ \t]*)\"[^\"]+\"\s*:", obj_text)
    prop_indent = prop_indent_match.group(1) if prop_indent_match else close_indent + "\t"

    trimmed = before_close.rstrip()
    needs_comma = not trimmed.endswith(",")
    insert_piece = ("," if needs_comma else "") + "\n" + prop_indent + '"shadow":256'

    updated = trimmed + insert_piece + "\n" + close_indent + "}" + after_close
    return updated, True


def process_s72_text(text: str):
    spans = find_top_level_object_spans(text)
    if not spans:
        return text, 0

    chunks = []
    cursor = 0
    changed_lights = 0

    for start, end in spans:
        chunks.append(text[cursor:start])
        obj_text = text[start:end]
        new_obj_text, changed = ensure_shadow_256_in_light_object(obj_text)
        chunks.append(new_obj_text)
        if changed:
            changed_lights += 1
        cursor = end

    chunks.append(text[cursor:])
    return "".join(chunks), changed_lights


def main():
    parser = argparse.ArgumentParser(
        description="Set shadow:256 on all LIGHT objects in an s72 file (supports comments/trailing commas)."
    )
    parser.add_argument("scene_path", help="Path to .s72 file (relative paths are resolved from current directory).")
    args = parser.parse_args()

    scene_path = Path(args.scene_path)
    if not scene_path.is_absolute():
        scene_path = (Path.cwd() / scene_path).resolve()

    if not scene_path.exists():
        print(f"Error: file does not exist: {scene_path}", file=sys.stderr)
        sys.exit(1)

    if scene_path.suffix.lower() != ".s72":
        print(f"Error: expected a .s72 file, got: {scene_path}", file=sys.stderr)
        sys.exit(1)

    original = scene_path.read_text(encoding="utf-8")
    updated, changed_lights = process_s72_text(original)

    if updated != original:
        scene_path.write_text(updated, encoding="utf-8", newline="")

    print(f"Updated {changed_lights} LIGHT object(s) in: {scene_path}")


if __name__ == "__main__":
    main()
