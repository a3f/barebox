import pytest
from .py import helper


@pytest.fixture(scope='function')
def barebox(strategy, target):
    strategy.transition('barebox')
    return target.get_driver('BareboxDriver')


@pytest.fixture(scope='function')
def shell(strategy, target):
    strategy.transition('shell')
    return target.get_driver('ShellDriver')


@pytest.fixture(scope="session")
def barebox_config(strategy, target):
    strategy.transition('barebox')
    command = target.get_driver("BareboxDriver")
    return helper.get_config(command)
