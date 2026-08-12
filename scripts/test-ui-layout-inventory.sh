#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TOOL="$(assert_project_path tools/dusk_ui_layout_inventory/dusk_ui_layout_inventory.py)"
TESTS="$(assert_project_path test/ui-layout-inventory)"
OUTPUT="$(assert_project_path build/host/ui-layout-inventory)"
RAW="$(assert_project_path build/host/ui-layout-inventory/raw)"
INVENTORIES="$(assert_project_path build/host/ui-layout-inventory/inventories)"
LOGS="$(assert_project_path logs/ui-layout-inventory)"
SUMMARY="$(assert_project_path build/host/ui-layout-inventory/UI_LAYOUT_INVENTORY_SUMMARY.json)"

python3 -B -m unittest discover -s "$TESTS" -p 'test_*.py' -v

if [ -z "${DUSKLIGHT_GAME_IMAGE:-}" ]; then
  printf '%s\n' \
    "UI_LAYOUT_INVENTORY_FIXTURES_OK tests=7 source_acquisition=SKIPPED_NO_GAME_IMAGE"
  exit 0
fi

GAME_IMAGE="$(assert_project_path "$DUSKLIGHT_GAME_IMAGE")"
[ -f "$GAME_IMAGE" ] && [ ! -L "$GAME_IMAGE" ] ||
  die "l'image locale doit être un fichier régulier non symbolique"
PROBE="$(assert_project_path build/host/link-loader/probe/dusk_link_loader_probe)"
[ -x "$PROBE" ] || die "probe absent ; exécuter scripts/build-link-loader-probe.sh"
safe_mkdir "$OUTPUT"
safe_mkdir "$RAW"
safe_mkdir "$INVENTORIES"
safe_mkdir "$LOGS"

run_layout() {
  local key="$1" archive="$2" resource="$3" layout="$4"
  local raw="$RAW/$key.blo" inventory="$INVENTORIES/$key.json"
  local log="$LOGS/$key.log"
  assert_project_path "$raw" >/dev/null
  assert_project_path "$inventory" >/dev/null
  assert_project_path "$log" >/dev/null
  env -u http_proxy -u https_proxy -u HTTP_PROXY -u HTTPS_PROXY \
    -u ALL_PROXY -u all_proxy -u NO_PROXY -u no_proxy \
    DUSKLIGHT_GAME_IMAGE="$GAME_IMAGE" \
    DUSKLIGHT_TEST_EXPECT_DISC_ID=GZ2P01 \
    DUSKLIGHT_TEST_EXPECT_REVISION=0 \
    DUSKLIGHT_LIST_RARC_PATH="$archive" \
    DUSKLIGHT_RARC_RESOURCE_NAME="$resource" \
    DUSKLIGHT_RARC_RESOURCE_OUTPUT="$raw" \
    "$PROBE" >"$log" 2>&1
  [ -s "$raw" ] || die "layout BLO2 non extrait : $resource"
  python3 -B "$TOOL" parse \
    --input "$raw" --archive "$archive" --layout "$layout" \
    --output "$inventory"
  python3 -B "$TOOL" validate --input "$inventory"
}

run_layout title /res/Layout/Title2D.arc \
  zelda_press_start.blo zelda_press_start.blo
run_layout file_select /res/Object/fileSel.arc \
  zelda_file_select.blo zelda_file_select.blo
run_layout file_copy /res/Object/fileSel.arc \
  zelda_file_select_copy_select.blo zelda_file_select_copy_select.blo
run_layout file_yes_no /res/Object/fileSel.arc \
  zelda_file_select_yes_no_window.blo zelda_file_select_yes_no_window.blo
run_layout file_3menu /res/Object/fileSel.arc \
  zelda_file_select_3menu_window.blo zelda_file_select_3menu_window.blo
run_layout file_details /res/Object/fileSel.arc \
  zelda_file_select_details.blo zelda_file_select_details.blo
run_layout hud /res/Layout/main2D.arc \
  zelda_game_image.blo zelda_game_image.blo
run_layout hud_kantera /res/Layout/main2D.arc \
  zelda_game_image_kantera.blo zelda_game_image_kantera.blo
run_layout hud_pikari /res/Layout/main2D.arc \
  zelda_icon_pikari.blo zelda_icon_pikari.blo

python3 -B - "$INVENTORIES" "$SUMMARY" <<'PY'
import hashlib
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
expected = {
    "title": (10, 7, 0, 1, 7),
    "file_select": (291, 251, 30, 2, 28),
    "file_copy": (93, 78, 15, 0, 0),
    "file_yes_no": (27, 20, 5, 1, 4),
    "file_3menu": (39, 30, 5, 1, 6),
    "file_details": (47, 32, 14, 0, 0),
    "hud": (346, 216, 25, 1, 35),
    "hud_kantera": (17, 10, 3, 0, 0),
    "hud_pikari": (5, 3, 1, 0, 0),
}
layouts = []
stable_ids = set()
totals = {key: 0 for key in
          ("panes", "materials", "textures", "fonts", "text_boxes",
           "pictures", "groups", "windows")}
for key, wanted in expected.items():
    path = source / f"{key}.json"
    with path.open(encoding="utf-8") as stream:
        inventory = json.load(stream)
    counts = inventory["counts"]
    actual = tuple(counts[name] for name in
                   ("panes", "materials", "textures", "fonts", "text_boxes"))
    if actual != wanted:
        raise SystemExit(f"inventaire inattendu pour {key}: {actual} != {wanted}")
    for pane in inventory["panes"]:
        pane_id = pane["stable_id"]
        if pane_id in stable_ids:
            raise SystemExit(f"identité pane dupliquée globalement: {pane_id}")
        stable_ids.add(pane_id)
    for name in totals:
        totals[name] += counts[name]
    layouts.append({
        "key": key,
        "archive": inventory["source"]["archive"],
        "layout": inventory["source"]["layout"],
        "source_sha256": inventory["source"]["sha256"],
        "inventory_sha256": inventory["inventory_sha256"],
        "counts": counts,
    })
if totals != {
        "panes": 875, "materials": 647, "textures": 98, "fonts": 6,
        "text_boxes": 80, "pictures": 567, "groups": 228, "windows": 0,
}:
    raise SystemExit(f"totaux BLO2 inattendus: {totals}")
summary = {
    "schema": "dusklight.ui.blo2-inventory-summary.v1",
    "layouts": layouts,
    "totals": totals,
    "stable_id_count": len(stable_ids),
}
encoded = json.dumps(summary, sort_keys=True, separators=(",", ":")).encode()
summary["summary_sha256"] = hashlib.sha256(encoded).hexdigest()
with output.open("w", encoding="utf-8", newline="\n") as stream:
    json.dump(summary, stream, indent=2, sort_keys=True)
    stream.write("\n")
PY

printf '%s\n' \
  "UI_LAYOUT_INVENTORY_HOST_OK layouts=9 panes=875 materials=647 textures=98 fonts=6 text_boxes=80 negative_tests=5"
