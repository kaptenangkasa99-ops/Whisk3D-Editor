#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.." # script lives in platform/android/ -> repo root 2 levels up

ANDROID_NDK="${ANDROID_NDK:-/opt/android-ndk}"
NDK_BUILD="$ANDROID_NDK/ndk-build"
AAPK_PATH="${AAPK_PATH:-$(pwd)/platform/android/app/src/main/jniLibs}"
APROJECT_PATH="${APROJECT_PATH:-$(pwd)/platform/android}"

if [[ ! -x "$NDK_BUILD" ]]; then
    echo "ERROR: ndk-build not found."
    exit 1
fi

if [[ ! -d thirdparty/SDL2 ]]; then
    echo "ERROR: run the script from the repo root."
    exit 1
fi

# LIMITED -j, NEVER plain $(nproc): there are ~200 compilation units PER ABI and with
# unlimited parallelism the OOM killer closes the entire session (same criteria as
# W3dCompilarJobs, main/io/CompilarJuego.h). Can be overridden with W3D_JOBS=n.
if [[ -z "${W3D_JOBS:-}" ]]; then
    W3D_NPROC="$(nproc 2>/dev/null || echo 2)"
    W3D_JOBS=$(( W3D_NPROC / 2 ))
    if (( W3D_JOBS < 1 )); then W3D_JOBS=1; fi
    if (( W3D_JOBS > 8 )); then W3D_JOBS=8; fi
fi
echo "Compiling with -j${W3D_JOBS} (W3D_JOBS to change it)."

# If no ABI is specified, compile all.
if [[ $# -eq 0 ]]; then
    set -- arm64 amd64 arm32
fi

abis=()

for abi in "$@"; do
    case "$abi" in
        arm64-v8a|aarch64|arm64)
            abis+=(arm64-v8a)
            ;;
        x86_64|amd64)
            abis+=(x86_64)
            ;;
        armeabi-v7a|arm|arm32)
            abis+=(armeabi-v7a)
            ;;
        *)
            echo "Unknown ABI: $abi"
            exit 1
            ;;
    esac
done

mkdir -p "$AAPK_PATH"

# Editor resources -> APK assets. REGENERATED on each build from canonical res/
# (it is gitignored and NOT committed, to avoid duplication/outdating).
echo "Copying res/ to APK (assets)..."
mkdir -p "$APROJECT_PATH/app/src/main/assets"   # assets/ is gitignored (not committed) -> create if it doesn't exist
rm -rf "$APROJECT_PATH/app/src/main/assets/res"
cp -r res "$APROJECT_PATH/app/src/main/assets/res"

build_one() {
    local abi="$1"

    echo "[BUILD] $abi"

    # jni/ lives in platform/android -> NDK_PROJECT_PATH there (libs/ and obj/ go inside
    # platform/android, not in the repo root next to submodules).
    "$NDK_BUILD" \
        -j"${W3D_JOBS}" \
        -s \
        APP_ABI="$abi" \
        NDK_PROJECT_PATH="$APROJECT_PATH" \
        APP_BUILD_SCRIPT="$APROJECT_PATH/jni/Android.mk" \
        NDK_APPLICATION_MK="$APROJECT_PATH/jni/Application.mk" \
        >/dev/null

    mkdir -p "$AAPK_PATH/$abi"
    cp "$APROJECT_PATH/libs/$abi"/*.so "$AAPK_PATH/$abi"/

    echo "[ OK ] $abi"
}

pids=()

for abi in "${abis[@]}"; do
    build_one "$abi" &
    pids+=($!)
done

failed=0

for pid in "${pids[@]}"; do
    wait "$pid" || failed=1
done

if [[ $failed -ne 0 ]]; then
    echo
    echo "Some compilation failed."
    exit 1
fi

echo
echo "All set."
echo "Libraries copied to:"
echo "  $AAPK_PATH"

echo "Packaging with gradlew..."
cd "$APROJECT_PATH"

# Check if gradle-wrapper.jar exists; download if missing (e.g. fresh git checkout without LFS/binary wrapper)
WRAPPER_JAR="$APROJECT_PATH/gradle/wrapper/gradle-wrapper.jar"
if [[ ! -f "$WRAPPER_JAR" ]]; then
    echo "gradle-wrapper.jar not found. Fetching..."
    mkdir -p "$APROJECT_PATH/gradle/wrapper"
    if command -v wget >/dev/null 2>&1; then
        wget -q https://raw.githubusercontent.com/gradle/gradle/v8.5.0/gradle/wrapper/gradle-wrapper.jar -O "$WRAPPER_JAR"
    elif command -v curl >/dev/null 2>&1; then
        curl -sSL https://raw.githubusercontent.com/gradle/gradle/v8.5.0/gradle/wrapper/gradle-wrapper.jar -o "$WRAPPER_JAR"
    else
        echo "ERROR: Neither wget nor curl found to download gradle-wrapper.jar" >&2
        exit 1
    fi
fi

chmod +x ./gradlew
./gradlew assembleDebug
echo "Application packaged with gradlew (signed with debug keystore)."

# The APK name is built by gradle (app/build.gradle: whisk3d-editor-<date>-<abis>.apk) and changes with the
# date and compiled ABIs -> do NOT hardcode a pattern here. Takes the NEWEST .apk from the
# output folder. The '|| true' is due to 'set -euo pipefail': if the glob finds nothing the pipe fails
# and killed the script with exit 2 even if the APK packaged fine (this block is just reporting).
OUTDIR="$APROJECT_PATH/app/distribution/android/app/outputs/apk/debug"
APK=$(find "$OUTDIR" -maxdepth 1 -name '*.apk' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-) || true
if [[ -n "${APK:-}" ]]; then
    echo "output: $APK"
else
    echo "ERROR: gradlew finished but there is no .apk in $OUTDIR" >&2
    exit 1
fi