// SPDX-License-Identifier: GPL-2.0-only

#include <asm-generic/gdb-scripts.h>

.pushsection ".debug_gdb_scripts", "MS", %progbits, 1
.byte SECTION_SCRIPT_ID_PYTHON_FILE
#ifdef __PBL__
.asciz "scripts/gdb/barebox-gdb.py"
#else
.asciz "scripts/gdb/pbl-gdb.py"
#endif
.popsection
