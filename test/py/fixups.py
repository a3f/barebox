# SPDX-License-Identifier: LGPL-2.1-or-later
# SPDX-FileCopyrightText: 2016-2017 Pengutronix, Jan Luebbe
# SPDX-FileCopyrightText: 2016-2017 Pengutronix, Rouven Czerwinski

"""
Labgrid Fixups

These should eventually be upstreamed into Labgrid
"""

import shlex
from labgrid.util import gen_marker, re_vt100
from .exceptions import (BareboxReboot, BareboxBug, BareboxPanic,
                         BareboxAbort, BareboxPrefetchAbort, BareboxDataAbort,
                         BareboxGuardPageAbort, BareboxNullPointerAbort,
                         BareboxShellRestart)


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
        idx, before, match, _ = self.console.expect(
            [r'PANIC: unable to handle (.*?) at address (.*)',
             r'PANIC: unable to handle (.*)',
             r'PANIC: (.*)',
             r'BUG: (.*)',
             r'\r\n\r\n(barebox \d\d\d\d\.\d\d\.\d(?:.*?))\r\n' +
             r'(?:Buildsystem version: (?:.*?)\r\n)?\r\n\r\n',
             rf'{marker}(.*){marker}\s+(\d+)\s+.*{self.prompt}',
             self.prompt],
            timeout=timeout)
        try:
            arg = match.group(1).decode('utf-8')
        except (IndexError, AttributeError):
            arg = None

        match idx:
            case 0 | 1:  # unhandled exception
                self.target.deactivate(self)
                self.target.activate(self)
                try:
                    addr = match.group(2).decode('utf-8')
                except IndexError:
                    addr = None
                match arg:
                    case "prefetch abort":
                        raise BareboxPrefetchAbort()
                    case "NULL pointer dereference":
                        raise BareboxNullPointerAbort(addr)
                    case "stack overflow":
                        raise BareboxGuardPageAbort(addr)
                    case "paging request":
                        raise BareboxDataAbort(addr)
                    case _:
                        raise BareboxAbort(arg)
            case 2:  # PANIC:
                raise BareboxPanic(arg)
            case 3:  # BUG:
                raise BareboxBug(arg)
            case 4:  # barebox banner
                raise BareboxReboot(arg)
            case 5:  # marker followed by prompt
                # Remove VT100 Codes and split by newline
                data = re_vt100.sub('', match.group(1).decode('utf-8')).split('\r\n')[1:-1]
                self.logger.debug("Received Data: %s", data)

                # Get exit code
                exitcode = int(match.group(2))
                return (data, [], exitcode)
            case 6:  # plain prompt
                raise BareboxShellRestart(before + match.group(0))

    return None
