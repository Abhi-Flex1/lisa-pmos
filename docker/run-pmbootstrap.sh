#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CONTAINER_NAME="pmos-lisa-builder"
IMAGE_NAME="pmos-builder:latest"

# Detect container runtime (docker, podman, or colima)
if command -v docker &>/dev/null; then
    RUNNER="docker"
elif command -v podman &>/dev/null; then
    RUNNER="podman"
else
    echo "Error: Neither docker nor podman was found."
    echo "On macOS, install Docker Desktop or run:"
    echo "  brew install colima docker"
    echo "  colima start"
    exit 1
fi

echo "==> Using container runtime: $RUNNER"

# Build container image if not already built
if ! $RUNNER image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "==> Building container image '$IMAGE_NAME'..."
    $RUNNER build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
fi

# Run interactive session or pass arguments
echo "==> Launching pmbootstrap environment..."
$RUNNER run --rm -it \
    --privileged \
    -v /dev:/dev \
    -v "$REPO_ROOT:/workspace/postmarket-lisa" \
    -v pmos_workdir:/home/builder/.local/var/pmbootstrap \
    -w /workspace/postmarket-lisa \
    "$IMAGE_NAME" "$@"
