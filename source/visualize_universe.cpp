// -----------------------------------------------------------------------------
// Copyright  : (C) 2014 Andreas-C. Bernstein
//                  2015 Sebastian Thiele
//					2015 Pavel Karpashevich
// License    : MIT (see the file LICENSE)
// Maintainer : Pavel Karpashevich <pavel.karpashevich@uni-weimar.de>
// Stability  : experimantal project
//
// visualize_universe Example
// -----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning (disable: 4996)         // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#endif

#define _USE_MATH_DEFINES
#include "fensterchen.hpp"
#include <string>
#include <iostream>
#include <sstream>      // std::stringstream
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>

///GLM INCLUDES
#define GLM_FORCE_RADIANS
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

///PROJECT INCLUDES
#include <volume_loader_raw.hpp>
#include <transfer_function.hpp>
#include <utils.hpp>
#include <turntable.hpp>
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>        // stb_image.h for PNG loading
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

///IMGUI INCLUDES
#include <imgui_impl_glfw_gl3.h>
using namespace std;

typedef struct halo {
	float x, y, z;		/* position of body */
	float vx, vy, vz;		/* velocity of body */
	float ax, ay, az;		/* acceleration */
	float phi;			/* potential */
	int64_t ident;		/* unique identifier */
} halo;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#undef PI
         const float PI = 3.14159265358979323846f;

#ifdef INT_MAX
#define IM_INT_MAX INT_MAX
#else
#define IM_INT_MAX 2147483647
#endif

// Play it nice with Windows users. Notepad in 2014 still doesn't display text data with Unix-style \n.
#ifdef _MSC_VER
#define STR_NEWLINE "\r\n"
#else
#define STR_NEWLINE "\n"
#endif

const std::string g_file_vertex_shader("../../../source/shader/volume.vert");
const std::string g_file_fragment_shader("../../../source/shader/volume.frag");

const std::string g_GUI_file_vertex_shader("../../../source/shader/pass_through_GUI.vert");
const std::string g_GUI_file_fragment_shader("../../../source/shader/pass_through_GUI.frag");

//Making changes here
const std::string g_multi_vertex_shader("../../../source/shader/multi.vert");
const std::string g_multi_fragment_shader("../../../source/shader/multi.frag");

GLuint loadShaders(std::string const& vs, std::string const& fs)
{
    std::string v = readFile(vs);
    std::string f = readFile(fs);
    return createProgram(v, f);
}

GLuint loadShaders(
    std::string const& vs,
    std::string const& fs,
    const int task_nbr,
    const int enable_lighting,
    const int enable_shadowing,
    const int enable_opeacity_cor,
	const int enable_vol0,
	const int enable_vol2,
	const int enable_vol3,
	const int enable_vol4,
	const int pair_nbr,
	const int func_nbr)
{
    std::string v = readFile(vs);
    std::string f = readFile(fs);

    std::stringstream ss1;
    ss1 << task_nbr;

    int index = (int)f.find("#define TASK");
    f.replace(index + 13, 2, ss1.str());

    std::stringstream ss2;
    ss2 << enable_opeacity_cor;

    index = (int)f.find("#define ENABLE_OPACITY_CORRECTION");
    f.replace(index + 34, 1, ss2.str());

    std::stringstream ss3;
    ss3 << enable_lighting;

    index = (int)f.find("#define ENABLE_LIGHTNING");
    f.replace(index + 25, 1, ss3.str());

    std::stringstream ss4;
    ss4 << enable_shadowing;

    index = (int)f.find("#define ENABLE_SHADOWING");
    f.replace(index + 25, 1, ss4.str());

	std::stringstream ss5;
	ss5 << enable_vol0;

	index = (int)f.find("#define ENABLE_VOL0");
	f.replace(index + 20, 1, ss5.str());

	std::stringstream ss6;
	ss6 << enable_vol2;

	index = (int)f.find("#define ENABLE_VOL2");
	f.replace(index + 20, 1, ss6.str());

	std::stringstream ss7;
	ss7 << enable_vol3;

	index = (int)f.find("#define ENABLE_VOL3");
	f.replace(index + 20, 1, ss7.str());

	std::stringstream ss8;
	ss8 << enable_vol4;

	index = (int)f.find("#define ENABLE_VOL4");
	f.replace(index + 20, 1, ss8.str());

	std::stringstream ss9;
	ss9 << pair_nbr;

	index = (int)f.find("#define PAIR");
	f.replace(index + 13, 2, ss9.str());

	std::stringstream ss10;
	ss10 << func_nbr;

	index = (int)f.find("#define FUNC");
	f.replace(index + 13, 2, ss10.str());

    //std::cout << f << std::endl;

    return createProgram(v, f);
}

Turntable  g_turntable;

///SETUP VOLUME RAYCASTER HERE
// set the volume file
std::string g_file_string0 = "../../../data/universeacc_w256_h256_d256_c3_b8.raw";
std::string g_file_string2 = "../../../data/universevel_w256_h256_d256_c3_b8.raw";
std::string g_file_string3 = "../../../data/universeden_w256_h256_d256_c1_b8.raw";
std::string g_file_string4 = "../../../data/universephi_w256_h256_d256_c1_b8.raw";

// set the sampling distance for the ray traversal
float       g_sampling_distance = 0.001f;
float       g_sampling_distance_fact = 0.5f;
float       g_sampling_distance_fact_move = 2.0f;
float       g_sampling_distance_fact_ref = 1.0f;

float       g_iso_value = 0.2f;

// set the light position and color for shading
// set the light position and color for shading
glm::vec3   g_light_pos = glm::vec3(10.0, 10.0, 10.0);
glm::vec3   g_ambient_light_color = glm::vec3(0.1f, 0.1f, 0.1f);
glm::vec3   g_diffuse_light_color = glm::vec3(0.2f, 0.2f, 0.2f);
glm::vec3   g_specula_light_color = glm::vec3(0.2f, 0.2f, 0.2f);
float       g_ref_coef = 12.0;

// set backgorund color here
//glm::vec3   g_background_color = glm::vec3(1.0f, 1.0f, 1.0f); //white
glm::vec3   g_background_color = glm::vec3(0.08f, 0.08f, 0.08f);   //grey

glm::ivec2  g_window_res = glm::ivec2(1600, 800);
Window g_win(g_window_res);

// Volume Rendering GLSL Program
GLuint g_volume_program(0);
std::string g_error_message;
bool g_reload_shader_error = false;

Transfer_function g_transfer_fun, g_transfer_fun2, g_transfer_fun3, g_transfer_fun4;
int g_current_tf_data_value = 0;
GLuint g_transfer_texture;
GLuint g_transfer_texture2;
GLuint g_transfer_texture3;
GLuint g_transfer_texture4;

GLuint g_multi_tran1;
GLuint g_multi_tran2;
GLuint g_multi_tran3;
GLuint g_multi_tran4;

GLuint g_gisto1;
GLuint g_gisto2;
GLuint g_gisto3;
GLuint g_gisto4;
GLuint g_gisto5;
GLuint g_gisto6;

bool g_transfer_dirty = true;
bool g_redraw_tf = true;
bool g_lighting_toggle = false;
bool g_shadow_toggle = false;
bool g_opacity_correction_toggle = false;

bool g_vol0_toggle = true;
bool g_vol2_toggle = false;
bool g_vol3_toggle = false;
bool g_vol4_toggle = false;

int g_active_function = 0;
int g_active_multi = 1;

// imgui variables
static bool g_show_gui = true;
static bool mousePressed[2] = { false, false };

bool g_show_transfer_function_in_window = false;
glm::vec2 g_transfer_function_pos = glm::vec2(0.0f);
glm::vec2 g_multi_function_pos = glm::vec2(320.0f, 230.0f);
glm::vec2 g_gist_pos = glm::vec2(320.0f, 230.0f);
glm::vec2 g_transfer_function_size = glm::vec2(0.0f);
glm::vec2 g_multi_function_size = glm::vec2(150.0f);
glm::vec2 g_gist_size = glm::vec2(150.0f);

//imgui values
bool g_over_gui = false;
bool g_reload_shader = false;
bool g_reload_shader_pressed = false;
bool g_show_transfer_function = false;

int g_task_chosen = 21;
int g_task_chosen_old = g_task_chosen;
int g_pair_chosen = 24;
int g_pair_chosen_old = g_pair_chosen;
int g_func_chosen = 1;
int g_func_chosen_old = g_func_chosen;

bool  g_pause = false;

Volume_loader_raw g_volume_loader;
volume_data_type g_volume_data;
glm::ivec3 g_vol_dimensions;
glm::vec3 g_max_volume_bounds;
unsigned g_channel_size = 0;
unsigned g_channel_count = 0;
GLuint g_volume_texture0 = 0;
GLuint g_volume_texture2 = 0;
GLuint g_volume_texture3 = 0;
GLuint g_volume_texture4 = 0;
Cube g_cube;

int g_bilinear_interpolation = true;

bool first_frame = true;

struct Manipulator
{
    Manipulator()
    : m_turntable()
    , m_mouse_button_pressed(0, 0, 0)
    , m_mouse(0.0f, 0.0f)
    , m_lastMouse(0.0f, 0.0f)
    {}

    glm::mat4 matrix()
    {
        m_turntable.rotate(m_slidelastMouse, m_slideMouse);
        return m_turntable.matrix();
    }

    glm::mat4 matrix(Window const& g_win)
    {
        m_mouse = g_win.mousePosition();
        
        if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
            if (!m_mouse_button_pressed[0]) {
                m_mouse_button_pressed[0] = 1;
            }
            m_turntable.rotate(m_lastMouse, m_mouse);
            m_slideMouse = m_mouse;
            m_slidelastMouse = m_lastMouse;
        }
        else {
            m_mouse_button_pressed[0] = 0;
            m_turntable.rotate(m_slidelastMouse, m_slideMouse);
            //m_slideMouse *= 0.99f;
            //m_slidelastMouse *= 0.99f;
        }

        if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
            if (!m_mouse_button_pressed[1]) {
                m_mouse_button_pressed[1] = 1;
            }
            m_turntable.pan(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[1] = 0;
        }

        if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            if (!m_mouse_button_pressed[2]) {
                m_mouse_button_pressed[2] = 1;
            }
            m_turntable.zoom(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[2] = 0;
        }

        m_lastMouse = m_mouse;
        return m_turntable.matrix();
    }

private:
    Turntable  m_turntable;
    glm::ivec3 m_mouse_button_pressed;
    glm::vec2  m_mouse;
    glm::vec2  m_lastMouse;
    glm::vec2  m_slideMouse;
    glm::vec2  m_slidelastMouse;
};

GLuint read_volume(std::string& volume_string){

    //init volume g_volume_loader
    //Volume_loader_raw g_volume_loader;
    //read volume dimensions
	GLuint volume_texture = 0;
	std::string g_file_string = volume_string;
    g_vol_dimensions = g_volume_loader.get_dimensions(g_file_string);

    g_sampling_distance = 1.0f / glm::max(glm::max(g_vol_dimensions.x, g_vol_dimensions.y), g_vol_dimensions.z);

    unsigned max_dim = std::max(std::max(g_vol_dimensions.x,
        g_vol_dimensions.y),
        g_vol_dimensions.z);

    // calculating max volume bounds of volume (0.0 .. 1.0)
    g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

    // loading volume file data
    g_volume_data = g_volume_loader.load_volume(g_file_string);
    g_channel_size = g_volume_loader.get_bit_per_channel(g_file_string) / 8;
    g_channel_count = g_volume_loader.get_channel_count(g_file_string);

    // setting up proxy geometry
    g_cube.freeVAO();
    g_cube = Cube(glm::vec3(0.0, 0.0, 0.0), g_max_volume_bounds);

    //glActiveTexture(GL_TEXTURE0);
    volume_texture = createTexture3D(g_vol_dimensions.x, g_vol_dimensions.y, g_vol_dimensions.z, g_channel_size, g_channel_count, (char*)&g_volume_data[0]);

    return volume_texture;

}

void UpdateImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // Setup resolution (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(g_win.getGLFWwindow(), &w, &h);
    glfwGetFramebufferSize(g_win.getGLFWwindow(), &display_w, &display_h);
    io.DisplaySize = ImVec2((float)display_w, (float)display_h);                                   // Display size, in pixels. For clamping windows positions.

    // Setup time step
    static double time = 0.0f;
    const double current_time = glfwGetTime();
    io.DeltaTime = (float)(current_time - time);
    time = current_time;
    
    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    double mouse_x, mouse_y;
    glfwGetCursorPos(g_win.getGLFWwindow(), &mouse_x, &mouse_y);
    mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
    mouse_y *= (float)display_h / h;
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
    io.MouseDown[0] = mousePressed[0] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = mousePressed[1] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_RIGHT) != 0;

    // Start the frame
    //ImGui::NewFrame();
    ImGui_ImplGlfwGL3_NewFrame();
}


void showGUI(){

    ImGui::Begin("Volume Settings", &g_show_gui, ImVec2(300, 500));
    static float f;
    g_over_gui = ImGui::IsMouseHoveringAnyWindow();

    // Calculate and show frame rate
    static ImVector<float> ms_per_frame; if (ms_per_frame.empty()) { ms_per_frame.resize(400); memset(&ms_per_frame.front(), 0, ms_per_frame.size()*sizeof(float)); }
    static int ms_per_frame_idx = 0;
    static float ms_per_frame_accum = 0.0f;
    if (!g_pause){
        ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
        ms_per_frame[ms_per_frame_idx] = ImGui::GetIO().DeltaTime * 1000.0f;
        ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];

        ms_per_frame_idx = (ms_per_frame_idx + 1) % ms_per_frame.size();
    }
    const float ms_per_frame_avg = ms_per_frame_accum / 120;

    if (ImGui::CollapsingHeader("Task", 0, true, true))
    {        
        if (ImGui::TreeNode("Introduction")){
            ImGui::RadioButton("Max Intensity Projection", &g_task_chosen, 21);
            ImGui::RadioButton("Average Intensity Projection", &g_task_chosen, 22);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Iso Surface Rendering")){
            ImGui::RadioButton("Inaccurate", &g_task_chosen, 31);
            ImGui::RadioButton("Binary Search", &g_task_chosen, 32);
            ImGui::SliderFloat("Iso Value", &g_iso_value, 0.0f, 1.0f, "%.8f", 1.0f);
            ImGui::TreePop();
        }       
 
		if (ImGui::TreeNode("Direct Volume Rendering")){
			ImGui::RadioButton("Compositing", &g_task_chosen, 41);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Multi-dimensional Transfer Function")){
			ImGui::RadioButton("Compositing", &g_task_chosen, 5);
			ImGui::TreePop();
        }

		if (ImGui::TreeNode("Volume Pair Selection")){
			ImGui::RadioButton("Acceleration+Velocity", &g_pair_chosen, 12);
			ImGui::RadioButton("Acceleration+Density", &g_pair_chosen, 13);
			ImGui::RadioButton("Acceleration+Phi", &g_pair_chosen, 14);
			ImGui::RadioButton("Velocity+Density", &g_pair_chosen, 23);
			ImGui::RadioButton("Velocity+Phi", &g_pair_chosen, 24);
			ImGui::RadioButton("Density+Phi", &g_pair_chosen, 34);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Function Selection")){
			ImGui::RadioButton("Manual(Div-Div)", &g_func_chosen, 1);
			ImGui::RadioButton("Diverging-Diverging", &g_func_chosen, 2);
			ImGui::RadioButton("Diverging-Sequential", &g_func_chosen, 3);
			ImGui::RadioButton("Sequential-Sequential", &g_func_chosen, 4);
			ImGui::RadioButton("Qualitative-Sequential", &g_func_chosen, 5);
			ImGui::TreePop();
		}

        
        g_reload_shader ^= ImGui::Checkbox("1", &g_lighting_toggle); ImGui::SameLine();
		g_task_chosen == 41 || g_task_chosen == 31 || g_task_chosen == 32 || g_task_chosen == 5 ? ImGui::Text("Enable Lighting") : ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Enable Lighting");

        g_reload_shader ^= ImGui::Checkbox("2", &g_shadow_toggle); ImGui::SameLine();
		g_task_chosen == 41 || g_task_chosen == 31 || g_task_chosen == 32 || g_task_chosen == 5 ? ImGui::Text("Enable Shadows") : ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Enable Shadows");

        g_reload_shader ^= ImGui::Checkbox("3", &g_opacity_correction_toggle); ImGui::SameLine();
		g_task_chosen == 41 || g_task_chosen == 5 ? ImGui::Text("Opacity Correction") : ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Opacity Correction");

        if (g_task_chosen != g_task_chosen_old){
            g_reload_shader = true;
            g_task_chosen_old = g_task_chosen;
        }

		if (g_pair_chosen != g_pair_chosen_old){
			g_reload_shader = true;
			g_pair_chosen_old = g_pair_chosen;
		}

		if (g_func_chosen != g_func_chosen_old){
			g_reload_shader = true;
			g_func_chosen_old = g_func_chosen;
		}




    }

    if (ImGui::CollapsingHeader("Load Volumes", 0, true, false))
    {


        //bool load_volume_1 = false;
       // bool load_volume_2 = false;
        //bool load_volume_3 = false;

        ImGui::Text("Volumes");
		g_reload_shader ^= ImGui::Checkbox("Accelartion", &g_vol0_toggle);
		g_reload_shader ^= ImGui::Checkbox("Velocity", &g_vol2_toggle);
		g_reload_shader ^= ImGui::Checkbox("Density", &g_vol3_toggle);
		g_reload_shader ^= ImGui::Checkbox("Phi", &g_vol4_toggle);
        //load_volume_1 ^= ImGui::Button("Load Volume Head");
        //load_volume_2 ^= ImGui::Button("Load Volume Engine");
        //load_volume_3 ^= ImGui::Button("Load Volume Bucky");


        //if (load_volume_1){
        //    g_file_string0 = "../../../data/head_w256_h256_d225_c1_b8.raw";
         //   read_volume(g_file_string0);
        //}
        //if (load_volume_2){
       //     g_file_string0 = "../../../data/Engine_w256_h256_d256_c1_b8.raw";
        //    read_volume(g_file_string0);
       // }

       // if (load_volume_3){
       //     g_file_string0 = "../../../data/Bucky_uncertainty_data_w32_h32_d32_c1_b8.raw";
       //     read_volume(g_file_string0);
      //  }
    }


    if (ImGui::CollapsingHeader("Lighting Settings"))
    {
        ImGui::SliderFloat3("Position Light", &g_light_pos[0], -10.0f, 10.0f);

        ImGui::ColorEdit3("Ambient Color", &g_ambient_light_color[0]);
        ImGui::ColorEdit3("Diffuse Color", &g_diffuse_light_color[0]);
        ImGui::ColorEdit3("Specular Color", &g_specula_light_color[0]);

        ImGui::SliderFloat("Reflection Coefficient kd", &g_ref_coef, 0.0f, 20.0f, "%.5f", 1.0f);


    }

    if (ImGui::CollapsingHeader("Quality Settings"))
    {
        ImGui::Text("Interpolation");
        ImGui::RadioButton("Nearest Neighbour", &g_bilinear_interpolation, 0);
        ImGui::RadioButton("Bilinear", &g_bilinear_interpolation, 1);

        ImGui::Text("Slamping Size");
        ImGui::SliderFloat("sampling factor", &g_sampling_distance_fact, 0.1f, 10.0f, "%.5f", 4.0f);
        ImGui::SliderFloat("sampling factor move", &g_sampling_distance_fact_move, 0.1f, 10.0f, "%.5f", 4.0f);
        ImGui::SliderFloat("reference sampling factor", &g_sampling_distance_fact_ref, 0.1f, 10.0f, "%.5f", 4.0f);
    }

    if (ImGui::CollapsingHeader("Shader", 0, true, true))
    {
        static ImVec4 text_color(1.0, 1.0, 1.0, 1.0);

        if (g_reload_shader_error) {
            text_color = ImVec4(1.0, 0.0, 0.0, 1.0);
            ImGui::TextColored(text_color, "Shader Error");
        }
        else
        {
            text_color = ImVec4(0.0, 1.0, 0.0, 1.0);
            ImGui::TextColored(text_color, "Shader Ok");
        }

        
        ImGui::TextWrapped(g_error_message.c_str());

        g_reload_shader ^= ImGui::Button("Reload Shader");

    }

    if (ImGui::CollapsingHeader("Timing"))
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms_per_frame_avg, 1000.0f / ms_per_frame_avg);

        float min = *std::min_element(ms_per_frame.begin(), ms_per_frame.end());
        float max = *std::max_element(ms_per_frame.begin(), ms_per_frame.end());

        if (max - min < 10.0f){
            float mid = (max + min) * 0.5f;
            min = mid - 5.0f;
            max = mid + 5.0f;
        }

        static size_t values_offset = 0;

        char buf[50];
        sprintf(buf, "avg %f", ms_per_frame_avg);
        ImGui::PlotLines("Frame Times", &ms_per_frame.front(), (int)ms_per_frame.size(), (int)values_offset, buf, min - max * 0.1f, max * 1.1f, ImVec2(0, 70));

        ImGui::SameLine(); ImGui::Checkbox("pause", &g_pause);

    }

    if (ImGui::CollapsingHeader("Window options"))
    {        
        if (ImGui::TreeNode("Window Size")){
            const char* items[] = { "640x480", "720x576", "1280x720", "1920x1080", "1920x1200", "2048x1536" };
            static int item2 = -1;
            bool press = ImGui::Combo("Window Size", &item2, items, IM_ARRAYSIZE(items));    

            if (press){
                glm::ivec2 win_re_size = glm::ivec2(640, 480);

                switch (item2){
                case 0:
                    win_re_size = glm::ivec2(640, 480);
                    break;
                case 1:
                    win_re_size = glm::ivec2(720, 576);
                    break;
                case 2:
                    win_re_size = glm::ivec2(1280, 720);
                    break;
                case 3:
                    win_re_size = glm::ivec2(1920, 1080);
                    break;
                case 4:
                    win_re_size = glm::ivec2(1920, 1200);
                    break;
                case 5:
                    win_re_size = glm::ivec2(1920, 1536);
                    break;
                default:
                    break;
                }                
                g_win.resize(win_re_size);                
            }

            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode("Background Color")){
            ImGui::ColorEdit3("BC", &g_background_color[0]);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Style Editor"))
        {
            ImGui::ShowStyleEditor();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Logging"))
        {
            ImGui::LogButtons();
            ImGui::TreePop();
        }
    }

    ImGui::End();
    g_show_transfer_function = ImGui::Begin("Transfer Function Window", &g_show_transfer_function_in_window, ImVec2(300, 500));

    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetItemBoxMin().y;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    static unsigned byte_size = 255;

    static ImVector<float> A; if (A.empty()){ A.resize(byte_size); }

    if (g_redraw_tf){
        g_redraw_tf = false;
		if (g_active_function == 0){
			image_data_type color_con = g_transfer_fun.get_RGBA_transfer_function_buffer();
			for (unsigned i = 0; i != byte_size; ++i){
				A[i] = color_con[i * 4 + 3];
			}
		}
		if (g_active_function == 2){
			image_data_type color_con = g_transfer_fun2.get_RGBA_transfer_function_buffer();
			for (unsigned i = 0; i != byte_size; ++i){
				A[i] = color_con[i * 4 + 3];
			}
		}
		if (g_active_function == 3){
			image_data_type color_con = g_transfer_fun3.get_RGBA_transfer_function_buffer();
			for (unsigned i = 0; i != byte_size; ++i){
				A[i] = color_con[i * 4 + 3];
			}
		}
		if (g_active_function == 4){
			image_data_type color_con = g_transfer_fun4.get_RGBA_transfer_function_buffer();
			for (unsigned i = 0; i != byte_size; ++i){
				A[i] = color_con[i * 4 + 3];
			}
		}



    }

    ImGui::PlotLines("", &A.front(), (int)A.size(), (int)0, "", 0.0, 255.0, ImVec2(0, 70));

    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetIO().DisplaySize.y - ImGui::GetItemBoxMin().y - 70;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    ImGui::SameLine(); ImGui::Text("Color:RGB Plot: Alpha");
	
	
	const char* items[] = { "Volume 1", "Volume 2", "Volume 3", "Volume 4" };
	static int item2 = -1;
	bool press = ImGui::Combo("Volume", &item2, items, IM_ARRAYSIZE(items));

	if (press){
		//g_active_function = 0;

		switch (item2){
		case 0:
			g_active_function = 0;
			g_transfer_dirty = true;
			g_redraw_tf = true;
			break;
		case 1:
			g_active_function = 2;
			g_transfer_dirty = true;
			g_redraw_tf = true;
			break;
		case 2:
			g_active_function = 3;
			g_transfer_dirty = true;
			g_redraw_tf = true;
			break;
		case 3:
			g_active_function = 4;
			g_transfer_dirty = true;
			g_redraw_tf = true;
			break;
		}
	}
	
    
	static int data_value = 0;
    ImGui::SliderInt("Data Value", &data_value, 0, 255);
    static float col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
    ImGui::ColorEdit4("color", col);
    bool add_entry_to_tf = false;
    add_entry_to_tf ^= ImGui::Button("Add entry"); ImGui::SameLine();

    bool reset_tf = false;
    reset_tf ^= ImGui::Button("Reset");

    if (reset_tf)
	
	{
		if (g_active_function == 0){
			g_transfer_fun.reset();
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 2){
			g_transfer_fun2.reset();
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 3){
			g_transfer_fun3.reset();
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 4){
			g_transfer_fun4.reset();
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}

    }



    if (add_entry_to_tf){
        
		if (g_active_function == 0){
			g_current_tf_data_value = data_value;
			g_transfer_fun.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 2){
			g_current_tf_data_value = data_value;
			g_transfer_fun2.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 3){
			g_current_tf_data_value = data_value;
			g_transfer_fun3.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}
		if (g_active_function == 4){
			g_current_tf_data_value = data_value;
			g_transfer_fun4.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
			g_transfer_dirty = true;
			g_redraw_tf = true;
		}

    }

	if (ImGui::CollapsingHeader("Manipulate Values")){
		if (g_active_function == 0){
			Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();

			bool delete_entry_from_tf = false;

			static std::vector<int> g_c_data_value;

			if (g_c_data_value.size() != con.size())
				g_c_data_value.resize(con.size());

			int i = 0;

			for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
			{

				int c_data_value = c->first;
				glm::vec4 c_color_value = c->second;

				g_c_data_value[i] = c_data_value;

				std::stringstream ss;
				if (c->first < 10)
					ss << c->first << "  ";
				else if (c->first < 100)
					ss << c->first << " ";
				else
					ss << c->first;

				bool change_value = false;
				change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

				if (change_value){
					if (con.find(g_c_data_value[i]) == con.end()){
						g_transfer_fun.remove(c_data_value);
						g_transfer_fun.add((unsigned)g_c_data_value[i], c_color_value);
						g_current_tf_data_value = g_c_data_value[i];
						g_transfer_dirty = true;
						g_redraw_tf = true;
					}
				}

				//delete             
				bool delete_entry_from_tf = false;
				delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

				if (delete_entry_from_tf){
					g_current_tf_data_value = c_data_value;
					g_transfer_fun.remove(g_current_tf_data_value);
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
				memcpy(&n_col, &c_color_value, sizeof(float) * 4);

				bool change_color = false;
				change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

				if (change_color){
					g_transfer_fun.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
					g_current_tf_data_value = g_c_data_value[i];
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				ImGui::Separator();

				++i;
			}
		}
		if (g_active_function == 2){
			Transfer_function::container_type con = g_transfer_fun2.get_piecewise_container();

			bool delete_entry_from_tf = false;

			static std::vector<int> g_c_data_value;

			if (g_c_data_value.size() != con.size())
				g_c_data_value.resize(con.size());

			int i = 0;

			for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
			{

				int c_data_value = c->first;
				glm::vec4 c_color_value = c->second;

				g_c_data_value[i] = c_data_value;

				std::stringstream ss;
				if (c->first < 10)
					ss << c->first << "  ";
				else if (c->first < 100)
					ss << c->first << " ";
				else
					ss << c->first;

				bool change_value = false;
				change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

				if (change_value){
					if (con.find(g_c_data_value[i]) == con.end()){
						g_transfer_fun2.remove(c_data_value);
						g_transfer_fun2.add((unsigned)g_c_data_value[i], c_color_value);
						g_current_tf_data_value = g_c_data_value[i];
						g_transfer_dirty = true;
						g_redraw_tf = true;
					}
				}

				//delete             
				bool delete_entry_from_tf = false;
				delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

				if (delete_entry_from_tf){
					g_current_tf_data_value = c_data_value;
					g_transfer_fun2.remove(g_current_tf_data_value);
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
				memcpy(&n_col, &c_color_value, sizeof(float) * 4);

				bool change_color = false;
				change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

				if (change_color){
					g_transfer_fun2.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
					g_current_tf_data_value = g_c_data_value[i];
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				ImGui::Separator();

				++i;
			}
		}
		if (g_active_function == 3){
			Transfer_function::container_type con = g_transfer_fun3.get_piecewise_container();

			bool delete_entry_from_tf = false;

			static std::vector<int> g_c_data_value;

			if (g_c_data_value.size() != con.size())
				g_c_data_value.resize(con.size());

			int i = 0;

			for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
			{

				int c_data_value = c->first;
				glm::vec4 c_color_value = c->second;

				g_c_data_value[i] = c_data_value;

				std::stringstream ss;
				if (c->first < 10)
					ss << c->first << "  ";
				else if (c->first < 100)
					ss << c->first << " ";
				else
					ss << c->first;

				bool change_value = false;
				change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

				if (change_value){
					if (con.find(g_c_data_value[i]) == con.end()){
						g_transfer_fun3.remove(c_data_value);
						g_transfer_fun3.add((unsigned)g_c_data_value[i], c_color_value);
						g_current_tf_data_value = g_c_data_value[i];
						g_transfer_dirty = true;
						g_redraw_tf = true;
					}
				}

				//delete             
				bool delete_entry_from_tf = false;
				delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

				if (delete_entry_from_tf){
					g_current_tf_data_value = c_data_value;
					g_transfer_fun3.remove(g_current_tf_data_value);
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
				memcpy(&n_col, &c_color_value, sizeof(float) * 4);

				bool change_color = false;
				change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

				if (change_color){
					g_transfer_fun3.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
					g_current_tf_data_value = g_c_data_value[i];
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				ImGui::Separator();

				++i;
			}
		}
		if (g_active_function == 4){
			Transfer_function::container_type con = g_transfer_fun4.get_piecewise_container();

			bool delete_entry_from_tf = false;

			static std::vector<int> g_c_data_value;

			if (g_c_data_value.size() != con.size())
				g_c_data_value.resize(con.size());

			int i = 0;

			for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
			{

				int c_data_value = c->first;
				glm::vec4 c_color_value = c->second;

				g_c_data_value[i] = c_data_value;

				std::stringstream ss;
				if (c->first < 10)
					ss << c->first << "  ";
				else if (c->first < 100)
					ss << c->first << " ";
				else
					ss << c->first;

				bool change_value = false;
				change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

				if (change_value){
					if (con.find(g_c_data_value[i]) == con.end()){
						g_transfer_fun4.remove(c_data_value);
						g_transfer_fun4.add((unsigned)g_c_data_value[i], c_color_value);
						g_current_tf_data_value = g_c_data_value[i];
						g_transfer_dirty = true;
						g_redraw_tf = true;
					}
				}

				//delete             
				bool delete_entry_from_tf = false;
				delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

				if (delete_entry_from_tf){
					g_current_tf_data_value = c_data_value;
					g_transfer_fun4.remove(g_current_tf_data_value);
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
				memcpy(&n_col, &c_color_value, sizeof(float) * 4);

				bool change_color = false;
				change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

				if (change_color){
					g_transfer_fun4.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
					g_current_tf_data_value = g_c_data_value[i];
					g_transfer_dirty = true;
					g_redraw_tf = true;
				}

				ImGui::Separator();

				++i;
			}
		}
	}


	if (ImGui::CollapsingHeader("Transfer Function - Save/Load", 0, true, false))
	{
		if (g_active_function == 0){
			ImGui::Text("Transferfunctions");
			bool load_tf_1 = false;
			bool load_tf_2 = false;
			bool load_tf_3 = false;
			bool load_tf_4 = false;
			bool load_tf_5 = false;
			bool load_tf_6 = false;
			bool save_tf_1 = false;
			bool save_tf_2 = false;
			bool save_tf_3 = false;
			bool save_tf_4 = false;
			bool save_tf_5 = false;
			bool save_tf_6 = false;

			save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
			load_tf_1 ^= ImGui::Button("Load TF1");
			save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
			load_tf_2 ^= ImGui::Button("Load TF2");
			save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
			load_tf_3 ^= ImGui::Button("Load TF3");
			save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
			load_tf_4 ^= ImGui::Button("Load TF4");
			save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
			load_tf_5 ^= ImGui::Button("Load TF5");
			save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
			load_tf_6 ^= ImGui::Button("Load TF6");

			if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
				Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
				std::vector<Transfer_function::element_type> save_vect;

				for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
				{
					save_vect.push_back(*c);
				}

				std::ofstream tf_file;

				if (save_tf_1){ tf_file.open("TF1", std::ios::out | std::ofstream::binary); }
				if (save_tf_2){ tf_file.open("TF2", std::ios::out | std::ofstream::binary); }
				if (save_tf_3){ tf_file.open("TF3", std::ios::out | std::ofstream::binary); }
				if (save_tf_4){ tf_file.open("TF4", std::ios::out | std::ofstream::binary); }
				if (save_tf_5){ tf_file.open("TF5", std::ios::out | std::ofstream::binary); }
				if (save_tf_6){ tf_file.open("TF6", std::ios::out | std::ofstream::binary); }

				//std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
				tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
				tf_file.close();
			}

			if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
				Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
				std::vector<Transfer_function::element_type> load_vect;

				std::ifstream tf_file;

				if (load_tf_1){ tf_file.open("TF1", std::ios::in | std::ifstream::binary); }
				if (load_tf_2){ tf_file.open("TF2", std::ios::in | std::ifstream::binary); }
				if (load_tf_3){ tf_file.open("TF3", std::ios::in | std::ifstream::binary); }
				if (load_tf_4){ tf_file.open("TF4", std::ios::in | std::ifstream::binary); }
				if (load_tf_5){ tf_file.open("TF5", std::ios::in | std::ifstream::binary); }
				if (load_tf_6){ tf_file.open("TF6", std::ios::in | std::ifstream::binary); }


				if (tf_file.good()){
					tf_file.seekg(0, tf_file.end);

					size_t size = tf_file.tellg();
					unsigned elements = (int)size / (unsigned)sizeof(Transfer_function::element_type);
					tf_file.seekg(0);

					load_vect.resize(elements);
					tf_file.read((char*)&load_vect[0], size);

					g_transfer_fun.reset();
					g_transfer_dirty = true;
					for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
					{
						g_transfer_fun.add(c->first, c->second);
					}
				}

				tf_file.close();

			}
		}
		if (g_active_function == 2){
			ImGui::Text("Transferfunctions");
			bool load_tf_1 = false;
			bool load_tf_2 = false;
			bool load_tf_3 = false;
			bool load_tf_4 = false;
			bool load_tf_5 = false;
			bool load_tf_6 = false;
			bool save_tf_1 = false;
			bool save_tf_2 = false;
			bool save_tf_3 = false;
			bool save_tf_4 = false;
			bool save_tf_5 = false;
			bool save_tf_6 = false;

			save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
			load_tf_1 ^= ImGui::Button("Load TF1");
			save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
			load_tf_2 ^= ImGui::Button("Load TF2");
			save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
			load_tf_3 ^= ImGui::Button("Load TF3");
			save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
			load_tf_4 ^= ImGui::Button("Load TF4");
			save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
			load_tf_5 ^= ImGui::Button("Load TF5");
			save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
			load_tf_6 ^= ImGui::Button("Load TF6");

			if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
				Transfer_function::container_type con = g_transfer_fun2.get_piecewise_container();
				std::vector<Transfer_function::element_type> save_vect;

				for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
				{
					save_vect.push_back(*c);
				}

				std::ofstream tf_file;

				if (save_tf_1){ tf_file.open("TF1", std::ios::out | std::ofstream::binary); }
				if (save_tf_2){ tf_file.open("TF2", std::ios::out | std::ofstream::binary); }
				if (save_tf_3){ tf_file.open("TF3", std::ios::out | std::ofstream::binary); }
				if (save_tf_4){ tf_file.open("TF4", std::ios::out | std::ofstream::binary); }
				if (save_tf_5){ tf_file.open("TF5", std::ios::out | std::ofstream::binary); }
				if (save_tf_6){ tf_file.open("TF6", std::ios::out | std::ofstream::binary); }

				//std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
				tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
				tf_file.close();
			}

			if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
				Transfer_function::container_type con = g_transfer_fun2.get_piecewise_container();
				std::vector<Transfer_function::element_type> load_vect;

				std::ifstream tf_file;

				if (load_tf_1){ tf_file.open("TF1", std::ios::in | std::ifstream::binary); }
				if (load_tf_2){ tf_file.open("TF2", std::ios::in | std::ifstream::binary); }
				if (load_tf_3){ tf_file.open("TF3", std::ios::in | std::ifstream::binary); }
				if (load_tf_4){ tf_file.open("TF4", std::ios::in | std::ifstream::binary); }
				if (load_tf_5){ tf_file.open("TF5", std::ios::in | std::ifstream::binary); }
				if (load_tf_6){ tf_file.open("TF6", std::ios::in | std::ifstream::binary); }


				if (tf_file.good()){
					tf_file.seekg(0, tf_file.end);

					size_t size = tf_file.tellg();
					unsigned elements = (int)size / (unsigned)sizeof(Transfer_function::element_type);
					tf_file.seekg(0);

					load_vect.resize(elements);
					tf_file.read((char*)&load_vect[0], size);

					g_transfer_fun2.reset();
					g_transfer_dirty = true;
					for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
					{
						g_transfer_fun2.add(c->first, c->second);
					}
				}

				tf_file.close();

			}
		}
		if (g_active_function == 3){
			ImGui::Text("Transferfunctions");
			bool load_tf_1 = false;
			bool load_tf_2 = false;
			bool load_tf_3 = false;
			bool load_tf_4 = false;
			bool load_tf_5 = false;
			bool load_tf_6 = false;
			bool save_tf_1 = false;
			bool save_tf_2 = false;
			bool save_tf_3 = false;
			bool save_tf_4 = false;
			bool save_tf_5 = false;
			bool save_tf_6 = false;

			save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
			load_tf_1 ^= ImGui::Button("Load TF1");
			save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
			load_tf_2 ^= ImGui::Button("Load TF2");
			save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
			load_tf_3 ^= ImGui::Button("Load TF3");
			save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
			load_tf_4 ^= ImGui::Button("Load TF4");
			save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
			load_tf_5 ^= ImGui::Button("Load TF5");
			save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
			load_tf_6 ^= ImGui::Button("Load TF6");

			if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
				Transfer_function::container_type con = g_transfer_fun3.get_piecewise_container();
				std::vector<Transfer_function::element_type> save_vect;

				for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
				{
					save_vect.push_back(*c);
				}

				std::ofstream tf_file;

				if (save_tf_1){ tf_file.open("TF1", std::ios::out | std::ofstream::binary); }
				if (save_tf_2){ tf_file.open("TF2", std::ios::out | std::ofstream::binary); }
				if (save_tf_3){ tf_file.open("TF3", std::ios::out | std::ofstream::binary); }
				if (save_tf_4){ tf_file.open("TF4", std::ios::out | std::ofstream::binary); }
				if (save_tf_5){ tf_file.open("TF5", std::ios::out | std::ofstream::binary); }
				if (save_tf_6){ tf_file.open("TF6", std::ios::out | std::ofstream::binary); }

				//std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
				tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
				tf_file.close();
			}

			if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
				Transfer_function::container_type con = g_transfer_fun3.get_piecewise_container();
				std::vector<Transfer_function::element_type> load_vect;

				std::ifstream tf_file;

				if (load_tf_1){ tf_file.open("TF1", std::ios::in | std::ifstream::binary); }
				if (load_tf_2){ tf_file.open("TF2", std::ios::in | std::ifstream::binary); }
				if (load_tf_3){ tf_file.open("TF3", std::ios::in | std::ifstream::binary); }
				if (load_tf_4){ tf_file.open("TF4", std::ios::in | std::ifstream::binary); }
				if (load_tf_5){ tf_file.open("TF5", std::ios::in | std::ifstream::binary); }
				if (load_tf_6){ tf_file.open("TF6", std::ios::in | std::ifstream::binary); }


				if (tf_file.good()){
					tf_file.seekg(0, tf_file.end);

					size_t size = tf_file.tellg();
					unsigned elements = (int)size / (unsigned)sizeof(Transfer_function::element_type);
					tf_file.seekg(0);

					load_vect.resize(elements);
					tf_file.read((char*)&load_vect[0], size);

					g_transfer_fun3.reset();
					g_transfer_dirty = true;
					for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
					{
						g_transfer_fun3.add(c->first, c->second);
					}
				}

				tf_file.close();

			}
		}
		if (g_active_function == 4){
			ImGui::Text("Transferfunctions");
			bool load_tf_1 = false;
			bool load_tf_2 = false;
			bool load_tf_3 = false;
			bool load_tf_4 = false;
			bool load_tf_5 = false;
			bool load_tf_6 = false;
			bool save_tf_1 = false;
			bool save_tf_2 = false;
			bool save_tf_3 = false;
			bool save_tf_4 = false;
			bool save_tf_5 = false;
			bool save_tf_6 = false;

			save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
			load_tf_1 ^= ImGui::Button("Load TF1");
			save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
			load_tf_2 ^= ImGui::Button("Load TF2");
			save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
			load_tf_3 ^= ImGui::Button("Load TF3");
			save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
			load_tf_4 ^= ImGui::Button("Load TF4");
			save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
			load_tf_5 ^= ImGui::Button("Load TF5");
			save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
			load_tf_6 ^= ImGui::Button("Load TF6");

			if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
				Transfer_function::container_type con = g_transfer_fun4.get_piecewise_container();
				std::vector<Transfer_function::element_type> save_vect;

				for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
				{
					save_vect.push_back(*c);
				}

				std::ofstream tf_file;

				if (save_tf_1){ tf_file.open("TF1", std::ios::out | std::ofstream::binary); }
				if (save_tf_2){ tf_file.open("TF2", std::ios::out | std::ofstream::binary); }
				if (save_tf_3){ tf_file.open("TF3", std::ios::out | std::ofstream::binary); }
				if (save_tf_4){ tf_file.open("TF4", std::ios::out | std::ofstream::binary); }
				if (save_tf_5){ tf_file.open("TF5", std::ios::out | std::ofstream::binary); }
				if (save_tf_6){ tf_file.open("TF6", std::ios::out | std::ofstream::binary); }

				//std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
				tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
				tf_file.close();
			}

			if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
				Transfer_function::container_type con = g_transfer_fun4.get_piecewise_container();
				std::vector<Transfer_function::element_type> load_vect;

				std::ifstream tf_file;

				if (load_tf_1){ tf_file.open("TF1", std::ios::in | std::ifstream::binary); }
				if (load_tf_2){ tf_file.open("TF2", std::ios::in | std::ifstream::binary); }
				if (load_tf_3){ tf_file.open("TF3", std::ios::in | std::ifstream::binary); }
				if (load_tf_4){ tf_file.open("TF4", std::ios::in | std::ifstream::binary); }
				if (load_tf_5){ tf_file.open("TF5", std::ios::in | std::ifstream::binary); }
				if (load_tf_6){ tf_file.open("TF6", std::ios::in | std::ifstream::binary); }


				if (tf_file.good()){
					tf_file.seekg(0, tf_file.end);

					size_t size = tf_file.tellg();
					unsigned elements = (int)size / (unsigned)sizeof(Transfer_function::element_type);
					tf_file.seekg(0);

					load_vect.resize(elements);
					tf_file.read((char*)&load_vect[0], size);

					g_transfer_fun4.reset();
					g_transfer_dirty = true;
					for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
					{
						g_transfer_fun4.add(c->first, c->second);
					}
				}

				tf_file.close();

			}
		}
	}

    ImGui::End();
}

int main(int argc, char* argv[])
{
    //g_win = Window(g_window_res);
    //InitImGui();
    ImGui_ImplGlfwGL3_Init(g_win.getGLFWwindow(), true);

    // initialize the transfer function

    // first clear possible old values
    g_transfer_fun.reset();
	g_transfer_fun2.reset();
	g_transfer_fun3.reset();
	g_transfer_fun4.reset();

    // the add_stop method takes:
    //  - unsigned char or float - data value     (0.0 .. 1.0) or (0..255)
    //  - vec4f         - color and alpha value   (0.0 .. 1.0) per channel
    g_transfer_fun.add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
    g_transfer_fun.add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));
	g_transfer_fun2.add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
	g_transfer_fun2.add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));
	g_transfer_fun3.add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
	g_transfer_fun3.add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));
	g_transfer_fun4.add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
	g_transfer_fun4.add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));
    g_transfer_dirty = true;

    ///NOTHING TODO HERE-------------------------------------------------------------------------------

    
	// init and upload volume texture
	glActiveTexture(GL_TEXTURE0);
    g_volume_texture0 = read_volume(g_file_string0);

	glActiveTexture(GL_TEXTURE2);
	g_volume_texture2 = read_volume(g_file_string2);

	glActiveTexture(GL_TEXTURE3);
	g_volume_texture3 = read_volume(g_file_string3);

	glActiveTexture(GL_TEXTURE4);
	g_volume_texture4 = read_volume(g_file_string4);

	//Delete from here----------------------------------------------------------------------------------------
	FILE *ptr = fopen("../../../data/ds14_scivis_0128_e4_dt04_1.0000", "rb");
	struct halo h;
	int status;
	if (!ptr)
	{
		printf("Unable to open file!\n");
		return 1;
	}
	size_t offset = 2720 + 16 * (4 + 20);
	fseek(ptr, offset, SEEK_SET);
	fseek(ptr, 0, SEEK_END);
	size_t ntotal = (ftell(ptr) - offset) / sizeof(halo);
	fseek(ptr, offset, SEEK_SET);
	int nhalos = ntotal;
	halo *halos = (halo *)malloc(nhalos * sizeof(halo));
	status = fread(halos, sizeof(halo), nhalos, ptr);
	if (status == -1){
		printf("Invalid read");
	}

	float acc_min = 0, acc_max = 0, vel_min = 0, vel_max = 0, phi_min = 0, phi_max = 0, den_min = 0, den_max = 0;
	size_t acc_width = 100, vel_width = 100, phi_width = 100, den_width = 100;

	size_t acc_index, vel_index, phi_index, den_index;
	float d_acc, d_vel, d_phi, d_den;

	float maxax = 0, minax = 0, maxay = 0, minay = 0, maxaz = 0, minaz = 0, minacc=0, maxacc=0;
	float maxvx = 0, minvx = 0, maxvy = 0, minvy = 0, maxvz = 0, minvz = 0, minvel=0, maxvel=0;
	float maxphi = 0, minphi = 0;
	float xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;
	int newx, newy, newz;
	float dx, dy, dz;

	for (int i = 0; i < ntotal; i++){

		if (halos[i].ax>maxax)
			maxax = halos[i].ax;
		if (halos[i].ax<minax)
			minax = halos[i].ax;
		if (halos[i].ay>maxay)
			maxay = halos[i].ay;
		if (halos[i].ay<minay)
			minay = halos[i].ay;
		if (halos[i].az>maxaz)
			maxaz = halos[i].az;
		if (halos[i].az<minaz)
			minaz = halos[i].az;

		if (halos[i].vx>maxvx)
			maxvx = halos[i].vx;
		if (halos[i].vx<minvx)
			minvx = halos[i].vx;
		if (halos[i].vy>maxvy)
			maxvy = halos[i].vy;
		if (halos[i].vy<minvy)
			minvy = halos[i].vy;
		if (halos[i].vz>maxvz)
			maxvz = halos[i].vz;
		if (halos[i].vz<minvz)
			minvz = halos[i].vz;

		if (halos[i].phi>maxphi)
			maxphi = halos[i].phi;
		if (halos[i].vx<minphi)
			minphi = halos[i].phi;

		if (halos[i].x>xmax)
			xmax = halos[i].x;
		if (halos[i].x<xmin)
			xmin = halos[i].x;
		if (halos[i].y>ymax)
			ymax = halos[i].y;
		if (halos[i].y<ymin)
			ymin = halos[i].y;
		if (halos[i].z>zmax)
			zmax = halos[i].z;
		if (halos[i].z<zmin)
			zmin = halos[i].z;
	}

	



	for (int i = 0; i < ntotal; i++){

		halos[i].ax = halos[i].ax - minax;
		halos[i].ay = halos[i].ay - minay;
		halos[i].az = halos[i].az - minaz;

		halos[i].vx = halos[i].vx - minvx;
		halos[i].vy = halos[i].vy - minvy;
		halos[i].vz = halos[i].vz - minvz;

		halos[i].phi = halos[i].phi - minphi;

		halos[i].x = halos[i].x - xmin;
		halos[i].y = halos[i].y - ymin;
		halos[i].z = halos[i].z - zmin;

	}

	dx = (xmax - xmin) / 20;
	dy = (ymax - ymin) / 20;
	dz = (zmax - zmin) / 20;

	int denser[20][20][20] = {};
	for (int i = 0; i < ntotal; i++){
		newx = (int)(halos[i].x / dx);
		if (newx == 20)
			newx = newx - 1;
		newy = (int)(halos[i].y / dy);
		if (newy == 20)
			newy = newy - 1;
		newz = (int)(halos[i].z / dz);
		if (newz == 20)
			newz = newz - 1;
		denser[newx][newy][newz] = denser[newx][newy][newz] + 1;
	}

	for (int i = 0; i < 20; i++)
		for (int j = 0; j < 20; j++)
			for (int k = 0; k < 20; k++){
		if (denser[i][j][k]>den_max)
			den_max = denser[i][j][k];
			}

	d_den = den_max / den_width;

	for (int i = 0; i < 20; i++)
		for (int j = 0; j < 20; j++)
			for (int k = 0; k < 20; k++){
		denser[i][j][k] = denser[i][j][k] /d_den;
		if (denser[i][j][k] = den_width)
			denser[i][j][k] = denser[i][j][k] - 1;
			}

	for (int i = 0; i < ntotal; i++){
		if (sqrt(pow(halos[i].ax,2)+pow(halos[i].ay,2)+pow(halos[i].az,2))>maxacc)
			maxacc = sqrt(pow(halos[i].ax, 2) + pow(halos[i].ay, 2) + pow(halos[i].az, 2));
		if (sqrt(pow(halos[i].vx, 2) + pow(halos[i].vy, 2) + pow(halos[i].vz, 2))>maxvel)
			maxvel = sqrt(pow(halos[i].vx, 2) + pow(halos[i].vy, 2) + pow(halos[i].vz, 2));
	}

	d_acc = maxacc / acc_width;
	d_vel = maxvel / vel_width;
	d_phi = (maxphi - minphi) / phi_width;

	
	float gist1[40000] = {};
	float gist2[40000] = {};
	float gist3[40000] = {};
	float gist4[40000] = {};
	float gist5[40000] = {};
	float gist6[40000] = {};

	

	for (int i = 0; i < ntotal; i++){
		acc_index = (size_t)((sqrt(pow(halos[i].ax, 2) + pow(halos[i].ay, 2) + pow(halos[i].az, 2))) / d_acc);
		if (acc_index == acc_width)
			acc_index = acc_index - 1;
		vel_index = (size_t)((sqrt(pow(halos[i].vx, 2) + pow(halos[i].vy, 2) + pow(halos[i].vz, 2))) / d_vel);
		if (vel_index == vel_width)
			vel_index = vel_index - 1;
		phi_index = (size_t)(halos[i].phi / d_phi);
		if (phi_index == phi_width)
			phi_index = phi_index - 1;
		int cx = (int)halos[i].x / dx;
		int cy = (int)halos[i].y / dy;
		int cz = (int)halos[i].z / dz;
		den_index = denser[cx][cy][cz];

		gist1[(acc_index + vel_index*acc_width) * 4] = 1.0;
		gist1[(acc_index + vel_index*acc_width) * 4 + 1] = 1.0;
		gist1[(acc_index + vel_index*acc_width) * 4 + 2] = 1.0;
		gist1[(acc_index + vel_index*acc_width) * 4 + 3] = gist1[(acc_index + vel_index*acc_width) * 4 + 3] + 0.01;

		gist2[(acc_index + den_index*acc_width) * 4] = 1.0;
		gist2[(acc_index + den_index*acc_width) * 4 + 1] = 1.0;
		gist2[(acc_index + den_index*acc_width) * 4 + 2] = 1.0;
		gist2[(acc_index + den_index*acc_width) * 4 + 3] = gist2[(acc_index + den_index*acc_width) * 4 + 3] + 0.01;

		gist3[(acc_index + phi_index*acc_width) * 4] = 1.0;
		gist3[(acc_index + phi_index*acc_width) * 4 + 1] = 1.0;
		gist3[(acc_index + phi_index*acc_width) * 4 + 2] = 1.0;
		gist3[(acc_index + phi_index*acc_width) * 4 + 3] = gist3[(acc_index + phi_index*acc_width) * 4 + 3] + 0.01;

		gist4[(vel_index + den_index*vel_width) * 4] = 1.0;
		gist4[(vel_index + den_index*vel_width) * 4 + 1] = 1.0;
		gist4[(vel_index + den_index*vel_width) * 4 + 2] = 1.0;
		gist4[(vel_index + den_index*vel_width) * 4 + 3] = gist4[(vel_index + den_index*vel_width) * 4 + 3] + 0.01;

		gist5[(vel_index + phi_index*vel_width) * 4] = 1.0;
		gist5[(vel_index + phi_index*vel_width) * 4 + 1] = 1.0;
		gist5[(vel_index + phi_index*vel_width) * 4 + 2] = 1.0;
		gist5[(vel_index + phi_index*vel_width) * 4 + 3] = gist5[(vel_index + phi_index*vel_width) * 4 + 3] + 0.01;

		gist6[(den_index + phi_index*den_width) * 4] = 1.0;
		gist6[(den_index + phi_index*den_width) * 4 + 1] = 1.0;
		gist6[(den_index + phi_index*den_width) * 4 + 2] = 1.0;
		gist6[(den_index + phi_index*den_width) * 4 + 3] = gist6[(den_index + phi_index*den_width) * 4 + 3] + 0.01;

	}

	//To here-------------------------------------------------------------------------------------------------

    // init and upload transfer function texture
    glActiveTexture(GL_TEXTURE1);
    g_transfer_texture = createTexture2D(255u, 1u, (char*)&g_transfer_fun.get_RGBA_transfer_function_buffer()[0]);
	glActiveTexture(GL_TEXTURE5);
	g_transfer_texture2 = createTexture2D(255u, 1u, (char*)&g_transfer_fun2.get_RGBA_transfer_function_buffer()[0]);
	glActiveTexture(GL_TEXTURE6);
	g_transfer_texture3 = createTexture2D(255u, 1u, (char*)&g_transfer_fun3.get_RGBA_transfer_function_buffer()[0]);
	glActiveTexture(GL_TEXTURE7);
	g_transfer_texture4 = createTexture2D(255u, 1u, (char*)&g_transfer_fun4.get_RGBA_transfer_function_buffer()[0]);

	// init and upload multi dimensional transfer function texture
	float pixels1[] = {
		243.0f, 115.0f, 0.0f, 0.001f, //orange
		204.0f, 232.0f, 139.0f, 0.05f, // light green
		0.0f, 136.0f, 55.0f, 0.05f, // green
		254.0f, 154.0f, 166.0f, 0.1f, //light pink
		230.0f, 230.0f, 230.0f, 0.1f, //white
		154.0f, 201.0f, 213.0f, 0.1f, //light blue
		240.0f, 4.0f, 127.0f, 0.1f, //super pink
		205.0f, 154.0f, 204.0f, 0.1f, //purple
		90.0f, 77.0f, 164.0f, 0.1f // violet
	};
	glActiveTexture(GL_TEXTURE8);
	g_multi_tran1 = multiTexture2D(3, 3, pixels1);

	float pixels2[] = {
		196.0f, 179.0f, 216.0f, 0.8f, //light violet
		230.0f, 230.0f, 230.0f, 0.3f, //light grey
		255.0f, 204.0f, 128.0f, 0.3f, // light orange
		124.0f, 103.0f, 171.0f, 0.2f, //violet
		191.0f, 191.0f, 191.0f, 0.2f, //grey
		243.0f, 89.0f, 38.0f, 0.5f, //orange
		36.0f, 13.0f, 94.0f, 0.001f, //super violet
		127.0f, 127.0f, 127.0f, 0.3f, //super grey
		179.0f, 0.0f, 0.0f, 0.5f //super orange

	};
	glActiveTexture(GL_TEXTURE9);
	g_multi_tran2 = multiTexture2D(3, 3, pixels2);

	float pixels3[] = {
		255.0f, 255.0f, 255.0f, 0.05f, //white
		180.0f, 211.0f, 225.0f, 0.05f, //ligth blue
		80.0f, 157.0f, 194.0f, 0.05f, // blue
		243.0f, 230.0f, 179.0f, 0.1f, //light yellow
		179.0f, 179.0f, 179.0f, 0.1f, //grey
		55.0f, 99.0f, 135.0f, 0.1f, //dark blue
		243.0f, 179.0f, 0.0f, 0.001f, //yellow
		179.0f, 102.0f, 0.0f, 0.1f, //brown
		0.0f, 0.0f, 0.0f, 0.1f // black
	};
	glActiveTexture(GL_TEXTURE10);
	g_multi_tran3 = multiTexture2D(3, 3, pixels3);


	
	//other way around just in case
	//float pixels4[] = {
	//	240.0f, 4.0f, 127.0f, 0.2f, //super pink
	//	205.0f, 154.0f, 204.0f, 0.3f, //purple
	//	90.0f, 77.0f, 164.0f, 0.3f, // violet
	//	254.0f, 154.0f, 166.0f, 0.2f, //light pink
	//	230.0f, 230.0f, 230.0f, 0.2f, //white
	//	154.0f, 201.0f, 213.0f, 0.5f, //light blue
	//	243.0f, 115.0f, 0.0f, 0.005f, //orange
	//	204.0f, 232.0f, 139.0f, 0.3f, // light green
	//	0.0f, 136.0f, 55.0f, 0.3f // green

	//};

	float pixels4[] = {
		204.0f, 232.0f, 215.0f, 0.0f, //light green
		206.0f, 120.0f, 237.0f, 0.0f, //light blue
		251.0f, 180.0f, 217.0f, 0.1f, // light pink
		128.0f, 195.0f, 155.0f, 0.1f, //green
		133.0f, 168.0f, 208.0f, 0.1f, //blue
		246.0f, 104.0f, 179.0f, 0.1f, //pink
		0.0f, 136.0f, 55.0f, 0.0f, //dark green
		10.0f, 80.0f, 161.0f, 0.1f, //dark blue
		214.0f, 0.0f, 102.0f, 0.1f //dark pink

	};
	glActiveTexture(GL_TEXTURE11);
	g_multi_tran4 = multiTexture2D(3, 3, pixels4);



	glActiveTexture(GL_TEXTURE12);
	g_gisto1 = multiTexture2D(100, 100, gist1);

	glActiveTexture(GL_TEXTURE13);
	g_gisto2 = multiTexture2D(100, 100, gist2);

	glActiveTexture(GL_TEXTURE14);
	g_gisto3 = multiTexture2D(100, 100, gist3);

	glActiveTexture(GL_TEXTURE15);
	g_gisto4 = multiTexture2D(100, 100, gist4);

	glActiveTexture(GL_TEXTURE16);
	g_gisto5 = multiTexture2D(100, 100, gist5);

	glActiveTexture(GL_TEXTURE17);
	g_gisto6 = multiTexture2D(100, 100, gist6);




    // loading actual raytracing shader code (volume.vert, volume.frag)
    // edit volume.frag to define the result of our volume raycaster  
    try {
        g_volume_program = loadShaders(g_file_vertex_shader, g_file_fragment_shader,
            g_task_chosen,
            g_lighting_toggle,
            g_shadow_toggle,
            g_opacity_correction_toggle,
			g_vol0_toggle,
			g_vol2_toggle,
			g_vol3_toggle,
			g_vol4_toggle,
			g_pair_chosen,
			g_func_chosen);
    }
    catch (std::logic_error& e) {
        //std::cerr << e.what() << std::endl;
        std::stringstream ss;
        ss << e.what() << std::endl;
        g_error_message = ss.str();
        g_reload_shader_error = true;
    }

    // init object manipulator (turntable)
    Manipulator manipulator;

    // manage keys here
    // add new input if neccessary (ie changing sampling distance, isovalues, ...)
    while (!g_win.shouldClose()) {
        float sampling_fact = g_sampling_distance_fact;
        if (!first_frame > 0.0){

            // exit window with escape
            if (ImGui::IsKeyPressed(GLFW_KEY_ESCAPE)) {                
                g_win.stop();
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_LEFT)) {
                g_light_pos.x -= 0.5f;
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT)) {
                g_light_pos.x += 0.5f;
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_UP)) {
                g_light_pos.z -= 0.5f;
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_DOWN)) {
                g_light_pos.z += 0.5f;
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_MINUS)) {
                g_iso_value -= 0.002f;
                g_iso_value = std::max(g_iso_value, 0.0f);
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_EQUAL) || ImGui::IsKeyPressed(GLFW_KEY_KP_ADD)) {
                g_iso_value += 0.002f;
                g_iso_value = std::min(g_iso_value, 1.0f);
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_D)) {
                g_sampling_distance -= 0.0001f;
                g_sampling_distance = std::max(g_sampling_distance, 0.0001f);
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_S)) {
                g_sampling_distance += 0.0001f;
                g_sampling_distance = std::min(g_sampling_distance, 0.2f);
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_R)) {
                g_reload_shader = true;
            }
                        
            if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_LEFT) || ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_MIDDLE) || ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
                sampling_fact = g_sampling_distance_fact_move;
            }   
                       
        }
        // to add key inputs:
        // check ImGui::IsKeyPressed(KEY_NAME)
        // - KEY_NAME - key name      (GLFW_KEY_A ... GLFW_KEY_Z)

        //if (ImGui::IsKeyPressed(GLFW_KEY_X)){
        //    
        //        ... do something
        //    
        //}

        /// reload shader if key R ist pressed
        if (g_reload_shader){

            GLuint newProgram(0);
            try {
                //std::cout << "Reload shaders" << std::endl;
                newProgram = loadShaders(g_file_vertex_shader, g_file_fragment_shader, g_task_chosen, g_lighting_toggle, g_shadow_toggle, g_opacity_correction_toggle, g_vol0_toggle, g_vol2_toggle, g_vol3_toggle, g_vol4_toggle, g_pair_chosen, g_func_chosen);
                g_error_message = "";
            }
            catch (std::logic_error& e) {
                //std::cerr << e.what() << std::endl;
                std::stringstream ss;
                ss << e.what() << std::endl;
                g_error_message = ss.str();
                g_reload_shader_error = true;
                newProgram = 0;
            }
            if (0 != newProgram) {
                glDeleteProgram(g_volume_program);
                g_volume_program = newProgram;
                g_reload_shader_error = false;

            }
            else
            {
                g_reload_shader_error = true;

            }
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_R)) {
            if (g_reload_shader_pressed != true) {
                g_reload_shader = true;
                g_reload_shader_pressed = true;
            }
            else{
                g_reload_shader = false;
            }
        }
        else {
            g_reload_shader = false;
            g_reload_shader_pressed = false;
        }


        if (g_transfer_dirty && !first_frame){
            g_transfer_dirty = false;

            static unsigned byte_size = 255;
			if (g_active_function == 0)
				image_data_type color_con = g_transfer_fun.get_RGBA_transfer_function_buffer();
			if (g_active_function == 2)
				image_data_type color_con = g_transfer_fun2.get_RGBA_transfer_function_buffer();
			if (g_active_function == 3)
				image_data_type color_con = g_transfer_fun3.get_RGBA_transfer_function_buffer();
			if (g_active_function == 4)
				image_data_type color_con = g_transfer_fun4.get_RGBA_transfer_function_buffer();

			if (g_active_function == 0)
			glActiveTexture(GL_TEXTURE1);
            updateTexture2D(g_transfer_texture, 255u, 1u, (char*)&g_transfer_fun.get_RGBA_transfer_function_buffer()[0]);
			if (g_active_function == 2)
			glActiveTexture(GL_TEXTURE5);
			updateTexture2D(g_transfer_texture2, 255u, 1u, (char*)&g_transfer_fun2.get_RGBA_transfer_function_buffer()[0]);
			if (g_active_function == 3)
			glActiveTexture(GL_TEXTURE6);
			updateTexture2D(g_transfer_texture3, 255u, 1u, (char*)&g_transfer_fun3.get_RGBA_transfer_function_buffer()[0]);
			if (g_active_function == 4)
			glActiveTexture(GL_TEXTURE7);
			updateTexture2D(g_transfer_texture4, 255u, 1u, (char*)&g_transfer_fun4.get_RGBA_transfer_function_buffer()[0]);
            
        }
        
        if (g_bilinear_interpolation){
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else{
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        glm::ivec2 size = g_win.windowSize();
        glViewport(0, 0, size.x, size.y);
        glClearColor(g_background_color.x, g_background_color.y, g_background_color.z, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float fovy = 45.0f;
        float aspect = (float)size.x / (float)size.y;
        float zNear = 0.025f, zFar = 10.0f;
        glm::mat4 projection = glm::perspective(fovy, aspect, zNear, zFar);

        glm::vec3 translate_rot = g_max_volume_bounds * glm::vec3(-0.5f, -0.5f, -0.5f);
        glm::vec3 translate_pos = g_max_volume_bounds * glm::vec3(+0.5f, -0.0f, -0.0f);

        glm::vec3 eye = glm::vec3(0.0f, 0.0f, 1.5f);
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        glm::mat4 view = glm::lookAt(eye, target, up);

        glm::mat4 turntable_matrix = manipulator.matrix();

        if (!g_over_gui){
            turntable_matrix = manipulator.matrix(g_win);
        }

        glm::mat4 model_view = view
            * glm::translate(translate_pos)
            * turntable_matrix
            // rotate head upright
            * glm::rotate(0.5f*float(M_PI), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(0.5f*float(M_PI), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::translate(translate_rot)
            ;

        glm::vec4 camera_translate = glm::column(glm::inverse(model_view), 3);
        glm::vec3 camera_location = glm::vec3(camera_translate.x, camera_translate.y, camera_translate.z);

        camera_location /= glm::vec3(camera_translate.w);

        glm::vec4 light_location = glm::vec4(g_light_pos, 1.0f) * model_view;

       
		//glBindTexture(GL_TEXTURE_2D, g_transfer_texture2);
		//glBindTexture(GL_TEXTURE_2D, g_transfer_texture3);
		//glBindTexture(GL_TEXTURE_2D, g_transfer_texture4);

        glUseProgram(g_volume_program);

		glUniform1i(glGetUniformLocation(g_volume_program, "volume_texture0"), 0);
		glUniform1i(glGetUniformLocation(g_volume_program, "volume_texture2"), 2);
		glUniform1i(glGetUniformLocation(g_volume_program, "volume_texture3"), 3);
		glUniform1i(glGetUniformLocation(g_volume_program, "volume_texture4"), 4);
		
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, g_transfer_texture);
		glUniform1i(glGetUniformLocation(g_volume_program, "transfer_texture"), 1);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, g_transfer_texture2);
		glUniform1i(glGetUniformLocation(g_volume_program, "transfer_texture2"), 5);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, g_transfer_texture3);
		glUniform1i(glGetUniformLocation(g_volume_program, "transfer_texture3"), 6);
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, g_transfer_texture4);
		glUniform1i(glGetUniformLocation(g_volume_program, "transfer_texture4"), 7);

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, g_multi_tran1);
		glUniform1i(glGetUniformLocation(g_volume_program, "multi_transfer1"), 8);
		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, g_multi_tran2);
		glUniform1i(glGetUniformLocation(g_volume_program, "multi_transfer2"), 9);
		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_2D, g_multi_tran3);
		glUniform1i(glGetUniformLocation(g_volume_program, "multi_transfer3"), 10);
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, g_multi_tran4);
		glUniform1i(glGetUniformLocation(g_volume_program, "multi_transfer4"), 11);

		//glActiveTexture(GL_TEXTURE12);
		//glBindTexture(GL_TEXTURE_2D, g_gisto1);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram1"), 12);
		//glActiveTexture(GL_TEXTURE13);
		//glBindTexture(GL_TEXTURE_2D, g_gisto2);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram2"), 13);
		//glActiveTexture(GL_TEXTURE14);
		//glBindTexture(GL_TEXTURE_2D, g_gisto3);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram3"), 14);
		//glActiveTexture(GL_TEXTURE15);
		//glBindTexture(GL_TEXTURE_2D, g_gisto4);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram4"), 15);
		//glActiveTexture(GL_TEXTURE16);
		//glBindTexture(GL_TEXTURE_2D, g_gisto5);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram5"), 16);
		//glActiveTexture(GL_TEXTURE17);
		//glBindTexture(GL_TEXTURE_2D, g_gisto6);
		//glUniform1i(glGetUniformLocation(g_volume_program, "gistogram6"), 17);

        glUniform3fv(glGetUniformLocation(g_volume_program, "camera_location"), 1,
            glm::value_ptr(camera_location));
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance"), g_sampling_distance * sampling_fact);
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance_ref"), g_sampling_distance_fact_ref);
        glUniform1f(glGetUniformLocation(g_volume_program, "iso_value"), g_iso_value);
        glUniform3fv(glGetUniformLocation(g_volume_program, "max_bounds"), 1,
            glm::value_ptr(g_max_volume_bounds));
        glUniform3iv(glGetUniformLocation(g_volume_program, "volume_dimensions"), 1,
            glm::value_ptr(g_vol_dimensions));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_position"), 1,
            glm::value_ptr(g_light_pos));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_ambient_color"), 1,
            glm::value_ptr(g_ambient_light_color));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_diffuse_color"), 1,
            glm::value_ptr(g_diffuse_light_color));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_specular_color"), 1,
            glm::value_ptr(g_specula_light_color));
        glUniform1f(glGetUniformLocation(g_volume_program, "light_ref_coef"), g_ref_coef);

        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Modelview"), 1, GL_FALSE,
            glm::value_ptr(model_view));
        if (!g_pause)
            g_cube.draw();
        glUseProgram(0);

        //IMGUI ROUTINE begin    
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel = 0;
        mousePressed[0] = mousePressed[1] = false;
        glfwPollEvents();
        UpdateImGui();
        showGUI();

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        //IMGUI ROUTINE end
        
        
		
		//displaying correct transfer function in transfer function window
		if ((g_show_transfer_function) && (g_active_function == 0)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_transfer_texture);
			g_transfer_fun.draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		}
		if ((g_show_transfer_function) && (g_active_function == 2)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_transfer_texture2);
			g_transfer_fun2.draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture2);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		}

		if ((g_show_transfer_function) && (g_active_function == 3)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_transfer_texture3);
			g_transfer_fun3.draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture3);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		}

		if ((g_show_transfer_function) && (g_active_function == 4)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_transfer_texture4);
			g_transfer_fun4.draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture4);
			glBindTexture(GL_TEXTURE_2D, 0);
			
		}
		//displaying multidimensional transfer function currently in use
		if ((g_task_chosen == 5) && (g_func_chosen == 2)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_multi_tran1);
			g_transfer_fun.draw_texture(g_multi_function_pos, g_multi_function_size, g_multi_tran1);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_func_chosen == 3)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_multi_tran2);
			g_transfer_fun.draw_texture(g_multi_function_pos, g_multi_function_size, g_multi_tran2);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_func_chosen == 4)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_multi_tran3);
			g_transfer_fun.draw_texture(g_multi_function_pos, g_multi_function_size, g_multi_tran3);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_func_chosen == 5)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_multi_tran4);
			g_transfer_fun.draw_texture(g_multi_function_pos, g_multi_function_size, g_multi_tran4);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		//displaying histograms
		if ((g_task_chosen == 5) && (g_pair_chosen == 12)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto1);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto1);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_pair_chosen == 13)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto2);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto2);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_pair_chosen == 14)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto3);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto3);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_pair_chosen == 23)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto4);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto4);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_pair_chosen == 24)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto5);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto5);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if ((g_task_chosen == 5) && (g_pair_chosen == 34)){
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, g_gisto6);
			g_transfer_fun.draw_texture(g_gist_pos, g_gist_size, g_gisto6);
			glBindTexture(GL_TEXTURE_2D, 0);
		}



		glBindTexture(GL_TEXTURE_2D, 0);
		
		g_win.update();
        first_frame = false;
    }

    //ImGui::Shutdown();
    ImGui_ImplGlfwGL3_Shutdown();

    return 0;
}
