# SPDX-License-Identifier: LGPL-2.1-or-later

class BareboxReboot(Exception):
    pass


class BareboxBug(Exception):
    pass


class BareboxPanic(BareboxBug):
    pass


class BareboxAbort(BareboxPanic):
    def __init__(self, message="unhandled exception"):
        super().__init__(message)
    pass


class BareboxPrefetchAbort(BareboxAbort):
    pass


class BareboxDataAbort(BareboxAbort):
    pass


class BareboxGuardPageAbort(BareboxDataAbort):
    pass


class BareboxNullPointerAbort(BareboxDataAbort):
    pass


class BareboxShellRestart(Exception):
    pass
