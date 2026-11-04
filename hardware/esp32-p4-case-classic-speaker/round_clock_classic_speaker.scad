// Round Weather Display - "classic" standing case with rear speaker
// Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C (round, outline 115 mm)
// + dual speaker 100 x 45 mm (mounting holes 92 x 37 mm) firing backwards
//
// Upright round body on two small feet. The back cover is the speaker grille;
// the speaker is bolted to the inside of the cover.
//
//   shell  front housing with feet   print FRONT FACE DOWN, no supports
//   cover  back cover + grille       print OUTSIDE DOWN
//
// Case coordinates: display axis is Z, Z=0 is the front face (display looks
// towards -Z), +Y is up.

/* [Part] */
part = "all"; // [all, exploded, shell, cover]

/* [MEASURE: display module] */
// Round cover glass diameter (Waveshare: outline 115 x 115 mm)
glass_d = 115.0;
// Glass + LCD panel thickness at the rim
glass_t = 6.0;
// Depth needed behind the panel (PCB, USB-A port, connectors)
board_depth = 24.0;
// Largest radius of PCB/components behind the panel (pillars stay outside it)
board_r = 54.0;
// How far the front lip overlaps the glass edge
lip_overlap = 2.0;
// How far the glass sits back from the front face
glass_recess = 2.0;

/* [MEASURE: speaker] */
spk_size = [100, 45];
spk_holes = [92, 37];
spk_driver_d = 36;
spk_driver_x = 22;
// Depth of the speaker behind its mounting plate (magnets)
spk_depth = 20;
// Space between speaker magnets and the display PCB
spk_clear = 3;

/* [Case] */
// Width of the ring around the glass
bezel = 8.5;
// Front edge chamfer (45 degrees, printable face-down)
front_chamfer = 3;
// Back edge rounding
back_round = 4;
fit = 0.3;
cover_t = 3.0;
// Feet: angle from the bottom, width, how far below the round body
foot_angle = 38;
foot_w = 22;
foot_h = 7;
screw_pilot_d = 2.5;   // M3 self-tapping (4.0 for heat-set inserts)
screw_hole_d = 3.4;
pillar_gap = 1.0;      // fill with foam tape
cable_slot = [14, 8];

/* [MEASURE: factory reset hole] */
// Paperclip hole in the back cover for the board's BOOT button (hold 10 s = factory reset)
reset_hole = true;
// Position of the BOOT button, seen FROM THE FRONT of the clock (x right, y up), in mm from the centre
boot_button_xy = [0, -40];
// Guide tube from the cover towards the button, so the paperclip finds it
reset_tube_len = 18;

$fn = 128;

// ---- derived ----------------------------------------------------------------
inner_r  = glass_d / 2 + fit;
outer_r  = inner_r + bezel;
lip_t    = glass_recess;                       // material in front of the glass
open_r   = glass_d / 2 - lip_overlap;
depth    = lip_t + glass_t + board_depth + spk_depth + spk_clear + cover_t;
z_glass_back = lip_t + glass_t;
cover_r  = inner_r + 6.0;                      // cover sits in a rebate on a 6 mm ledge
boss_r   = inner_r + 3.0;                      // cover screws go into that ledge
pillar_r = (board_r + inner_r) / 2;
floor_y  = -(outer_r + foot_h);

echo(str("Case: diameter ", 2 * outer_r, " mm, depth ", depth, " mm, height ", outer_r * 2 + foot_h, " mm"));

// ---- body -------------------------------------------------------------------
module body_profile() {
  // 2D half profile (x = radius, y = depth) for rotate_extrude
  hull() {
    // front chamfer
    translate([0, 0]) square([outer_r - front_chamfer, 0.01]);
    translate([0, front_chamfer]) square([outer_r, 0.01]);
    // back rounding
    translate([outer_r - back_round, depth - back_round]) circle(r = back_round, $fn = 48);
    translate([0, depth - 0.01]) square([outer_r - back_round, 0.01]);
  }
}

module foot(side) {
  a = 270 + side * foot_angle;
  r = 3;
  hull() {
    // anchor inside the round body
    intersection() {
      rotate([0, 0, a]) translate([outer_r - 12, -foot_w / 2, front_chamfer]) cube([12, foot_w, depth - front_chamfer - back_round]);
      cylinder(r = outer_r - 0.5, h = depth);
    }
    // pad on the floor
    fx = cos(a) * (outer_r + 2);
    for (dx = [-foot_w / 2 + r, foot_w / 2 - r], z = [front_chamfer + r, depth - back_round - r])
      translate([fx + dx, floor_y + r, z]) sphere(r = r, $fn = 32);
  }
}

module below_floor() { translate([-200, floor_y - 400, -50]) cube([400, 400, 400]); }

boss_angles   = [45, 135, 225, 315];
pillar_angles = [0, 90, 180, 270];

module shell() {
  difference() {
    union() {
      rotate_extrude($fn = 160) body_profile();
      difference() { union() { foot(-1); foot(1); } below_floor(); }
    }
    // glass pocket and cavity, open to the back
    translate([0, 0, lip_t]) cylinder(r = inner_r, h = depth);
    // rebate for the cover
    translate([0, 0, depth - cover_t]) cylinder(r = cover_r + fit, h = cover_t + 1);
    // front opening with a small chamfer
    translate([0, 0, -0.01]) cylinder(r1 = open_r + lip_t, r2 = open_r, h = lip_t + 0.02);
    // cover screw pilots (in the thick ring, behind the rebate)
    for (a = boss_angles) rotate([0, 0, a])
      translate([boss_r, 0, depth - cover_t - 12]) cylinder(d = screw_pilot_d, h = 13, $fn = 24);
  }
}

// ---- cover ------------------------------------------------------------------
module grille_holes(hole = 2.4, pitch = 3.8) {
  for (sx = [-1, 1]) translate([sx * spk_driver_x, 0, 0]) {
    n = ceil(spk_driver_d / pitch);
    for (i = [-n:n], j = [-n:n]) {
      x = i * pitch + (abs(j) % 2) * pitch / 2;
      y = j * pitch * 0.866;
      if (sqrt(x * x + y * y) < spk_driver_d / 2 - 2)
        translate([x, y, 0]) cylinder(d = hole, h = 50, center = true, $fn = 16);
    }
  }
}

module cover() {
  z_in = depth - cover_t;
  pillar_len = z_in - z_glass_back - pillar_gap;
  difference() {
    union() {
      translate([0, 0, z_in]) cylinder(r = cover_r, h = cover_t);
      // pillars clamp the glass rim against the front lip
      for (a = pillar_angles) rotate([0, 0, a])
        translate([pillar_r, 0, z_in - pillar_len]) cylinder(d = 5, h = pillar_len, $fn = 32);
      // guide tube for the paperclip
      if (reset_hole) translate([-boot_button_xy[0], boot_button_xy[1], z_in - reset_tube_len])
        cylinder(d = 5, h = reset_tube_len, $fn = 24);
      // low locating rim around the speaker plate
      translate([0, 0, z_in - 2]) difference() {
        cube([spk_size[0] + 0.8 + 3, spk_size[1] + 0.8 + 3, 4], center = true);
        cube([spk_size[0] + 0.8, spk_size[1] + 0.8, 6], center = true);
      }
    }
    translate([0, 0, depth]) grille_holes();
    // speaker bolts: M3 countersunk from the outside, nuts inside
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * spk_holes[0] / 2, sy * spk_holes[1] / 2, z_in - 1]) {
      cylinder(d = screw_hole_d, h = cover_t + 2, $fn = 24);
      translate([0, 0, cover_t + 1 - 1.7]) cylinder(d1 = screw_hole_d, d2 = 6.4, h = 1.71, $fn = 24);
    }
    // cover screws
    for (a = boss_angles) rotate([0, 0, a]) translate([boss_r, 0, z_in - 1]) {
      cylinder(d = screw_hole_d, h = cover_t + 2, $fn = 24);
      translate([0, 0, cover_t + 1 - 1.7]) cylinder(d1 = screw_hole_d, d2 = 6.4, h = 1.71, $fn = 24);
    }
    // ventilation slots above and below the speaker
    for (y = [spk_size[1] / 2 + 12, -spk_size[1] / 2 - 9], x = [-30:7:30])
      if (!(y < 0 && abs(x) < 10))
        translate([x, y, depth - cover_t / 2])
          hull() { translate([0, -4, 0]) cylinder(d = 3, h = cover_t + 2, center = true, $fn = 16);
                   translate([0,  4, 0]) cylinder(d = 3, h = cover_t + 2, center = true, $fn = 16); }
    // paperclip hole for the BOOT button (case x runs right-to-left seen from the front)
    if (reset_hole) translate([-boot_button_xy[0], boot_button_xy[1], 0]) cylinder(d = 2.2, h = 200, center = true, $fn = 20);
    // USB cable slot, bottom centre
    translate([0, -spk_size[1] / 2 - 9, depth - cover_t / 2]) cube([cable_slot[0], cable_slot[1], cover_t + 2], center = true);
  }
}

// ---- output -----------------------------------------------------------------
module world() { translate([0, 0, -floor_y]) rotate([90, 0, 0]) rotate([0, 0, 0]) children(); }

if (part == "shell") {
  shell();
} else if (part == "cover") {
  translate([0, 0, depth]) mirror([0, 0, 1]) cover();
} else {
  gap = (part == "exploded") ? 45 : 0;
  world() {
    color("BurlyWood") shell();
    color("Tan") translate([0, 0, gap]) cover();
    color([0.05, 0.05, 0.08]) translate([0, 0, lip_t]) cylinder(r = glass_d / 2, h = glass_t);
    color("DimGray") translate([0, 0, depth - cover_t - spk_depth / 2 + gap * 0.6])
      cube([spk_size[0], spk_size[1], spk_depth], center = true);
  }
}
