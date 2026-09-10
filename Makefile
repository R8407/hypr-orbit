PLUGIN_NAME := hypr-orbit

PREFIX      ?= /usr/local
LIBDIR      ?= $(PREFIX)/lib/hyprland

CXX         := g++

CXXFLAGS := \
	-O2 \
	-fPIC \
	-fno-lto \
	-fno-gnu-unique \
	-std=c++26 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wno-unused-parameter

CXXFLAGS += -DWLR_USE_UNSTABLE
CXXFLAGS += $(shell pkg-config --cflags \
	hyprland \
	hyprutils \
	hyprgraphics \
	hyprcursor \
	cairo \
	pixman-1 \
	libdrm \
	wayland-server \
	xkbcommon \
	libinput)

LDFLAGS  := -shared
LDLIBS   := $(shell pkg-config --libs \
	hyprland \
	hyprutils \
	hyprgraphics \
	hyprcursor \
	cairo \
	pixman-1 \
	libdrm \
	wayland-server \
	xkbcommon \
	libinput)

SOURCES := \
	src/main.cpp \
	src/orbit/Snapshot.cpp \
	src/orbit/Geometry.cpp \
	src/orbit/Renderer.cpp

.PHONY: all clean install uninstall

all: $(PLUGIN_NAME).so

$(PLUGIN_NAME).so: $(SOURCES)
	@echo "Building $(PLUGIN_NAME)..."
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
	@echo "Done: $(PLUGIN_NAME).so"

clean:
	rm -f $(PLUGIN_NAME).so

install: $(PLUGIN_NAME).so
	install -Dm755 $(PLUGIN_NAME).so $(DESTDIR)$(LIBDIR)/$(PLUGIN_NAME).so

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(PLUGIN_NAME).so
