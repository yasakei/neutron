CXX = g++
SRCDIR = src
INCDIR = include
BUILDDIR = build
BOXDIR = box
TARGET = neutron
SRCS = $(shell find src -type f -name "*.cpp")
LIBSRCS = $(shell find libs -type f -name "*.cpp")
BOXSRCS = $(shell find $(BOXDIR) -type f -name "*.cpp")
OBJS = $(SRCS:src/%.cpp=build/%.o)
LIBOBJS = $(LIBSRCS:libs/%.cpp=build/%.o)
BOXOBJS = $(BOXSRCS:$(BOXDIR)/%.cpp=build/box/%.o)
DEPENDENCIES = -I$(INCDIR) -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -I$(BOXDIR) -Ilibs/websocket

# Default to release build
DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0 -DDEBUG_TRACE_EXECUTION -DDEBUG_PRINT_CODE
else
    CXXFLAGS = -std=c++17 -Wall -Wextra -O2
endif

.PHONY: all clean directories release

all: directories shared_libs $(TARGET)

release:
	$(MAKE) all DEBUG=0
	strip $(TARGET)
	@echo "Release build complete. Binary located at ./$(TARGET)"

directories:
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(BUILDDIR)/core
	@mkdir -p $(BUILDDIR)/json
	@mkdir -p $(BUILDDIR)/http
	@mkdir -p $(BUILDDIR)/time
	@mkdir -p $(BUILDDIR)/sys
	@mkdir -p $(BUILDDIR)/box

$(TARGET): $(OBJS) $(LIBOBJS)
	$(CXX) $(CXXFLAGS) -rdynamic $(OBJS) $(LIBOBJS) -lcurl -ljsoncpp -o $(TARGET)
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

.PHONY: shared_libs
shared_libs:
	@for dir in $(wildcard $(BOXDIR)/*); do \
		if [ -f "$dir/native.cpp" ]; then \
			module_name=$(basename $dir); \
			echo "dir: $dir, module_name: $module_name"; \
			$(CXX) $(CXXFLAGS) -shared -fPIC $(DEPENDENCIES) $dir/native.cpp $dir/*.cpp -lcurl -ljsoncpp -o $dir/$module_name.so; \
			echo "Created shared library: $dir/$module_name.so"; \
		elif [ -f "$dir/native.c" ]; then \
			module_name=$(basename $dir); \
			$(CXX) $(CXXFLAGS) -shared -fPIC $(DEPENDENCIES) $dir/native.c -lcurl -ljsoncpp -o $dir/$module_name.so; \
			echo "Created shared library: $dir/$module_name.so"; \
		fi \
	done


clean:
	rm -rf $(BUILDDIR) $(TARGET)

install:
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)