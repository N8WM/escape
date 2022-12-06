#version 330 core
out vec4 color;
in vec3 vertex_pos;
in vec2 vertex_tex;
uniform vec3 fill;
uniform vec3 campos;
uniform int character;
uniform sampler2D text_tex;

float average(vec3 color) {
	return (color.r + color.g + color.b) / 3.;
}

void main()
{
	vec2 tex_off = vec2(character % 10, 9 - floor(character / 10.)) / 10.;
	vec3 pxl = texture(text_tex, vertex_tex / 10. + tex_off).rgb;
	color = vec4(fill, average(pxl));
}
