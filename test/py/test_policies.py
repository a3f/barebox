# SPDX-License-Identifier: GPL-2.0-or-later

import pytest


def test_security_policies(barebox, env):
    if 'policies' not in env.get_target_features():
        pytest.xfail('policies feature flag missing')

    assert 'Active Policy: devel' in barebox.run_check('sconfig')

    assert barebox.run_check('sconfig -l') == \
           ['devel', 'factory', 'lockdown', 'tamper']

    assert barebox.run_check('varinfo global.bootm.verify') == \
        ['bootm.verify: available (type: enum) ' + \
         '(values: "none", "hash", "signature", "available")']

    barebox.run_check('sconfig -s factory')
    assert 'Active Policy: factory' in barebox.run_check('sconfig')

    stdout = barebox.run_check('sconfig -v -s devel')
    assert ['+SCONFIG_BOOT_UNSIGNED_IMAGES',
            '+SCONFIG_CMD_GO'] == stdout
    assert 'Active Policy: devel' in barebox.run_check('sconfig')

    stdout, _, rc = barebox.run('go')
    assert 'go - start application at address or file' in stdout
    assert 'go: Operation not permitted' not in stdout
    assert rc == 1

    stdout = barebox.run_check('sconfig -v -s tamper')
    assert ['-SCONFIG_SECURITY_POLICY_SELECT',
            '-SCONFIG_BOOT_UNSIGNED_IMAGES',
            '-SCONFIG_FASTBOOT_CMD_BASE',
            '-SCONFIG_FASTBOOT_CMD_OEM',
            '-SCONFIG_SHELL',
            '-SCONFIG_SHELL_INTERACTIVE',
            '-SCONFIG_CMD_GO'] == stdout
    assert 'Active Policy: tamper' in barebox.run_check('sconfig')

    _, _, rc = barebox.run('sconfig -s devel')
    assert rc != 0
    assert 'Active Policy: tamper' in barebox.run_check('sconfig')

    stdout, _, rc = barebox.run('go')
    assert 'go - start application at address or file' not in stdout
    assert 'go: Operation not permitted' in stdout
    assert rc == 127

    assert barebox.run_check('varinfo global.bootm.verify') == \
        ['bootm.verify: signature (type: enum)']
