import pytest
from .helper import *

def test_bselftest(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_SELFTEST")

    _, _, returncode = barebox.run('selftest')
    assert returncode == 0
