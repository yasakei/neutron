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
    JSONLIB = -ljsoncpp
endif

LIBTARGET = libneutron_runtime$(SHARED_EXT)

SRCS = $(shell find src -type f -name "*.cpp" ! -name "main.cpp" ! -name "bytecode_runner.cpp")
LIBSRCS = $(shell find libs -type f -name \"*.cpp\")
BOXSRCS = $(shell find $(BOXDIR) -type f -name \"*.cpp\")
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

$(LIBTARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) $(OBJS) -lcurl $(JSONLIB) -o $(LIBTARGET)
	@echo \"Runtime library created. Located at ./$(LIBTARGET)\"

$(TARGET): $(OBJS) build/main.o
	$(CXX) $(CXXFLAGS) $(RTLDYNAMIC_FLAG) build/main.o $(OBJS) -lcurl $(JSONLIB) -o $(TARGET)
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

.PHONY: shared_libs
shared_libs:
	@for dir in $(wildcard $(BOXDIR)/*); do \\
		if [ -f \"$dir/native.cpp\" ]; then \\
			module_name=`basename $$dir`; \\
			echo \"Building shared library for $$module_name...\"; \\
			$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) $$dir/native.cpp -lcurl $(JSONLIB) -o $$dir/$$module_name$(SHARED_EXT); \\
			echo \"Created shared library: $$dir/$$module_name$(SHARED_EXT)\"; \\
		elif [ -f \"$dir/native.c\" ]; then \\
			module_name=`basename $$dir`; \\
			$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) $$dir/native.c -lcurl $(JSONLIB) -o $$dir/$$module_name$(SHARED_EXT); \\
			echo \"Created shared library: $$dir/$$module_name$(SHARED_EXT)\"; \\
		fi \\
	done


clean:
	rm -rf $(BUILDDIR) $(TARGET) $(LIBTARGET)

# Rule to build individual box modules
# Pattern: box/module_name/module_name.dylib
box/%/%.dylib:
	@modulename=$*; \\
	echo \"Building box module: $modulename\"; \\
	if [ -f \"box/$modulename/native.cpp\" ]; then \\
		$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) box/$modulename/native.cpp -lcurl $(JSONLIB) -o box/$modulename/$modulename.dylib; \\
		echo \"Created shared library: box/$modulename/$modulename.dylib\"; \\
	elif [ -f \"box/$modulename/native.c\" ]; then \\
		$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) box/$modulename/native.c -lcurl $(JSONLIB) -o box/$modulename/$modulename.dylib; \\
		echo \"Created shared library: box/$modulename/$modulename.dylib\"; \\
	else \\
		echo \"Error: No native.c or native.cpp file found in box/$modulename\"; \\
		exit 1; \\
	fi

# Rule for Linux modules (if needed)
box/%/%.so:
	@modulename=$*; \\
	echo \"Building box module: $modulename\"; \\
	if [ -f \"box/$modulename/native.cpp\" ]; then \\
		$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) box/$modulename/native.cpp -lcurl $(JSONLIB) -o box/$modulename/$modulename.so; \\
		echo \"Created shared library: box/$modulename/$modulename.so\"; \\
	elif [ -f \"box/$modulename/native.c\" ]; then \\
		$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -fPIC $(DEPENDENCIES) box/$modulename/native.c -lcurl $(JSONLIB) -o box/$modulename/$modulename.so; \\
		echo \"Created shared library: box/$modulename/$modulename.so\"; \\
	else \\
		echo \"Error: No native.c or native.cpp file found in box/$modulename\"; \\
		exit 1; \\
	fi

install:
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)