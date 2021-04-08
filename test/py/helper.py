from labgrid.driver import BareboxDriver
import pytest


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
            with open('.config') as f:
                out = f.read().splitlines()
        except OSError:
            return []

    options = []
    for line in out:
        if line and line.startswith("CONFIG_"):
            options.append(line.split('=')[0])
    return options


def skip_disabled(config, option):
    if bool(config) and not option in config:
        pytest.skip("skipping test due to disabled " + option + "=y dependency")
