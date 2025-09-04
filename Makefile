CC ?= gcc
# TODO -02 optimizations
CFLAGS ?= -Wall -Wextra -O0 -DHAVE_READLINE=1
LDFLAGS ?=
LDLIBS ?= -lreadline
SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj
TARGET = erlisp

MAIN_OBJ := $(OBJ_DIR)/$(TARGET).o
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
OBJECTS_NOMAIN := $(filter-out $(MAIN_OBJ),$(OBJECTS))
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS := $(patsubst $(TEST_DIR)/test_%.c,$(OBJ_DIR)/test_%.o,$(TEST_SOURCES))


.PHONY: all debug clean test run

all: $(TARGET)

debug: CFLAGS += -g -fsanitize=address
debug: LDFLAGS += -g
debug: LDLIBS += -fsanitize=address
debug: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: test_$(TARGET)
	./$(TEST_DIR)/$<

$(OBJ_DIR)/test_%.o: $(TEST_DIR)/test_%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test_$(TARGET): $(OBJECTS_NOMAIN) $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS_NOMAIN) $(TEST_OBJECTS) $(LDLIBS) -o $(TEST_DIR)/test_$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_TARGET)
