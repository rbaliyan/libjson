# Library name
LIB_NAME ?= json

# Directory Structure
SRC_DIR ?= src
OBJ_DIR ?= obj
LIB_DIR ?= lib
BIN_DIR ?= bin
INC_DIR ?= inc
TEST_DIR ?= test

# Execuatbles
CC := gcc
CPP := g++
AR := ar
RM := rm -rf

# Supported Extension
C_EXT := c
H_EXT := h
OBJ_EXT := o
LIB_EXT := a
LIB_PREFIX := lib
DEPEND_EXT := d
TEST_EXEC := test

# If SRC dir is empty search in Current Directory
ifeq "$(wildcard $(SRC_DIR) )" ""
	# src does not exists or is empty use current directory
	SRC_DIR :=.
	C_FILES := $(wildcard $(SRC_DIR)/*.$(C_EXT))
else
	# C Langauge
	C_FILES := $(shell find $(SRC_DIR) -name \*.$(C_EXT) )
	TEST_FILES := $(shell find $(TEST_DIR) -name \*.$(C_EXT) )
endif

# If INC dir is empty search in Current Directory
ifeq "$(wildcard $(INC_DIR) )" ""
	INC_DIRS +=.
else
	# Include dirs
	INC_DIRS += $(sort $(dir $(shell find $(INC_DIR) -name \*.$(H_EXT) )))
endif


# Object Files and dependency files
C_OBJFILES := $(patsubst $(SRC_DIR)/%.$(C_EXT),$(OBJ_DIR)/%.$(OBJ_EXT),$(C_FILES))
C_DEPEND_FILES := $(patsubst %.$(OBJ_EXT),%.$(DEPEND_EXT),$(C_OBJFILES))

TEST_OBJFILES := $(patsubst $(TEST_DIR)/%.$(C_EXT),$(OBJ_DIR)/%.$(OBJ_EXT),$(TEST_FILES))
TEST_DEPEND_FILES := $(patsubst %.$(OBJ_EXT),%.$(DEPEND_EXT),$(TEST_OBJFILES))


# Objects
OBJECTS := $(C_OBJFILES)

# Depend Files
DEPEND_FILES := $(C_DEPEND_FILES)

############################################
# Create obj Dirs
############################################
OBJ_DIRS:=$(dir $(OBJECTS))
ifneq  ("$(OBJ_DIRS)","")
    $(shell mkdir -p $(OBJ_DIRS))
endif

ifneq  ("$(BIN_DIR)","")
    $(shell mkdir -p $(BIN_DIR))
endif

OS := $(shell uname)

ifeq ($(OS),Darwin)
	GLOBAL_CFLAG += '-DFUNOPEN_SUPPORT' -DDARWIN_SYS
else
	ifeq ($(OS),Linux)
		GLOBAL_CFLAG += '-DFMEMOPEN_SUPPORT'
	endif
endif

# If Verbose mode is not requsted then silent output
ifeq (,$(V))
.SILENT:
endif


###########################################
# Flags
############################################
# Optimization level
OPTIMIZE := -O0

# Debug Flag
DEBUG := -g

LIB_FLAG +=$(addprefix -L,$(LIB_DIRS))
LIB_FLAG +=$(addprefix -l,$(LIBS))

INCLUDE_FLAG = $(addprefix -I,$(INC_DIRS))
GLOBAL_CFLAG += $(INCLUDE_FLAG)

TEST_LIB_FLAGS += $(addprefix -L,$(LIB_DIR))
TEST_LIB_FLAGS += -l$(LIB_NAME)

CFLAGS += $(GLOBAL_CFLAG) 

# Target Binary
TARGET ?= $(LIB_DIR)/$(LIB_PREFIX)$(LIB_NAME).$(LIB_EXT)

############################################
# Targets
############################################
all: release test lib
release Release: CFLAGS += $(OPTIMIZE)
release Release:$(TARGET)
debug Debug: CFLAGS += $(DEBUG)
debug Debug: $(TARGET)

lib    :  $(TARGET)
test_run: CFLAGS += $(DEBUG)
test: CFLAGS += $(DEBUG)
test : $(BIN_DIR)/$(TEST_EXEC)

# Include Depend Files
-include $(DEPEND_FILES)

test_run: test
	@echo "Runing tests"
	$(BIN_DIR)/$(TEST_EXEC)

$(BIN_DIR)/$(TEST_EXEC): $(TEST_OBJFILES) $(LIB_DIR)/$(LIB_PREFIX)$(LIB_NAME).$(LIB_EXT)
	$(CC) $(CFLAGS) $< $(LIB_FLAG) $(TEST_LIB_FLAGS) -o $@

$(TEST_OBJFILES): $(OBJ_DIR)/%.$(OBJ_EXT): $(TEST_DIR)/%.$(C_EXT)
	@$(CC) -E $(CFLAGS) $< > $(OBJ_DIR)/$*.E
	$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $(OBJ_DIR)/$*.d
	@mv -f $(OBJ_DIR)/$*.d $(OBJ_DIR)/$*.d.tmp;
	@sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' < $(OBJ_DIR)/$*.d.tmp > $(OBJ_DIR)/$*.d
	@rm -f $(OBJ_DIR)/$*.d.tmp

###########################################
# Rules
###########################################
#Compile C files
$(C_OBJFILES): $(OBJ_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(C_EXT)
	@$(CC) -E $(CFLAGS) $< > $(OBJ_DIR)/$*.E
	$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $(OBJ_DIR)/$*.d
	@mv -f $(OBJ_DIR)/$*.d $(OBJ_DIR)/$*.d.tmp;
	@sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' < $(OBJ_DIR)/$*.d.tmp > $(OBJ_DIR)/$*.d
	@$(RM) $(OBJ_DIR)/$*.d.tmp

# Library File
$(LIB_DIR)/$(LIB_PREFIX)$(LIB_NAME).$(LIB_EXT) : $(OBJECTS)
	mkdir -p $(LIB_DIR);
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

# Clean Everything
clean cleanDebug cleanRelease:
	@echo "Remove Files"
	@$(RM) $(OBJ_DIR)
	@$(RM) $(LIB_DIR)
	@$(RM) $(BIN_DIR)

# Generate Tag files
# ctags
tags: cscope $(C_FILES) $(CPP_FILES) $(ASM_FILES)
	@$(TAG) $(TAG_FLAGS) $(C_FILES) $(CPP_FILES) $(ASM_FILES)

# cscope tags
cscope : $(C_FILES) $(CPP_FILES) $(ASM_FILES)
	@$(CSCOPE) $(CSCOPE_FLAGS)


.PHONY: clean install connect tags cscope
