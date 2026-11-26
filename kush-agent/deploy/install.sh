#!/usr/bin/env bash
# Install kush-agent + admin panel on a fresh Ubuntu/Debian VPS (run as root from the kush-agent directory).
#   sudo bash deploy/install.sh                 # panel on 127.0.0.1:8080 (reach it through an SSH tunnel)
#   sudo DOMAIN=panel.example.com bash deploy/install.sh   # + HTTPS with Caddy (point the DNS A record at the VPS first)
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
APP=/opt/kush-agent ETC=/etc/kush-agent STATE=/var/lib/kush-agent
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 1; }

echo "== packages"
apt-get update -q
apt-get install -y -q qemu-system-x86 qemu-utils python3 python3-venv rsync openssl

echo "== user and directories"
id kush >/dev/null 2>&1 || useradd --system --home "$STATE" --shell /usr/sbin/nologin kush
getent group kvm >/dev/null && usermod -aG kvm kush || true
mkdir -p "$APP" "$ETC" "$STATE/images" "$STATE/vms" "$STATE/runs"
rsync -a --delete --exclude '.git' --exclude '__pycache__' --exclude 'tests' "$SRC/" "$APP/"

echo "== python environment"
python3 -m venv "$APP/venv"
"$APP/venv/bin/pip" install -q --upgrade pip
"$APP/venv/bin/pip" install -q "$APP"

echo "== configuration"
[ -f "$ETC/models.yaml" ] || cp "$APP/config/models.example.yaml" "$ETC/models.yaml"
if [ ! -f "$ETC/env" ]; then
  PASS=$(openssl rand -base64 18 | tr -d '/+=' | cut -c1-20)
  cat > "$ETC/env" <<ENV
# Secrets for kush-agent. Edit, then: systemctl restart kush-admin
KUSH_ADMIN_PASSWORD=$PASS
KUSH_ADMIN_SECRET=$(openssl rand -hex 32)
KUSH_STATE_DIR=$STATE
DEEPSEEK_API_KEY=
ANTHROPIC_API_KEY=
ENV
  echo ">>> admin password: $PASS   (stored in $ETC/env)"
fi
chown -R kush:kush "$STATE" "$ETC"
chmod 600 "$ETC/env"

echo "== systemd service"
EXTRA=""
[ -n "${DOMAIN:-}" ] && EXTRA="--secure-cookie"
sed "s|@EXTRA@|$EXTRA|" "$APP/deploy/kush-admin.service" > /etc/systemd/system/kush-admin.service
systemctl daemon-reload
systemctl enable --now kush-admin
systemctl restart kush-admin

if [ -n "${DOMAIN:-}" ]; then
  echo "== HTTPS with Caddy for $DOMAIN"
  apt-get install -y -q caddy
  sed "s|@DOMAIN@|$DOMAIN|" "$APP/deploy/Caddyfile" > /etc/caddy/Caddyfile
  systemctl reload caddy || systemctl restart caddy
  echo ">>> open https://$DOMAIN"
else
  echo ">>> from your computer: ssh -L 8080:127.0.0.1:8080 root@<vps-ip>   then open http://localhost:8080"
fi
echo ">>> put ISO / qcow2 files in $STATE/images  (owner: kush)"
bash "$APP/deploy/check-host.sh" || true
