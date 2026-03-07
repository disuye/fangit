#!/bin/bash
# fangit Release Build Script (Ninja)
# ./scripts/ninja-release.sh              <- ARM release + signed DMG
# ./scripts/ninja-release.sh universal    <- Universal (arm64+x86_64) release + signed DMG
# RUN:
# ./scripts/ninja-release.sh && ./scripts/ninja-release.sh universal

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
QT_VERSION="6.10.1"
QT_PREFIX="$HOME/Qt/${QT_VERSION}/macos"
MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"

# Signing + Notarization
SIGNING_ID="Developer ID Application: Daniel Findlay (Y9MKGP7V9D)"
NOTARY_PROFILE="Notary-Password"

# Parse arguments
ARCH_FLAGS=""
ARCH_LABEL="ARM"
BUILD_DIR="$PROJECT_DIR/build-mac-arm-release"

case "$1" in
    universal)
        ARCH_FLAGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0"
        ARCH_LABEL="Universal (arm64+x86_64)"
        BUILD_DIR="$PROJECT_DIR/build-mac-universal-release"
        ;;
    "")
        ;;
    *)
        echo "Usage: $0 [universal]"
        exit 1
        ;;
esac

VERSION=$(sed -n 's/.*VERSION_STR "\([^"]*\)".*/\1/p' "$PROJECT_DIR/src/MainWindow.h")
DMG_NAME="fangitv${VERSION}.dmg"

echo "Building $ARCH_LABEL Release (v${VERSION})..."
echo "Using Qt from: $QT_PREFIX"
echo ""

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 1. Configure
echo "⚙️  Configuring with CMake (Ninja)..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    $ARCH_FLAGS
echo ""

# 2. Build
echo "🔨 Building fangit ($ARCH_LABEL Release)..."
cd "$BUILD_DIR"
time ninja
echo ""

# 3. Deploy Qt frameworks into .app
echo "📦 Deploying Qt frameworks..."
"$MACDEPLOYQT" fangit.app
echo ""

# 4. Sign with hardened runtime
echo "🔏 Signing..."
codesign --deep --force --verify --verbose \
    --sign "$SIGNING_ID" \
    --options runtime \
    fangit.app
codesign --verify --deep --strict fangit.app
echo "Signature OK"
echo ""

# 5. Create DMG
echo "📀 Creating DMG..."
hdiutil create -volname "fangit" \
    -srcfolder fangit.app \
    -ov -format UDZO \
    "$DMG_NAME"
codesign --force --sign "$SIGNING_ID" "$DMG_NAME"
echo ""

# 6. Notarize
echo "🍎 Submitting for notarization..."
xcrun notarytool submit "$DMG_NAME" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait
echo ""

# 7. Staple
echo "📎 Stapling notarization ticket..."
xcrun stapler staple "$DMG_NAME"
echo ""

echo "✓ macOS $ARCH_LABEL release build complete!"
echo "  App bundle: $BUILD_DIR/fangit.app"
echo "  DMG ready:  $BUILD_DIR/$DMG_NAME"
echo "  Signed, notarized, and stapled."
