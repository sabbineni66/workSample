// 3D World - OpenGL CS184 Computer Graphics Project
// by Frank Gennari
// 5/1/02

#include "3DWorld.h"
#include "mesh.h"
#include "player_state.h"
#include "physics_objects.h"
#include "openal_wrap.h"
#include "model3d.h"


bool const REMOVE_ALL_COLL   = 1;
bool const ALWAYS_ADD_TO_HCM = 0;
unsigned const CAMERA_STEPS  = 10;
unsigned const PURGE_THRESH  = 20;
float const CAMERA_MESH_DZ   = 0.1; // max dz on mesh


// Global Variables
bool camera_on_snow(0);
int camera_coll_id(-1);
float czmin(FAR_DISTANCE), czmax(-FAR_DISTANCE), coll_rmax(0.0);
point camera_last_pos(all_zeros); // not sure about this, need to reset sometimes
coll_obj_group coll_objects;
cobj_groups_t cobj_groups;
cobj_draw_groups cdraw_groups;

extern bool lm_alloc, has_snow;
extern int camera_coll_smooth, game_mode, world_mode, xoff, yoff, camera_change, display_mode, scrolling, animate2;
extern int camera_in_air, mesh_scale_change, camera_invincible, camera_flight, do_run, num_smileys, iticks;
extern unsigned snow_coverage_resolution;
extern float TIMESTEP, temperature, zmin, base_gravity, ftick, tstep, zbottom, ztop, fticks, jump_height, NEAR_CLIP;
extern double camera_zh, tfticks;
extern dwobject def_objects[];
extern obj_type object_types[];
extern player_state *sstates;
extern platform_cont platforms;
extern set<unsigned> moving_cobjs;
extern reflective_cobjs_t reflective_cobjs;
extern model3ds all_models;


void add_coll_point(int i, int j, int index, float zminv, float zmaxv, int add_to_hcm, int is_dynamic, int dhcm);
void free_all_coll_objects();
bool proc_movable_cobj(point const &orig_pos, point &player_pos, unsigned index, int type);


bool decal_obj::is_on_cobj(int cobj, vector3d *delta) const {

	if (cobj < 0) return 0;
	coll_obj const &c(coll_objects.get_cobj(cobj)); // can this fail if the cobj was destroyed? coll_objects only increases in size
	// spheres and cylinder sides not supported - decals look bad on rounded objects
	if (c.status != COLL_STATIC || (!c.has_flat_top_bot() && c.type != COLL_CYLINDER_ROT)) return 0;
	//if (c.type == COLL_CUBE && c.cp.cobj_type == COBJ_TYPE_MODEL3D) return 0; // model3d bounding volume - should we include these?
	point center(ipos + get_platform_delta());

	if (c.is_moving()) {
		vector3d const local_delta(c.get_center_of_mass() - cobj_cent_mass); // look at cobj LLC delta
		center += local_delta;
		if (delta) {*delta = local_delta;}
	}
	if (!sphere_cube_intersect(center, DECAL_OFFSET, c)) return 0;
	if (c.type == COLL_CUBE) return 1;
	float const draw_radius(0.75*radius);

	if (c.type == COLL_CYLINDER) {
		if (c.cp.surfs & 1) return 0; // draw_ends=0
		if (orient != plus_z && orient != -plus_z) return 0; // not on the cylinder end
		return (draw_radius < c.radius && dist_xy_less_than(center, c.points[orient == plus_z], (c.radius - draw_radius)));
	}
	if (c.type == COLL_CYLINDER_ROT) {
		if (c.cp.surfs & 1) return 0; // draw_ends=0
		vector3d const cylin_dir((c.points[1] - c.points[0]).get_norm());
		float const dp(dot_product(orient, cylin_dir));
		if (fabs(dp) < 0.99) return 0; // not on the cylinder end
		float const c_radius((dp > 0.0) ? c.radius2 : c.radius);
		return (draw_radius < c_radius && dist_less_than(center, c.points[dp > 0.0], (c_radius - draw_radius)));
	}
	assert(c.type == COLL_POLYGON);

	if (c.thickness > MIN_POLY_THICK) { // extruded polygon
		return sphere_ext_poly_intersect(c.points, c.npoints, c.norm, (center - orient*DECAL_OFFSET), -0.5*DECAL_OFFSET, c.thickness, MIN_POLY_THICK);
	}
	float t; // thin polygon case
	vector3d const check_dir(orient*(MIN_POLY_THICK + DECAL_OFFSET));
	point const p1(center - check_dir), p2(center + check_dir);
	if (!line_poly_intersect(p1, p2, c.points, c.npoints, c.norm, t)) return 0; // doesn't really work on extruded polygons; maybe should check that dist to polygon edge < draw_radius
	return (draw_radius < min_dist_from_pt_to_polygon_edge(center, c.points, c.npoints));
}


void decal_obj::check_cobj() {

	if (!status || cid < 0) return; // already disabled, or no bound cobj
	vector3d delta(zero_vector);
	
	if (is_on_cobj(cid, &delta)) { // try to find the cobj this is attached to (likely a split part of the original)
		pos += delta; ipos += delta; cobj_cent_mass += delta; // move by this delta
	}
	else {
		int const xpos(get_xpos(ipos.x)), ypos(get_ypos(ipos.y));
		if (point_outside_mesh(xpos, ypos)) {status = 0; return;}
		vector<int> const &cvals(v_collision_matrix[ypos][xpos].cvals);
		cid = -1;

		for (unsigned i = 0; i < cvals.size(); ++i) {
			if (is_on_cobj(cvals[i])) {cid = cvals[i]; break;}
		}
		if (cid >= 0) {cobj_cent_mass = coll_objects.get_cobj(cid).get_center_of_mass();}
	}
	if (cid < 0) {status = 0; return;} // not found, no longer on a cobj so remove it
}


vector3d decal_obj::get_platform_delta() const {

	if (cid >= 0 && coll_objects.get_cobj(cid).platform_id >= 0) {return platforms.get_cobj_platform(coll_objects.get_cobj(cid)).get_delta();}
	return all_zeros;
}


class cobj_manager_t {

	vector<int> index_stack;
	coll_obj_group &cobjs;
	unsigned index_top;

	void extend_index_stack(unsigned start, unsigned end) {
		index_stack.resize(end);

		for (size_t i = start; i < end; ++i) { // initialize
			index_stack[i] = (int)i; // put on the free list
		}
	}

public:
	unsigned cobjs_removed;

	cobj_manager_t(coll_obj_group &cobjs_) : cobjs(cobjs_), index_top(0), cobjs_removed(0) {
		extend_index_stack(0, cobjs.size());
	}

	void reserve_cobjs(size_t size) {
		size_t const old_size(cobjs.size());
		if (old_size >= size) return; // already large enough
		size_t const new_size(max(size, 2*old_size)); // prevent small incremental reserves
		//cout << "Resizing cojs vector from " << old_size << " to " << new_size << endl;
		cobjs.resize(new_size);
		extend_index_stack(old_size, new_size);
	}

	int get_next_avail_index() {
		size_t const old_size(cobjs.size());
		assert(index_stack.size() == old_size);
		if (index_top >= old_size) reserve_cobjs(2*index_top + 4); // approx double in size
		int const index(index_stack[index_top]);
		assert(size_t(index) < cobjs.size());
		assert(cobjs[index].status == COLL_UNUSED);
		cobjs[index].status = COLL_PENDING;
		index_stack[index_top++] = -1;
		return index;
	}

	void free_index(int index) {
		assert(cobjs[index].status != COLL_UNUSED);
		cobjs[index].status = COLL_UNUSED;

		if (!cobjs[index].fixed) {
			assert(index_top > 0);
			index_stack[--index_top] = index;
		}
	}

	bool swap_and_set_as_coll_objects(coll_obj_group &new_cobjs) {
		if (!cobjs.empty()) return 0;
		cobjs.swap(new_cobjs);
		unsigned const ncobjs(cobjs.size());
		cobjs.resize(cobjs.capacity()); // use up all available capacity
		extend_index_stack(0, cobjs.size());

		for (unsigned i = 0; i < ncobjs; ++i) {
			coll_obj temp_cobj(cobjs[i]);
			temp_cobj.add_as_fixed_cobj(); // don't need to remove it
			assert(cobjs[i].id == (int)i);
		}
		return 1;
	}
};

cobj_manager_t cobj_manager(coll_objects);


void reserve_coll_objects(unsigned size) {
	cobj_manager.reserve_cobjs(size);
}

bool swap_and_set_as_coll_objects(coll_obj_group &new_cobjs) {
	return cobj_manager.swap_and_set_as_coll_objects(new_cobjs);
}

void add_reflective_cobj(unsigned index) {
	coll_objects[index].set_reflective_flag(1);
	reflective_cobjs.add_cobj(index);
}

inline void get_params(int &x1, int &y1, int &x2, int &y2, const float d[3][2]) {

	x1 = max(0, get_xpos(d[0][0]));
	y1 = max(0, get_ypos(d[1][0]));
	x2 = min((MESH_X_SIZE-1), get_xpos(d[0][1]));
	y2 = min((MESH_Y_SIZE-1), get_ypos(d[1][1]));
}


void add_coll_cube_to_matrix(int index, int dhcm) {

	int x1, x2, y1, y2;
	coll_obj const &cobj(coll_objects[index]);
	bool const is_dynamic(cobj.status == COLL_DYNAMIC);
	cube_t const bcube(cobj.get_platform_max_bcube()); // adjust the size of the cube to account for all possible platform locations
	get_params(x1, y1, x2, y2, bcube.d);

	for (int i = y1; i <= y2; ++i) {
		for (int j = x1; j <= x2; ++j) {add_coll_point(i, j, index, bcube.d[2][0], bcube.d[2][1], 1, is_dynamic, dhcm);}
	}
}

int add_coll_cube(cube_t &cube, cobj_params const &cparams, int platform_id, int dhcm) {

	int const index(cobj_manager.get_next_avail_index());
	coll_obj &cobj(coll_objects[index]);
	cube.normalize();
	cobj.copy_from(cube);
	// cache the center point and radius
	coll_objects.set_coll_obj_props(index, COLL_CUBE, cobj.get_bsphere_radius(), 0.0, platform_id, cparams);
	add_coll_cube_to_matrix(index, dhcm);
	return index;
}


void add_coll_cylinder_to_matrix(int index, int dhcm) {

	int xx1, xx2, yy1, yy2;
	coll_obj const &cobj(coll_objects[index]);
	float zminc(cobj.d[2][0]), zmaxc(cobj.d[2][1]), zmin0(zminc), zmax0(zmaxc);
	point const p1(cobj.points[0]), p2(cobj.points[1]);
	float const x1(p1.x), x2(p2.x), y1(p1.y), y2(p2.y), z1(p1.z), z2(p2.z);
	float const radius(cobj.radius), radius2(cobj.radius2), dr(radius2 - radius), rscale((z1-z2)/fabs(dr));
	float const length(p2p_dist(p1, p2)), dt(HALF_DXY/length), r_off(radius + dt*fabs(dr));
	int const radx(int(ceil(radius*DX_VAL_INV))+1), rady(int(ceil(radius*DY_VAL_INV))+1), rxry(radx*rady);
	int const xpos(get_xpos(x1)), ypos(get_ypos(y1));
	bool const is_dynamic(cobj.status == COLL_DYNAMIC);
	get_params(xx1, yy1, xx2, yy2, cobj.d);

	if (cobj.type == COLL_CYLINDER_ROT) {
		float xylen(sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1)));
		float const rmin(min(radius, radius2)), rmax(max(radius, radius2));
		bool const vertical(x1 == x2 && y1 == y2), horizontal(fabs(z1 - z2) < TOLERANCE);
		bool const vert_trunc_cone(z1 != z2 && radius != radius2 && rmax > HALF_DXY);

		for (int i = yy1; i <= yy2; ++i) {
			float const yv(get_yval(i)), v2y(y1 - yv);

			for (int j = xx1; j <= xx2; ++j) {
				float xv(get_xval(j));

				if (vertical) { // vertical
					if (vert_trunc_cone) { // calculate zmin/zmax
						xv -= x1;
						float const rval(min(rmax, (float)sqrt(xv*xv + v2y*v2y)));
						zmaxc = ((rval > rmin) ? max(zmin0, min(zmax0, (rscale*(rval - rmin) + z2))) : zmax0);
					} // else near constant radius, so zminc/zmaxc are correct
				}
				else if (horizontal) {
					zminc = z1 - rmax;
					zmaxc = z1 + rmax;
				}
				else { // diagonal
					// too complex/slow to get this right, so just use the bbox zminc/zmaxc
				}
				int add_to_hcm(0);
				
				if (i >= yy1-1 && i <= yy2+1 && j >= xx1-1 && j <= xx2+1) {
					if (vertical) {
						add_to_hcm = ((i-ypos)*(i-ypos) + (j-xpos)*(j-xpos) <= rxry);
					}
					else {
						float const dist(fabs((x2-x1)*(y1-yv) - (x1-xv)*(y2-y1))/xylen - HALF_DXY);

						if (dist < rmax) {
							if (horizontal) {
								float const t(((x1-x2)*(x1-xv) + (y1-y2)*(y1-yv))/(xylen*xylen)); // location along cylinder axis
								add_to_hcm = (t >= -dt && t <= 1.0+dt && dist < min(rmax, (r_off + t*dr)));
							}
							else { // diagonal
								add_to_hcm = 1; // again, too complex/slow to get this right, so just be conservative
							}
						}
					}
				}
				add_coll_point(i, j, index, zminc, zmaxc, add_to_hcm, is_dynamic, dhcm);
			}
		}
	}
	else { // standard vertical constant-radius cylinder
		assert(cobj.type == COLL_CYLINDER);
		int const crsq(radx*rady);

		for (int i = yy1; i <= yy2; ++i) {
			for (int j = xx1; j <= xx2; ++j) {
				int const distsq((i - ypos)*(i - ypos) + (j - xpos)*(j - xpos));
				if (distsq <= crsq) {add_coll_point(i, j, index, z1, z2, (distsq <= rxry), is_dynamic, dhcm);}
			}
		}
	}
}

int add_coll_cylinder(point const &p1, point const &p2, float radius, float radius2, cobj_params const &cparams, int platform_id, int dhcm) {

	int type;
	int const index(cobj_manager.get_next_avail_index());
	coll_obj &cobj(coll_objects[index]);
	radius  = fabs(radius);
	radius2 = fabs(radius2);
	assert(radius > 0.0 || radius2 > 0.0);
	bool const nonvert(p1.x != p2.x || p1.y != p2.y || (fabs(radius - radius2)/max(radius, radius2)) > 0.2);
	type = (nonvert ? COLL_CYLINDER_ROT : COLL_CYLINDER);
	cobj.points[0] = p1;
	cobj.points[1] = p2;
	
	if (!nonvert) { // standard vertical constant-radius cylinder
		if (p2.z < p1.z) {swap(cobj.points[0], cobj.points[1]);}
	}
	if (dist_less_than(cobj.points[0], cobj.points[1], TOLERANCE)) { // no near zero length cylinders
		cout << "pt0 = " << cobj.points[0].str() << ", pt1 = " << cobj.points[1].str() << endl;
		assert(0);
	}
	coll_objects.set_coll_obj_props(index, type, radius, radius2, platform_id, cparams);
	add_coll_cylinder_to_matrix(index, dhcm);
	return index;
}


void add_coll_torus_to_matrix(int index, int dhcm) {

	coll_obj &cobj(coll_objects[index]);
	coll_obj const orig_cobj(cobj); // make a copy of the cobj so that we can modify it then restore it
	cylinder_3dw const cylin(cobj.get_bounding_cylinder());
	cobj.points[0] = cylin.p1;
	cobj.points[1] = cylin.p2;
	cobj.radius    = cylin.r1;
	cobj.radius2   = cylin.r2;
	cobj.type = (cobj.is_cylin_vertical() ? COLL_CYLINDER : COLL_CYLINDER_ROT);
	cobj.calc_bcube();
	add_coll_cylinder_to_matrix(index, dhcm);
	cobj = orig_cobj; // restore the original
}

int add_coll_torus(point const &p1, vector3d const &dir, float ro, float ri, cobj_params const &cparams, int platform_id, int dhcm) {

	assert(ro > 0.0 && ri > 0.0);
	assert(dir != zero_vector);
	int const index(cobj_manager.get_next_avail_index());
	coll_obj &cobj(coll_objects[index]);
	cobj.points[0] = cobj.points[1] = p1; // set both points to the same - some drawing code treats the torus as a cylinder
	cobj.norm      = dir.get_norm();
	coll_objects.set_coll_obj_props(index, COLL_TORUS, ro, ri, platform_id, cparams);
	add_coll_torus_to_matrix(index, dhcm);
	return index;
}


void add_coll_capsule_to_matrix(int index, int dhcm) {

	coll_obj &cobj(coll_objects[index]);
	coll_obj const orig_cobj(cobj); // make a copy of the cobj so that we can modify it then restore it
	cobj.type = ((cobj.is_cylin_vertical() && cobj.radius == cobj.radius2) ? COLL_CYLINDER : COLL_CYLINDER_ROT);
	vector3d const dir(cobj.points[1] - cobj.points[0]);
	float const len(dir.mag());
	assert(len > TOLERANCE);
	cobj.points[0] -= (cobj.radius /len)*dir; // extend cylinder out by radius to get a bounding cylinder for this capsule
	cobj.points[1] += (cobj.radius2/len)*dir;
	bool const swap_r(cobj.radius2 < cobj.radius);
	if (swap_r) {swap(cobj.radius, cobj.radius2);}
	
	if (cobj.radius < cobj.radius2) { // update radius values
		cobj.radius  *= max(0.0f, (len - cobj.radius))/len;
		cobj.radius2 *= (len + cobj.radius2)/len;
	}
	if (swap_r) {swap(cobj.radius, cobj.radius2);}
	cobj.calc_bcube();
	add_coll_cylinder_to_matrix(index, dhcm);
	cobj = orig_cobj; // restore the original
}

int add_coll_capsule(point const &p1, point const &p2, float radius, float radius2, cobj_params const &cparams, int platform_id, int dhcm) {

	assert(radius > 0.0 && radius2 > 0.0);
	assert(p1 != p2);
	int const index(cobj_manager.get_next_avail_index());
	coll_obj &cobj(coll_objects[index]);
	cobj.points[0] = p1;
	cobj.points[1] = p2;
	coll_objects.set_coll_obj_props(index, COLL_CAPSULE, radius, radius2, platform_id, cparams);
	add_coll_capsule_to_matrix(index, dhcm);
	return index;
}


void add_coll_sphere_to_matrix(int index, int dhcm) {

	int x1, x2, y1, y2;
	coll_obj const &cobj(coll_objects[index]);
	point const pt(cobj.points[0]);
	bool const is_dynamic(cobj.status == COLL_DYNAMIC);
	float const radius(cobj.radius);
	int const radx(int(radius*DX_VAL_INV) + 1), rady(int(radius*DY_VAL_INV) + 1);
	int const xpos(get_xpos(pt.x)), ypos(get_ypos(pt.y));
	get_params(x1, y1, x2, y2, cobj.d);
	int const rxry(radx*rady), crsq(radx*rady);

	for (int i = y1; i <= y2; ++i) {
		for (int j = x1; j <= x2; ++j) {
			int const distsq((i - ypos)*(i - ypos) + (j - xpos)*(j - xpos));

			if (distsq <= crsq) { // nasty offset by HALF_DXY to account for discretization error
				float const dz(sqrt(max(0.0f, (radius*radius - max(0.0f, (distsq*dxdy - HALF_DXY))))));
				add_coll_point(i, j, index, (pt.z-dz), (pt.z+dz), (distsq <= rxry), is_dynamic, dhcm);
			}
		}
	}
}


// doesn't work for ellipses when X != Y
int add_coll_sphere(point const &pt, float radius, cobj_params const &cparams, int platform_id, int dhcm, bool reflective) {

	radius = fabs(radius);
	int const index(cobj_manager.get_next_avail_index());
	coll_objects[index].points[0] = pt;
	coll_objects.set_coll_obj_props(index, COLL_SPHERE, radius, radius, platform_id, cparams);
	add_coll_sphere_to_matrix(index, dhcm);
	if (reflective) {add_reflective_cobj(index);}
	return index;
}


void add_coll_polygon_to_matrix(int index, int dhcm) { // coll_obj member function?

	int x1, x2, y1, y2;
	coll_obj const &cobj(coll_objects[index]);
	get_params(x1, y1, x2, y2, cobj.d);
	bool const is_dynamic(cobj.status == COLL_DYNAMIC);
	float const zminc(cobj.d[2][0]), zmaxc(cobj.d[2][1]); // thickness has already been added/subtracted

	if (cobj.thickness == 0.0 && (x2-x1) <= 1 && (y2-y1) <=1) { // small polygon
		for (int i = y1; i <= y2; ++i) {
			for (int j = x1; j <= x2; ++j) {
				add_coll_point(i, j, index, zminc, zmaxc, 1, is_dynamic, dhcm);
			}
		}
		return;
	}
	vector3d const norm(cobj.norm);
	float const dval(-dot_product(norm, cobj.points[0]));
	float const thick(0.5*cobj.thickness), dx(0.5*DX_VAL), dy(0.5*DY_VAL);
	float const dzx(norm.z == 0.0 ? 0.0 : DX_VAL*norm.x/norm.z), dzy(norm.z == 0.0 ? 0.0 : DY_VAL*norm.y/norm.z);
	float const delta_z(sqrt(dzx*dzx + dzy*dzy));
	vector<tquad_t> pts;
	if (cobj.thickness > MIN_POLY_THICK) {thick_poly_to_sides(cobj.points, cobj.npoints, norm, cobj.thickness, pts);}
	cube_t cube;
	cube.d[2][0] = zminc - SMALL_NUMBER;
	cube.d[2][1] = zmaxc + SMALL_NUMBER;

	for (int i = y1; i <= y2; ++i) {
		float const yv(get_yval(i));
		cube.d[1][0] = yv - dy - (i==y1)*thick;
		cube.d[1][1] = yv + dy + (i==y2)*thick;

		for (int j = x1; j <= x2; ++j) {
			float const xv(get_xval(j));
			float z1(zmaxc), z2(zminc);
			cube.d[0][0] = xv - dx - (j==x1)*thick;
			cube.d[0][1] = xv + dx + (j==x2)*thick;

			if (cobj.thickness > MIN_POLY_THICK) { // thick polygon	
				assert(!pts.empty());
				bool inside(0);

				for (unsigned k = 0; k < pts.size(); ++k) {
					point const *const p(pts[k].pts);
					vector3d const pn(get_poly_norm(p));

					if (fabs(pn.z) > 1.0E-3) { // ignore near-vertical polygon edges (for now)
						inside |= get_poly_zminmax(p, pts[k].npts, pn, -dot_product(pn, p[0]), cube, z1, z2);
					}
				}
				if (!inside) continue;
			}
			else if (!get_poly_zminmax(cobj.points, cobj.npoints, norm, dval, cube, z1, z2)) continue;
			// adjust z bounds so that they are for the entire cell x/y bounds, not a single point (conservative)
			z1 = max(zminc, (z1 - delta_z));
			z2 = min(zmaxc, (z2 + delta_z));
			add_coll_point(i, j, index, z1, z2, 1, is_dynamic, dhcm);
		} // for j
	} // for i
}

void set_coll_polygon(coll_obj &cobj, const point *points, int npoints, vector3d const &normal, float thickness) {

	assert(npoints >= 3 && points != NULL); // too strict?
	assert(npoints <= N_COLL_POLY_PTS);
	assert(normal  != zero_vector);
	cobj.norm = normal;
	for (int i = 0; i < npoints; ++i) {cobj.points[i] = points[i];}
	cobj.npoints   = npoints;
	cobj.thickness = thickness;
}

// must be planar, convex polygon with unique consecutive points
int add_coll_polygon(const point *points, int npoints, cobj_params const &cparams, float thickness, int platform_id, int dhcm) {

	assert(npoints >= 3 && points != NULL); // too strict?
	assert(npoints <= N_COLL_POLY_PTS);
	int const index(cobj_manager.get_next_avail_index());
	//if (thickness == 0.0) thickness = MIN_POLY_THICK;
	vector3d normal(get_poly_norm(points));

	if (npoints == 4) { // average the normal from both triangles in case they're not coplanar
		point const p2[3] = {points[0], points[2], points[3]};
		vector3d const norm2(get_poly_norm(p2));
		normal += ((dot_product(normal, norm2) < 0.0) ? -norm2 : norm2);
		normal.normalize();
	}
	if (normal == zero_vector) {
		cout << "degenerate polygon created: points:" << endl;
		for (int i = 0; i < npoints; ++i) {cout << points[i].str() << endl;}
		cout << "valid: " << is_poly_valid(points) << endl;
		normal = plus_z; // this shouldn't be possible, but FP accuracy/errors make this tough to prevent
	}
	set_coll_polygon(coll_objects[index], points, npoints, normal, thickness);
	float brad;
	point center; // unused
	polygon_bounding_sphere(points, npoints, thickness, center, brad);
	coll_objects.set_coll_obj_props(index, COLL_POLYGON, brad, 0.0, platform_id, cparams);
	add_coll_polygon_to_matrix(index, dhcm);
	return index;
}


// must be planar, convex polygon with unique consecutive points
int add_simple_coll_polygon(const point *points, int npoints, cobj_params const &cparams, vector3d const &normal, int dhcm) {

	int const index(cobj_manager.get_next_avail_index());
	set_coll_polygon(coll_objects[index], points, npoints, normal, 0.0);
	float brad;
	point center; // unused
	polygon_bounding_sphere(points, npoints, 0.0, center, brad);
	coll_objects.set_coll_obj_props(index, COLL_POLYGON, brad, 0.0, -1, cparams);
	add_coll_polygon_to_matrix(index, dhcm);
	return index;
}


void coll_obj::add_as_fixed_cobj() {

	calc_volume();
	fixed = 1;
	id    = add_coll_cobj();
}


int coll_obj::add_coll_cobj() {

	int cid(-1);
	set_bit_flag_to(cp.flags, COBJ_DYNAMIC, (status == COLL_DYNAMIC));

	switch (type) {
	case COLL_CUBE:
		cid = add_coll_cube(*this, cp, platform_id);
		break;
	case COLL_SPHERE:
		cid = add_coll_sphere(points[0], radius, cp, platform_id);
		break;
	case COLL_CYLINDER:
	case COLL_CYLINDER_ROT:
		cid = add_coll_cylinder(points[0], points[1], radius, radius2, cp, platform_id);
		break;
	case COLL_TORUS:
		cid = add_coll_torus(points[0], norm, radius, radius2, cp, platform_id);
		break;
	case COLL_CAPSULE:
		cid = add_coll_capsule(points[0], points[1], radius, radius2, cp, platform_id);
		break;
	case COLL_POLYGON:
		cid = add_coll_polygon(points, npoints, cp, thickness, platform_id);
		break;
	default: assert(0);
	}
	assert(cid >= 0 && size_t(cid) < coll_objects.size());
	coll_obj &cobj(coll_objects[cid]);
	cobj.destroy  = destroy;
	cobj.fixed    = fixed;
	cobj.group_id = group_id;
	cobj.v_fall   = v_fall;
	cobj.texture_offset = texture_offset;
	cobj.cgroup_id = cgroup_id;
	cobj.dgroup_id = dgroup_id;
	if (type == COLL_CUBE) {cobj.radius2 = radius2;} // copy corner radius
	if (cgroup_id >= 0) {cobj_groups.add_cobj(cgroup_id, cid);}
	if (is_reflective()) {reflective_cobjs.add_cobj(cid);}
	return cid;
}


void coll_obj::re_add_coll_cobj(int index, int remove_old) {

	if (!fixed) return;
	assert(index >= 0);
	assert(id == -1 || id == (int)index);
	if (remove_old) {remove_coll_object(id, 0);} // might already have been removed

	switch (type) {
	case COLL_CUBE:         add_coll_cube_to_matrix    (index, 0); break;
	case COLL_SPHERE:       add_coll_sphere_to_matrix  (index, 0); break;
	case COLL_CYLINDER:     add_coll_cylinder_to_matrix(index, 0); break;
	case COLL_CYLINDER_ROT: add_coll_cylinder_to_matrix(index, 0); break;
	case COLL_TORUS:        add_coll_torus_to_matrix   (index, 0); break;
	case COLL_CAPSULE:      add_coll_capsule_to_matrix (index, 0); break;
	case COLL_POLYGON:      add_coll_polygon_to_matrix (index, 0); break;
	default: assert(0);
	}
	cp.flags &= ~COBJ_DYNAMIC;
	status    = COLL_STATIC;
	counter   = 0;
	id        = index;
}

void coll_cell::clear(bool clear_vectors) {

	if (clear_vectors) {cvals.clear();}
	zmin =  FAR_DISTANCE;
	zmax = -FAR_DISTANCE;
}


void cobj_stats() {

	unsigned ncv(0), nonempty(0), ncobj(0);
	unsigned const csize((unsigned)coll_objects.size());

	for (int y = 0; y < MESH_Y_SIZE; ++y) {
		for (int x = 0; x < MESH_X_SIZE; ++x) {
			unsigned const sz((unsigned)v_collision_matrix[y][x].cvals.size());
			ncv += sz;
			nonempty += (sz > 0);
		}
	}
	for (unsigned i = 0; i < csize; ++i) {
		if (coll_objects[i].status == COLL_STATIC) ++ncobj;
	}
	if (ncobj > 0) {
		cout << "bins = " << XY_MULT_SIZE << ", ne = " << nonempty << ", cobjs = " << ncobj
			 << ", ent = " << ncv << ", per c = " << ncv/ncobj << ", per bin = " << ncv/XY_MULT_SIZE << endl;
	}
}


void add_coll_point(int i, int j, int index, float zminv, float zmaxv, int add_to_hcm, int is_dynamic, int dhcm) {

	assert(!point_outside_mesh(j, i));
	coll_cell &vcm(v_collision_matrix[i][j]);
	vcm.add_entry(index);
	coll_obj const &cobj(coll_objects.get_cobj(index));
	unsigned const size((unsigned)vcm.cvals.size());

	if (size > 1 && cobj.status == COLL_STATIC && coll_objects[vcm.cvals[size-2]].status == COLL_DYNAMIC) {
		std::rotate(vcm.cvals.begin(), vcm.cvals.begin()+size-1, vcm.cvals.end()); // rotate last point to first point???
	}
	if (is_dynamic) return;

	// update the z values if this cobj is part of a vertically moving platform
	// if it's a cube then it's handled in add_coll_cube_to_matrix()
	if (cobj.type != COLL_CUBE && cobj.platform_id >= 0) {
		platform const &pf(platforms.get_cobj_platform(cobj));

		if (pf.is_rotation()) {
			if (pf.get_rot_dir().z == 0.0) { // rotating around x/y axes, will go up and down
				// FIXME: unclear how to calculate this; use bounding sphere? case split on primitive type and sweep over all possible rotations (calling rotate_about())?
			}
		}
		else { // translation
			vector3d const range(pf.get_range());

			if (range.x == 0.0 && range.y == 0.0) { // vertical platform
				if (range.z > 0.0) {zmaxv += range.z;} // travels up
				else               {zminv += range.z;} // travels down
			}
		}
	}
	if (dhcm == 0 && add_to_hcm && h_collision_matrix[i][j] < zmaxv && (mesh_height[i][j] + 2.0*object_types[SMILEY].radius) > zminv) {
		h_collision_matrix[i][j] = zmaxv;
	}
	if (add_to_hcm || ALWAYS_ADD_TO_HCM) {
		vcm.update_zmm(zminv, zmaxv);

		if (!lm_alloc) { // if the lighting has already been computed, we can't change czmin/czmax/get_zval()/get_zpos()
			czmin = min(zminv, czmin);
			czmax = max(zmaxv, czmax);
		}
	}
}


int remove_coll_object(int index, bool reset_draw) {

	if (index < 0) return 0;
	coll_obj &c(coll_objects.get_cobj(index));

	if (c.status == COLL_UNUSED) {
		assert(REMOVE_ALL_COLL);
		return 0;
	}
	if (c.status == COLL_FREED) return 0;
	coll_objects.remove_index_from_ids(index);
	if (reset_draw) {c.cp.draw = 0;}
	c.status   = COLL_FREED;
	c.waypt_id = -1; // is this necessary?
	
	if (c.status == COLL_STATIC) {
		//free_index(index); // can't do this here - object's collision id needs to be held until purge
		++cobj_manager.cobjs_removed;
		return 0;
	}
	int x1, y1, x2, y2;
	get_params(x1, y1, x2, y2, c.d);

	for (int i = y1; i <= y2; ++i) {
		for (int j = x1; j <= x2; ++j) {
			vector<int> &cvals(v_collision_matrix[i][j].cvals);
			unsigned const num_cvals(cvals.size());
			
			for (unsigned k = 0; k < num_cvals; ++k) {
				if (cvals[k] == index) {
					cvals.erase(cvals.begin()+k); // can't change zmin or zmax (I think)
					break; // should only be in here once
				}
			}
		}
	}
	cobj_manager.free_index(index);
	return 1;
}


int remove_reset_coll_obj(int &index) {

	int const retval(remove_coll_object(index));
	index = -1;
	return retval;
}


void purge_coll_freed(bool force) {

	if (!force && cobj_manager.cobjs_removed < PURGE_THRESH) return;
	//RESET_TIME;

	for (int i = 0; i < MESH_Y_SIZE; ++i) {
		for (int j = 0; j < MESH_X_SIZE; ++j) {
			bool changed(0);
			coll_cell &vcm(v_collision_matrix[i][j]);
			unsigned const size((unsigned)vcm.cvals.size());

			for (unsigned k = 0; k < size && !changed; ++k) {
				if (coll_objects[vcm.cvals[k]].freed_unused()) changed = 1;
			}
			// Note: don't actually have to recalculate zmin/zmax unless a removed object was on the top or bottom of the coll cell
			if (!changed) continue;
			vcm.zmin = mesh_height[i][j];
			vcm.zmax = zmin;
			vector<int>::const_iterator in(vcm.cvals.begin());
			vector<int>::iterator o(vcm.cvals.begin());

			for (; in != vcm.cvals.end(); ++in) {
				coll_obj &cobj(coll_objects[*in]);

				if (!cobj.freed_unused()) {
					if (cobj.status == COLL_STATIC) {vcm.update_zmm(cobj.d[2][0], cobj.d[2][1]);}
					*o++ = *in;
				}
			}
			vcm.cvals.erase(o, vcm.cvals.end()); // excess capacity?
			h_collision_matrix[i][j] = vcm.zmax; // need to think about add_to_hcm...
		}
	}
	unsigned const ncobjs((unsigned)coll_objects.size());

	for (unsigned i = 0; i < ncobjs; ++i) {
		if (coll_objects[i].status == COLL_FREED) cobj_manager.free_index(i);
	}
	cobj_manager.cobjs_removed = 0;
	//PRINT_TIME("Purge");
}


void remove_all_coll_obj() {

	camera_coll_id = -1; // camera is special - keeps state

	for (int i = 0; i < MESH_Y_SIZE; ++i) {
		for (int j = 0; j < MESH_X_SIZE; ++j) {
			v_collision_matrix[i][j].clear(1);
			h_collision_matrix[i][j] = mesh_height[i][j];
		}
	}
	for (unsigned i = 0; i < coll_objects.size(); ++i) {
		if (coll_objects[i].status != COLL_UNUSED) {
			coll_objects.remove_index_from_ids(i);
			cobj_manager.free_index(i);
		}
	}
	free_all_coll_objects();
}


void coll_obj_group::set_coll_obj_props(int index, int type, float radius, float radius2, int platform_id, cobj_params const &cparams) {
	
	coll_obj &cobj(at(index)); // Note: this is the *only* place a new cobj is allocated/created
	cobj.texture_offset = zero_vector;
	cobj.cp          = cparams;
	cobj.id          = index;
	cobj.radius      = radius;
	cobj.radius2     = radius2;
	cobj.v_fall      = 0.0;
	cobj.type        = type;
	cobj.platform_id = platform_id;
	cobj.group_id    = -1;
	cobj.fixed       = 0;
	cobj.counter     = 0;
	cobj.destroy     = 0;
	cobj.coll_type   = 0;
	cobj.last_coll   = 0;
	cobj.is_billboard= 0;
	cobj.falling     = 0;
	cobj.setup_internal_state();
	if (cparams.flags & COBJ_DYNAMIC) {dynamic_ids.must_insert(index);}
	if (cparams.draw    ) {drawn_ids.must_insert   (index);}
	if (platform_id >= 0) {platform_ids.must_insert(index);}
	if ((type == COLL_CUBE || type == COLL_SPHERE) && cparams.light_atten != 0.0) {has_lt_atten = 1;}
	if (cparams.cobj_type == COBJ_TYPE_VOX_TERRAIN) {has_voxel_cobjs = 1;}
}


void coll_obj_group::remove_index_from_ids(int index) {

	if (index < 0)  return;
	coll_obj &cobj(at(index));
	if (cobj.fixed) return; // won't actually be freed
	if (cobj.status == COLL_DYNAMIC) {coll_objects.dynamic_ids.must_erase (index);}
	if (cobj.cp.draw               ) {coll_objects.drawn_ids.must_erase   (index);}
	if (cobj.platform_id >= 0      ) {coll_objects.platform_ids.must_erase(index);}
	if (cobj.cgroup_id >= 0)         {cobj_groups.remove_cobj(cobj.cgroup_id, index);}
	cobj.cp.draw     = 0;
	cobj.platform_id = cobj.cgroup_id = cobj.dgroup_id = -1;
}

void coll_obj_group::set_cur_draw_stream_from_drawn_ids() {
	vector<unsigned> &to_draw(get_cur_draw_stream());
	to_draw.clear();
	to_draw.insert(to_draw.end(), drawn_ids.begin(), drawn_ids.end());
}


// only works for mesh collisions
int collision_detect_large_sphere(point &pos, float radius, unsigned flags) {

	int const xpos(get_xpos(pos.x)), ypos(get_ypos(pos.y));
	float const rdx(radius*DX_VAL_INV), rdy(radius*DY_VAL_INV);
	int const crdx((int)ceil(rdx)), crdy((int)ceil(rdy));
	int const x1(max(0, (xpos - (int)rdx))), x2(min(MESH_X_SIZE-1, (xpos + crdx)));
	int const y1(max(0, (ypos - (int)rdy))), y2(min(MESH_Y_SIZE-1, (ypos + crdy)));
	if (x1 > x2 || y1 > y2) return 0; // not sure if this can ever be false
	int const rsq(crdx*crdy);
	float const rsqf(radius*radius);
	bool const z_up(!(flags & (Z_STOPPED | FLOATING)));
	int coll(0);

	for (int i = y1; i <= y2; ++i) {
		float const yval(get_yval(i));
		int const y_dist((i - ypos)*(i - ypos));
		if (y_dist > rsq) continue; // can never satisfy condition below

		for (int j = x1; j <= x2; ++j) {
			if (y_dist + (j - xpos)*(j - xpos) > rsq) continue;
			point const mpt(get_xval(j), yval, mesh_height[i][j]);
			vector3d const v(pos, mpt);
			float const mag_sq(v.mag_sq());
			if (mag_sq >= rsqf) continue;
			float const old_z(pos.z), mag(sqrt(mag_sq));
			pos = mpt;
			if (mag < TOLERANCE) {pos.x += radius;} // avoid divide by zero, choose x-direction (arbitrary)
			else {pos += v*(radius/mag);}
			if (!z_up && pos.z >= old_z) pos.z = old_z;
			coll = 1; // return 1; ?
		}
	}
	//assert(!is_nan(pos));
	return coll;
}


int check_legal_move(int x_new, int y_new, float zval, float radius, int &cindex) { // not dynamically updated

	if (point_outside_mesh(x_new, y_new)) return 0; // object out of simulation region
	coll_cell const &cell(v_collision_matrix[y_new][x_new]);
	if (cell.cvals.empty()) return 1;
	float const xval(get_xval(x_new)), yval(get_yval(y_new)), z1(zval - radius), z2(zval + radius);
	point const pval(xval, yval, zval);

	for (int k = (int)cell.cvals.size()-1; k >= 0; --k) { // iterate backwards
		int const index(cell.cvals[k]);
		if (index < 0) continue;
		coll_obj &cobj(coll_objects.get_cobj(index));
		if (cobj.no_collision()) continue;
		if (cobj.status == COLL_STATIC) {
			if (z1 > cell.zmax || z2 < cell.zmin) return 1; // should be OK here since this is approximate, not quite right with, but not quite right without
		}
		else continue; // smileys collision with dynamic objects can be handled by check_vert_collision()
		if (z1 > cobj.d[2][1] || z2 < cobj.d[2][0]) continue;
		bool coll(0);
		
		switch (cobj.type) {
		case COLL_CUBE:
			coll = ((pval.x + radius) >= cobj.d[0][0] && (pval.x - radius) <= cobj.d[0][1] && (pval.y + radius) >= cobj.d[1][0] && (pval.y - radius) <= cobj.d[1][1]);
			break;
		case COLL_SPHERE:
			coll = dist_less_than(pval, cobj.points[0], (cobj.radius + radius));
			break;
		case COLL_CYLINDER:
			coll = dist_xy_less_than(pval, cobj.points[0], (cobj.radius + radius));
			break;
		case COLL_CYLINDER_ROT:
			coll = sphere_int_cylinder_sides(pval, radius, cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2);
			break;
		case COLL_TORUS:
			coll = sphere_torus_intersect(pval, radius, cobj.points[0], cobj.norm, cobj.radius2, cobj.radius);
			break;
		case COLL_CAPSULE:
			coll = (dist_less_than(pval, cobj.points[0], (cobj.radius + radius)) || dist_less_than(pval, cobj.points[1], (cobj.radius2 + radius)));
			if (!coll && cobj.is_cylin_vertical() && cobj.radius == cobj.radius2) { // vertical, constant radius
				coll = dist_xy_less_than(pval, cobj.points[0], (cobj.radius + radius));
			}
			else if (!coll) { // non-vertical or variable radius
				coll = sphere_int_cylinder_sides(pval, radius, cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2);
			}
			break;
		case COLL_POLYGON: { // must be coplanar
			float thick, rdist;

			if (sphere_ext_poly_int_base(cobj.points[0], cobj.norm, pval, radius, cobj.thickness, thick, rdist)) {
				point const pos(pval - cobj.norm*rdist);
				assert(cobj.npoints > 0);
				coll = planar_contour_intersect(cobj.points, cobj.npoints, pos, cobj.norm);
			}
			break;
		}
		default: assert(0);
		}
		if (coll) {
			cindex = index;
			return 0;
		}
	}
	return 1;
}


// Note: only used for waypoints
bool is_point_interior(point const &pos, float radius) { // is query point interior to mesh, cobjs, or voxels

	if (!is_over_mesh(pos)) return 0; // off scene bounds, outside
	if (is_under_mesh(pos)) return 1; // under mesh, inside
	if (pos.z >= czmax) return 0; // above all cobjs (and mesh), outside
	int inside_count(0);
	float const scene_radius(get_scene_radius());

	for (int z = -1; z <= 1; ++z) {
		for (int y = -1; y <= 1; ++y) {
			for (int x = -1; x <= 1; ++x) {
				if (x == 0 && y == 0 && z == 0) continue;
				vector3d const dir(x, y, z);
				point const pos2(pos + dir*scene_radius);
				point cpos;
				vector3d cnorm;
				int cindex;

				if (check_coll_line_exact(pos, pos2, cpos, cnorm, cindex, 0.0, -1, 0, 0, 1)) {
					coll_obj const &cobj(coll_objects.get_cobj(cindex));
					if (radius > 0.0 && cobj.line_intersect(pos, (pos + dir*radius))) return 1; // intersects a short line, so inside/close to a cobj

					if (cobj.type == COLL_CUBE) { // cube
						unsigned const dim(get_max_dim(cnorm)); // cube intersection dim
						bool const idir(cnorm[dim] > 0); // cube intersection dir
						inside_count += ((cobj.cp.surfs & EFLAGS[dim][idir]) ? 1 : -1); // inside if cube side is disabled
					}
					else if (cobj.type == COLL_POLYGON && cobj.thickness <= MIN_POLY_THICK) { // planar/thin polygon
						inside_count += ((dot_product(cnorm, cobj.norm) > 0.0) ? 1 : -1); // inside if hit the polygon back face
					}
					else {
						--inside_count; // sphere, cylinder, cone, or extruded polygon: assume outside
					}
				}
				else {
					--inside_count; // no collision, assume outside
				}
			}
		}
	}
	return (inside_count > 0);
}


// ************ begin vert_coll_detector ************


bool dwobject::proc_stuck(bool static_top_coll) {

	float const friction(object_types[type].friction_factor);
	if (friction < 2.0*STICK_THRESHOLD || friction < rand_uniform(2.0, 3.0)*STICK_THRESHOLD) return 0;
	flags |= (static_top_coll ? ALL_COLL_STOPPED : XYZ_STOPPED); // stuck in coll object
	status = 4;
	return 1;
}


bool vert_coll_detector::safe_norm_div(float rad, float radius, vector3d &norm) {

	if (fabs(rad) < 10.0*TOLERANCE) {
		obj.pos.x += radius; // arbitrary
		norm.assign(1.0, 0.0, 0.0);
		return 0;
	}
	return 1;
}


void vert_coll_detector::check_cobj(int index) {

	coll_obj const &cobj(coll_objects[index]);
	if (cobj.no_collision())                         return; // collisions are disabled for this cobj
	if (type == PROJC    && obj.source  == cobj.id)  return; // can't shoot yourself with a projectile
	if (player           && obj.coll_id == cobj.id)  return; // can't collide with yourself
	if (type == LANDMINE && obj.invalid_coll(cobj))  return;
	if (skip_dynamic && cobj.status == COLL_DYNAMIC) return;
	if (only_drawn   && !cobj.cp.might_be_drawn())   return;
	
	if (type == SMOKE) { // special case to allow smoke to pass through small spheres and cylinders
		switch (cobj.type) { // Note: cubes and polygons could be split into many small pieces, so we don't check their sizes
		case COLL_CYLINDER:
		case COLL_CYLINDER_ROT:
		case COLL_TORUS:
			if (cobj.radius2 < 0.25*object_types[type].radius) return;
		case COLL_SPHERE: // fallthrough from above
			if (cobj.radius  < 0.25*object_types[type].radius) return;
			break;
		}
	}
	if (z1 > cobj.d[2][1] || z2 < cobj.d[2][0]) return;
	if (pos.x < (cobj.d[0][0]-o_radius) || pos.x > (cobj.d[0][1]+o_radius)) return;
	if (pos.y < (cobj.d[1][0]-o_radius) || pos.y > (cobj.d[1][1]+o_radius)) return;
	bool const player_step(player && ((type == CAMERA && camera_change) || (cobj.d[2][1] - z1) <= o_radius*C_STEP_HEIGHT));
	check_cobj_intersect(index, 1, player_step);

	if (type == CAMERA) {
		if (camera_zh > 0.0) {
			unsigned const nsteps((unsigned)ceil(camera_zh/o_radius));
			float const step_sz(camera_zh/nsteps), pz(pos.z), opz(obj.pos.z), poz(pold.z);

			for (unsigned i = 1; i <= nsteps; ++i) {
				float const step(i*step_sz);
				pos.z     = pz  + step;
				obj.pos.z = opz + step;
				pold.z    = poz + step;
				if (pos.z-o_radius > cobj.d[2][1] || pos.z+o_radius < cobj.d[2][0]) continue;
				check_cobj_intersect(index, 0, 0);
			}
			pos.z     = pz;
			obj.pos.z = opz;
			pold.z    = poz;
		}
		//if (game_mode && sstates != NULL) {} // collision detection for player weapon?
	}
}


float decal_dist_to_cube_edge(cube_t const &cube, point const &pos, int dim) {
	int const ds((dim+1)%3), dt((dim+2)%3);
	return min(min((cube.d[ds][1] - pos[ds]), (pos[ds] - cube.d[ds][0])), min((cube.d[dt][1] - pos[dt]), (pos[dt] - cube.d[dt][0])));
}

bool decal_contained_in_union_cube_face(coll_obj const &cobj, point const &pos, vector3d const &norm, float radius, int dim) {
	
	// check for decal contained in union of surfaces of nearby cubes (to handle split cube)
	bool const dir(norm[dim] > 0.0);
	int const ds((dim+1)%3), dt((dim+2)%3);
	vector<unsigned> cobjs;
	cube_t bcube;
	bcube.set_from_sphere(pos, radius); // should really be disk in direction dir
	get_intersecting_cobjs_tree(bcube, cobjs, -1, 0.0, 0, 0); // duplicates should be okay
	if (cobjs.size() < 2) return 0; // not split cobj case
	float area_covered(0.0);

	for (unsigned i = 0; i < cobjs.size(); ++i) {
		coll_obj const &c(coll_objects.get_cobj(cobjs[i]));
		if (c.type != COLL_CUBE || c.may_be_dynamic()) continue;
		if (fabs(cobj.d[dim][dir] - c.d[dim][dir]) > 0.01*radius) continue; // faces not aligned
		float const s1(max((pos[ds] - radius), c.d[ds][0])), t1(max((pos[dt] - radius), c.d[dt][0]));
		float const s2(min((pos[ds] + radius), c.d[ds][1])), t2(min((pos[dt] + radius), c.d[dt][1]));
		area_covered += (s2 - s1)*(t2 - t1);
	}
	return (area_covered > 0.99*(4.0*radius*radius));
}

bool decal_contained_in_cobj(coll_obj const &cobj, point const &pos, vector3d const &norm, float radius, int dim) {

	if (cobj.type == COLL_CUBE) {
		if (decal_dist_to_cube_edge(cobj, pos, dim) > radius) return 1; // decal contained in cube surface
		if (/*radius > 0.01 &&*/ decal_contained_in_union_cube_face(cobj, pos, norm, radius, dim)) return 1;
	}
	vector3d vab[2];
	get_ortho_vectors(norm, vab);
	int cindex(-1); // unused

	for (unsigned d = 0; d < 4; ++d) {
		point const p0(pos + ((d & 1) ? 1.0 : -1.0)*radius*vab[d>>1]);
		point const p1(p0 + 2.0*DECAL_OFFSET*norm), p2(p0 - 2.0*DECAL_OFFSET*norm); // move behind the decal into the cobj
		if (cobj.line_intersect(p1, p2) || check_coll_line(p1, p2, cindex, cobj.id, 1, 0, 0)) continue; // exclude voxels
		return 0;
	}
	return 1;
}


void gen_explosion_decal(point const &pos, float radius, vector3d const &coll_norm, coll_obj const &cobj, int dim, colorRGBA const &color) { // not sure this belongs here

	if (!cobj.has_flat_top_bot() && cobj.type != COLL_CYLINDER_ROT) return;
	if (!cobj.can_be_scorched()) return;
	float const sz(5.0*radius*rand_uniform(0.8, 1.2));
	float max_sz(sz);

	if (cobj.type == COLL_CUBE) {
		max_sz = decal_dist_to_cube_edge(cobj, pos, dim); // only compute max_sz for cubes; other cobjs use sz or fail
		if (max_sz < sz && decal_contained_in_union_cube_face(cobj, pos, coll_norm, sz, dim)) {max_sz = sz;} // check for full sized decal contained in split cubes
	}
	else if (cobj.type == COLL_POLYGON) {max_sz = min_dist_from_pt_to_polygon_edge(pos, cobj.points, cobj.npoints);} // may not be correct for extruded polygons
	if (max_sz > 0.5*sz) {gen_decal(pos, min(sz, max_sz), coll_norm, FLARE3_TEX, cobj.id, colorRGBA(color, 0.75), 0, 1, 240*TICKS_PER_SECOND);} // explosion (4 min.)
}


bool get_sphere_poly_int_val(point const &sc, float sr, point const *const points, unsigned npoints, vector3d const &normal, float thickness, float &val, vector3d &cnorm) {

	// compute normal based on extruded sides
	vector<tquad_t> pts;
	thick_poly_to_sides(points, npoints, normal, thickness, pts);
	if (!sphere_intersect_poly_sides(pts, sc, sr, val, cnorm, 1)) return 0; // no collision
	bool intersects(0), inside(1);
						
	for (unsigned i = 0; i < pts.size(); ++i) { // inefficient and inexact, but closer to correct
		vector3d const norm2(pts[i].get_norm());
		float rdist2(dot_product_ptv(norm2, sc, points[0]));
							
		if (sphere_poly_intersect(pts[i].pts, pts[i].npts, sc, norm2, rdist2, sr)) {
			intersects = 1;
			break;
		}
		if (rdist2 > 0.0) {inside = 0;}
	}
	return (intersects || inside); // intersects some face or is completely inside
}


bool sphere_sphere_int(point const &sc1, point const &sc2, float sr1, float sr2, vector3d &cnorm, point &new_sc) {
	
	float dist_sq(p2p_dist_sq(sc1, sc2)), radius(sr1 + sr2);
	if (dist_sq > radius*radius) return 0;
	cnorm  = ((dist_sq == 0.0) ? plus_z : (sc1 - sc2)/sqrt(dist_sq)); // arbitrarily choose +z to avoid divide-by-zero
	new_sc = sc2 + cnorm*radius;
	return 1;
}


bool coll_obj::sphere_intersects_exact(point const &sc, float sr, vector3d &cnorm, point &new_sc) const {

	switch (type) { // within bounding box of collision object
	case COLL_CUBE: {
		unsigned cdir(0); // unused
		return sphere_cube_intersect(sc, sr, *this, sc, new_sc, cnorm, cdir, 0);
	}
	case COLL_SPHERE:
		return sphere_sphere_int(sc, points[0], sr, radius, cnorm, new_sc);
	case COLL_CYLINDER:
	case COLL_CYLINDER_ROT:
		return sphere_intersect_cylinder_ipt(sc, sr, points[0], points[1], radius, radius2, !(cp.surfs & 1), new_sc, cnorm, 1);
	case COLL_TORUS:
		return sphere_torus_intersect(sc, sr, points[0], norm, radius2, radius, new_sc, cnorm, 1);
	case COLL_CAPSULE:
		if (sphere_sphere_int(sc, points[0], sr, radius,  cnorm, new_sc)) return 1;
		if (sphere_sphere_int(sc, points[1], sr, radius2, cnorm, new_sc)) return 1;
		return sphere_intersect_cylinder_ipt(sc, sr, points[0], points[1], radius, radius2, 0, new_sc, cnorm, 1);
	case COLL_POLYGON: { // must be coplanar
		float thick, rdist;
		cnorm = norm;
		if (dot_product_ptv(cnorm, sc, points[0]) < 0.0) {cnorm.negate();} // sc or points[0]?
		if (!sphere_ext_poly_int_base(points[0], cnorm, sc, sr, thickness, thick, rdist)) return 0;
		if (!sphere_poly_intersect(points, npoints, sc, cnorm, rdist, max(0.0f, (thick - MIN_POLY_THICK)))) return 0;
		float val;

		if (thickness > MIN_POLY_THICK) { // compute norm based on extruded sides
			if (!get_sphere_poly_int_val(sc, sr, points, npoints, norm, thickness, val, cnorm)) return 0;
		}
		else {val = 1.01*(thick - rdist);} // non-thick polygon
		new_sc += cnorm*val; // calculate intersection point
		return 1;
	}
	default: assert(0);
	} // switch
	return 0;
}


void coll_obj::convert_cube_to_ext_polygon() {

	assert(type == COLL_CUBE);
	type      = COLL_POLYGON;
	npoints   = 4;
	thickness = get_dz(); // height
	assert(thickness > 0.0);
	norm      = plus_z;
	cp.surfs  = 0;
	cp.flags |= COBJ_WAS_CUBE;
	if (cp.tscale != 0.0) {texture_offset += get_llc();}
	float const z(0.5*(d[2][1] + d[2][0]));
	for (unsigned i = 0; i < 4; ++i) {points[i].assign(d[0][(i>>1)^(i&1)], d[1][i>>1], z);} // CCW: {x1,y1 x2,y1 x2,y2 x1,y2}
	// Note: bounding cube, volume, area, vcm, cobj bvh, etc. remain unchanged
}


void vert_coll_detector::check_cobj_intersect(int index, bool enable_cfs, bool player_step) {

	coll_obj const &cobj(coll_objects[index]);
	if (skip_movable && cobj.is_movable()) return;
	if (obj.type == GRASS && cobj.cp.coll_func == leafy_plant_collision) return; // no grass collision with leafy plants (can coexist with grass)
	
	if (cobj.type == COLL_CUBE || cobj.type == COLL_CYLINDER) {
		if (o_radius > 0.9*LARGE_OBJ_RAD && !sphere_cube_intersect(pos, o_radius, cobj)) return;
	}
	if (type == LEAF && cobj.is_tree_leaf()) {
		return; // skip leaf-leaf collisions, since they don't really make sense in the real world (leaves will bend to allow another leaf to fall)
	}
	vector3d norm(zero_vector), pvel(zero_vector);
	bool coll_top(0), coll_bot(0);
	if (cobj.platform_id >= 0) {pvel = platforms.get_cobj_platform(cobj).get_velocity();} // calculate platform velocity
	vector3d const mdir(motion_dir - pvel*fticks); // not sure if this helps
	float zmaxc(cobj.d[2][1]), zminc(cobj.d[2][0]);
	point const orig_pos(obj.pos);

	switch (cobj.type) { // within bounding box of collision object
	case COLL_CUBE: {
		if (!sphere_cube_intersect(pos, o_radius, cobj, (pold - mdir), obj.pos, norm, cdir, 0)) break; // shouldn't get here much when this fails
		coll_top = (cdir == 5);
		coll_bot = (cdir == 4);
		lcoll    = 1;

		if (!coll_top && !coll_bot && player_step) { // can step up onto the object
			lcoll   = 0;
			obj.pos = pos; // reset pos
			norm    = zero_vector;
			break;
		}
		if (coll_top) { // +z collision
			if (cobj.contains_pt_xy(pos)) {lcoll = 2;}
			float const rdist(max(max(max((pos.x-(cobj.d[0][1]+o_radius)), ((cobj.d[0][0]-o_radius)-pos.x)),
				(pos.y-(cobj.d[1][1]+o_radius))), ((cobj.d[1][0]-o_radius)-pos.y)));
				
			if (rdist > 0.0) {
				obj.pos.z -= o_radius;
				if (o_radius > rdist) {obj.pos.z += sqrt(o_radius*o_radius - rdist*rdist);}
			}
			break;
		}
		break;
	}
	case COLL_SPHERE: {
		float const radius(cobj.radius + o_radius);
		float rad(p2p_dist_sq(pos, cobj.points[0])), reff(radius);
		if (player && cobj.cp.coll_func == landmine_collision) {reff += 1.5*object_types[type].radius;} // landmine
		if (type == LANDMINE && cobj.is_player()) {reff += 1.5*object_types[unsigned(cobj.type)].radius;} // landmine
		
		if (rad <= reff*reff) {
			lcoll = 1;
			rad   = sqrt(rad);
			if (!safe_norm_div(rad, radius, norm)) break;
			norm  = (pos - cobj.points[0])/rad;
			if (rad <= radius) {obj.pos = cobj.points[0] + norm*radius;}
		}
		break;
	}
	case COLL_CYLINDER: {
		point const center(cobj.get_center_pt());
		float rad(p2p_dist_xy_sq(pos, center)), radius(cobj.radius); // rad is xy dist

		if (rad <= (radius + o_radius)*(radius + o_radius)) {
			rad    = sqrt(rad);
			lcoll  = 1;
			zmaxc += o_radius;
			zminc -= o_radius;
			float const pozm(pold.z - mdir.z);

			if (!(cobj.cp.surfs & 1) && pozm > (zmaxc - SMALL_NUMBER) && pos.z <= zmaxc) { // collision with top
				if (rad <= radius) {lcoll = 2;}
				norm.assign(0.0, 0.0, 1.0);
				float const rdist(rad - radius);
				obj.pos.z = zmaxc;
				coll_top  = 1;
					
				if (rdist > 0.0) {
					obj.pos.z -= o_radius;
					if (o_radius >= rdist) {obj.pos.z += sqrt(o_radius*o_radius - rdist*rdist);}
				}
			}
			else if (!(cobj.cp.surfs & 1) && pozm < (zminc + SMALL_NUMBER) && pos.z >= zminc) { // collision with bottom
				norm.assign(0.0, 0.0, -1.0);
				obj.pos.z = zminc - o_radius;
				coll_bot  = 1;
			}
			else { // collision with side
				if (player_step) {
					norm = plus_z;
					break; // OK, can step up onto cylinder
				}
				radius += o_radius;
				if (!safe_norm_div(rad, radius, norm)) break;
				//float const objz(obj.pos.z);
				norm.assign((pos.x - center.x)/rad, (pos.y - center.y)/rad, 0.0);
				for (unsigned d = 0; d < 2; ++d) {obj.pos[d] = center[d] + norm[d]*radius;}

				/*if (objz > (zmaxc - o_radius) && objz < zmaxc) { // hit on the top edge
					obj.pos.x -= norm.x*o_radius;
					obj.pos.y -= norm.y*o_radius;
					norm.z += (objz - (zmaxc - o_radius))/o_radius; // denominator isn't exactly correct
					norm.normalize();
					obj.pos += norm*o_radius;
				}
				else if (objz < (zminc + o_radius) && objz > zminc) { // hit on the bottom edge
					obj.pos.x -= norm.x*o_radius;
					obj.pos.y -= norm.y*o_radius;
					norm.z -= ((zminc + o_radius) - objz)/o_radius; // denominator isn't exactly correct
					norm.normalize();
					obj.pos += norm*o_radius;
				}*/
			}
		}
		break;
	}
	case COLL_CYLINDER_ROT:
		if (sphere_intersect_cylinder_ipt(pos, o_radius, cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2, !(cobj.cp.surfs & 1), obj.pos, norm, 1)) {lcoll = 1;}
		break;

	case COLL_TORUS:
		if (sphere_torus_intersect(pos, o_radius, cobj.points[0], cobj.norm, cobj.radius2, cobj.radius, obj.pos, norm, 1)) {lcoll = 1;}
		break;

	case COLL_CAPSULE: {
		if (sphere_sphere_int(pos, cobj.points[0], o_radius, cobj.radius,  norm, obj.pos)) {lcoll = 1;}
		if (sphere_sphere_int(pos, cobj.points[1], o_radius, cobj.radius2, norm, obj.pos)) {lcoll = 1;}
		if (sphere_intersect_cylinder_ipt(pos, o_radius, cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2, 0, obj.pos, norm, 1)) {lcoll = 1;}
		break;
	}
	case COLL_POLYGON: { // must be coplanar
		float thick, rdist;
		norm = cobj.norm;
		if (dot_product_ptv(norm, (pold - mdir), cobj.points[0]) < 0.0) {norm.negate();} // pos or cobj.points[0]?

		if (sphere_ext_poly_int_base(cobj.points[0], norm, pos, o_radius, cobj.thickness, thick, rdist)) {
			//if (rdist < 0) {rdist = -rdist; norm.negate();}

			if (sphere_poly_intersect(cobj.points, cobj.npoints, pos, norm, rdist, max(0.0f, (thick - MIN_POLY_THICK)))) {
				float val;

				if (cobj.thickness > MIN_POLY_THICK) { // compute norm based on extruded sides
					if (!get_sphere_poly_int_val(pos, o_radius, cobj.points, cobj.npoints, cobj.norm, cobj.thickness, val, norm)) break;
				}
				else {val = 1.01*(thick - rdist);} // non-thick polygon

				if (fabs(norm.z) < 0.5 && player_step) { // more horizontal than vertical edge
					norm = zero_vector;
					break; // can step up onto the object
				}
				assert(!is_nan(norm));
				obj.pos += norm*val; // calculate intersection point
				lcoll    = (norm.z > 0.99) ? 2 : 1; // top collision if normal is nearly vertical
			} // end sphere poly int
		} // end sphere int check
		break;
	} // end COLL_POLY scope
	default: assert(0);
	} // switch
	if (!lcoll) return; // no collision
	if (!coll_top && !coll_bot && (type == CAMERA || type == SMILEY)) {proc_movable_cobj(orig_pos, obj.pos, index, type);} // try to move object
	assert(norm != zero_vector);
	assert(!is_nan(norm));
	bool is_moving(0);
	obj_type const &otype(object_types[type]);
	float const friction(otype.friction_factor*((obj.flags & FROZEN_FLAG) ? 0.5 : 1.0)); // frozen objects have half friction
	bool const is_coll_func_pass(do_coll_funcs && enable_cfs && iter == 0);

	// collision with the top of a cube attached to a platform (on first iteration only)
	if (cobj.platform_id >= 0) {
		platform const &pf(platforms.get_cobj_platform(cobj));
		is_moving = (lcoll == 2 || friction >= STICK_THRESHOLD);

		if (animate2 && is_coll_func_pass) {
			if (is_moving) {obj.pos += pf.get_last_delta();} // move with the platform (clip v if large -z?)
			// the coll_top part isn't really right - we want to check for collsion with another object above
			else if ((coll_bot && pf.get_last_delta().z < 0.0) /*|| (coll_top && pf.get_last_delta().z > 0.0)*/) {
				if (player) {
					int const ix((type == CAMERA) ? CAMERA_ID : obj_index);
					smiley_collision(ix, NO_SOURCE, -plus_z, pos, 2000.0, CRUSHED); // lots of damage
				} // other objects?
			}
		}
		// reset last pos (init_dir) if object is only moving on a platform
		bool const platform_moving(pf.is_moving());
		//if (type == BALL && platform_moving) {obj.init_dir = obj.pos;}
		if (platform_moving) {obj.flags |= PLATFORM_COLL;}
	}
	if (player && is_coll_func_pass && cobj.cp.damage != 0.0) {
		int const ix((type == CAMERA) ? CAMERA_ID : obj_index);
		smiley_collision(ix, NO_SOURCE, norm, pos, fticks*cobj.cp.damage, COLLISION); // damage can be positive or negative
	}
	if (animate2 && !player && obj.health <= 0.1) {obj.disable();}
	vector3d v_old(zero_vector), v0(obj.velocity);
	bool const static_top_coll(lcoll == 2 && cobj.truly_static());
	point const decal_pos(obj.pos); // capture actual coll pos for use in decals before applying friction stick adjustment

	if (is_moving || friction < STICK_THRESHOLD) {
		v_old = obj.velocity;

		if (otype.elasticity == 0.0 || cobj.cp.elastic == 0.0 || (obj.flags & IS_CUBE_FLAG) || !obj.object_bounce(3, norm, cobj.cp.elastic, 0.0, pvel)) {
			if (static_top_coll) {
				obj.flags |= STATIC_COBJ_COLL; // collision with top
				if (otype.flags & OBJ_IS_DROP) {obj.velocity = zero_vector;}
			}
			if (type != DYNAM_PART && obj.velocity != zero_vector) {
				assert(TIMESTEP > 0.0);
				float friction_adj(friction);
				if (norm.z > 0.25 && (cobj.is_wet() || cobj.is_snow_cov())) {friction_adj *= 0.25;} // slippery when wet, icy, or snow covered
				if (friction_adj > 0.0) {obj.velocity *= (1.0 - min(1.0f, (tstep/TIMESTEP)*friction_adj));} // apply kinetic friction
				//for (unsigned i = 0; i < 3; ++i) {obj.velocity[i] *= (1.0 - fabs(norm[i]));} // norm must be normalized
				orthogonalize_dir(obj.velocity, norm, obj.velocity, 0); // rolling friction model
			}
		}
		else if (already_bounced) {
			obj.velocity = v_old; // can only bounce once
		}
		else {
			already_bounced = 1;
			if (otype.flags & OBJ_IS_CYLIN) {obj.init_dir.x += PI*signed_rand_float();}
			
			if (cobj.status == COLL_STATIC) { // only static collisions to avoid camera/smiley bounce sounds
				if (type == BALL) {
					float const vmag(obj.velocity.mag());
					if (vmag > 1.0) {gen_sound(SOUND_BOING, obj.pos, min(1.0, 0.1*vmag));}
				}
				else if (type == SAWBLADE) {
					gen_sound(SOUND_RICOCHET, obj.pos, 1.0, 0.5);
					if (cobj.cp.elastic >= 0.5) {gen_particles(obj.pos, (1 + (rand()&3)), 0.5, 1);} // create spark particles
				}
				else if (type == SHELLC && obj.direction == 0) {gen_sound(SOUND_SHELLC, obj.pos, 0.1, 1.0);} // M16
			}
		}
	}
	else { // sticks
		if (cobj.status == COLL_STATIC) {
			if (!obj.proc_stuck(static_top_coll) && static_top_coll) {obj.flags |= STATIC_COBJ_COLL;} // coll with top
			obj.pos -= norm*(0.1*o_radius); // make sure it still intersects
		}
		obj.velocity = zero_vector; // I think this is correct
	}
	// only use cubes for now, because leaves colliding with tree leaves and branches and resetting the normals is too unstable
	if (cobj.type == COLL_CUBE && obj.is_flat()) {obj.set_orient_for_coll(&norm);}
		
	if ((otype.flags & OBJ_IS_CYLIN) && !already_bounced) {
		if (fabs(norm.z) == 1.0) { // z collision
			obj.set_orient_for_coll(&norm);
		}
		else { // roll in the direction of the slope with axis along z
			obj.orientation = vector3d(norm.x, norm.y, 0.0).get_norm();
			obj.init_dir.x  = 0.0;
			obj.angle       = 90.0;
		}
	}
	if (do_coll_funcs && enable_cfs && cobj.cp.coll_func != NULL && type != TELEPORTER) { // call collision function
		float energy_mult(1.0);
		if (type == PLASMA) {energy_mult *= obj.init_dir.x*obj.init_dir.x;} // size squared
		float const energy(get_coll_energy(v_old, obj.velocity, otype.mass));
			
		if (!cobj.cp.coll_func(cobj.cp.cf_index, obj_index, v_old, obj.pos, energy_mult*energy, type)) { // invalid collision - reset local collision
			lcoll = 0;
			obj   = temp;
			return;
		}
	}
	if (!(otype.flags & OBJ_IS_DROP) && type != LEAF && type != CHARRED && type != SHRAPNEL &&
		type != BEAM && type != LASER && type != FIRE && type != SMOKE && type != PARTICLE && type != WAYPOINT)
	{
		coll_objects[index].register_coll(TICKS_PER_SECOND, IMPACT);
	}
	obj.verify_data();
		
	if (!obj.disabled() && (otype.flags & EXPL_ON_COLL)) {
		gen_explosion_decal((decal_pos - norm*o_radius), o_radius, norm, cobj, (cdir >> 1), ((type == FREEZE_BOMB) ? ICE_C : BLACK));
		obj.disable();
	}
	if (!obj.disabled()) {
		float sz_scale(1.0);
		int blood_tid(-1);
		colorRGBA color;
		tex_range_t tex_range;

		if (type == BLOOD && (fabs(obj.velocity.z) > 1.0 || v0.z > 1.0) && !(obj.flags & STATIC_COBJ_COLL) && (rand()&1) == 0) { // only when on a not-bottom surface
			blood_tid = BLUR_CENT_TEX; // blood droplet splat
			color     = BLOOD_C;
			sz_scale  = 2.0;
		}
		else if (type == CHUNK && !(obj.flags & (TYPE_FLAG | FROZEN_FLAG)) && (fabs(obj.velocity.z) > 1.0 || fabs(v0.z) > 1.0)) {
			blood_tid = BLOOD_SPLAT_TEX; // bloody chunk splat
			tex_range = tex_range_t::from_atlas((rand()&1), (rand()&1), 2, 2); // 2x2 texture atlas
			color     = WHITE; // color is in the texture
			sz_scale  = 4.0;
		}
		if (blood_tid >= 0 && !(obj.flags & OBJ_COLLIDED)) { // only on first collision
			float const sz(sz_scale*o_radius*rand_uniform(0.6, 1.4));
			
			if (decal_contained_in_cobj(cobj, decal_pos, norm, sz, (cdir >> 1))) {
				gen_decal((decal_pos - norm*o_radius), sz, norm, blood_tid, index, color, 0, (blood_tid == BLOOD_SPLAT_TEX), 60*TICKS_PER_SECOND, 1.0, tex_range);
			}
		}
		if (!(obj.flags & FROZEN_FLAG)) {deform_obj(obj, norm, v0);} // skip deformation of frozen chunks
	}
	if (cnorm != NULL) *cnorm = norm;
	obj.flags |= OBJ_COLLIDED;
	coll      |= lcoll; // if not an invalid collision
	lcoll      = 0; // reset local collision
	init_reset_pos(); // reset local state
	if (friction >= STICK_THRESHOLD && (obj.flags & Z_STOPPED)) {obj.pos.z = pos.z = z_old;}
}


void vert_coll_detector::init_reset_pos() {

	temp = obj; // backup copy
	pos  = obj.pos; // reset local state
	z1   = pos.z - o_radius;
	z2   = pos.z + o_radius;
	if (type == CAMERA) z2 += camera_zh;
}


int vert_coll_detector::check_coll() {

	pold -= obj.velocity*tstep;
	assert(!is_nan(pold));
	assert(type >= 0 && type < NUM_TOT_OBJS);
	o_radius = obj.get_true_radius();
	init_reset_pos();

	if (only_cobj >= 0) {
		assert((unsigned)only_cobj < coll_objects.size());
		check_cobj(only_cobj);
		return coll;
	}
	for (int d = 0; d < 1+!skip_dynamic; ++d) { // using v_collision_matrix doesn't seem to help
		get_coll_sphere_cobjs_tree(obj.pos, o_radius, -1, *this, (d != 0));
	}
	return coll;
}


// ************ end vert_coll_detector ************


// 0 = no vert coll, 1 = X coll, 2 = Y coll, 3 = X + Y coll
int dwobject::check_vert_collision(int obj_index, int do_coll_funcs, int iter, vector3d *cnorm,
	vector3d const &mdir, bool skip_dynamic, bool only_drawn, int only_cobj, bool skip_movable)
{
	if (world_mode == WMODE_INF_TERRAIN) {
		point const p_last(pos - velocity*tstep);
		float const o_radius(get_true_radius());
		vector3d cnorm(plus_z);
		
		if (proc_city_sphere_coll(pos, p_last, o_radius, p_last.z, 0, 1, &cnorm)) { // xy_only=0, inc_cars=1
			obj_type const &otype(object_types[type]);
			float const friction(otype.friction_factor*((flags & FROZEN_FLAG) ? 0.5 : 1.0)); // frozen objects have half friction
			if (animate2 && health <= 0.1) {disable();}

			if (friction < STICK_THRESHOLD) {
				if (otype.elasticity == 0.0 || (flags & IS_CUBE_FLAG) || !object_bounce(3, cnorm, 0.8, 0.0)) { // elasticity is hard-coded to 0.8 here
					if (type != DYNAM_PART && velocity != zero_vector) {
						if (friction > 0.0) {velocity *= (1.0 - min(1.0f, (tstep/TIMESTEP)*friction));} // apply kinetic friction
						orthogonalize_dir(velocity, cnorm, velocity, 0); // rolling friction model
					}
				}
				else { // play bounce sounds
					if (type == BALL) {
						if (velocity.mag() > 1.0) {gen_sound(SOUND_BOING, pos, min(1.0, 0.1*velocity.mag()));}
					}
					else if (type == SAWBLADE) {gen_sound(SOUND_RICOCHET, pos, 1.0, 0.5);}
					else if (type == SHELLC && direction == 0) {gen_sound(SOUND_SHELLC, pos, 0.1, 1.0);} // M16
				}
			}
			else { // sticks
				pos -= cnorm*(0.1*o_radius); // make sure it still intersects
				velocity = zero_vector; // I think this is correct
			}
			verify_data();
			if (!disabled() && (otype.flags & EXPL_ON_COLL)) {disable();}
			flags |= OBJ_COLLIDED;
		}
		return 0; // no vert coll
	}
	if (world_mode != WMODE_GROUND) return 0;
	vert_coll_detector vcd(*this, obj_index, do_coll_funcs, iter, cnorm, mdir, skip_dynamic, only_drawn, only_cobj, skip_movable);
	return vcd.check_coll();
}


int dwobject::multistep_coll(point const &last_pos, int obj_index, unsigned nsteps) {

	int any_coll(0);
	vector3d cmove(pos, last_pos);
	float const dist(cmove.mag()); // 0.018

	if (dist < 1.0E-6 || nsteps == 1) {
		any_coll |= check_vert_collision(obj_index, 1, 0); // collision detection
	}
	else {
		float const step(dist/(float)nsteps);
		vector3d const dpos(cmove);
		cmove /= dist;
		pos    = last_pos; // Note: can get stuck in this position if forced off the mesh by a collision

		for (unsigned i = 0; i < nsteps && !disabled(); ++i) {
			point const lpos(pos);
			pos      += cmove*step;
			any_coll |= check_vert_collision(obj_index, (i==nsteps-1), 0, NULL, dpos); // collision detection

			if (type == CAMERA && !camera_change) {
				for (unsigned d = 0; d < 2; ++d) { // x,y
					if (dpos[d]*(pos[d] - lpos[d]) < 0.0) pos[d] = lpos[d]; // negative progress in this dimension, revert
				}
			}
		}
	}
	return any_coll;
}


void create_footsteps(point const &pos, float sz, vector3d const &view_dir, point &prev_foot_pos, unsigned &step_num, bool &foot_down, bool is_camera) {

	if (!has_snow /*&& !is_camera*/) return; // only camera has footsteps, all players have snow footprints; non-snow footsteps disabled for now
	bool const prev_foot_down(foot_down);
	float const stride(1.8*sz), foot_length(0.5*sz), crush_depth(1.0*sz), foot_spacing(0.25*sz); // spacing from center
	if      (!foot_down && !dist_less_than(prev_foot_pos, pos, stride     )) {foot_down = 1; prev_foot_pos = pos; ++step_num;} // foot down and update pos
	else if ( foot_down && !dist_less_than(prev_foot_pos, pos, foot_length)) {foot_down = 0;} // foot up
	vector3d const right_dir(cross_product(view_dir, plus_z).get_norm());
	point const step_pos(pos + ((step_num&1) ? foot_spacing : -foot_spacing)*right_dir - vector3d(0.0, 0.0, 0.5*sz)); // alternate left and right feet
	if (!foot_down) return;
	bool const crushed(has_snow ? crush_snow_at_pt(step_pos, crush_depth) : 0);
	if (is_camera && !prev_foot_down) {gen_sound((crushed ? SOUND_SNOW_STEP : SOUND_FOOTSTEP), pos, 0.025, (crushed ? 1.5 : 1.2));} // on down step
}

void play_camera_footstep_sound() {

	if (!(display_mode & 0x0100)) return;
	static double fs_time(0.0);
	static point last_pos(all_zeros), prev_frame_pos(all_zeros);
	point const pos(get_camera_pos());
	if (dist_less_than(pos, prev_frame_pos, 0.001*CAMERA_RADIUS)) {fs_time = tfticks;} // reset timer if camera hasn't moved
	prev_frame_pos = pos;
	if (tfticks - fs_time < 0.36*TICKS_PER_SECOND) return; // too soon
	if (dist_less_than(pos, last_pos, 0.5*CAMERA_RADIUS)) return;
	last_pos = pos;
	fs_time  = tfticks;
	gen_sound(SOUND_SNOW_STEP, pos, 0.05, 1.25);
}


void add_camera_cobj(point const &pos) {
	camera_coll_id = add_coll_sphere(pos, CAMERA_RADIUS, cobj_params(object_types[CAMERA].elasticity, object_types[CAMERA].color, 0, 1, camera_collision, 1));
}


float get_max_mesh_height_within_radius(point const &pos, float radius, bool is_camera) {

	float const mh_radius(is_camera ? max(radius, 1.5f*NEAR_CLIP) : radius); // include the near clipping plane for the camera/player with safety margin
	float mh(int_mesh_zval_pt_off(pos, 1, 0));

	for (int dy = -1; dy <= 1; ++dy) { // take max in 4 directions to prevent intersections with the terrain on steep slopes
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue; // already added
			max_eq(mh, int_mesh_zval_pt_off((pos + vector3d(dx*mh_radius, dy*mh_radius, 0.0)), 1, 0));
		}
	}
	return mh;
}

void force_onto_surface_mesh(point &pos) { // for camera

	bool const cflight(game_mode && camera_flight);
	int coll(0);
	float const radius(CAMERA_RADIUS);
	player_state &sstate(sstates[CAMERA_ID]);
	int &jump_time(sstate.jump_time);
	dwobject camera_obj(def_objects[CAMERA]); // make a fresh copy
	
	if (maybe_teleport_object(pos, radius, CAMERA_ID, CAMERA)) {
		camera_last_pos = pos;
		add_camera_filter(colorRGBA(0.5, 0.5, 1.0, 1.0), TICKS_PER_SECOND/4, -1, CAM_FILT_TELEPORT, 1); // a quarter second of fading light blue
	}
	else {maybe_use_jump_pad(pos, sstate.velocity, radius, CAMERA_ID);}

	if (!cflight) { // make sure camera is over simulation region
		camera_in_air = 0;
		player_clip_to_scene(pos);
	}
	remove_coll_object(camera_coll_id);
	camera_coll_id = -1;
	camera_obj.pos = pos;
	camera_obj.velocity.assign(0.0, 0.0, -1.0);
	//camera_obj.velocity += (pos - camera_last_pos)/tstep; // ???

	if (world_mode == WMODE_GROUND) {
		unsigned const nsteps(CAMERA_STEPS); // *** make the number of steps determined by fticks? ***
		coll  = camera_obj.multistep_coll(camera_last_pos, 0, nsteps);
		pos.x = camera_obj.pos.x;
		pos.y = camera_obj.pos.y;
		if (!cflight) {player_clip_to_scene(pos);}
	}
	else if (!cflight) { // tiled terrain mode
		pos.z -= radius; // bottom of camera sphere
		adjust_zval_for_model_coll(pos, get_max_mesh_height_within_radius(pos, radius, 1), C_STEP_HEIGHT*radius);
		pos.z += radius;
		proc_city_sphere_coll(pos, camera_last_pos, CAMERA_RADIUS, camera_last_pos.z, 0); // use prev pos for building collisions; z dir
		camera_last_pos = pos;
		camera_change   = 0;
		return; // infinite terrain mode
	}
	if (cflight) {
		if (jump_time) {pos.z += 0.5*JUMP_ACCEL*fticks*radius; jump_time = 0;}
		if (coll) {pos.z = camera_obj.pos.z;}
		float const mesh_z(get_max_mesh_height_within_radius(pos, radius, 1));
		pos.z = min((camera_last_pos.z + float(C_STEP_HEIGHT*radius)), pos.z); // don't fall and don't rise too quickly
		if (pos.z + radius > zbottom) {pos.z = max(pos.z, (mesh_z + radius));} // if not under the mesh
	}
	else if (point_outside_mesh((get_xpos(pos.x) - xoff), (get_ypos(pos.y) - yoff))) {
		pos = camera_last_pos;
		camera_change = 0;
		jump_time     = 0;
		return;
	}
	set_true_obj_height(pos, camera_last_pos, C_STEP_HEIGHT, sstate.zvel, CAMERA, CAMERA_ID, cflight, camera_on_snow); // status return value is unused?
	camera_on_snow = 0;
	//if (display_mode & 0x0100) {create_footsteps(pos, radius, cview_dir, sstate.prev_foot_pos, sstate.step_num, sstate.foot_down, 1);}
	
	if (display_mode & 0x10) { // walk on snow (jumping?)
		float zval;
		vector3d norm;
		
		if (get_snow_height(pos, radius, zval, norm)) {
			pos.z = zval + radius;
			camera_on_snow = 1;
			camera_in_air  = 0;
		}
	}
	if (camera_coll_smooth) {collision_detect_large_sphere(pos, radius, (unsigned char)0);}
	proc_city_sphere_coll(pos, camera_last_pos, CAMERA_RADIUS, camera_last_pos.z, 0); // use prev pos for building collisions; z dir
	point const adj_pos(pos + vector3d(0.0, 0.0, camera_zh));
	if (temperature > W_FREEZE_POINT && is_underwater(adj_pos, 1) && (rand()&1)) {gen_bubble(adj_pos);}

	if (!cflight && jump_time == 0 && camera_change == 0 && camera_last_pos.z != 0.0 && (pos.z - camera_last_pos.z) > CAMERA_MESH_DZ &&
		is_above_mesh(pos) && is_over_mesh(camera_last_pos))
	{
		pos = camera_last_pos; // mesh is too steep for camera to climb
	}
	else {
		camera_last_pos = pos;
	}
	if (camera_change == 1) {
		camera_last_pos = pos;
		camera_change   = 2;
	}
	else {
		camera_change = 0;
	}
	point pos2(pos);
	pos2.z += 0.5*camera_zh;
	add_camera_cobj(pos2);
}


bool calc_sphere_z_bounds(point const &sc, float sr, point &pos, float &zt, float &zb) {

	vector3d const vtemp(pos, sc);
	if (vtemp.mag_sq() > sr*sr) return 0;
	float const arg(sr*sr - vtemp.x*vtemp.x - vtemp.y*vtemp.y);
	assert(arg >= 0.0);
	float const sqrt_arg(sqrt(arg));
	zt = sc.z + sqrt_arg;
	zb = sc.z - sqrt_arg;
	return 1;
}

bool calc_cylin_z_bounds(point const &p1, point const &p2, float r1, float r2, point const &pos, float radius, float &zt, float &zb) {

	float t, rad;
	vector3d v1, v2;
	if (!sphere_int_cylinder_pretest(pos, radius, p1, p2, r1, r2, 0, v1, v2, t, rad)) return 0;
	float const rdist(v2.mag());
	if (fabs(rdist) < TOLERANCE) return 0;
	float const val(fabs((rad/rdist - 1.0)*v2.z));
	zt = pos.z + val;
	zb = pos.z - val;
	return 1;
}


// 0 = no change, 1 = moved up, 2 = falling (unused), 3 = stuck
int set_true_obj_height(point &pos, point const &lpos, float step_height, float &zvel, int type, int id,
	bool flight, bool on_snow, bool skip_dynamic, bool test_only, bool skip_movable)
{
	int const xpos(get_xpos(pos.x) - xoff), ypos(get_ypos(pos.y) - yoff);
	bool const is_camera(type == CAMERA), is_player(is_camera || (type == SMILEY && id >= 0));
	if (is_player) {assert(id >= CAMERA_ID && id < num_smileys && sstates != nullptr);}
	player_state *sstate(is_player ? &sstates[id] : nullptr);

	if (point_outside_mesh(xpos, ypos)) {
		if (is_player) {sstate->fall_counter = 0;}
		zvel = 0.0;
		return 0;
	}
	float const radius(object_types[type].radius), step(step_height*radius); // *** step height determined by fticks? ***
	float const mh(get_max_mesh_height_within_radius(pos, radius, is_camera));
	int no_jump_placeholder(0);
	int &jump_time((sstate == nullptr || !is_player) ? no_jump_placeholder : sstate->jump_time);
	pos.z = max(pos.z, (mh + radius));
	bool jumping(0);

	if (is_player && !test_only) {
		if (display_mode & 0x0100) {create_footsteps(pos, radius, sstate->velocity.get_norm(), sstate->prev_foot_pos, sstate->step_num, sstate->foot_down, is_camera);}

		/*if (display_mode & 0x10) { // walk on snow (smiley and camera, though doesn't actually set smiley z value correctly)
			float zval;
			vector3d norm;
			if (get_snow_height(pos, radius, zval, norm)) {pos.z = zval + radius;}
		}*/
	}
	if (jump_time > 0) {
		float const jump_val((float(jump_time)/TICKS_PER_SECOND - (JUMP_COOL - JUMP_TIME))/JUMP_TIME); // jt == JC => 1.0; jt == JC-JT => 0.0

		if (jump_val > 0.0) { // in the first half of the jump (acceleration)
			pos.z  += JUMP_ACCEL*fticks*radius*jump_val*jump_val*jump_height; // quadratic falloff
			jumping = 1;
		}
		jump_time = max(0, jump_time-iticks);
	}
	float zmu(mh), z1(pos.z - radius), z2(pos.z + radius);
	if (is_camera /*|| type == WAYPOINT*/) {z2 += camera_zh;} // add camera height
	coll_cell const &cell(v_collision_matrix[ypos][xpos]);
	int any_coll(0), moved(0);
	float zceil(0.0), zfloor(0.0);

	for (int k = (int)cell.cvals.size()-1; k >= 0; --k) { // iterate backwards
		int const index(cell.cvals[k]);
		if (index < 0) continue;
		coll_obj const &cobj(coll_objects.get_cobj(index));
		if (cobj.d[2][0] > z2)         continue; // above the top of the object - can't affect it
		if (any_coll && cobj.d[2][1] < min(z1, min(zmu, zfloor))) continue; // top below a previously seen floor
		if (!cobj.contains_pt_xy(pos)) continue; // test bounding cube
		if (cobj.no_collision())       continue;
		if (skip_dynamic && cobj.status == COLL_DYNAMIC) continue;
		if (skip_movable && cobj.is_movable()) continue;
		float zt(0.0), zb(0.0);
		int coll(0);
		
		switch (cobj.type) {
		case COLL_CUBE:
			zt = cobj.d[2][1]; zb = cobj.d[2][0];
			coll = 1;
			break;
		case COLL_CYLINDER:
			coll = dist_xy_less_than(pos, cobj.points[0], cobj.radius);
			if (coll) {zt = cobj.d[2][1]; zb = cobj.d[2][0];}
			break;
		case COLL_SPHERE:
			coll = calc_sphere_z_bounds(cobj.points[0], cobj.radius, pos, zt, zb);
			break;
		case COLL_CYLINDER_ROT:
			coll = calc_cylin_z_bounds(cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2, pos, radius, zt, zb);
			break;

		case COLL_TORUS:
			if (cobj.has_z_normal()) { // Note: +z torus only
				coll = (dist_xy_less_than(pos, cobj.points[0], cobj.radius+cobj.radius2) && !dist_xy_less_than(pos, cobj.points[0], cobj.radius-cobj.radius2)); // check the hole
				if (coll) {zt = cobj.d[2][1]; zb = cobj.d[2][0];}
			}
			else {
				cylinder_3dw const cylin(cobj.get_bounding_cylinder());
				coll = calc_cylin_z_bounds(cylin.p1, cylin.p2, cylin.r1, cylin.r2, pos, radius, zt, zb);
			}
			break;

		case COLL_CAPSULE:
			coll = calc_cylin_z_bounds(cobj.points[0], cobj.points[1], cobj.radius, cobj.radius2, pos, radius, zt, zb);

			for (unsigned d = 0; d < 2; ++d) {
				float zt2, zb2;

				if (calc_sphere_z_bounds(cobj.points[d], (d ? cobj.radius2 : cobj.radius), pos, zt2, zb2)) {
					if (coll) {zt = max(zt, zt2); zb = min(zb, zb2);}
					else {zt = zt2; zb = zb2; coll = 1;}
				}
			}
			break;

		case COLL_POLYGON: { // must be coplanar, may not work correctly if vertical (but check_vert_collision() should take care of that case)
			coll = 0;
			float const thick(0.5*cobj.thickness);
			bool const poly_z(fabs(cobj.norm.z) > 0.5); // hack to fix bouncy polygons and such - should use a better fix eventually

			if (cobj.thickness > MIN_POLY_THICK && !poly_z) {
				float val;
				vector3d norm;
				vector<tquad_t> pts;
				thick_poly_to_sides(cobj.points, cobj.npoints, cobj.norm, cobj.thickness, pts);
				if (!sphere_intersect_poly_sides(pts, pos, radius, val, norm, 0)) break; // no collision
				float const zminc(cobj.d[2][0]), zmaxc(cobj.d[2][1]);
				zb = zmaxc; zt = zminc;

				if (get_poly_zvals(pts, pos.x, pos.y, zb, zt)) {
					zb   = max(zminc, zb);
					zt   = min(zmaxc, zt);
					coll = (zb < zt);
				}
			}
			else if (point_in_polygon_2d(pos.x, pos.y, cobj.points, cobj.npoints, 0, 1)) {
				float const rdist(dot_product_ptv(cobj.norm, pos, cobj.points[0]));
				// works best if the polygon has a face oriented in +z or close
				// note that we don't care if the polygon is intersected in z
				zt   = pos.z + cobj.norm.z*(-rdist + thick);
				zb   = pos.z + cobj.norm.z*(-rdist - thick);
				coll = 1;
				if (zt < zb) swap(zt, zb);
			}
			// clamp to actual polygon bounds (for cases where the object intersects the polygon's plane but not the polygon)
			zt = max(cobj.d[2][0], min(cobj.d[2][1], zt));
			zb = max(cobj.d[2][0], min(cobj.d[2][1], zb));
			break;
		}
		default: assert(0);
		} // end switch

		if (coll) {
			if (zt < zb) {
				cout << "type = " << int(cobj.type) << ", zb = " << zb << ", zt = " << zt << ", pos.z = " << pos.z << endl;
				assert(0);
			}
			if (cobj.platform_id >= 0) {zt = max(zb, zt-1.0E-6f);} // subtract a small value so that camera/smiley still collides with cobj
			if (zt <= z1) {zmu = max(zmu, zt);}
			
			if (any_coll) {
				zceil  = max(zceil,  zt);
				zfloor = min(zfloor, zb);
			}
			else {
				zceil  = zt;
				zfloor = zb;
			}
			// add some tolerance to count the equal (standing on) case as colliding
			if ((z2 + 0.01*radius) > zb && (z1 - 0.01*radius) < zt && is_player && !test_only && cobj.cp.damage != 0.0 && cobj.sphere_intersects(pos, 1.01*radius)) {
				smiley_collision(id, NO_SOURCE, plus_z, pos, fticks*cobj.cp.damage, COLLISION); // damage can be positive or negative
			}
			if (z2 > zb && z1 < zt) { // overlap: top of object above bottom of surface and bottom of object below top of surface
				if ((zt - z1) <= step) { // step up onto surface
					pos.z = max(pos.z, zt + radius);
					zmu   = max(zmu, zt);
					moved = 1;
				}
				else if (is_camera && camera_change) {
					pos.z = zt + radius;
					zmu   = max(zmu, zt);
				}
				else { // stuck against side of surface
					if (!flight && (jumping || pos.z > zb)) { // head inside the object
						if (is_player) {sstate->fall_counter = 0;}
						jump_time = min(jump_time, (jumping ? int((JUMP_COOL - JUMP_TIME)*TICKS_PER_SECOND) : 1)); // end jump time / prevent starting a new jump
						pos  = lpos; // reset to last known good position
						zvel = 0.0;
						return 3;
					}
					else { // fall down below zb - can recover
						pos.z -= (z2 - zb);
					}
				}
			}
			any_coll = 1;
		} // if coll
	} // for k
	bool falling(0);

	if (flight) {
		// do nothing
	}
	else if (jumping) {
		falling = 1; // if we're jumping, assume we're in free fall
	}
	else if (!any_coll || z2 < zfloor) {
		pos.z = mh;
		bool const on_ice(is_camera && (camera_coll_smooth || game_mode) && temperature <= W_FREEZE_POINT && is_underwater(pos));
		if (on_ice) {pos.z = water_matrix[ypos][xpos];} // standing on ice
		pos.z += radius;
		if (!on_ice && type != WAYPOINT) {modify_grass_at(pos, radius, (type != FIRE), (type == FIRE));} // crush or burn grass
	}
	else {
		zceil = max(zceil, mh);

		if (z1 > zceil) { // bottom of object above all top surfaces
			pos.z = zceil + radius; // object falls to the floor
		}
		else {
			pos.z = zmu + radius; // on mesh or top surface of cobj
		}
	}
	if ((is_camera && camera_change) || mesh_scale_change || on_snow || flight) {
		zvel = 0.0;
	}
	else if ((pos.z - lpos.z) < -step) { // falling through the air
		falling   = 1;
		jump_time = max(jump_time, 1); // prevent a new jump from starting
	}
	else {
		zvel = 0.0;
	}
	if (is_camera) {
		if (falling)        {camera_in_air     = 1;}
		if (!camera_in_air) {camera_invincible = 0;}
	}
	if (type == WAYPOINT) {
		// do nothing
	}
	else if (flight) {
		zvel = 0.0;
		if (is_player) {sstate->fall_counter = 0;}
	}
	else if (falling) {
		zvel  = max(-object_types[type].terminal_vel, (zvel - base_gravity*GRAVITY*tstep*object_types[type].gravity));
		pos.z = max(pos.z, (lpos.z + tstep*zvel));
		
		if (is_player) {
			if (sstate->fall_counter == 0) {sstate->last_dz = 0.0;}
			++sstate->fall_counter;
			sstate->last_dz  += (pos.z - lpos.z);
			sstate->last_zvel = zvel;
		}
	}
	else if (is_player) { // falling for several frames continuously and finally stops
		if (sstate->fall_counter > 4 && sstate->last_dz < 0.0 && sstate->last_zvel < 0.0) {player_fall(id);}
		sstate->fall_counter = 0;
	}
	return moved;
}


void create_sky_vis_zval_texture(unsigned &tid) {

	RESET_TIME;
	unsigned const divs_per_cell(max(1U, snow_coverage_resolution));
	unsigned const nx(MESH_X_SIZE*divs_per_cell), ny(MESH_Y_SIZE*divs_per_cell);
	float const dx(DX_VAL/divs_per_cell), dy(DY_VAL/divs_per_cell);
	vector<float> zvals;
	zvals.resize(nx*ny, czmin);
	all_models.build_cobj_trees(1);

#pragma omp parallel for schedule(dynamic,1)
	for (int y = 0; y < (int)ny; ++y) {
		for (unsigned x = 0; x < nx; ++x) {
			float const xv(-X_SCENE_SIZE + (x + 0.5)*dx), yv(-Y_SCENE_SIZE + (y + 0.5)*dy);
			float &zval(zvals[y*nx + x]);
			bool z_set(0);
			int cindex(-1);
			
			for (int sy = -1; sy <= 1; ++sy) { // take the min of 9 uniformly spaced samples in a grid pattern
				for (int sx = -1; sx <= 1; ++sx) {
					float const xr(0.5*dx*sx), yr(0.5*dy*sy), xb(xv + xr), yb(yv + yr), xt(xb + xr), yt(yb + yr);
					point const p1(xt, yt, czmax), p2(xb, yb, czmin);

					if (cindex >= 0) { // check if this ray intersects the same cobj at the same z-value
						coll_obj const &c(coll_objects.get_cobj(cindex));
						if ((c.type == COLL_CUBE || c.type == COLL_CYLINDER || (c.type == COLL_POLYGON && fabs(c.norm.z) > 0.99)) && c.line_intersect(p1, p2)) continue;
					}
					point cpos;
					vector3d coll_norm; // unused
					colorRGBA model_color; // unused
					bool const cobj_coll(check_coll_line_exact(p1, p2, cpos, coll_norm, cindex, 0.0, -1, 0, 0, 1, 1, 0));
					bool const model_coll(all_models.check_coll_line(p1, (cobj_coll ? cpos : p2), cpos, coll_norm, model_color, 1));
					if (!cobj_coll && !model_coll) continue;
					zval  = (z_set ? min(zval, cpos.z) : cpos.z);
					z_set = 1;
				}
			}
		}
	}
	setup_texture(tid, 0, 0, 0, 0, 0, 0); // nearest=0
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, nx, ny, 0, GL_RED, GL_FLOAT, &zvals.front());
	PRINT_TIME("Sky Zval Matrix");
}

