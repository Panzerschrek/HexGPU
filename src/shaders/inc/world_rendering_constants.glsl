const vec3 c_fire_light_color= vec3(1.4, 1.2, 0.8);
const vec3 c_sky_light_color= vec3(0.95, 0.95, 1.0); // TODO - make depending on daytime, moon visibility, weather, etc.
// const vec3 c_sky_light_color= vec3(0.2, 0.2, 0.2);
const vec3 c_ambient_light_color= vec3(0.06f, 0.06f, 0.06f);

// Texture coordnates are initialiiy somewhat scaled. Rescale them back.
const vec2 c_tex_coord_scale= vec2(1.0 / 16.0, 1.0 / 8.0);
