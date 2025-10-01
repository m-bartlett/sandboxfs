# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS =
MAKEFILE := $(firstword $(MAKEFILE_LIST))


# Binary name and installation path
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

$(OBJECTS): $(SOURCE_FILES) $(HEADERS) $(MAKEFILE)

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	sudo chown root:root $(BINARY)
	sudo chmod 4755 $(BINARY)

install-local: $(BINARY)
	install --owner=root --group=root --mode=4755 --preserve-timestamps $(BINARY) ./bin/


%.o: %.c %.h
	@$(CC) $(CFLAGS) -c $< -o $@


# Install the binary with setuid bit
install: $(BINARY)
	@echo "Installing $(BINARY) to $(INSTALL_DIR)..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Error: Installation requires root privileges"; \
		echo "Please run 'sudo make install'"; \
		exit 1; \
	fi
	install -m 0755 $(BINARY) $(INSTALL_DIR)
	chown root:root $(INSTALL_DIR)/$(BINARY)
	chmod u+s $(INSTALL_DIR)/$(BINARY)
	@echo "Installation complete. $(BINARY) installed as setuid root."
	@echo "SECURITY NOTE: Please review the code and ensure this is appropriate for your environment."

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
	rm -f $(BINARY) $(OBJECTS)

# Help information
help:
	@echo "Available targets:"
	@echo "  all        - Build the $(BINARY) program (default)"
	@echo "  install    - Install the program to $(INSTALL_DIR) with setuid bit (requires root)"
	@echo "  uninstall  - Remove the program from $(INSTALL_DIR) (requires root)"
	@echo "  clean      - Remove built binaries and object files"
	@echo "  help       - Display this help information"

.PHONY: all install uninstall clean help