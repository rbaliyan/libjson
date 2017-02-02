MAKE := make
LIB_NAME ?= json
INC_DIRS ?= $(BASE_DIR)/inc
LIB_DIRS ?= $(BASE_DIR)/lib
LIBS ?= $(LIB_NAME)
PROJECT_EXT_DEPEND ?= $(BASE_DIR)
include $(BASE_DIR)/Makefile.common
