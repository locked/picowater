pcb_width = 98;
pcb_height = 56;
pcb_thickness = 1.6;
pcb_screw_hole_radius = 2.4;	// M4/2-margin

thickness = 2;
round_radius = 2;

width = pcb_width + thickness * 2;	// y
height = pcb_height + thickness * 2;	// x
bottom_depth = 5;	// z
top_depth = 25;		// z

screw_hole_radius = 1.6;	// M2/2
screw_posx = 4.5;
screw_posy = 4.5;

rj45 = [16, thickness*2, 13.8];
powerjack = [10, thickness*2, 12];

$fn = 50;

posx = screw_posx;
posy = screw_posy;
holes = [
    [posx, posy, 0],
    [pcb_height-posx, posy, 0],
    [posx, pcb_width-posy, 0],
    [pcb_height-posx, pcb_width-posy, 0]
];

