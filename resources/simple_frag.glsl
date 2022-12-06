#version 330 core

out vec4 color;

in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;

uniform vec3 campos;
uniform float tex_repeat;
uniform sampler2D asphalt_tex;
uniform sampler2D house_tex;
uniform sampler2D grass_tex;
uniform sampler2D car_tex;
uniform int which_tex;

void main()
{
	vec3 tex_rgb;
	switch (which_tex) {
		case 0:
			tex_rgb = texture(asphalt_tex, vertex_tex * tex_repeat).rgb;
			tex_rgb *= 0.3f;
			break;
		case 1:
			tex_rgb = texture(house_tex, vertex_tex * tex_repeat).rgb;
			break;
		case 2:
			tex_rgb = texture(car_tex, vertex_tex * tex_repeat).rgb;
			break;
		case 3:
			tex_rgb = texture(grass_tex, vertex_tex * tex_repeat).rgb;
			break;
		case 4:
			tex_rgb = texture(car_tex, vertex_tex * tex_repeat).rgb;
			tex_rgb *= vec3(1.0, 0.6, 0.6);
			break;
	}

	float ambient = 0.3f;
	vec3 n = normalize(vertex_normal);
	vec3 lp = vec3(10000, -10000, 0);
	vec3 ld = normalize(vertex_pos - lp);

	float diffuse = dot(n, ld);
	diffuse = clamp(diffuse, 0, 1);

	color.rgb = tex_rgb;
	color.rgb *= (diffuse * (1.0 - ambient) + ambient);

	if (which_tex == 2) {
		vec3 cd = normalize(vertex_pos - campos);
		vec3 h = normalize(cd+ld);
		float spec = dot(n,h);
		spec = clamp(spec,0,1);
		spec = pow(spec,20);
		color.rgb += vec3(1,1,1)*spec;
	}
	color.a=1;
}
