CXX = g++
CC = gcc
SRCDIR = src
INCDIR = include
BUILDDIR = build
BOXDIR = box
TARGET = neutron
SRCS = $(shell find src -type f -name "*.cpp")
LIBSRCS_CPP = $(shell find libs -type f -name "*.cpp")
LIBSRCS_C = $(shell find libs -type f -name "*.c")
BOXSRCS = $(shell find $(BOXDIR) -type f -name "*.cpp")
OBJS = $(SRCS:src/%.cpp=build/%.o)
LIBOBJS_CPP = $(LIBSRCS_CPP:libs/%.cpp=build/%.o)
LIBOBJS_C = $(LIBSRCS_C:libs/%.c=build/%.o)
BOXOBJS = $(BOXSRCS:$(BOXDIR)/%.cpp=build/box/%.o)
DEPENDENCIES = -I$(INCDIR) -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -I$(BOXDIR)

# Default to release build
DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0
    CFLAGS = -Wall -Wextra -g -O0
else
    CXXFLAGS = -std=c++17 -Wall -Wextra -O2
    CFLAGS = -Wall -Wextra -O2
endif

.PHONY: all clean directories release

all: directories $(TARGET)

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

$(TARGET): $(OBJS) $(LIBOBJS_CPP) $(LIBOBJS_C) $(BOXOBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBOBJS_CPP) $(LIBOBJS_C) $(BOXOBJS) -lcurl -o $(TARGET)
	@echo "Compilation complete. Binary located at ./$(TARGET)"

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

build/%.o: libs/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

build/%.o: libs/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPENDENCIES) -c $< -o $@

build/box/%.o: $(BOXDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPENDENCIES) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
