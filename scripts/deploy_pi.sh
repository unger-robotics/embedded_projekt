#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# deploy_pi.sh
#
# Zweck:
#   - optional: Dashboard builden (apps/dashboard -> dist/)
#   - Bridge-Code (apps/bridge) auf den Pi rsyncen
#   - Dashboard dist/ auf den Pi rsyncen
#   - optional: systemd service restart + status
#
# Nutzung (vom Repo-Root):
#   chmod +x scripts/deploy_pi.sh
#   ./scripts/deploy_pi.sh --host rover.local
#
# Env Overrides:
#   PI_HOST, PI_USER, REMOTE_ROOT, SERVICE
# -----------------------------------------------------------------------------

PI_HOST="${PI_HOST:-rover.local}"
PI_USER="${PI_USER:-pi}"
REMOTE_ROOT="${REMOTE_ROOT:-/home/pi/embedded_projekt}"
SERVICE="${SERVICE:-embedded-bridge.service}"

DO_BUILD=1
DO_RESTART=1
DO_NPM_CI=0

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --host <name>          Pi hostname (default: ${PI_HOST})
  --user <name>          SSH user (default: ${PI_USER})
  --root <path>          Remote project root (default: ${REMOTE_ROOT})
  --service <name>       systemd service (default: ${SERVICE})

  --no-build             Skip dashboard build
  --ci                   Run 'npm ci' before build (slower, reproducible)
  --no-restart           Skip systemd restart/status
  -h, --help             Show this help

Examples:
  $0 --host rover.local
  PI_HOST=rover.local $0
  $0 --no-build --no-restart
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)       PI_HOST="$2"; shift 2 ;;
    --user)       PI_USER="$2"; shift 2 ;;
    --root)       REMOTE_ROOT="$2"; shift 2 ;;
    --service)    SERVICE="$2"; shift 2 ;;
    --no-build)   DO_BUILD=0; shift 1 ;;
    --ci)         DO_NPM_CI=1; shift 1 ;;
    --no-restart) DO_RESTART=0; shift 1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
BRIDGE_DIR="${ROOT_DIR}/apps/bridge"
DASH_DIR="${ROOT_DIR}/apps/dashboard"
DIST_DIR="${DASH_DIR}/dist"

SSH_TARGET="${PI_USER}@${PI_HOST}"
RSYNC_SSH="ssh -o BatchMode=yes -o ConnectTimeout=5"

# --- Preflight ---------------------------------------------------------------
command -v rsync >/dev/null 2>&1 || { echo "rsync not found" >&2; exit 1; }
command -v ssh   >/dev/null 2>&1 || { echo "ssh not found" >&2; exit 1; }

[[ -d "${BRIDGE_DIR}" ]] || { echo "Missing: ${BRIDGE_DIR}" >&2; exit 1; }
[[ -d "${DASH_DIR}"   ]] || { echo "Missing: ${DASH_DIR}" >&2; exit 1; }

# --- Build dashboard ----------------------------------------------------------
if [[ "${DO_BUILD}" -eq 1 ]]; then
  command -v npm >/dev/null 2>&1 || { echo "npm not found (needed for build)" >&2; exit 1; }
  echo "[deploy] build dashboard: ${DASH_DIR}"
  (
    cd "${DASH_DIR}"
    if [[ "${DO_NPM_CI}" -eq 1 ]]; then
      npm ci
    fi
    npm run build
  )
fi

[[ -d "${DIST_DIR}" ]] || {
  echo "Missing dist/: ${DIST_DIR} (run build or remove --no-build)" >&2
  exit 1
}

# --- Ensure remote dirs -------------------------------------------------------
echo "[deploy] ensure remote dirs under: ${REMOTE_ROOT}"
ssh "${SSH_TARGET}" "mkdir -p '${REMOTE_ROOT}' '${REMOTE_ROOT}/apps/bridge' '${REMOTE_ROOT}/apps/dashboard' '${REMOTE_ROOT}/apps/dashboard/dist'"

# --- Rsync bridge -------------------------------------------------------------
echo "[deploy] rsync bridge -> ${SSH_TARGET}:${REMOTE_ROOT}/apps/bridge/"
rsync -avh --delete -e "${RSYNC_SSH}" \
  --exclude '.venv/' \
  --exclude '__pycache__/' \
  --exclude '*.pyc' \
  --exclude '.DS_Store' \
  --exclude '.pytest_cache/' \
  --exclude '.mypy_cache/' \
  --exclude '.ruff_cache/' \
  "${BRIDGE_DIR}/" \
  "${SSH_TARGET}:${REMOTE_ROOT}/apps/bridge/"

# --- Rsync dist ---------------------------------------------------------------
echo "[deploy] rsync dashboard dist -> ${SSH_TARGET}:${REMOTE_ROOT}/apps/dashboard/dist/"
rsync -avh --delete -e "${RSYNC_SSH}" \
  --exclude '.DS_Store' \
  "${DIST_DIR}/" \
  "${SSH_TARGET}:${REMOTE_ROOT}/apps/dashboard/dist/"

# --- Restart service (optional) ----------------------------------------------
if [[ "${DO_RESTART}" -eq 1 ]]; then
  echo "[deploy] restart service: ${SERVICE}"
  ssh "${SSH_TARGET}" "sudo systemctl restart '${SERVICE}' && sudo systemctl status '${SERVICE}' --no-pager -l | sed -n '1,25p'"
fi

echo "[deploy] done"
