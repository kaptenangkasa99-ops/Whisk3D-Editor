#!/usr/bin/env bash
set -euo pipefail

# 1. Update system dependencies and install Java 17 + minimal build tools & runtime libs
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    openjdk-17-jdk-headless \
    cmake \
    ninja-build \
    make \
    git \
    unzip \
    wget \
    libc6 \
    libstdc++6 \
    zlib1g
sudo apt-get clean && sudo rm -rf /var/lib/apt/lists/*

# 2. Set environment paths for current session
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME=$HOME/android-sdk
export ANDROID_NDK=$HOME/android-ndk
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$ANDROID_NDK

# 3. Clean up older API installations (if present)
rm -rf "$ANDROID_HOME/platforms/android-33"
rm -rf "$ANDROID_HOME/build-tools/33.0.2"

# 4. Download and extract Android Command-line Tools
mkdir -p "$ANDROID_HOME/cmdline-tools"
cd /tmp
wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O cmdline-tools.zip
unzip -q cmdline-tools.zip
rm -rf "$ANDROID_HOME/cmdline-tools/latest"
mv cmdline-tools "$ANDROID_HOME/cmdline-tools/latest"
rm -f cmdline-tools.zip

# 5. Accept licenses and install Android SDK components
yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses > /dev/null
$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager "platform-tools" "platforms;android-36" "build-tools;36.0.0"

# 6. Download and extract Android NDK (r25c)
cd /tmp
wget -q https://dl.google.com/android/repository/android-ndk-r25c-linux.zip -O ndk.zip
rm -rf "$ANDROID_NDK" "$HOME/android-ndk-r25c"
unzip -q ndk.zip -d "$HOME/"
mv "$HOME/android-ndk-r25c" "$ANDROID_NDK"
rm -f ndk.zip

# 7. Correctly persist environment variables in ~/.bashrc
grep -qF 'JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' ~/.bashrc || echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
grep -qF 'ANDROID_HOME=$HOME/android-sdk' ~/.bashrc || echo 'export ANDROID_HOME=$HOME/android-sdk' >> ~/.bashrc
grep -qF 'ANDROID_NDK=$HOME/android-ndk' ~/.bashrc || echo 'export ANDROID_NDK=$HOME/android-ndk' >> ~/.bashrc
grep -qF 'ANDROID_NDK' ~/.bashrc || echo 'export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$ANDROID_NDK' >> ~/.bashrc

# 8. Return to workspace
cd /workspaces/Whisk3D-Editor

source ~/.bashrc

ndk-build -v
$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --version