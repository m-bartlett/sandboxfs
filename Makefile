# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS =
MAKEFILE := $(firstword $(MAKEFILE_LIST))
MAKEFLAGS += --no-builtin-rules

BINARY = sandboxfs
PREFIX ?= /usr/local
BINPREFIX  = $(PREFIX)/bin
INSTALL_TARGET := $(addprefix $(BINPREFIX)/,$(BINARY))

# SOURCE_DIR := src
# SOURCE_FILES := $(shell find $(SOURCE_DIR) -type f -name '*.c')
SOURCE_FILES := $(filter-out test-%, $(wildcard *.c))
OBJECTS      := $(SOURCE_FILES:.c=.o)
HEADERS      := $(filter-out main% test-%, $(wildcard *.h))

# LIBS :=
# LINK_LIBS :=
# LDFLAGS := $(shell pkg-config --libs $(LIBS)) $(addprefix -l, $(LINK_LIBS))
# CFLAGS := $(shell pkg-config --cflags $(LIBS))

all: $(BINARY)

print:
	@echo "$(BINARY)"
	@echo "$(OBJECTS)"
	@echo "$(SOURCE_FILES)"
	@echo "$(HEADERS)"


$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJECTS): $(SOURCE_FILES) $(HEADERS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


install:
	install --owner=root --group=root --mode=4755 $(BINARY) $(INSTALL_TARGET)

install-local: $(BINARY)
	chown root:root $(BINARY)
	chmod 4755 $(BINARY)




# Uninstall the binary
uninstall:
	@echo "Removing $(BINARY) from $(INSTALL_DIR)..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Error: Uninstallation requires root privileges"; \
		echo "Please run 'sudo make uninstall'"; \
		exit 1; \
	fi
	rm -f $(INSTALL_DIR)/$(BINARY)
	@echo "Uninstallation complete."

# Clean build files
clean:
	@rm -fv $(BINARY) $(OBJECTS)

# Help information
help:
	@echo "Available targets:"
	@echo "  all        - Build the $(BINARY) program (default)"
	@echo "  install    - Install the program to $(INSTALL_DIR) with setuid bit (requires root)"
	@echo "  uninstall  - Remove the program from $(INSTALL_DIR) (requires root)"
	@echo "  clean      - Remove built binaries and object files"
	@echo "  help       - Display this help information"

.PHONY: all install uninstall clean help