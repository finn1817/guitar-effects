#!/bin/bash

echo "========================================"
echo "Guitar Effects App - Mac/Linux Installer"
echo "========================================"
echo ""

# Check if script is in correct directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "ERROR: CMakeLists.txt not found!"
    echo "Please run this script from the project root directory."
    exit 1
fi

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="mac"
    echo "Detected: macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "Detected: Linux"
else
    echo "ERROR: Unsupported OS"
    exit 1
fi

echo ""
echo "Step 1: Installing dependencies..."

if [ "$OS" == "mac" ]; then
    # Check for Homebrew
    if ! command -v brew &> /dev/null; then
        echo "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    echo "Installing packages via Homebrew..."
    brew install cmake qt@6 pkg-config
    
    QT_PATH="/opt/homebrew/opt/qt@6"
    if [ ! -d "$QT_PATH" ]; then
        QT_PATH="/usr/local/opt/qt@6"
    fi
else
    # Linux
    echo "Installing packages via apt..."
    sudo apt-get update
    sudo apt-get install -y build-essential cmake qt6-base-dev qt6-multimedia-dev libasound2-dev
    
    QT_PATH="/usr/lib/x86_64-linux-gnu/qt6"
fi

echo "Dependencies installed!"
echo ""

echo "Step 2: Downloading miniaudio..."
if [ ! -d "miniaudio" ]; then
    mkdir miniaudio
    cd miniaudio
    curl -O https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    cd ..
fi
echo "miniaudio ready!"
echo ""

echo "Step 3: Building the project..."
rm -rf build
mkdir build
cd build

if [ "$OS" == "mac" ]; then
    cmake -DCMAKE_PREFIX_PATH="$QT_PATH" ..
else
    cmake ..
fi

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

cd ..
echo "Build complete!"
echo ""

echo "Step 4: Creating application data directories..."
if [ "$OS" == "mac" ]; then
    mkdir -p "$HOME/Library/Application Support/GuitarEffectsApp/Presets"
    mkdir -p "$HOME/Library/Application Support/GuitarEffectsApp/Clips"
else
    mkdir -p "$HOME/.local/share/GuitarEffectsApp/Presets"
    mkdir -p "$HOME/.local/share/GuitarEffectsApp/Clips"
fi
echo "Directories created!"
echo ""

echo "Step 5: Making launcher executable..."
chmod +x run.sh
echo ""

echo "========================================"
echo "Installation complete!"
echo "========================================"
echo ""
echo "To run the app, execute: ./run.sh"
echo "or double-click run.sh in Finder/File Manager"
echo ""