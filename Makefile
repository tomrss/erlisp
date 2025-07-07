CC = gcc
# if readline
CFLAGS = -Wall -Wextra -O2 -g -DHAVE_READLINE=1
LFLAGS = -g -lreadline
# else (no readline): comment above, decomment this
# CFLAGS = -Wall -Wextra -O2 -g
# LFLAGS = -g
# endif
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


.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: test_$(TARGET)
	./$(TEST_DIR)/$<

$(OBJ_DIR)/test_%.o: $(TEST_DIR)/test_%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test_$(TARGET): $(OBJECTS_NOMAIN) $(TEST_OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS_NOMAIN) $(TEST_OBJECTS) -o $(TEST_DIR)/test_$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_TARGET)
