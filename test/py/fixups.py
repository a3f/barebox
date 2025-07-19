# SPDX-License-Identifier: LGPL-2.1-or-later
# SPDX-FileCopyrightText: 2016-2017 Pengutronix, Jan Luebbe
# SPDX-FileCopyrightText: 2016-2017 Pengutronix, Rouven Czerwinski

"""
Labgrid Fixups

These should eventually be upstreamed into Labgrid
"""

import shlex
from labgrid.util import gen_marker, re_vt100


def barebox_run_thorough(self, cmd: str, *, timeout: int = 30,
                         adjust_log_level: bool = True, codec: str = "utf-8",
                         decodeerrors: str = "strict"):
    """
    Runs the specified command on the shell and returns the output.

    Args:
        cmd (str): command to run on the shell
        timeout (int): optional, timeout in seconds

    Returns:
        Tuple[List[str],List[str], int]: if successful, None otherwise
    """
    # FIXME: use codec, decodeerrors
    marker = gen_marker()
    # hide marker from expect
    hidden_marker = f'"{marker[:4]}""{marker[4:]}"'
    # generate command with marker and log level adjustment
    cmp_command = f'echo -o /cmd {shlex.quote(cmd)}; echo {hidden_marker};'
    if self.saved_log_level and adjust_log_level:
        cmp_command += f' global.loglevel={self.saved_log_level};'
    cmp_command += f' sh /cmd; echo {hidden_marker} $?;'
    if self.saved_log_level and adjust_log_level:
        cmp_command += ' global.loglevel=0;'

    if self._status == 1:
        self.console.sendline(cmp_command)
        _, _, match, _ = self.console.expect(
            rf'{marker}(.*){marker}\s+(\d+)\s+.*{self.prompt}',
            timeout=timeout)
        # Remove VT100 Codes and split by newline
        data = re_vt100.sub('', match.group(1).decode('utf-8')).split('\r\n')[1:-1]
        self.logger.debug("Received Data: %s", data)
        # Get exit code
        exitcode = int(match.group(2))
        return (data, [], exitcode)

    return None
