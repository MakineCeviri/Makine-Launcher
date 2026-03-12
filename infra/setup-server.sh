#!/usr/bin/env bash
# MakineAI CI/CD Server Setup
#
# Bu script sunucuda Docker + Portainer + GitHub Actions runner kurar.
# SSH ile sunucuya baglanip calistir:
#   ssh user@sunucu "bash -s" < infra/setup-server.sh
#
# Gereksinimler:
#   - Docker + Docker Compose kurulu
#   - GitHub PAT (repo scope): Settings > Developer settings > PAT
#   - Runner token: github.com/MakineCeviri/MakineAI-Launcher/settings/actions/runners/new
#
set -euo pipefail

REPO="MakineCeviri/MakineAI-Launcher"
WORKDIR="/opt/makineai"

echo "=== MakineAI CI/CD Server Setup ==="
echo ""

# ── 1. Portainer (skip if already running) ─────────────────────────────
if docker ps --format '{{.Names}}' | grep -q portainer; then
    echo "[OK] Portainer already running"
else
    echo "[1/4] Installing Portainer..."
    docker volume create portainer_data
    docker run -d \
        --name portainer \
        --restart always \
        -p 9443:9443 \
        -v /var/run/docker.sock:/var/run/docker.sock \
        -v portainer_data:/data \
        portainer/portainer-ce:lts
    echo "[OK] Portainer: https://$(hostname -I | awk '{print $1}'):9443"
fi

# ── 2. Work directory ──────────────────────────────────────────────────
echo "[2/4] Setting up $WORKDIR..."
mkdir -p "$WORKDIR/releases"
mkdir -p "$WORKDIR/runner"

# ── 3. GitHub Actions runner ───────────────────────────────────────────
echo "[3/4] Setting up GitHub Actions runner..."
echo ""

if [ -f "$WORKDIR/runner/.runner" ]; then
    echo "[OK] Runner already configured"
else
    echo "Runner token gerekli."
    echo "  1. https://github.com/$REPO/settings/actions/runners/new adresine git"
    echo "  2. 'Linux' sec, token'i kopyala"
    echo ""
    read -rp "Runner token: " RUNNER_TOKEN

    if [ -z "$RUNNER_TOKEN" ]; then
        echo "[ERROR] Token bos, runner kurulumu atlaniyor"
    else
        cd "$WORKDIR/runner"

        # Download latest runner
        RUNNER_VERSION=$(curl -s https://api.github.com/repos/actions/runner/releases/latest | grep -oP '"tag_name":\s*"v\K[^"]+')
        RUNNER_FILE="actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"

        if [ ! -f "$RUNNER_FILE" ]; then
            echo "Downloading runner v${RUNNER_VERSION}..."
            curl -sL "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/${RUNNER_FILE}" -o "$RUNNER_FILE"
            tar xzf "$RUNNER_FILE"
        fi

        # Configure
        ./config.sh \
            --url "https://github.com/$REPO" \
            --token "$RUNNER_TOKEN" \
            --name "makineai-server" \
            --labels "deploy,linux,self-hosted" \
            --work "_work" \
            --unattended \
            --replace

        # Install as service
        sudo ./svc.sh install
        sudo ./svc.sh start

        echo "[OK] Runner installed and started"
    fi
fi

# ── 4. Release file server (Caddy) ────────────────────────────────────
echo "[4/4] Setting up release file server..."

if docker ps --format '{{.Names}}' | grep -q makineai-caddy; then
    echo "[OK] Caddy already running"
else
    docker run -d \
        --name makineai-caddy \
        --restart unless-stopped \
        -p 8080:80 \
        -v "$WORKDIR/releases:/srv/releases:ro" \
        caddy:2-alpine \
        caddy file-server --root /srv/releases --browse --listen :80
    echo "[OK] Release server: http://$(hostname -I | awk '{print $1}'):8080"
fi

# ── Summary ────────────────────────────────────────────────────────────
echo ""
echo "=== Setup Complete ==="
echo ""
echo "Servisler:"
echo "  Portainer:      https://$(hostname -I | awk '{print $1}'):9443"
echo "  Release server: http://$(hostname -I | awk '{print $1}'):8080/releases/"
echo "  Runner:         $([ -f "$WORKDIR/runner/.runner" ] && echo 'Active' || echo 'Not configured')"
echo ""
echo "Sonraki adimlar:"
echo "  1. Portainer'a gir, admin hesabi olustur"
echo "  2. GitHub repo Settings > Secrets > SIGNING_PFX_BASE64 ekle"
echo "  3. GitHub Actions > Release Pipeline > Run workflow"
echo ""
echo "Sertifika olusturmak icin (PC'de):"
echo "  just setup-cert"
echo "  # PFX'i base64'e cevir:"
echo "  [Convert]::ToBase64String([IO.File]::ReadAllBytes('scripts/certs/MakineAI-CodeSign.pfx'))"
echo "  # Ciktiyi SIGNING_PFX_BASE64 secret'ina yapistir"
