#!/bin/sh

DEVICE_NAME=$1
TIMEOUT=$2

sudo xcode-select -s /Applications/Xcode_${xcodever}.app/Contents/Developer

xcrun simctl boot "$DEVICE_NAME"
xcrun simctl install booted build/build/Release-iphonesimulator/luanti.app

# Run the iOS app in the background
xcrun simctl launch --console booted org.luanti.luanti --run-unittests 2> log.txt &
APP_PID=$!

# Initialize variables
CHECK_INTERVAL=15
ELAPSED_TIME=0
FOUND_RESULT=false

# Monitor the log file
while [ $ELAPSED_TIME -lt $TIMEOUT ]; do
    if grep -q "Unit Test Results:" log.txt; then
        FOUND_RESULT=true
        break
    fi
    sleep $CHECK_INTERVAL
    ELAPSED_TIME=$((ELAPSED_TIME + CHECK_INTERVAL))
done

# Terminate the app
if $FOUND_RESULT; then
    echo "Unit test results found. Terminating the app."
else
    echo "Timeout reached. Terminating the app."
fi
xcrun simctl terminate booted org.luanti.luanti
xcrun simctl shutdown booted

