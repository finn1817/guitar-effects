#!/bin/bash
echo "Starting Guitar Effects App..."
if [ -f "build/GuitarEffectsApp" ]; then
    ./build/GuitarEffectsApp
elif [ -f "build/GuitarEffectsApp.app/Contents/MacOS/GuitarEffectsApp" ]; then
    ./build/GuitarEffectsApp.app/Contents/MacOS/GuitarEffectsApp
else
    echo "ERROR: Application not found!"
    echo "Please run install.sh first."
    read -p "Press enter to continue..."
fi