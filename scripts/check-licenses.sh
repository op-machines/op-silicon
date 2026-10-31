#!/bin/sh
# SPDX-License-Identifier: MIT
# Checks the licence separation of op-silicon. Exit 1 on any violation.
cd "$(dirname "$0")/.." || exit 2
fail=0
tmpfail=$(mktemp)
err() { echo "FEHLER: $*"; fail=1; }

for dir in machines/*/; do
  [ -d "$dir" ] || continue
  id=$(basename "$dir")
  lic="$dir/LICENSE"
  if [ ! -f "$lic" ]; then err "$dir: LICENSE fehlt"; continue; fi
  if grep -q "GNU GENERAL PUBLIC LICENSE" "$lic"; then want="GPL-2.0-or-later"
  elif grep -q "^MIT License" "$lic"; then want="MIT"
  else err "$lic: Lizenz nicht erkannt"; continue; fi
  find "$dir" -type f | while read -r f; do
    case "$f" in */LICENSE|*/COPYING|*/.gitkeep) continue ;; esac
    spdx=$(head -n 5 "$f" | grep -o 'SPDX-License-Identifier: [A-Za-z0-9.+-]*' | head -n 1 | cut -d' ' -f2)
    # JSON kennt keine Kommentare; es gilt die LICENSE des Ordners
    case "$f" in *.json) continue ;; esac
    if [ -z "$spdx" ]; then echo "FEHLER: $f: kein SPDX-Kopf"; echo x >> "$tmpfail"
    elif [ "$spdx" != "$want" ]; then echo "FEHLER: $f: SPDX $spdx passt nicht zu LICENSE ($want)"; echo x >> "$tmpfail"; fi
  done
  if [ "$id" = silsid ]; then
    if grep -rhE '#include *"\.\./\.\./|machines/sil' "$dir" | grep -v 'machines/silsid' ; then
      err "$dir bindet Dateien anderer Maschinen ein"
    fi
  fi
done

# GPL-Begriffe ausserhalb von machines/silsid/
hits=$(grep -rniE 'resid|dag lem|sid_tables|mame|nuked|gearmulator|vice' . \
  --exclude-dir=.git --exclude-dir=node_modules --exclude-dir=local --exclude-dir=reports --exclude-dir=dist \
  | grep -vE '^\./(machines/silsid/|scripts/check-licenses\.sh:|README\.md:|NOTICE:|THIRD_PARTY\.md:|site/index\.html:|site/public/data/)' | grep -iE '\b(resid|dag lem|sid_tables|mame|nuked|gearmulator|vice)\b')
[ -n "$hits" ] && { echo "$hits"; err "GPL-Begriffe ausserhalb von machines/silsid/"; }

[ -s "$tmpfail" ] && fail=1
rm -f "$tmpfail"
[ $fail = 0 ] && echo "Lizenzprüfung ok"
exit $fail
