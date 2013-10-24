$fn=16;

module square_segment()
{
	difference() {
		cube(size = [10, 10, 1], center = true);
		cube(size = [8, 8, 2], center = true);
	}
}

module upright_and_diagonal(angle)
{
	rotate(a = angle, v = [0, 0, 1]) {
		translate(v = [-4.5, -4.5, -4.5], center = true) {
			cube(size = [0.7, 0.7, 10], center = true);
			translate(v = [4.5, 0, 0], center = true)
				rotate(a = 45, v = [0, 1, 0])
					cube(size = [0.7, 0.7, 14.142], center = true);
		}
	}
}

module truss_segment()
{
	union() {
		square_segment();
		upright_and_diagonal(0);
		upright_and_diagonal(90);
		upright_and_diagonal(-90);
		upright_and_diagonal(180);
	}
}

module truss(length)
{
	rotate(a = 90, v = [0, 1, 0])
	for ( i = [0 : length]) {
		translate(v = [0, 0, i * 10])
			truss_segment();
	}
}

module nose_cone()
{
	intersection() {
		union() {
			translate(v = [-10, 0, 0])
			rotate(a = 90, v = [0, 1, 0])
				cylinder(h = 5, r1 = 10, r2 = 7, center = true);
			translate(v = [-17.5, u, 0])
				rotate(a = 90, v = [0, 1, 0])
					cylinder(h = 10, r1 = 3, r2 = 10, center = true);
			translate(v = [-21.4, 0, 0])
				sphere(r = 3.2);
		}
		rotate(a = 90, v = [0, 1, 0])
			cylinder(h = 60, r1 = 9.5, r2 = 9.5, center = true);
	}
}

module corner_thruster_a()
{
	rotate (a = 90, v = [0, 1, 0]) {
		union() {
			difference() {
				cylinder(h = 5, r1 = 2, r2 = 2.5, center = true);
				translate(v = [0, 0, 1])
					cylinder(h = 5, r1 = 2, r2 = 2.5, center = true);
			}
			translate(v = [0, 0, -4])
				sphere(r = 2.5);
		}
	}
}

module corner_thruster(angle)
{
	rotate(a = angle, v = [1, 0, 0])
		translate(v = [38, -5, -5])
			corner_thruster_a();
}

module tail()
{
		translate(v = [31.0, 0, 0]) {
			cube(size = [4, 15, 15], center = true);
			translate(v = [5, 0, 0])
			rotate(a = 90, v = [0, 1, 0]) {
				difference() {
					cylinder(h = 10, r1 = 3, r2 = 4, center = true);
					translate(v = [0, 0, 1])
					cylinder(h = 10, r1 = 3, r2 = 4, center = true);
				}
			}
		}
}

module fuel_tank()
{
	sphere(r = 5.5);
	translate(v = [12, 0, 0])
		sphere(r = 5.5);
}

module whole_ship() {
	union()
	{
		nose_cone();
		truss(3);
		tail();
		fuel_tank();
		corner_thruster(0);
		corner_thruster(90);
		corner_thruster(-90);
		corner_thruster(180);
	}
}

rotate(a = 180, v = [0, 1, 0])
rotate(a = -90, v = [1, 0, 0])
whole_ship();

