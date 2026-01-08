#!/bin/zsh

# Clean build and upload script for PlatformIO

# Kill any process using the serial port (like Serial Monitor)
echo "🔌 Closing any open serial connections..."

# Find and kill ALL processes using the serial port
for port in /dev/cu.usbmodem*; do
    if [ -e "$port" ]; then
        pids=$(lsof -t "$port" 2>/dev/null)
        if [ -n "$pids" ]; then
            echo "   Killing processes on $port: $pids"
            echo "$pids" | xargs kill -9 2>/dev/null
        fi
    fi
done

# Also kill common serial monitor processes
pkill -9 -f "pio device monitor" 2>/dev/null
pkill -9 -f "screen /dev/cu.usbmodem" 2>/dev/null
pkill -9 -f "minicom" 2>/dev/null
pkill -9 -f "serial-monitor" 2>/dev/null

sleep 2

echo "🧹 Cleaning build files..."
pio run -t clean

if [ $? -ne 0 ]; then
    echo "❌ Clean failed!"
    exit 1
fi

echo ""
echo "🔨 Building project..."
pio run

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "📤 Uploading to device..."
echo "   (Close Serial Monitor in VS Code if this fails!)"

# Try to force-close the port by toggling DTR
stty -f /dev/cu.usbmodemF412FA6E9A602 hupcl 2>/dev/null
sleep 1

pio run -t upload --upload-port /dev/cu.usbmodemF412FA6E9A602

if [ $? -ne 0 ]; then
    echo "❌ Upload failed!"
    exit 1
fi

echo ""
echo "✅ Done! Code uploaded successfully."
