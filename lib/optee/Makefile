# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += \
	-I$(srctree)/include/linux \
	-I$(src)/optee_client/public \
	-I$(src)/optee_test/host/xtest/adbg/include

obj-y += optee_client.o

optee_client-y += optee_client/libteec/src/tee_client_api.o

CFLAGS_optee_client/libteec/src/tee_client_api.o += \
	-I$(src)/optee_client/libteec/include \
	-DBINARY_PREFIX=\"TEEC\"
