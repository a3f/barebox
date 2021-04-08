import pytest
from .helper import *


def test_barebox_true(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_TRUE")

    _, _, returncode = barebox.run('true')
    assert returncode == 0


def test_barebox_false(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_FALSE")

    _, _, returncode = barebox.run('false')
    assert returncode == 1
