#!/bin/bash
# Quick Setup Script for AquaSmart Project

echo "🚀 AquaSmart Project Setup"
echo "================================"

# Check Python
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 not found. Please install Python 3.8+"
    exit 1
fi
echo "✅ Python found: $(python3 --version)"

# Check pip
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip not found. Installing pip..."
    python3 -m pip install --upgrade pip
fi

# Install PlatformIO
echo ""
echo "📦 Installing PlatformIO..."
pip3 install --upgrade platformio

# Initialize project
echo ""
echo "🔧 Initializing PlatformIO project..."
pio project init --ide vscode

# Install libraries
echo ""
echo "📚 Installing project libraries..."
pio lib install

# Build project
echo ""
echo "🏗️  Building project..."
pio run --clean

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    echo "📋 Next steps:"
    echo "   1. Connect ESP32 via USB"
    echo "   2. Run: pio run --target upload"
    echo "   3. Monitor: pio run --target monitor"
else
    echo ""
    echo "❌ Build failed. Check errors above."
    exit 1
fi

echo ""
echo "================================"
echo "🎉 Setup complete! Happy coding!"
