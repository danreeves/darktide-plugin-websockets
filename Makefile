# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Ilib -Isrc -I"C:/msys64/mingw64/include" --std=c++11 -s

# Linker flags
LDFLAGS = -L"C:/msys64/mingw64/lib" -lstdc++ -lwsock32 -lssl -lcrypto -lws2_32

# Target
TARGET = darktide_ws_plugin.dll

# Source files
SRCS = ./src/plugin.cpp

script: ./src/script.lua
	powershell.exe ./build.ps1

# Build target
plugin: script $(SRCS)
	$(CC) $(CFLAGS) -shared -o $(TARGET) $(SRCS) $(LDFLAGS)

# Clean target
clean:
	rm $(TARGET) 
