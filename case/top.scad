include <common.scad>;

module case () {
    difference () {
        minkowski() {
            cube([height-round_radius*2, width-round_radius*2, top_depth]);
            cylinder(r=round_radius, h=0.0001);
        }

        translate([round_radius,round_radius,thickness]) minkowski() {
            cube([pcb_height-round_radius*2, pcb_width-round_radius*2, top_depth]);
            cylinder(r=round_radius, h=1);
        }
    }

    // Screw supports
    for (hole = holes) {
        translate([hole[0], hole[1], hole[2]+top_depth])
            cylinder(r=pcb_screw_hole_radius, h=1.5);
        translate(hole)
            cylinder(r=screw_hole_radius+2, h=top_depth);
        translate(hole)
            cylinder(r=screw_hole_radius+4, h=6);
    }

    translate([pcb_height+thickness, pcb_width-21, top_depth-2])
    rotate([90, 180, 90])
    linear_extrude(thickness/3)
    text("PicoWater", size=5);
}

module screw_holes() {
    for (hole = holes) {
        translate([hole[0], hole[1], -thickness])
            cylinder(r=screw_hole_radius, h=top_depth+thickness+1.5);
        translate([hole[0], hole[1], -bottom_depth+thickness])
            cylinder(r=3.5, h=bottom_depth-thickness+2.5);
    }
}

module rj45() {
    rj45_posx = pcb_height / 2 - rj45[0] / 2;
    rj45_posz = top_depth - rj45[2] + 0.001;
    translate([rj45_posx, -thickness, rj45_posz]) cube(rj45);

    translate([0, 8.5, rj45_posz]) rotate([0,0,90]) cube(rj45);
    translate([0, 28, rj45_posz]) rotate([0,0,90]) cube(rj45);
}

module powerjack() {
    powerjack_posx = 31.5;
    powerjack_posz = top_depth - powerjack[2] + 0.001;
    translate([height, powerjack_posx, powerjack_posz]) rotate([0,0,90]) cube(powerjack);
}

module debug_hole() {
    debug_posx = pcb_height + thickness;
    debug_posy = 83;
    debug_posz = top_depth - 4;
    translate([debug_posx, debug_posy, debug_posz]) rotate([0, -90, 0]) cylinder(r=2, h=thickness);
}

module led_hole() {
    led_posx = pcb_height + thickness;
    led_posy = pcb_width - 19;
    led_posz = top_depth - 0.99;
    translate([0, led_posy, led_posz]) rotate([0, -90, 0]) cube([1, 2, thickness]);
}

module airvents() {
    rad = 1;
    posx = pcb_height / 2 - (5*rad*3/2);
    posy = pcb_width / 2 - 5*rad;
    for (i = [0,1,2,3,4,5]) {
        for (j = [0,1,2,3,4,5]) {
            translate([posx + i*rad*3, posy + j*rad*3, 0])
                cylinder(r=rad, h=thickness, $fn=10);
        }
    }
}

module uart() {
    uart_posx = pcb_height - thickness + 1;
    uart_posy = pcb_width - thickness - 50;
    translate([uart_posx, uart_posy, 0])
    rotate([0, 0, 90])
        cube([3, 10, thickness]);
}

module usb_debug() {
    usb_debug_posx = -thickness;
    usb_debug_posy = pcb_width - thickness - 35 - 5;
    usb_debug_posz = top_depth - 10 - 2;
    translate([usb_debug_posx, usb_debug_posy, usb_debug_posz])
        cube([thickness, 10, 5]);
}

module rtc_bat() {
    rtc_bat_posx = 17;
    rtc_bat_posy = pcb_width - 0.1;
    rtc_bat_posz = top_depth - 5 + 0.1;
    translate([rtc_bat_posx, rtc_bat_posy, rtc_bat_posz])
        cube([10, thickness + 0.1, 5]);
}

difference () {
    case();
    screw_holes();
    rj45();
    powerjack();
    debug_hole();
    led_hole();
    airvents();
    uart();
    usb_debug();
    rtc_bat();
}
