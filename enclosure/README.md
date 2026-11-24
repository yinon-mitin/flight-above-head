# Enclosure

Parametric OpenSCAD case for the 256×128 mm P2 HUB75E panel, ESP32-S3 (HW-678), BH1750 and a TTP223 touch module.
Edit the values at the top of `flight_above_head_case.scad`, then export:

```bash
cd enclosure
for p in shell back feet coupon; do
  openscad -D "part=\"$p\"" -o stl/$p.stl flight_above_head_case.scad
done
```

`-D 'part="assembly"'` previews everything together with a ghost panel.

## Parts and print settings

| STL | Orientation | Notes |
| --- | --- | --- |
| `shell.stl` | front face down | 261.4 × 133.4 × 48.1 mm, one piece, no supports |
| `back.stl` | outer face down | hex vents, wall keyholes, panel pillars, ESP32 standoffs |
| `feet.stl` | as exported | two 12° wedges for shelf use; remove for wall mounting |
| `coupon.stl` | as exported | right half of the top edge of the shell: check fit before the full print |

Black or dark grey PETG/ASA, 0.4 mm nozzle, 3 perimeters, 20% infill. PLA is only suitable at low brightness.

## Hardware

- 8 × M3 heat-set inserts (OD ~4.6) in the shell bosses, 8 × M3×8–10 countersunk screws for the cover.
- 4 × M3 inserts + 4 × M3 countersunk screws for the feet.
- ESP32 standoffs take M2.5 self-tapping screws.
- Diffuser: 0.5 mm tinted film or acrylic cut to 256×128, placed behind the front lip.
- Panel-mount DC jack 5.5×2.1 (hole `jack_d` = 11.2 mm, adjust to your part), TTP223 and BH1750 fixed with double-sided tape.

## Must be measured / confirmed

- `panel_t`: front plane to the highest point on the panel's rear (default 16 mm is a guess).
- `PANEL_HOLES`: rear mounting holes, if any. The panel is otherwise held by the four cover pillars (`PILLARS`)
  pressing on its PCB corners; check they do not land on components.
- `esp_l`, `esp_w`, `ESP_HOLES`: HW-678 outline and holes are placeholders. USB-C opening is on the left wall.
- `tol`: 0.6 mm total clearance around the panel.

## Power

The panel can draw 35 W (up to 7 A at 5 V). A common 5.5×2.1 jack is rated for about 3–5 A, so use a fused
supply, keep panel current away from the ESP32 board, and keep the firmware brightness limits
(24/255 idle, 150/255 aircraft, 255/255 alert) in mind. Do not feed 12 V into this case.
