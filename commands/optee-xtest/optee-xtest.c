// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: © 2022 Marc Kleine-Budde <mkl@pengutronix.de>

#include <command.h>
#include <common.h>
#include <getopt.h>

#include <adbg_int.h>
#include <pta_invoke_tests.h>
#include <tee_client_api.h>
#include <xtest_helpers.h>

ADBG_SUITE_DEFINE(benchmark);
#ifdef WITH_GP_TESTS
ADBG_SUITE_DEFINE(gp);
#endif
#ifdef CFG_PKCS11_TA
ADBG_SUITE_DEFINE(pkcs11);
#endif
ADBG_SUITE_DEFINE(regression);

TEEC_Context xtest_teec_ctx;
char *xtest_tee_name;

TEEC_Result xtest_teec_ctx_init(void)
{
	return TEEC_InitializeContext(xtest_tee_name, &xtest_teec_ctx);
}

TEEC_Result xtest_teec_open_session(TEEC_Session *session,
				    const TEEC_UUID *uuid, TEEC_Operation *op,
				    uint32_t *ret_orig)
{
	return TEEC_OpenSession(&xtest_teec_ctx, session, uuid,
				TEEC_LOGIN_PUBLIC, NULL, op, ret_orig);
}

void xtest_teec_ctx_deinit(void)
{
	TEEC_FinalizeContext(&xtest_teec_ctx);
}

static int do_optee_xtest(int argc, char *argv[])
{
	TEEC_Result tee_res = TEEC_ERROR_GENERIC;
	ADBG_Suite_Definition_t all = {
		.SuiteID_p = NULL,
		.cases = LIST_HEAD_INIT(all.cases),
	};

	printf("TEE test application started over %s TEE instance\n",
	       xtest_tee_name ? xtest_tee_name : "default");

	tee_res = xtest_teec_ctx_init();
	if (tee_res != TEEC_SUCCESS) {
		printf("Failed to open TEE context: 0x%d\n",
		       tee_res);
		return -1;
	}

	Do_ADBG_AppendToSuite(&all, &ADBG_Suite_regression);
	Do_ADBG_RunSuite(&all, 0, argv);
 
	xtest_teec_ctx_deinit();

	return 0;
}

BAREBOX_CMD_START(optee_xtest)
	.cmd = do_optee_xtest,
	BAREBOX_CMD_DESC("run optee test suite")
	BAREBOX_CMD_OPTS("[OPTIONS]")
	BAREBOX_CMD_GROUP(CMD_GRP_MISC)
BAREBOX_CMD_END
