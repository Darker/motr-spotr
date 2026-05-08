ANTENA_DIAMETER = 210.75;
ANTENNA_RIM_THICKNESS = 11.8;

CLAMP_RIDGE_THICKNESS = 2;
CLAMP_RIDGE_DIAMETER = ANTENA_DIAMETER;
CLAMP_RAIL_INNER_TOP = 20;
CLAMP_RAIL_INNER_BOTTOM = 2;
CLAMP_RAIL_OUTER_TOP = 10;
CLAMP_RAIL_THICKNESS = 2;
CLAMP_RAIL_OUTER_BOTTOM = 4;
CLAMP_RAIL_OUTER_BOTTOM_SUPPORT_HEIGHT = 4;

module cylinder_slice(radius, height, thickness, angle)
{
    rotate_extrude(angle=angle)
        translate([radius, 0]) 
            square([thickness, height]); 
}

module cylinder_ramp_slice(radius, height, thickness, angle, reverse=false)
{
    rotate_extrude(angle = angle)
        polygon(
            reverse ?
            // reversed slope: inner = height, outer = 0
            [
                [radius,            0],       // inner low
                [radius+thickness,  height],  // outer high
                [radius+thickness,  0]        // outer low
            ]
            :
            // normal slope: inner = 0, outer = height
            [
                [radius,            0],
                [radius,            height],
                [radius+thickness,  0]
            ]
        );
}


module radial_cylinders(
    count,          // number of cylinders
    circle_r,       // radius of the circle on which cylinders sit
    cyl_r,          // cylinder radius
    cyl_h,          // cylinder height
    phase = 0       // rotation offset in degrees
){
    for(i = [0 : count-1]) {
        angle = phase + 360/count * i;

        // Position cylinder on circle
        translate([circle_r*cos(angle), circle_r*sin(angle), 0])
            // Rotate cylinder so its axis is perpendicular to the circle plane
            rotate([0, 0, angle])
                cylinder(h = cyl_h, r = cyl_r, center = true);
    }
}



//cylinder(ANTENNA_RIM_THICKNESS, ANTENA_DIAMETER/2, ANTENA_DIAMETER/2, $fa=5);

// rotate_extrude(angle = 60, $fa=5)   // slice angle in degrees
//     translate([CLAMP_RIDGE_DIAMETER/2, 0])       // radius = 10
//         square([CLAMP_RIDGE_THICKNESS, ANTENNA_RIM_THICKNESS]);      // thickness = 2
union()
{
    cylinder_slice(CLAMP_RIDGE_DIAMETER/2, ANTENNA_RIM_THICKNESS, CLAMP_RIDGE_THICKNESS, 87, $fa=5);

    color("red")
    union()
    {
        difference()
        {
            translate([0,0,ANTENNA_RIM_THICKNESS])
            cylinder_slice(CLAMP_RIDGE_DIAMETER/2-CLAMP_RAIL_INNER_TOP, CLAMP_RAIL_THICKNESS, CLAMP_RAIL_INNER_TOP+CLAMP_RIDGE_THICKNESS, 87, $fa=5);

            translate([0,0,7+ANTENNA_RIM_THICKNESS/2])
            radial_cylinders(15, CLAMP_RIDGE_DIAMETER/2-CLAMP_RAIL_INNER_TOP/2, 4, 15, 9);
        }


        translate([0,0,-CLAMP_RAIL_THICKNESS])
        cylinder_slice(CLAMP_RIDGE_DIAMETER/2-CLAMP_RAIL_INNER_BOTTOM, CLAMP_RAIL_THICKNESS, CLAMP_RAIL_INNER_BOTTOM+CLAMP_RIDGE_THICKNESS, 87, $fa=5);
        translate([0,0,0])
        cylinder_ramp_slice(CLAMP_RIDGE_DIAMETER/2-CLAMP_RAIL_INNER_BOTTOM, CLAMP_RAIL_INNER_BOTTOM/2, CLAMP_RAIL_INNER_BOTTOM+CLAMP_RIDGE_THICKNESS, 87, reverse=true, $fa=5);
    }

    color("green")
    union()
    {
        translate([0,0,ANTENNA_RIM_THICKNESS])
        cylinder_slice(CLAMP_RIDGE_DIAMETER/2, CLAMP_RAIL_THICKNESS, CLAMP_RAIL_OUTER_TOP+CLAMP_RIDGE_THICKNESS, 87, $fa=5);

        translate([0,0,-CLAMP_RAIL_THICKNESS])
        cylinder_slice(CLAMP_RIDGE_DIAMETER/2, CLAMP_RAIL_THICKNESS, CLAMP_RAIL_OUTER_BOTTOM+CLAMP_RIDGE_THICKNESS, 87, $fa=5);

        translate([0,0,0])
        cylinder_ramp_slice(CLAMP_RIDGE_DIAMETER/2, CLAMP_RAIL_OUTER_BOTTOM_SUPPORT_HEIGHT, CLAMP_RAIL_OUTER_BOTTOM+CLAMP_RIDGE_THICKNESS, 87, $fa=5);
    }

}
