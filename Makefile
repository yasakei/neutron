# Detect the operating system
UNAME_S := $(shell uname -s)

SRCDIR = src
INCDIR = include
BUILDDIR = build
BOXDIR = box
TARGET = neutron

# Set appropriate compiler and flags based on OS
ifeq ($(UNAME_S),Darwin)
    # macOS settings
    CXX = clang++
    SHARED_EXT = .dylib
    SHARED_FLAGS = -dynamiclib
    RTLDYNAMIC_FLAG = 
    # Find jsoncpp library path
    ifeq (,$(wildcard /opt/homebrew/lib/libjsoncpp.dylib))
        ifeq (,$(wildcard /usr/local/lib/libjsoncpp.dylib))
            JSONLIB = -ljsoncpp
        else
            JSONLIB = -L/usr/local/lib -ljsoncpp
            DEPENDENCIES += -I/usr/local/include
        endif
    else
        JSONLIB = -L/opt/homebrew/lib -ljsoncpp
        DEPENDENCIES += -I/opt/homebrew/include
    endif
else
    # Linux settings
    CXX = g++
    SHARED_EXT = .so
    SHARED_FLAGS = -shared -rdynamic
    RTLDYNAMIC_FLAG = -rdynamic
    # Use pkg-config to find jsoncpp on Linux, if available
    ifneq ($(shell which pkg-config),)
        JSONLIB_PKG = $(shell pkg-config --libs jsoncpp)
        JSON_CFLAGS_PKG = $(shell pkg-config --cflags jsoncpp)
    endif
    # If pkg-config found jsoncpp, use it. Otherwise, fallback to default.
    ifneq ($(JSONLIB_PKG),)
        JSONLIB = $(JSONLIB_PKG)
        DEPENDENCIES += $(JSON_CFLAGS_PKG)
    else
        JSONLIB = -ljsoncpp
    endif
endif

LIBTARGET = libneutron_runtime$(SHARED_EXT)

SRCS = $(shell find src -type f -name "*.cpp" ! -name "main.cpp" ! -name "bytecode_runner.cpp")
LIBSRCS = $(shell find libs -type f -name "*.cpp")
BOXSRCS = $(shell find $(BOXDIR) -type f -name "*.cpp")
OBJS = $(SRCS:src/%.cpp=build/%.o)
LIBOBJS = $(LIBSRCS:libs/%.cpp=build/%.o)
BOXOBJS = $(BOXSRCS:$(BOXDIR)/%.cpp=build/box/%.o)
DEPENDENCIES = -I$(INCDIR) -I. -Ilibs -I$(BOXDIR)

# Default to release build
DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0 -DDEBUG_TRACE_EXECUTION -DDEBUG_PRINT_CODE -fPIC
else
    CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC
endif

.PHONY: all clean directories release

all: directories $(TARGET) $(LIBTARGET)

build-box:
	@if [ -z "$(MODULE)" ]; then \
		echo "Error: MODULE variable not set."; \
		exit 1; \
	fi
	@echo "Building box module: $(MODULE)"
	@if [ -f "box/$(MODULE)/native.cpp" ]; then \
		SOURCE_FILE="box/$(MODULE)/native.cpp"; \
		echo "Found source: $$SOURCE_FILE"; \
		$(CXX) -v $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) $$SOURCE_FILE $(JSONLIB) -lcurl -L. -lneutron_runtime $(PLUGIN_LDFLAGS) -o box/$(MODULE)/$(MODULE)$(SHARED_EXT) || exit $$?; \
	elif [ -f "box/$(MODULE)/native.c" ]; then \
		SOURCE_FILE="box/$(MODULE)/native.c"; \
		echo "Found source: $$SOURCE_FILE"; \
		$(CXX) -v $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) $$SOURCE_FILE $(JSONLIB) -lcurl -L. -lneutron_runtime $(PLUGIN_LDFLAGS) -o box/$(MODULE)/$(MODULE)$(SHARED_EXT) || exit $$?; \
	else \
		echo "Error: No native.c or native.cpp in box/$(MODULE)"; \
		exit 1; \
	fi
	@echo "Created box module: box/$(MODULE)/$(MODULE)$(SHARED_EXT)"

release:
	$(MAKE) all DEBUG=0
	strip $(TARGET)
	@echo \"Release build complete. Binary located at ./$(TARGET)\"

directories:
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(BUILDDIR)/core
	@mkdir -p $(BUILDDIR)/json
	@mkdir -p $(BUILDDIR)/http
	@mkdir -p $(BUILDDIR)/time
	@mkdir -p $(BUILDDIR)/sys
	@mkdir -p $(BUILDDIR)/box

$(LIBTARGET): $(OBJS) $(LIBOBJS)
	$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) $(OBJS) $(LIBOBJS) -lcurl $(JSONLIB) -o $(LIBTARGET)
	@echo \"Runtime library created. Located at ./$(LIBTARGET)\"

$(TARGET): build/main.o $(LIBTARGET)
	$(CXX) $(CXXFLAGS) $(RTLDYNAMIC_FLAG) build/main.o -L. -lneutron_runtime -o $(TARGET)
	@echo "Compilation complete. Binary located at ./$(TARGET)"

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

build/%.o: libs/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

build/box/%.o: $(BOXDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

build/main.o: src/main.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

### Box module build rule #################################################
# Invoked by:   ./neutron --build-box <name>
# Expects make target: box/<name>/<name>$(SHARED_EXT)
# We do NOT want to eagerly rebuild the main runtime here; assume user ran `make`.
# If libneutron_runtime$(SHARED_EXT) exists we link to it, otherwise on macOS we
# fall back to -Wl,-undefined,dynamic_lookup so symbols resolve at load.

# Per-platform extra flags for plugin style modules
ifeq ($(UNAME_S),Darwin)
PLUGIN_LDFLAGS = -Wl,-rpath,@loader_path/../../ -L. -lneutron_runtime
else
PLUGIN_LDFLAGS = -Wl,-rpath,'$ORIGIN/../..' -L. -lneutron_runtime
endif

# Pattern: box/foo/foo.dylib (mac) or box/foo/foo.so (linux)
box/%/%.$(SHARED_EXT):
	@modulename=$(basename $@ .$(SHARED_EXT)); \
	dirname=$(dirname $@); \
	module=$(basename "$dirname"); \
	if [ -f "box/$module/native.cpp" ]; then \
		SOURCES=$(find box/$module -name "*.cpp"); \
		echo "Building box module: $module -> $@"; \
		for src in $SOURCES; do echo "  -> $src"; done; \
		$(CXX) -v $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) $SOURCES $(JSONLIB) -lcurl -L. -lneutron_runtime $(PLUGIN_LDFLAGS) -o $@ || exit $?; \
	elif [ -f "box/$module/native.c" ]; then \
		$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) box/$module/native.c $(JSONLIB) -lcurl -L. -lneutron_runtime $(PLUGIN_LDFLAGS) -o $@; \
	else \
		echo "Error: No native.c or native.cpp in box/$module"; exit 1; \
	fi; \
	echo "Created box module: $@"

install:
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)