// Flight Above Head - enclosure for P2 256x128 mm HUB75E panel + ESP32-S3 (HW-678)
//
// Parts (select with -D 'part="..."'):
//   shell    front bezel + walls, print FRONT FACE DOWN (no supports)
//   back     rear cover, print OUTER FACE DOWN
//   feet     two tilt wedges for shelf use (remove for wall use), print as laid out
//   coupon   corner fit-test piece of the shell (print this first)
//   assembly preview with a ghost panel
//
// Frame: X = width, Y = height (+Y = top), Z = depth, Z=0 is the front face.
// All values are in mm. Items marked MEASURE must be checked with calipers.

part = "assembly";

// ---------- Panel (MEASURE) ----------
panel_w   = 256;
panel_h   = 128;
panel_t   = 16;      // front plane to highest point at the rear (connectors included)
extra_depth = 30;    // free depth behind the panel for ESP32 and wires
PANEL_HOLES = [];    // [[x, y], ...] panel rear mounting holes, centred; become locating pins
panel_hole_d = 3.2;
pin_h = 3;

// ---------- Shell ----------
tol       = 0.6;     // total clearance around the panel
wall      = 2.4;
front_t   = 1.6;     // front plate behind the lip
lip       = 0.4;     // front plate overlap over the panel edge (LEDs sit ~1 mm from the edge)
film_t    = 0.5;     // diffuser film / tinted acrylic behind the front plate
corner_r  = 3;
back_t    = 3;

// ---------- Fasteners ----------
insert_d  = 4.2;     // hole for M3 heat-set insert (OD ~4.6)
boss_d    = 7;
boss_h    = 12;
boss_x    = [-105, -35, 35, 105];

// ---------- Cover ----------
rim_h     = 3;
rim_w     = 1.2;
rim_clr   = 0.4;
pillar_d  = 6;
pillar_gap = 0.2;
PILLARS   = [[-110, -50], [-110, 50], [110, -50], [110, 50]];  // press on the panel PCB corners
key_x     = 70;          // wall-mount keyholes
key_y     = 28;
key_big   = 9;
key_small = 4.5;
key_slot  = 9;

// ---------- Electronics (MEASURE) ----------
esp_l = 70;  esp_w = 28;                           // placeholder HW-678 outline
ESP_HOLES = [[-31,-10], [31,-10], [-31,10], [31,10]];  // placeholder, board-centred
esp_hole_d = 2.5;
esp_standoff = 5;
esp_pcb_t = 1.6;
esp_cy = 0;
usb_w = 12;  usb_h = 5.5;                          // USB-C opening
jack_d = 11.2;  jack_y = -30;  jack_z_from_back = 12;   // DC 5.5x2.1 panel jack in the right wall
bh_x = -70;  touch_x = 70;  sensor_z = 26;         // BH1750 / TTP223 positions in the top wall
bh_hole_d = 4.5;
touch_d = 12;  touch_skin = 0.8;

// ---------- Feet ----------
tilt = 12;
wedge_w = 18;
wedge_min = 4;
foot_x = 60;
pad_t = 4.5;  pad_z0 = 22;  pad_h = 22.5;  foot_screw_z = [28, 40];

// ---------- Vent pattern ----------
hex_R = 4.8;  hex_web = 1.6;  hex_edge = 12;  hex_shrink = 0.5;

// ---------- Derived ----------
cav_w = panel_w + tol;
cav_h = panel_h + tol;
out_w = cav_w + 2*wall;
out_h = cav_h + 2*wall;
panel_front_z = front_t + film_t;
panel_back_z  = panel_front_z + panel_t;
depth = panel_back_z + extra_depth;
in_r  = max(corner_r - wall, 0.6);
pillar_h = depth - panel_back_z - pillar_gap;
Lw = depth + back_t;
H0 = Lw * tan(tilt);
BOSSES = [for (x = boss_x) for (s = [-1, 1]) [x, s * (cav_h/2 - 3.2)]];
esp_cx = -cav_w/2 + 3 + esp_l/2;
usb_z  = depth - esp_standoff - esp_pcb_t - 1.6;

$fn = 48;

module rrect(w, h, r) { offset(r = r) square([w - 2*r, h - 2*r], center = true); }

// ================= SHELL =================
module vent_slots_wall(sign) {
    // vertical stadium slots through the top (+1) or bottom (-1) wall, longer toward the centre
    z_mid = (panel_back_z + 2 + depth - 4) / 2;
    for (i = [-4:4]) {
        len = 24 - abs(i) * 2.4;
        translate([i * 6, sign * (cav_h/2 - 0.01), z_mid])
            rotate([-90, 0, 0])
                hull() {
                    translate([0, -(len - 2.2)/2, 0]) cylinder(d = 2.2, h = wall + 0.02, $fn = 16);
                    translate([0,  (len - 2.2)/2, 0]) cylinder(d = 2.2, h = wall + 0.02, $fn = 16);
                }
    }
}

module shell_body() {
    difference() {
        linear_extrude(depth) rrect(out_w, out_h, corner_r);
        // panel cavity
        translate([0, 0, front_t]) linear_extrude(depth) rrect(cav_w, cav_h, in_r);
        // front window
        translate([0, 0, -0.01]) linear_extrude(front_t + 0.02) rrect(panel_w - 2*lip, panel_h - 2*lip, 1);
        // top / bottom vents (rotate about X mirrors the slot axis for the bottom wall)
        vent_slots_wall(1);
        mirror([0, 1, 0]) vent_slots_wall(1);
        // BH1750 window (top wall)
        translate([bh_x, cav_h/2 - 0.1, sensor_z]) rotate([-90, 0, 0]) cylinder(d = bh_hole_d, h = wall + 0.2);
        // TTP223 thin spot from the inside + ring marking on the outside
        translate([touch_x, cav_h/2 - 0.01, sensor_z]) rotate([-90, 0, 0]) cylinder(d = touch_d, h = wall - touch_skin + 0.01);
        translate([touch_x, out_h/2 - 0.4, sensor_z]) rotate([-90, 0, 0])
            difference() { cylinder(d = touch_d, h = 0.5); translate([0,0,-0.1]) cylinder(d = touch_d - 1.2, h = 0.7); }
        // USB-C opening (left wall)
        translate([-out_w/2 - 0.1, esp_cy - usb_w/2, usb_z - usb_h/2]) cube([wall + 0.2, usb_w, usb_h]);
        // DC jack (right wall)
        translate([cav_w/2 - 0.1, jack_y, depth - jack_z_from_back]) rotate([0, 90, 0]) cylinder(d = jack_d, h = wall + 0.2);
    }
}

module shell() {
    difference() {
        union() {
            shell_body();
            // M3 insert bosses for the cover
            for (b = BOSSES) translate([b[0], b[1], depth - boss_h]) cylinder(d = boss_d, h = boss_h);
            // pads for the feet screws (bottom wall)
            for (sx = [-1, 1]) translate([sx * foot_x - wedge_w/2, -cav_h/2 - 0.01, pad_z0]) cube([wedge_w, pad_t + 0.01, pad_h]);
            // locating pins for panel holes
            for (h = PANEL_HOLES) translate([h[0], h[1], front_t - 0.01]) cylinder(d = panel_hole_d - 0.3, h = film_t + pin_h);
        }
        for (b = BOSSES) translate([b[0], b[1], depth - 7]) cylinder(d = insert_d, h = 7.1);
        for (sx = [-1, 1]) for (z = foot_screw_z)
            translate([sx * foot_x, -out_h/2 - 0.1, z]) rotate([-90, 0, 0]) cylinder(d = insert_d, h = wall + pad_t - 0.5);
    }
}

// ================= COVER =================
function near(x, y, p, r) = (pow(x - p[0], 2) + pow(y - p[1], 2)) < r*r;
function any_near(x, y, pts, r) = len([for (p = pts) if (near(x, y, p, r)) 1]) > 0;
function hex_ok(x, y) =
    let (rc = hex_R * hex_scale(x, y), gap = 1.2)
    abs(x) <= cav_w/2 - hex_edge && abs(y) <= cav_h/2 - hex_edge
    && !any_near(x, y, BOSSES, rc + 3.3 + gap)
    && !any_near(x, y, PILLARS, rc + pillar_d/2 + gap)
    && !any_near(x, y, [for (h = ESP_HOLES) [esp_cx + h[0], esp_cy + h[1]]], rc + 3 + gap)
    && !(abs(abs(x) - key_x) < rc + key_big/2 + gap && y > key_y - key_big/2 - rc - gap && y < key_y + key_slot + key_small/2 + rc + gap);
function hex_scale(x, y) =
    1 - hex_shrink * pow(sqrt(pow(x / (cav_w/2 - hex_edge), 2) + pow(y / (cav_h/2 - hex_edge), 2)) / sqrt(2), 1.5);

hex_Rp = hex_R + hex_web / sqrt(3);
hex_dx = 1.5 * hex_Rp;
hex_dy = sqrt(3) * hex_Rp;

module hex_perf() {
    nc = ceil(cav_w / 2 / hex_dx);
    nr = ceil(cav_h / 2 / hex_dy);
    for (i = [-nc : nc]) for (j = [-nr : nr]) {
        x = i * hex_dx;
        y = j * hex_dy + (abs(i) % 2 == 1 ? hex_dy / 2 : 0);
        if (hex_ok(x, y)) translate([x, y]) circle(r = hex_R * hex_scale(x, y), $fn = 6);
    }
}

module keyhole2d() { circle(d = key_big); hull() { circle(d = key_small); translate([0, key_slot]) circle(d = key_small); } }

module cover() {
    difference() {
        union() {
            linear_extrude(back_t) rrect(out_w, out_h, corner_r);
            // locating rim inside the cavity, notched around the bosses
            translate([0, 0, back_t - 0.01]) linear_extrude(rim_h) difference() {
                rrect(cav_w - 2*rim_clr, cav_h - 2*rim_clr, in_r);
                rrect(cav_w - 2*rim_clr - 2*rim_w, cav_h - 2*rim_clr - 2*rim_w, in_r);
                for (b = BOSSES) translate([b[0] - 5.5, b[1] < 0 ? -cav_h/2 - 1 : cav_h/2 - 9]) square([11, 10]);
            }
            for (p = PILLARS) translate([p[0], p[1], back_t - 0.01]) cylinder(d = pillar_d, h = pillar_h + 0.01);
            for (h = ESP_HOLES) translate([esp_cx + h[0], esp_cy + h[1], back_t - 0.01])
                cylinder(d = 6, h = esp_standoff + 0.01);
        }
        for (h = ESP_HOLES) translate([esp_cx + h[0], esp_cy + h[1], back_t]) cylinder(d = esp_hole_d, h = esp_standoff + 0.1);
        translate([0, 0, -0.01]) linear_extrude(back_t + 0.02) hex_perf();
        for (b = BOSSES) translate([b[0], b[1], -0.01]) {
            cylinder(d = 3.4, h = back_t + 0.02);
            cylinder(d1 = 6.6, d2 = 3.4, h = 1.8);
        }
        for (sx = [-1, 1]) translate([sx * key_x, key_y, -0.01]) linear_extrude(back_t + 0.02) keyhole2d();
    }
}

// ================= FEET =================
module wedge_box() {
    // box frame, centred on X, hanging below y=0, along Z=0..Lw
    difference() {
        multmatrix([[0,0,1,0],[0,1,0,0],[1,0,0,0],[0,0,0,1]])
            translate([0, 0, -wedge_w/2]) linear_extrude(wedge_w)
                polygon([[0, 0], [Lw, 0], [Lw, -wedge_min], [0, -H0 - wedge_min]]);
        for (z = foot_screw_z) translate([0, 0.1, z]) {
            rotate([90, 0, 0]) cylinder(d = 3.4, h = H0 + wedge_min + 1);
            translate([0, -H0 * (1 - z/Lw) - wedge_min, 0]) rotate([-90, 0, 0]) cylinder(d1 = 6.6, d2 = 3.4, h = 1.8);
        }
    }
}
module feet() { for (sx = [-1, 1]) translate([sx * (wedge_w/2 + 4), 0, 0]) rotate([-90, 0, 0]) wedge_box(); }

// ================= COUPON =================
module coupon() {
    intersection() {
        shell();
        translate([0, out_h/2 - 42, -1]) cube([out_w/2 + 1, 43, depth + 2]);
    }
}

// ================= ASSEMBLY =================
module assembly() {
    shell();
    translate([0, 0, depth + back_t]) mirror([0, 0, 1]) cover();
    color("DimGray", 0.5) translate([-panel_w/2, -panel_h/2, panel_front_z]) cube([panel_w, panel_h, panel_t]);
    if (tilt > 0) for (sx = [-1, 1]) translate([sx * foot_x, -out_h/2, 0]) wedge_box();
}

if (part == "shell") shell();
else if (part == "back") cover();
else if (part == "feet") feet();
else if (part == "coupon") coupon();
else assembly();
