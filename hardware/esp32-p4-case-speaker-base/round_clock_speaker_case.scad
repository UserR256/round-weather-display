// Round Weather Display - desk case with speaker base ("clock radio")
// Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C (round 800x800, outline 115 mm)
// + dual speaker 100 x 45 mm (mounting holes 92 x 37 mm)
//
// Parametric OpenSCAD model. Open in OpenSCAD (free, openscad.org), check the
// MEASURE parameters against your parts, pick a part and export STL.
//
//   shell   round front housing      print FRONT FACE DOWN, no supports
//   cover   back cover of the round  print OUTSIDE DOWN
//   base    speaker base             print TOP DOWN (open bottom up)
//   bottom  bottom plate of the base print flat
//
// Case coordinates for the round part: the display axis is Z, Z=0 is the
// front face (display looks towards -Z), +Y is "up" on the clock face.

/* [Part] */
part = "all"; // [all, exploded, shell, cover, base, bottom]

/* [MEASURE: display module] */
// Round cover glass diameter (Waveshare: outline 115 x 115 mm)
glass_d = 115.0;
// Glass + LCD panel thickness at the rim
glass_t = 6.0;
// Visible area diameter (Waveshare: 87.6 mm)
active_d = 87.6;
// Depth needed behind the panel (PCB, USB-A port, connectors, cables)
board_depth = 24.0;
// Largest radius of PCB/components behind the panel (pillars stay outside it)
board_r = 54.0;

/* [MEASURE: speaker] */
// Speaker plate outline (width, height)
spk_size = [100, 45];
// Mounting hole spacing (x, y)
spk_holes = [92, 37];
// Driver diameter (two drivers)
spk_driver_d = 36;
// Driver centre distance from the middle (x)
spk_driver_x = 22;
// Depth of the speaker behind its mounting plate (magnets)
spk_depth = 20;

/* [Case] */
wall = 3.5;
lip_t = 2.0;
opening_margin = 2.0;
fit = 0.3;
// Backwards tilt of the clock in degrees
tilt = 15;
// Width of the clock's flat foot
foot_w = 70;
// Extra height of the foot below the round case
foot_drop = 8;
cover_t = 2.5;
// Pilot hole for M3 self-tapping screws (4.0 for M3 heat-set inserts)
screw_pilot_d = 2.5;
screw_hole_d = 3.4;
// Gap between cover pillars and the glass rim (fill with foam tape)
pillar_gap = 1.0;

/* [Base] */
base_wall = 3.0;
// Front wall thickness (holds the speaker screws)
base_front_t = 5.0;
// Space behind the speaker
base_back_space = 20;
// Air gap between speaker plate and the base edges
base_clear = 1.5;
bottom_t = 3.0;
// Cable slot in the back wall (width, height), open to the bottom
cable_slot = [14, 9];
// Channel from the base into the clock for USB + speaker wires
wire_hole_d = 15;
corner_r = 6;

$fn = 96;

// ---- derived: round case -----------------------------------------------
inner_r   = glass_d / 2 + fit;
outer_r   = inner_r + wall;
depth     = lip_t + glass_t + board_depth + cover_t;
z_glass_back = lip_t + glass_t;
cover_r   = outer_r - wall / 2;
open_d    = active_d + 2 * opening_margin;
boss_r    = inner_r + 0.6;
boss_d    = 7.5;
boss_len  = 10;
pillar_r  = (board_r + inner_r) / 2;
table_h   = outer_r * cos(tilt) + foot_drop;   // distance centre -> foot plane
foot_z0   = 3;                                 // foot footprint along the case depth
foot_z1   = depth - 3;

// ---- derived: base --------------------------------------------------------
base_in_w = spk_size[0] + 2 * base_clear;
base_in_h = spk_size[1] + 2 * base_clear;
base_w    = base_in_w + 2 * base_wall;
base_h    = base_in_h + base_wall + bottom_t;        // top wall + bottom plate
base_d    = base_front_t + spk_depth + base_back_space + base_wall;

echo(str("Clock: diameter ", 2 * outer_r, " mm, depth ", depth, " mm"));
echo(str("Base: ", base_w, " x ", base_d, " x ", base_h, " mm (w x d x h)"));

// ---- helpers ----------------------------------------------------------------
// Rotation that puts the case on the table: the display looks towards world -Y
// (the front), tilted back by `tilt`; case +Y (up) -> roughly world +Z.
module to_world() { rotate([0, 0, 180]) rotate([90 + tilt, 0, 0]) children(); }
// Inverse, world -> case coordinates
module to_case() { rotate([-(90 + tilt), 0, 0]) rotate([0, 0, -180]) children(); }

// half-space below the foot plane, in case coordinates
module below_foot() {
  to_case() translate([-300, -300, -600 - table_h_world()]) cube([600, 600, 600]);
}
// height of the case centre above the foot plane in world coordinates
function table_h_world() = table_h;

// foot plane point (case coords) for case depth z
function foot_y(z) = (-table_h + z * sin(tilt)) / cos(tilt);

module foot_footprint() {
  r = 3;
  for (x = [-foot_w / 2 + r, foot_w / 2 - r], z = [foot_z0, foot_z1])
    translate([x, foot_y(z) - 2, z]) sphere(r = r, $fn = 32);
}

// world position of the foot centre (where the base screws go)
foot_mid_z = (foot_z0 + foot_z1) / 2;

module outer_body() {
  difference() {
    hull() {
      translate([0, 0, 1.5]) cylinder(r = outer_r, h = depth - 3);
      cylinder(r = outer_r - 1.5, h = depth);
      foot_footprint();
    }
    below_foot();
  }
}

boss_angles   = [45, 135, 225, 315];
pillar_angles = [90, 210, 330];
// two screws from the base into the foot, along world Z
foot_screw_x = [-foot_w / 2 + 12, foot_w / 2 - 12];

// a cylinder along world "up" starting at the foot plane, in case coordinates
module up_from_foot(x, z, d, h) {
  translate([x, foot_y(z), z]) rotate([-tilt, 0, 0]) rotate([-90, 0, 0])
    translate([0, 0, -1]) cylinder(d = d, h = h + 1);
}

// ---- round case: shell ------------------------------------------------------
module shell() {
  difference() {
    union() {
      difference() {
        outer_body();
        translate([0, 0, lip_t]) cylinder(r = inner_r, h = depth);
        translate([0, 0, depth - cover_t]) cylinder(r = cover_r + fit, h = cover_t + 1);
        translate([0, 0, -0.01]) cylinder(d1 = open_d + 2 * lip_t, d2 = open_d, h = lip_t + 0.02);
      }
      // screw bosses for the cover, merged into the wall (clipped so they never show outside)
      intersection() {
        cylinder(r = outer_r - 0.8, h = depth);
        union() for (a = boss_angles) rotate([0, 0, a])
          translate([boss_r, 0, depth - cover_t - boss_len])
            hull() {
              cylinder(d = boss_d, h = boss_len);
              translate([wall, 0, 0]) cylinder(d = boss_d, h = boss_len);
            }
      }
    }
    for (a = boss_angles) rotate([0, 0, a])
      translate([boss_r, 0, depth - cover_t - boss_len - 0.1])
        cylinder(d = screw_pilot_d, h = boss_len + 0.2, $fn = 24);
    // wire/USB channel from the foot into the cavity
    up_from_foot(0, foot_mid_z, wire_hole_d, 40);
    // pilot holes for the two screws from the base
    for (x = foot_screw_x) up_from_foot(x, foot_mid_z, screw_pilot_d, 9);
  }
}

// ---- round case: back cover ---------------------------------------------------
module cover() {
  z_in = depth - cover_t;
  pillar_len = depth - cover_t - z_glass_back - pillar_gap;
  difference() {
    union() {
      difference() {
        translate([0, 0, z_in]) cylinder(r = cover_r, h = cover_t);
        below_foot();
      }
      for (a = pillar_angles) rotate([0, 0, a])
        translate([pillar_r, 0, z_in - pillar_len]) cylinder(d = 5, h = pillar_len, $fn = 32);
    }
    for (a = boss_angles) rotate([0, 0, a]) translate([boss_r, 0, z_in - 1]) {
      cylinder(d = screw_hole_d, h = cover_t + 2, $fn = 24);
      translate([0, 0, cover_t + 1 - 1.7]) cylinder(d1 = screw_hole_d, d2 = 6.4, h = 1.71, $fn = 24);
    }
    // ventilation slots, top and bottom
    for (y = [cover_r - 22, -cover_r + 30], x = [-24:8:24])
      translate([x, y, depth - cover_t / 2])
        hull() { translate([0, -5, 0]) cylinder(d = 3, h = cover_t + 2, center = true, $fn = 16);
                 translate([0,  5, 0]) cylinder(d = 3, h = cover_t + 2, center = true, $fn = 16); }
  }
}

// ---- speaker base (world coordinates, front at y = 0, bottom at z = 0) -------
module rounded_box(size, r) {
  hull() for (x = [r, size[0] - r], y = [r, size[1] - r])
    translate([x, y, 0]) cylinder(r = r, h = size[2], $fn = 48);
}

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

// where the clock's foot sits on the base top (world), centred on the base
foot_world_y = base_d / 2;

module base() {
  zc = bottom_t + base_in_h / 2;          // speaker centre height
  difference() {
    translate([-base_w / 2, 0, 0]) rounded_box([base_w, base_d, base_h], corner_r);
    // inner cavity, open to the bottom (bottom plate closes it)
    translate([-base_in_w / 2, base_front_t, -1])
      rounded_box([base_in_w, base_d - base_front_t - base_wall, base_h - base_wall + 1], 2);
    // recess for the bottom plate
    translate([-base_w / 2 + base_wall / 2, base_wall / 2, -0.01])
      rounded_box([base_w - base_wall, base_d - base_wall, bottom_t + 0.01], corner_r - 1);
    // grille through the front wall
    translate([0, base_front_t / 2, zc]) rotate([90, 0, 0]) grille_holes();
    // speaker screw pilots, from inside into the front wall
    for (sx = [-1, 1], sz = [-1, 1])
      translate([sx * spk_holes[0] / 2, base_front_t + 0.01, zc + sz * spk_holes[1] / 2])
        rotate([90, 0, 0]) cylinder(d = screw_pilot_d, h = base_front_t - 1.2, $fn = 24);
    // wire/USB channel up into the clock
    translate([0, foot_world_y, base_h - base_wall - 1]) cylinder(d = wire_hole_d, h = base_wall + 2);
    // screws from inside the base up into the clock's foot
    for (x = foot_screw_x) translate([x, foot_world_y, base_h - base_wall - 1]) {
      cylinder(d = screw_hole_d, h = base_wall + 2, $fn = 24);
    }
    // cable slot in the back wall, open to the bottom
    translate([-cable_slot[0] / 2, base_d - base_wall - 1, -0.01]) cube([cable_slot[0], base_wall + 2, cable_slot[1]]);
  }
  // corner bosses for the bottom plate screws
  for (p = bottom_screws())
    translate([p[0], p[1], bottom_t])
      difference() {
        cylinder(d = 7, h = base_h - base_wall - bottom_t + 0.5, $fn = 32);
        translate([0, 0, -0.1]) cylinder(d = screw_pilot_d, h = 10, $fn = 24);
      }
}

// bosses overlap the walls by 1 mm (a boss exactly touching a wall is not printable/manifold)
function bottom_screws() = [
  [-base_in_w / 2 + 2.5, base_d - base_wall - 2.5],
  [ base_in_w / 2 - 2.5, base_d - base_wall - 2.5],
  [-base_in_w / 2 + 2.5, base_front_t + spk_depth + 4],
  [ base_in_w / 2 - 2.5, base_front_t + spk_depth + 4],
];

module bottom_plate() {
  difference() {
    translate([-base_w / 2 + base_wall / 2 + 0.2, base_wall / 2 + 0.2, 0])
      rounded_box([base_w - base_wall - 0.4, base_d - base_wall - 0.4, bottom_t], corner_r - 1.2);
    for (p = bottom_screws()) translate([p[0], p[1], -1]) {
      cylinder(d = screw_hole_d, h = bottom_t + 2, $fn = 24);
      cylinder(d1 = 6.4, d2 = screw_hole_d, h = 1.7, $fn = 24);
    }
    // cable slot continues through the plate edge
    translate([-cable_slot[0] / 2, base_d - base_wall - 6, -1]) cube([cable_slot[0], 10, bottom_t + 2]);
    // recesses for 10 mm rubber feet
    for (x = [-base_w / 2 + 12, base_w / 2 - 12], y = [10, base_d - 12])
      translate([x, y, -0.01]) cylinder(d = 10.5, h = 0.8, $fn = 32);
  }
}

module clock_world() {
  // put the foot centre (case coords: (0, foot_y(mid), mid)) on the base top centre
  translate([0, foot_world_y, base_h])
    to_world()
      translate([0, -foot_y(foot_mid_z), -foot_mid_z]) children();
}

// ---- output -------------------------------------------------------------------
if (part == "shell") {
  shell();
} else if (part == "cover") {
  translate([0, 0, depth]) mirror([0, 0, 1]) cover();
} else if (part == "base") {
  translate([0, 0, base_h]) mirror([0, 0, 1]) base();
} else if (part == "bottom") {
  bottom_plate();
} else {
  gap = (part == "exploded") ? 1 : 0;
  color("SlateGray") base();
  color("DimGray") translate([0, 0, -12 * gap]) bottom_plate();
  translate([0, 0, 25 * gap]) clock_world() {
    color("SlateGray") shell();
    color("LightSteelBlue") translate([0, 0, 30 * gap]) cover();
    color([0.05, 0.05, 0.08]) translate([0, 0, lip_t]) cylinder(r = glass_d / 2, h = glass_t);
  }
}
