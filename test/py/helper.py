# SPDX-License-Identifier: GPL-2.0-only

from labgrid.driver import BareboxDriver
import pytest
import os
import re
import shlex


def parse_config(lines):
    options = {}
    for line in lines:
        if line and line.startswith("CONFIG_"):
            key, val = line.split("=", 1)
            key = key.strip()
            val = val.strip()

            if val == "y":
                options[key] = True
            elif val == "m":
                options[key] = False
            elif val == "n":
                options[key] = None
            elif val.startswith('"') and val.endswith('"'):
                options[key] = val[1:-1]
            else:
                options[key] = int(val, base=0)

    return options


def open_config_file(path):
    try:
        with open(path) as f:
            return f.read().splitlines()
    except OSError:
        return []


def get_config(command):
    """Returns the enabled config options of barebox, either from
    a running instance if supported or by looking into .config
    in the build directory.
    Args:
        command (BareboxDriver): An instance of the BareboxDriver
    Returns:
        list: list of the enabled config options
    """
    assert isinstance(command, BareboxDriver)

    out, err, returncode = command.run("cat /env/data/config")
    if returncode != 0:
        out = open_config_file(os.environ['LG_BUILDDIR'] + "/.config")

    return parse_config(out)


def devinfo(barebox, device):
    info = {}
    section = None
    pattern = r'^\s*([^:]+):\s*(.*)$'

    for line in barebox.run_check(f"devinfo {device}"):
        line = line.rstrip()
        if match := re.match(r"^([^: ]+):$", line):
            section = match.group(1)
            if section in ["Parameters"]:
                info[section] = {}
            else:
                info[section] = []
            continue

        line = line.strip()
        if section is None or isinstance(info[section], dict):
            if match := re.match(pattern, line):
                key = match.group(1).strip()
                value = match.group(2).strip()
                # TODO: coerce to type?
                if section is None:
                    info[section] = line
                else:
                    info[section][key] = value
        elif section:
            info[section].append(line)

    return info


def format_dict_with_prefix(varset: dict, prefix: str) -> str:
    parts = []
    for k, v in varset.items():
        escaped_val = shlex.quote(str(v))
        parts.append(f"{prefix}{k}={escaped_val}")
    return " ".join(parts)


def globalvars_set(barebox, varset: dict, create=True):
    cmd, prefix = ("global ", "") if create else ("", "global.")
    barebox.run_check(cmd + format_dict_with_prefix(varset, prefix))


def nvvars_set(barebox, varset: dict, create=True):
    cmd, prefix = ("nv ", "") if create else ("", "nv.")
    barebox.run_check(cmd + format_dict_with_prefix(varset, prefix))


def getenv_int(barebox, var):
    return int(barebox.run_check(f"echo ${var}")[0])


def getstate_int(barebox, var, prefix="state.bootstate"):
    return getenv_int(barebox, f"{prefix}.{var}")


def getparam_int(info, var):
    return int(info["Parameters"][var].split()[0])


def _parse_dt_cells(cell_str):
    """Parse device tree cell array like '0x1000 0x100' into list of integers."""
    if not cell_str.strip():
        return []
    return [int(x, 0) for x in cell_str.split()]


def _parse_dt_value(value_str):
    """Parse device tree property value string into Python types.

    Handles:
    - <N...> - one or more 32-bit integers in big endian order (hex or decimal)
    - "string" - string values
    - Lists of multiple values separated by commas

    Returns:
    - Single integer if <N> with one value
    - List of integers if <N1 N2 ...> with multiple values
    - String if "..."
    - List if multiple comma-separated values
    """
    # Split by commas, respecting <...> and "..." boundaries
    parts = []
    current = ""
    depth = 0  # Track < > nesting
    in_string = False

    i = 0
    while i < len(value_str):
        char = value_str[i]

        if char == '"' and (i == 0 or value_str[i-1] != '\\'):
            in_string = not in_string
            current += char
        elif not in_string:
            if char == '<':
                depth += 1
                current += char
            elif char == '>':
                depth -= 1
                current += char
            elif char == ',' and depth == 0:
                # This comma separates list elements
                if current.strip():
                    parts.append(current.strip())
                current = ""
                i += 1
                continue
            else:
                current += char
        else:
            current += char

        i += 1

    # Add the last part
    if current.strip():
        parts.append(current.strip())

    # Parse each part into appropriate Python type
    parsed_parts = []
    for part in parts:
        if part.startswith('<') and part.endswith('>'):
            # Parse cell array: <0x1000 0x100> or <123 456>
            cell_str = part[1:-1]
            cells = _parse_dt_cells(cell_str)
            if len(cells) == 1:
                # Single integer value
                parsed_parts.append(cells[0])
            else:
                # Multiple cells as list
                parsed_parts.append(cells)
        elif part.startswith('"') and part.endswith('"'):
            # Parse string, removing quotes
            parsed_parts.append(part[1:-1])
        else:
            # Unknown format, keep as-is
            parsed_parts.append(part)

    # Return single value or list
    if len(parsed_parts) == 0:
        return None
    elif len(parsed_parts) == 1:
        return parsed_parts[0]
    else:
        return parsed_parts


def of_get_property(barebox, path):
    node, prop = os.path.split(path)

    stdout = barebox.run_check(f"of_dump -p {node}")
    for line in stdout:
        if line == f'{prop};':
            return True

        prefix = f'{prop} = '
        if line.startswith(prefix):
            # Drop the prefix and semicolon, then parse the value
            value_str = line[len(prefix):-1].strip()
            return _parse_dt_value(value_str)
    return False


def skip_disabled(config, *options):
    if bool(config):
        undefined = [opt for opt in options if opt not in config]

        if bool(undefined):
            pytest.skip("skipping test due to disabled " + (",".join(undefined)) + " dependency")
