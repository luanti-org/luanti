#!/bin/bash

# Install gh
# Login with gh auth login
#
# Usage: ./download_sign_android.sh 5.13.0

set -e

TAG="$1"
REPO="luanti-org/luanti"
WORKFLOW_NAME="android"
APKSIGNER="$HOME/Android/Sdk/build-tools/35.0.0/apksigner"
KEYSTORE="$HOME/Documents/keystore-luanti.jks"

if [[ -z "$TAG" ]]; then
  echo "Usage: $0 <tag>"
  exit 1
fi

echo "Finding workflow run for '$WORKFLOW_NAME' with tag '$TAG'..."

RUN_ID=$(gh run list --workflow="$WORKFLOW_NAME" --repo "$REPO" --branch "$TAG" --json databaseId -q '.[0].databaseId')

if [[ -z "$RUN_ID" ]]; then
  echo "No run found for workflow '$WORKFLOW_NAME' and tag '$TAG'"
  exit 1
fi

echo "Found run ID: $RUN_ID"

echo "Downloading all artifacts from run..."

mkdir -p unsigned
gh run download "$RUN_ID" --repo "$REPO" -D unsigned


echo "Signing packages"
read -s -p "Enter your password: " PASSWORD
echo

sign() {
    echo "Signing $2"
    "$APKSIGNER" sign --ks "$KEYSTORE" --ks-pass "pass:$PASSWORD" --min-sdk-version 21 --in "$1" --out "$2"
}

mkdir -p signed
sign unsigned/Luanti-arm64-v8a.apk/app-arm64-v8a-release-unsigned.apk "signed/luanti-$TAG-arm64-v8a.apk"
sign unsigned/Luanti-armeabi-v7a.apk/app-armeabi-v7a-release-unsigned.apk "signed/luanti-$TAG-armeabi-v7a.apk"
sign unsigned/Luanti-release.aab/app-release.aab "signed/luanti-$TAG.aab"
sign unsigned/Luanti-x86_64.apk/app-x86_64-release-unsigned.apk "signed/luanti-$TAG-x86_64.apk"
sign unsigned/Luanti-x86.apk/app-x86-release-unsigned.apk "signed/luanti-$TAG-x86.apk"

echo "Done."
