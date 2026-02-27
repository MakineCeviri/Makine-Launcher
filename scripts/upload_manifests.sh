#!/usr/bin/env bash
# Upload updated manifest files to R2 bucket.
# Requires: wrangler (npx wrangler) authenticated with Cloudflare.
#
# Usage:
#   bash scripts/upload_manifests.sh              # Upload all
#   bash scripts/upload_manifests.sh --dry-run    # Preview only

set -euo pipefail

BUCKET="makineai-translations"
ASSETS_DIR="${ASSETS_DIR:-C:/cedra/MakineAI-Assets}"
DRY_RUN="${1:-}"

echo "========================================"
echo "  R2 Manifest Upload"
echo "========================================"
echo "  Bucket: $BUCKET"
echo "  Source: $ASSETS_DIR"
[[ "$DRY_RUN" == "--dry-run" ]] && echo "  MODE: DRY RUN"
echo ""

upload_file() {
    local src="$1"
    local key="$2"
    local content_type="$3"

    if [[ "$DRY_RUN" == "--dry-run" ]]; then
        echo "  [DRY] $key ($content_type)"
    else
        npx wrangler r2 object put "$BUCKET/$key" \
            --file="$src" \
            --content-type="$content_type" \
            --cache-control="public, max-age=300" \
            2>/dev/null
        echo "  ✓ $key"
    fi
}

# 1. index.json
echo "Uploading index.json..."
upload_file "$ASSETS_DIR/index.json" "assets/index.json" "application/json"

# 2. Per-game package JSONs
echo ""
echo "Uploading packages/*.json..."
count=0
for f in "$ASSETS_DIR"/packages/*.json; do
    filename=$(basename "$f")
    upload_file "$f" "assets/packages/$filename" "application/json"
    count=$((count + 1))
done
echo "  Total: $count package files"

echo ""
echo "========================================"
echo "  Done! $((count + 1)) files uploaded."
echo "========================================"
