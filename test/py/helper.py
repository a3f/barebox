from labgrid.driver import BareboxDriver
import pytest
import os
import re
import shlex
import json
from itertools import filterfalse


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
        try:
            with open(os.environ['LG_BUILDDIR'] + "/.config") as f:
                out = f.read().splitlines()
        except OSError:
            return set()

    options = set()
    for line in out:
        if line and line.startswith("CONFIG_"):
            options.add(line.split('=')[0])
    return options


def get_iomem(command, dupes=False):
    """Returns a dictionary with the iomem reservations done
    in barebox
    Args:
        command (BareboxDriver): An instance of the BareboxDriver
    Returns:
        dict: nested dicts of iomem reservations
    """
    assert isinstance(command, BareboxDriver)

    out, _, returncode = command.run("iomem -j")
    if returncode != 0:
        return None

    iomem = json.loads('\n'.join(out))
    return iomem if dupes else transform_named_objects(iomem)


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

def of_get_property(barebox, path):
    node, prop = os.path.split(path)

    stdout = barebox.run_check(f"of_dump -p {node}")
    for line in stdout:
        if line == '{prop};':
            return True

        prefix = f'{prop} = '
        if line.startswith(prefix):
            # Also drop the semicolon
            return line[len(prefix):-1]
    return False


def transform_named_objects(obj):
    if 'children' not in obj:
        return obj

    children = obj['children']
    name_counts = {}
    for child in children:
        name_counts[child['name']] = name_counts.get(child['name'], 0) + 1

    new_children = {}
    for child in children:
        new_child = transform_named_objects(child)
        name = new_child['name']
        parts = name.split('@', 1)

        try:
            if len(parts) == 2 and \
               int(parts[1], 16) == int(new_child['start'], 16):
                name = parts[0]
        except ValueError:
            pass
        if name_counts[new_child['name']] == 1:
            new_children[name] = new_child
        # if duplicate, skip adding to dict

    obj['children'] = new_children
    return obj


def deep_lookup(data, lookup):
    if isinstance(lookup, dict):
        target_dict = lookup
        target_key = None
    else:
        target_dict = None
        target_key = lookup

    def matches(obj):
        if not isinstance(obj, dict):
            return False
        return all(obj.get(k) == v for k, v in target_dict.items())

    if isinstance(data, dict):
        if target_dict is not None:
            if target_dict == {}:
                yield data
                return
            elif matches(data):
                yield data
        if target_key:
            for key, value in data.items():
                if key == target_key:
                    yield value
                yield from deep_lookup(value, lookup)
        else:
            for value in data.values():
                yield from deep_lookup(value, lookup)

    elif isinstance(data, list):
        for item in data:
            yield from deep_lookup(item, lookup)


def skip_disabled(config, *options):
    if bool(config):
        undefined = list(filterfalse(config.__contains__, options))

        if bool(undefined):
            pytest.skip("skipping test due to disabled " + (",".join(undefined)) + " dependency")
