CC = clang
CFLAGS = -g -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer
# CFLAGS = -g -Wall -Wextra
LDFLAGS = -ldl -llua -lm -lpthread
SHARED = -fPIC -shared

BIN = ./bin
CORE_DIR = ./core
TEST_DIR = ./test
CORE_INCLUDES = -I$(CORE_DIR)
TEST_INCLUDES = -I$(TEST_DIR)

# find core file
CORE_SRC = $(shell find $(CORE_DIR) -type f -name "*.c")
CORE_OBJS = $(CORE_SRC:.c=.o)

# find all test module
TEST_MODULES = $(wildcard $(addprefix $(TEST_DIR)/, test_*.c))
TEST_OBJS = $(addprefix $(BIN)/, $(notdir $(TEST_MODULES:.c=.o)))
TEST_TARGETS = $(addprefix $(BIN)/, $(basename $(notdir $(TEST_MODULES))))
# test framework
TEST_FRAMEWORK = $(TEST_DIR)/framework.c
TEST_FRAMEWORK_OBJ = $(addprefix $(BIN)/, $(notdir $(TEST_FRAMEWORK:.c=.o)))

all: $(TEST_TARGETS)

# debug info
$(info test_module: $(TEST_MODULES))
$(info test_obj: $(TEST_OBJS))
$(info test_target: $(TEST_TARGETS))
$(info test_framework: $(TEST_FRAMEWORK), test_framework_obj: $(TEST_FRAMEWORK_OBJ))
$(info includes: $(CORE_INCLUDES), $(TEST_INCLUDES))
$(info core_src: $(CORE_SRC))

# pattern rule for core module
$(BIN)/%.o: $(CORE_DIR)/%.o | $(BIN)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# pattern rule to compile test module
$(BIN)/%: $(TEST_DIR)/%.o $(TEST_FRAMEWORK_OBJ) $(CORE_OBJS) | $(BIN)
	@$(CC) $(CFLAGS) $(CORE_INCLUDES) $(TEST_INCLUDES) $< $(TEST_FRAMEWORK_OBJ) $(CORE_OBJS) $(LDFLAGS) -o $@

# generate framework object file
$(TEST_FRAMEWORK_OBJ): $(TEST_FRAMEWORK) | $(BIN)
	@$(CC) $(CFLAGS) $(CORE_INCLUDES) $(TEST_INCLUDES) -c $< -o $@

# create bin directory
$(BIN):
	@mkdir -p $(BIN)

# run all test
runtest: all
	@for test in $(TEST_TARGETS); do\
		echo "Running $$test...";\
		$$test;\
	done

# clean up
clean:
	rm -rf $(TEST_OBJS) $(TEST_TARGETS) $(TEST_FRAMEWORK_OBJ)

cleanall: clean
	rm -rf $(BIN)

.PHONY: all runtest clean cleanall

