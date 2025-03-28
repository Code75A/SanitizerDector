.DEFAULT_GOAL=help
.PHONY: run build clean help asan ubsan msan

SRC = src
BUILD = build
OBJS = $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(wildcard $(SRC)/*.c))

# Common flags
CFLAGS = -I $(SRC)/include -fno-omit-frame-pointer -g
LDFLAGS = -lm -lpthread
CC = clang
TARGET = $(BUILD)/bit_shift_main

# Sanitizer-specific configurations
SANITIZER_CFLAGS =
SANITIZER_LDFLAGS =

# ASAN (AddressSanitizer)
asan: SANITIZER_CFLAGS = -fsanitize=address
asan: SANITIZER_LDFLAGS = -fsanitize=address
asan: build
	$(CC) $(CFLAGS) $(SANITIZER_CFLAGS) $(OBJS) $(LDFLAGS) $(SANITIZER_LDFLAGS) -o $(TARGET)
	@echo "Compiled with ASAN"

# UBSAN (UndefinedBehaviorSanitizer)
ubsan: SANITIZER_CFLAGS = -fsanitize=undefined
ubsan: SANITIZER_LDFLAGS = -fsanitize=undefined
ubsan: build
	$(CC) $(CFLAGS) $(SANITIZER_CFLAGS) $(OBJS) $(LDFLAGS) $(SANITIZER_LDFLAGS) -o $(TARGET)
	@echo "Compiled with UBSAN"

# MSAN (MemorySanitizer)
msan: SANITIZER_CFLAGS = -fsanitize=memory -fno-omit-frame-pointer
msan: SANITIZER_LDFLAGS = -fsanitize=memory
msan: build
	$(CC) $(CFLAGS) $(SANITIZER_CFLAGS) $(OBJS) $(LDFLAGS) $(SANITIZER_LDFLAGS) -o $(TARGET)
	@echo "Compiled with MSAN"

run: $(TARGET) ## Run the test case
	./$(TARGET)

build: $(TARGET) ## Build the test case

$(TARGET): $(OBJS) ## Build the binary
	$(CC) $(CFLAGS) $(SANITIZER_CFLAGS) $^ $(LDFLAGS) $(SANITIZER_LDFLAGS) -o $@

$(BUILD)/%.o: $(SRC)/%.c ## Build object files
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SANITIZER_CFLAGS) -c $< $(LDFLAGS) $(SANITIZER_LDFLAGS) -o $@

clean: ## Clean the test case
	rm -r $(BUILD)

help: ## Show this help
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'