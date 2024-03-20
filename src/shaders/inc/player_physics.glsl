bool GetEdgePlaneIntersection(vec4 plane, vec3 v0, vec3 v1, out vec3 result)
{
	float dist0= dot(plane, vec4(v0, 1.0));
	float dist1= dot(plane, vec4(v1, 1.0));
	float dist_diff= dist0 - dist1;
	if(dist_diff == 0.0)
		return false;

	float k0= dist0 / dist_diff;
	float k1= dist1 / dist_diff;
	result= v1 * k0 - v0 * k1;

	return true;
}

bool GetEdgeCicleIntersection(vec2 center, float radius, vec2 v0, vec2 v1, out vec2 resut)
{
	vec2 edge_vec= v1 - v0;
	float edge_vec_length= length(edge_vec);
	if(edge_vec_length == 0.0)
		return false;

	vec2 edge_vec_normalized= edge_vec / edge_vec_length;

	vec2 vec_to_center= center - v0;

	float vec_to_center_edge_projection= dot(vec_to_center, edge_vec_normalized);

	float vec_to_center_square_length= dot(vec_to_center, vec_to_center);

	float min_dist_squared= vec_to_center_square_length - vec_to_center_edge_projection * vec_to_center_edge_projection;

	float radius_squared= radius * radius;
	if(min_dist_squared > radius_squared)
		return false;

	float offset= sqrt(radius_squared - min_dist_squared);

	resut= v0 + edge_vec_normalized * (vec_to_center_edge_projection - offset);

	return true;
}

struct CollisionDetectionResult
{
	vec3 normal;
	float move_dist;
};

// Returns closest contact point and normal for it.
CollisionDetectionResult CollideCylinderWithBlock(
	vec3 old_cylinder_pos,
	vec3 cylinder_pos,
	float cylinder_radius,
	float cylinder_height,
	ivec3 block_coord)
{
	// For now approximate block as cylinder.

	// Collapse cylinder into point and extend block by cylinder dimensions.
	// Doing so we simplify cylinder to cylinder collision to point to point collision.

	const float block_radius= 1.0 / sqrt(3.0);
	float total_radius= cylinder_radius + block_radius;

	vec2 block_center=
		vec2(
			float(block_coord.x) * c_space_scale_x + block_radius,
			float(block_coord.y) + 1.0 - 0.5 * float(block_coord.x & 1));

	float min_z= float(block_coord.z) - cylinder_height;
	float max_z= float(block_coord.z) + 1.0;

	vec3 move_ray= cylinder_pos - old_cylinder_pos;

	CollisionDetectionResult result;
	result.normal= vec3(0.0, 0.0, 1.0);
	result.move_dist= dot(move_ray, move_ray);
	if(result.move_dist == 0.0)
		return result;

	// Make it slightly less to fix some edge cases.
	float top_circles_radius= total_radius - 0.01;

	// Give planes some thickness in order to allow push-back only in some range and avoid push-back by the opposite block plane.
	float planes_thickness= 0.4;

	// Find closest intersection point to old pos.

	// Find intersection with upper block side.
	if(old_cylinder_pos.z > max_z - planes_thickness && move_ray.z < 0.0)
	{
		vec3 intersection_point;
		vec4 upper_plane= vec4(0.0, 0.0, 1.0, -max_z);
		if(GetEdgePlaneIntersection(upper_plane, old_cylinder_pos, cylinder_pos, intersection_point))
		{
			vec2 vec_to_center= intersection_point.xy - block_center;
			float dist= length(vec_to_center);

			if(dist < top_circles_radius)
			{
				float d= dot(move_ray, intersection_point - old_cylinder_pos);
				if(d < result.move_dist)
				{
					result.move_dist= d;
					result.normal= upper_plane.xyz;
				}
			}
		}
	}

	// Find intersection with lower block side.
	if(old_cylinder_pos.z < min_z + planes_thickness && move_ray.z > 0.0)
	{
		vec3 intersection_point;
		vec4 lower_plane= vec4(0.0, 0.0, -1.0, min_z);
		if(GetEdgePlaneIntersection(lower_plane, old_cylinder_pos, cylinder_pos, intersection_point))
		{
			vec2 vec_to_center= intersection_point.xy - block_center;
			float dist= length(vec_to_center);

			if(dist < top_circles_radius)
			{
				float d= dot(move_ray, intersection_point - old_cylinder_pos);
				if(d < result.move_dist)
				{
					result.move_dist= d;
					result.normal= lower_plane.xyz;
				}
			}
		}
	}

	// Find intersection with cylinder.
	{
		vec2 intersection_point;
		if(GetEdgeCicleIntersection(
			block_center,
			total_radius,
			old_cylinder_pos.xy,
			cylinder_pos.xy,
			intersection_point))
		{
			vec2 xy_vec= cylinder_pos.xy - old_cylinder_pos.xy;
			float xy_dist= length(xy_vec);
			if(xy_dist > 0.0)
			{
				float xy_partial_dist= length(intersection_point - old_cylinder_pos.xy);
				vec3 vec_to_intersection_point= move_ray * (xy_partial_dist / xy_dist);
				float intersection_z= old_cylinder_pos.z + vec_to_intersection_point.z;
				if(intersection_z > min_z && intersection_z < max_z)
				{
					float d= dot(move_ray, vec_to_intersection_point);
					if(d < result.move_dist)
					{
						result.move_dist= d;
						result.normal= vec3(normalize(intersection_point - block_center), 0.0);
					}
				}
			}
		}
	}

	result.move_dist/= length(move_ray);
	return result;
}
