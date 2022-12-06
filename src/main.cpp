/*
CPE/CSC 471 Lab base code Wood/Dunn/Eckhardt
*/

#include <iostream>
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"

#include "WindowManager.h"
#include "Shape.h"
// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define CHARS " !\"#$%＿'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~□€■⹁ƒ"

constexpr float PI = 3.141592654f;

using namespace std;
using namespace glm;

constexpr float WORLD_SIZE = 50;
constexpr float TEX_SIZE = 5.0f;

constexpr float BUILDING_SIZE = 5.0f;
constexpr float BUILDING_HEIGHT = BUILDING_SIZE / 2;
constexpr float BUILDING_SPACING = 10.0f;

constexpr float ROAD_WIDTH = 2.5f;

constexpr float ROTATE_SPEED = 2.0f;
constexpr float FULL_SPEED = 10.0f;
constexpr float TURN_SPEED = 3.3f;
constexpr float SPEED_LERP = 2.0f;
constexpr float CAMERA_LERP = 7.0f;
constexpr float CAMERA_AHEAD = 1.3f;

constexpr vec2 ENEMY_SPAWNS[4] = {
	vec2(WORLD_SIZE / 2 - 10, WORLD_SIZE / 2 - 10),
	vec2(-WORLD_SIZE / 2 + 10, WORLD_SIZE / 2 - 10),
	vec2(WORLD_SIZE / 2 - 10, -WORLD_SIZE / 2 + 10),
	vec2(-WORLD_SIZE / 2 + 10, -WORLD_SIZE / 2 + 10)
};

float lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

vec3 lerp(vec3 a, vec3 b, float t)
{
	return a + t * (b - a);
}

double get_last_elapsed_time()
{
	static double lasttime = glfwGetTime();
	double actualtime = glfwGetTime();
	double difference = actualtime - lasttime;
	lasttime = actualtime;
	return difference;
}

struct Player {
	Player() {
		reset();
	}
	vec2 pos; // x-z plane
	float vel;
	float rot; // y rotation in world space
	int turn; // -1 ccw, +1 cw (updated externally)
	void reset() {
		pos = vec2(5, 0);
		vel = 0.001f;
		rot = -PI / 2.0f;
		turn = 0;
	}
	vec3 wpos() { return vec3(pos.x, 0.71, pos.y); }
	vec2 vel_comps() {
		return vec2(
			vel * cos(rot),
			vel * sin(rot)
		);
	}
	void update(float frametime) {
		rot += ROTATE_SPEED * turn * frametime;
		float targetSpeed = turn?TURN_SPEED:FULL_SPEED;
		vel = lerp(vel, targetSpeed, SPEED_LERP * frametime);
		pos += vel_comps() * frametime;
	}
};

Player player;
bool paused = false, restart = false;

class camera
{
public:
	glm::vec3 pos, rot, startpos, startrot;
	int w, a, s, d;
	camera()
	{
		reset();
	}
	void reset() {
		w = a = s = d = 0;
		startpos = glm::vec3(0, -10, -5);
		startrot = glm::vec3(PI / 3., 0, 0);
		pos.x = startpos.x - player.pos.x;
		pos.y = startpos.y;
		pos.z = startpos.z - player.pos.y;
		rot = startrot;
	}
	glm::mat4 process(double ftime)
	{
		float speed = 0;
		float yangle = 0;
		vec3 ppos = -player.wpos();
		vec2 vcomps = player.vel_comps();
		if (paused) {
			if (w == 1)
			{
				speed = 10 * ftime;
			}
			else if (s == 1)
			{
				speed = -10 * ftime;
			}
			if (a == 1)
				yangle = -3 * ftime;
			else if (d == 1)
				yangle = 3 * ftime;
			rot.y += yangle;
		}
		else {
			vec3 targetrot = rot;
			targetrot = startrot;
			targetrot.x += vcomps.y / (player.vel * 20);
			targetrot.z += vcomps.x / (player.vel * 20);
			rot = lerp(rot, targetrot, CAMERA_LERP * ftime);
		}
		glm::mat4 R =
			glm::rotate(glm::mat4(1), rot.z, glm::vec3(0, 0, 1)) *
			glm::rotate(glm::mat4(1), rot.y, glm::vec3(0, 1, 0)) *
			glm::rotate(glm::mat4(1), rot.x, glm ::vec3(1, 0, 0));
		if (paused) {
			glm::vec4 dir = glm::vec4(0, 0, speed, 1);
			dir = dir * R;
			pos += glm::vec3(dir.x, dir.y, dir.z);
		}
		else {
			ppos.x -= vcomps.x / player.vel * CAMERA_AHEAD;
			ppos.y = 0;
			ppos.z -= vcomps.y / player.vel * CAMERA_AHEAD;
			pos = lerp(pos, ppos + startpos, CAMERA_LERP * ftime);
		}
		glm::mat4 T = glm::translate(glm::mat4(1), pos);
		return R * T;
	}
};


camera mycam;

class item
{
public:
	item* parent;
	vector<item*> children;
	vec3 position, rotation, origin, scale;
	shared_ptr<Shape> shape;
	void (*preDraw)(shared_ptr<Program>);

public:
	item() {
		parent = nullptr;
		children = {};
		position = vec3(0, 0, 0);
		rotation = vec3(0, 0, 0);
		origin = vec3(0, 0, 0);
		scale = vec3(1, 1, 1);
		shape = nullptr;
		preDraw = nullptr;
	}

	shared_ptr<Shape> initShape(const string &meshName) {
		shape = make_shared<Shape>();
		shape->loadMesh(meshName);
		shape->resize();
		shape->init();
		return shape;
	}

	shared_ptr<Shape> initShape(shared_ptr<Shape> shape) {
		this->shape = shape;
		return this->shape;
	}

	void addParent(item* parent_ptr) {
		removeParent();
		parent = parent_ptr;
		parent_ptr->children.push_back(this);
	}

	void removeParent() {
		if (parent != nullptr)
			parent->removeChild(this);
	}

	void addChild(item* child_ptr) {
		children.push_back(child_ptr);
		child_ptr->removeParent();
		child_ptr->parent = this;
	}

	void addChildren(vector<item*> children_ptr) {
		for (item* child : children_ptr)
			addChild(child);
	}

	void removeChild(item* child_ptr) {
		child_ptr->parent = nullptr;
		children.erase(
			remove(
				children.begin(),
				children.end(),
				child_ptr
			),
			children.end()
		);
	}

	void removeChildren() {
		for (item* child : children)
			removeChild(child);
	}

	mat4 transform() {
		mat4 S = glm::scale(mat4(1), scale / 2.0f);
		return transformWithoutScale() * S;
	}

	mat4 transformWithoutScale() {
		mat4 P = mat4(1);
		mat4 O = translate(mat4(1), -origin);
		mat4 T = translate(mat4(1), position);
		mat4 R = (
			rotate(mat4(1), rotation.x, vec3(1, 0, 0)) *
			rotate(mat4(1), rotation.y, vec3(0, 1, 0)) *
			rotate(mat4(1), rotation.z, vec3(0, 0, 1))
			);

		if (parent != nullptr)
			P = parent->transformWithoutScale();

		return P * T * R * O;
	}

	vec3 worldPosition() {
		return vec3(transform() * vec4(0, 0, 0, 1));
	}

	bool collision(item *other, float resize = 1.0f, float resize_other = 1.0f) {
		float thisRad = sqrt(pow(this->scale.x / 3, 2) + pow(this->scale.z / 3, 2)) * resize;
		float otherRad = sqrt(pow(other->scale.x / 3, 2) + pow(other->scale.z / 3, 2)) * resize_other;
		return distance(this->worldPosition(), other->worldPosition()) <= thisRad + otherRad;
	}

	void draw(shared_ptr<Program> prog, vector<mat4>& context) {
		if (preDraw != nullptr) preDraw(prog);
		if (shape != nullptr) {
			if (context.size() != 0) {
				glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &context[0][0][0]);
				glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &context[1][0][0]);
			}
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &transform()[0][0]);
			glUniform3fv(prog->getUniform("campos"), 1, &mycam.pos[0]);
			shape->draw(prog, FALSE);
		}
		for (item* child : children)
			child->draw(prog, context);
	}
};

struct Enemy {
	vec2 pos, startPos; // x-z plane
	float vel;
	float rot; // y rotation in world space
	int turn; // -1 ccw, +1 cw
	item thing;
	Enemy(vec2 startPos) {
		this->startPos = startPos;
		reset();
	}
	void reset() {
		pos = startPos;
		vel = 0.001f;
		rot = -PI / 2.0f;
		turn = 0;
		thing.position = wpos();
		thing.rotation.y = -rot + PI / 2.;
	}
	vec3 wpos() { return vec3(pos.x, 0.71, pos.y); }
	vec2 vel_comps() {
		return vec2(
			vel * cos(rot),
			vel * sin(rot)
		);
	}
	void update(float frametime) {
		vec2 dir = normalize(vec2(cos(rot), sin(rot)));
		vec2 target = normalize(vec2(player.pos - pos));

		float turnFactor = target.y * dir.x - target.x * dir.y;
		if (abs(turnFactor) < 0.1f) turn = 0;
		else turn = sign(turnFactor);

		rot += ROTATE_SPEED * turn * frametime;
		float targetSpeed = turn ? TURN_SPEED : FULL_SPEED;
		vel = lerp(vel, targetSpeed, SPEED_LERP * frametime);
		pos += vel_comps() * frametime;

		thing.position = wpos();
		thing.rotation.y = -rot + PI / 2.;
	}
};

struct periph {
	item thing;
	float tex_repeat = 1;
	int which_tex = 0;
};

void text(
	string &message,
	int length,
	vec2 position,
	float size,
	vec3 color,
	shared_ptr<Program> shader,
	item &billboard
) {
	string chars = " !\"#$%＿'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~□€■⹁ƒ";
	int charnum = 95;
	float spacing = size * 0.3;
	float width = size * 0.7;
	vec2 tmppos = position;
	tmppos.x -= spacing * length / 2.;
	tmppos.y -= size / 2.;
	for (int i = 0; i < length; i++) {
		char c = message[i];
		charnum = chars.find(c);
		if (charnum == string::npos) charnum = 95;
		billboard.scale.x = width;
		billboard.scale.y = size;
		billboard.position.x = tmppos.x + (spacing - width) / 2.;
		billboard.position.y = tmppos.y;
		shader->bind();
		glDisable(GL_DEPTH_TEST);
		glUniform1i(shader->getUniform("character"), charnum);
		glUniform3f(shader->getUniform("fill"), color.r, color.g, color.b);
		billboard.draw(shader, vector<mat4> {});
		glEnable(GL_DEPTH_TEST);
		shader->unbind();
		tmppos.x += spacing;
	}
}

item car, ground, character; // character is billboard for text on screen
vector<periph> peripherals;
vector<Enemy> enemies;
/* peripherals are items that are children and don't need to be manually drawn */

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> simpleshader, textshader;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID, IndexBufferIDBox;

	//texture data
	GLuint TextTex, AsphaltTex, HouseTex, CarTex;

	// Controls
	int left, right;

	// Timer
	double starttime = glfwGetTime();
	double savetime = -1;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			mycam.w = 1;
		}
		if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		{
			mycam.w = 0;
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			mycam.s = 1;
		}
		if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		{
			mycam.s = 0;
		}
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
		{
			mycam.a = 1;
		}
		if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		{
			mycam.a = 0;
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS)
		{
			mycam.d = 1;
		}
		if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		{
			mycam.d = 0;
		}
		if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
		{
			paused = !paused;
		}

		if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
		{
			left = 1;
		}
		if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE)
		{
			left = 0;
		}
		if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
		{
			right = 1;
		}
		if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE)
		{
			right = 0;
		}
	}

	// callback for the mouse when clicked move the triangle when helper functions
	// written
	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;
		float newPt[2];
		if (action == GLFW_PRESS)
		{
			glfwGetCursorPos(window, &posX, &posY);
			std::cout << "Pos X " << posX <<  " Pos Y " << posY << std::endl;

			//change this to be the points converted to WORLD
			//THIS IS BROKEN< YOU GET TO FIX IT - yay!
			newPt[0] = 0;
			newPt[1] = 0;

			std::cout << "converted:" << newPt[0] << " " << newPt[1] << std::endl;
			glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
			//update the vertex array with the updated points
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(float)*6, sizeof(float)*2, newPt);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	//if the window is resized, capture the new size and reset the viewport
	void resizeCallback(GLFWwindow *window, int in_width, int in_height)
	{
		//get the window size - may be different then pixels for retina
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
	}

	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom()
	{
		string resourceDirectory = "../resources" ;
		// Initialize mesh.
		shared_ptr<Shape> simple_cube = ground.initShape(
			resourceDirectory + "/cube.obj"
		);
		item house;
		shared_ptr<Shape> house_shape = house.initShape(resourceDirectory + "/house/house.obj");

		for (int i = 0; i < pow(WORLD_SIZE / BUILDING_SPACING, 2); i++) {
			periph tmp, tmp_grass;
			tmp.which_tex = 1;
			tmp.thing.initShape(house_shape);
			tmp_grass.thing.initShape(simple_cube);

			tmp.thing.scale = vec3(BUILDING_SIZE, BUILDING_SIZE, BUILDING_SIZE);
			tmp.thing.position.x =
				(i * (int)round(BUILDING_SPACING)) %
				(int)round(WORLD_SIZE) +
				(BUILDING_SPACING - WORLD_SIZE) / 2.0f;
			tmp.thing.position.y = (BUILDING_HEIGHT + 1) / 2.0f;
			tmp.thing.position.z =
				(int)floor(i * (int)round(BUILDING_SPACING) /
				(int)round(WORLD_SIZE)) * BUILDING_SPACING +
				(BUILDING_SPACING - WORLD_SIZE) / 2.0f;
			peripherals.push_back(tmp);

			tmp_grass.tex_repeat = 5.0f;
			tmp_grass.which_tex = 3;
			tmp_grass.thing.scale = vec3(BUILDING_SPACING - ROAD_WIDTH);
			tmp_grass.thing.scale.y = 0.1f;
			tmp_grass.thing.position = tmp.thing.position;
			tmp_grass.thing.position.y = 0.5f;
			peripherals.push_back(tmp_grass);
		}

		shared_ptr<Shape> car_shape = car.initShape(
			resourceDirectory + "/car/car.obj"
		);

		ground.scale = vec3(WORLD_SIZE, 1, WORLD_SIZE);
		car.scale = vec3(1, 1, 1);
		car.position = player.wpos();
		car.rotation.y = PI / 2.;

		for (vec2 sp : ENEMY_SPAWNS) {
			Enemy en = Enemy(sp);
			en.thing.initShape(car_shape);
			en.thing.scale = vec3(1);
			enemies.push_back(en);
		}

		character.initShape(
			resourceDirectory + "/billboard.obj"
		);
		character.scale = vec3(0.1);

		int width, height, channels;
		char filepath[1000];
		unsigned char* data = nullptr;
		string str;

		//texture 1
		str = resourceDirectory + "/bmpfontclear.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &TextTex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//texture 2
		str = resourceDirectory + "/asphalt.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &AsphaltTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, AsphaltTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//texture 3
		str = resourceDirectory + "/house/tex/house.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &HouseTex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, HouseTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//texture 4
		str = resourceDirectory + "/grass.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &CarTex);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, CarTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//texture 5
		str = resourceDirectory + "/car/tex/car.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &CarTex);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, CarTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);


		//set the textures to the correct samplers in the fragment shader:
		GLuint Tex0Location = glGetUniformLocation(textshader->pid, "text_tex");
		GLuint Tex1Location = glGetUniformLocation(simpleshader->pid, "asphalt_tex");
		GLuint Tex2Location = glGetUniformLocation(simpleshader->pid, "house_tex");
		GLuint Tex3Location = glGetUniformLocation(simpleshader->pid, "grass_tex");
		GLuint Tex4Location = glGetUniformLocation(simpleshader->pid, "car_tex");
		// Then bind the uniform samplers to texture units:
		glUseProgram(textshader->pid);
		glUniform1i(Tex0Location, 0);
		glUseProgram(simpleshader->pid);
		glUniform1i(Tex1Location, 1);
		glUniform1i(Tex2Location, 2);
		glUniform1i(Tex3Location, 3);
		glUniform1i(Tex4Location, 4);
	}

	//General OGL initialization - set OGL state here
	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Initialize the simpleshader program.
		simpleshader = std::make_shared<Program>();
		simpleshader->setVerbose(true);
		simpleshader->setShaderNames(
			resourceDirectory + "/simple_vertex.glsl",
			resourceDirectory + "/simple_frag.glsl"
		);
		if (!simpleshader->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		simpleshader->addUniform("P");
		simpleshader->addUniform("V");
		simpleshader->addUniform("M");
		simpleshader->addUniform("campos");
		simpleshader->addUniform("tex_repeat");
		simpleshader->addUniform("which_tex"); // 0: asphalt, 1: concrete, 2: car
		simpleshader->addAttribute("vertPos");
		simpleshader->addAttribute("vertNor");
		simpleshader->addAttribute("vertTex");

		// Initialize the skyshader program.
		textshader = std::make_shared<Program>();
		textshader->setVerbose(true);
		textshader->setShaderNames(
			resourceDirectory + "/text_vertex.glsl",
			resourceDirectory + "/text_frag.glsl"
		);
		if (!textshader->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		textshader->addUniform("M");
		textshader->addUniform("fill");
		textshader->addUniform("campos");
		textshader->addUniform("character");
		textshader->addAttribute("vertPos");
		textshader->addAttribute("vertNor");
		textshader->addAttribute("vertTex");
	}


	void queue_reset() {
		paused = true;
		restart = true;
	}


	void reset() {
		paused = false;
		restart = false;
		player.reset();
		mycam.reset();
		car.position = player.wpos();
		car.rotation.y = -player.rot + PI / 2.;
		starttime = glfwGetTime();

		for (int i = 0; i < enemies.size(); i++)
			enemies[i].reset();
	}


	void update(double frametime)
	{
		player.turn = right - left;
		if (paused) {
			if (savetime < 0)
				savetime = glfwGetTime() - starttime;
			return;
		}
		else {
			if (restart) reset();
			else if (savetime >= 0)
				starttime = glfwGetTime() - savetime;
			savetime = -1;
		}
		player.update(frametime);
		car.position = player.wpos();
		car.rotation.y = -player.rot + PI / 2.;

		for (int i = 0; i < enemies.size(); i++) {
			enemies[i].update(frametime);
			for (periph p : peripherals)
				if (p.which_tex == 1 && enemies[i].thing.collision(&p.thing, 1.0f, 0.9f))
					enemies[i].reset();
			for (int j = 0; j < enemies.size(); j++) {
				if (i != j && enemies[i].thing.collision(&enemies[j].thing, 0.5f, 0.5f)) {
					enemies[i].reset();
					enemies[j].reset();
				}
			}
			if (WORLD_SIZE / 2 - abs(enemies[i].pos.x) - 0.3 <= 0 ||
				WORLD_SIZE / 2 - abs(enemies[i].pos.y) - 0.3 <= 0)
				enemies[i].reset();
			if (car.collision(&enemies[i].thing, 0.5f, 0.5f))
				queue_reset();
		}
		for (periph p : peripherals)
			if (p.which_tex == 1 && car.collision(&p.thing, 1.0f, 0.9f))
				queue_reset();
		if (WORLD_SIZE / 2 - abs(player.pos.x) - 0.3 <= 0 ||
			WORLD_SIZE / 2 - abs(player.pos.y) - 0.3 <= 0)
			queue_reset();
	}


	/****DRAW
	This is the most important function in your program - this is where you
	will actually issue the commands to draw any geometry you have set up to
	draw
	********/
	void render(double frametime)
	{
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width/(float)height;
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks - please leave these alone for now
		
		mat4 V, P; //View and Perspective matrix
		V = mycam.process(frametime);
		P = glm::perspective(
			(float)(PI / 4.),
			(float)((float)width / (float)height),
			0.1f, 1000.0f
		);
		vector<mat4> context = { P, V };

		// Draw using GLSL.

		simpleshader->bind();
		glUniform1f(simpleshader->getUniform("tex_repeat"), WORLD_SIZE / TEX_SIZE);
		glUniform1i(simpleshader->getUniform("which_tex"), 0);
		ground.draw(simpleshader, context);
		for (periph p : peripherals) {
			glUniform1f(simpleshader->getUniform("tex_repeat"), p.tex_repeat);
			glUniform1i(simpleshader->getUniform("which_tex"), p.which_tex);
			p.thing.draw(simpleshader, context);
		}
		glUniform1f(simpleshader->getUniform("tex_repeat"), 1);
		glUniform1i(simpleshader->getUniform("which_tex"), 2);
		car.draw(simpleshader, context);
		glUniform1i(simpleshader->getUniform("which_tex"), 4);
		for (Enemy en : enemies)
			en.thing.draw(simpleshader, context);
		simpleshader->unbind();

		string scoretext = "Score";
		text(
			scoretext,
			scoretext.length(),
			vec2(0, -0.8),
			0.1f,
			vec3(1.0, 0.3, 0.3),
			textshader,
			character
		);

		float time = glfwGetTime() - starttime;
		if (paused) time = savetime;
		char* time_text = (char*) malloc(sizeof(char) * 20);
		snprintf(time_text, sizeof(char) * 20, "%.2f", time);
		text(
			string(time_text),
			string(time_text).length(),
			vec2(0, -0.9),
			0.1f,
			vec3(0.1, 0.2, 1.0),
			textshader,
			character
		);
		free(time_text);

		if (paused && restart) {
			string txt = "You Died!";
			text(
				txt,
				txt.length(),
				vec2(0, 0.2),
				0.25f,
				vec3(1.0, 0.3, 0.3),
				textshader,
				character
			);
			txt = "Press [SPACE] to restart";
			text(
				txt,
				txt.length(),
				vec2(0, -0.1),
				0.1f,
				vec3(1.0, 0.3, 0.3),
				textshader,
				character
			);
		}
	}

};
//******************************************************************************************
int main(int argc, char **argv)
{
	std::string resourceDir = "../resources"; // Where the resources are loaded from
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	/* your main will always include a similar set up to establish your window
		and GL context, etc. */
	WindowManager * windowManager = new WindowManager();
	windowManager->init(1920, 1080);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	/* This is the code that will likely change program to program as you
		may need to initialize or set up different data and state */
	// Initialize scene.
	application->init(resourceDir);
	application->initGeom();

	// Loop until the user closes the window.
	while(! glfwWindowShouldClose(windowManager->getHandle()))
	{
		double frametime = get_last_elapsed_time();
		application->update(frametime);
		// Render scene.
		application->render(frametime);

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
