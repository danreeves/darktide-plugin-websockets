# Makefile

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++11 -shared -Os

# Target DLL name
TARGET = darktide_ws_pluginw64.dll

# Source file
SRC = src/darktide_ws.cpp

# LIBFILES=$(wildcard src/*.cpp)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
