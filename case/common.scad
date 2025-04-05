pcb_width = 98;
pcb_height = 56;
pcb_thickness = 1.6;
pcb_screw_hole_radius = 2.1;	// M4/2-margin

thickness = 2;
round_radius = 2;

width = pcb_width + thickness * 2;	// y
height = pcb_height + thickness * 2;	// x
bottom_depth = 5;	// z
top_depth = 26;		// z

screw_hole_radius = 1.1;	// M2/2
screw_posx = 3.5;
screw_posy = 3.5;

rj45 = [16, thickness*2, 13.8];
usb = [15, thickness*2, 9];

$fn = 50;

posx = screw_posx;
posy = screw_posy;
holes = [
    [posx, posy, 0],
    [pcb_height-posx, posy, 0],
    [posx, pcb_width-posy, 0],
    [pcb_height-posx, pcb_width-posy, 0]
];

