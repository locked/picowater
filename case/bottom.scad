include <common.scad>;

module case () {
    difference () {
        minkowski() {
            cube([height-round_radius*2, width-round_radius*2, bottom_depth]);
            cylinder(r=round_radius, h=pcb_thickness);
        }

        translate([round_radius,round_radius,thickness]) minkowski() {
            cube([pcb_height-round_radius*2, pcb_width-round_radius*2, bottom_depth]);
            cylinder(r=round_radius, h=1);
        }
    }

    // Screw supports
    for (hole = holes) {
        translate(hole)
            cylinder(r=screw_hole_radius+3, h=bottom_depth);
    }
}

module screw_holes() {
    for (hole = holes) {
        translate([hole[0], hole[1], -thickness])
            cylinder(r=screw_hole_radius+0.2, h=bottom_depth+thickness);

        translate([hole[0], hole[1], -bottom_depth+thickness])
            cylinder(r=3.3, h=bottom_depth-thickness+3, $fn=6);
    }
}

difference () {
    case();
    screw_holes();
}