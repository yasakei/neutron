# Makefile for Neutron Build System
# Includes targets for building box modules

# Detect OS
UNAME_S := $(shell uname -s)

# Define default target
.PHONY: all
all:
	@echo "Building Neutron runtime..."
	@cd build && make neutron_runtime

# Build a box module (external native modules)
.PHONY: build-box
build-box:
ifndef MODULE
	$(error Usage: make build-box MODULE=module_name)
endif
	@echo "Building box module: $(MODULE)"
	@echo "Module name: $(MODULE)"
	@echo "Module directory: $(MODULE)"
	@if [ ! -d "$(MODULE)" ]; then \
		echo "Error: Module directory $(MODULE) does not exist"; \
		exit 1; \
	fi
	@if [ ! -f "$(MODULE)/native.cpp" ]; then \
		echo "Error: $(MODULE)/native.cpp does not exist"; \
		exit 1; \
	fi
	@echo "Building module $(MODULE)..."
	@mkdir -p build/box
	@echo "Creating object file..."
	@$(CXX) -std=c++17 -fPIC -c $(MODULE)/native.cpp -Iinclude -I. -Iqr -o build/box/$(MODULE).o
	@echo "Creating shared library..."
	@$(CXX) -std=c++17 -fPIC -shared build/box/$(MODULE).o -o box/$(MODULE)/$(MODULE).so -Lbuild -lneutron_runtime
	@echo "Module $(MODULE) built successfully in box/$(MODULE)/$(MODULE).so"

# Alternative build-box target for different architectures
.PHONY: build-box-native
build-box-native: build-box

# If the box directory doesn't exist, create it
box-init:
	@mkdir -p box

# Install target
.PHONY: install
install:
	@echo "Installing Neutron runtime..."
	@cd build && make install

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	@cd build && make clean
	@rm -rf build/box
	@rm -rf box

# Test target
.PHONY: test
test:
	@echo "Running tests..."
	@cd build && make test