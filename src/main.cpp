#include <algorithm>
#include <filesystem>
#include <iostream>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "diagram.h"
#include "knot.h"
#include "history.h"
#include "shader.h"
#include "to_string.h"

// Data that will be associated with the GLFW window
struct InputData
{
    bool imgui_active = false;
} input_data;

// Viewport and camera settings
const uint32_t window_w = 1200;
const uint32_t window_h = 800;
const uint32_t ui_w = 720;
const uint32_t ui_h = 720;
const uint32_t depth_w = 1024;
const uint32_t depth_h = 1024;
bool first_mouse = true;
float last_x;
float last_y;
float zoom = 45.0f;
glm::mat4 arcball_camera_matrix = glm::lookAt(glm::vec3{ 0.0f, 0.0f, 20.0f }, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
glm::mat4 arcball_model_matrix = glm::mat4{ 1.0f };

// Global settings
bool simulation_active = false;

// Appearance settings
ImVec4 clear_color = ImVec4(0.311f, 0.320f, 0.343f, 1.0f);

// File paths
std::vector<std::string> available_csvs;
std::string current_csv;

// Cromwell move options
std::vector<std::string> cromwell_moves = { "Translation", "Commutation", "Stabilization", "Destabilization" };
std::string current_move = cromwell_moves[0];
int commutation_row_or_col = 0;
int commutation_index = 0;
int stabilization_cardinal = 0;
int stabilization_index_i = 0;
int stabilization_index_j = 0;
int destabilization_index_i = 0;
int destabilization_index_j = 0;

// The main window handle
GLFWwindow* window;

/**
 * A function for handling scrolling.
 */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (zoom >= 1.0f && zoom <= 90.0f)
    {
        zoom -= yoffset;
    }
    if (zoom <= 1.0f)
    {
        zoom = 1.0f;
    }
    if (zoom >= 90.0f)
    {
        zoom = 90.0f;
    }
}

/**
 * A function for handling key presses.
 */
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        // Close the GLFW window
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        // Reset the arcball camera
        arcball_camera_matrix = glm::lookAt(glm::vec3{ 6.0f, 0.0f, 0.0f }, glm::vec3{ 0.0f }, glm::vec3{ 1.0f, 1.0f, 0.0f });
        arcball_model_matrix = glm::mat4{ 1.0f };
    }
}

/**
 * Get a normalized vector from the center of a virtual sphere centered at the origin to
 * a point `point_on_sphere` on the virtual ball surface, such that `point_on_sphere`
 * is aligned on screen's (x, y) coordinates.  If (x, y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 get_arcball_vector(int x, int y)
{
    auto point_on_sphere = glm::vec3{
        1.0f * x / window_w * 2.0f - 1.0f,
        1.0f * y / window_h * 2.0f - 1.0f,
        0.0f
    };

    point_on_sphere.y = -point_on_sphere.y;

    const float op_squared = point_on_sphere.x * point_on_sphere.x + point_on_sphere.y * point_on_sphere.y;

    if (op_squared <= 1.0f * 1.0f)
    {
        // Pythagorean theorem
        point_on_sphere.z = sqrt(1.0f * 1.0f - op_squared);
    }
    else
    {
        // Nearest point
        point_on_sphere = glm::normalize(point_on_sphere);
    }

    return point_on_sphere;
}

/**
 * Performs arcball camera calculations.
 */
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // First, check if the user is interacting with the ImGui interface - if they are,
    // we don't want to process mouse events any further
    auto input_data = static_cast<InputData*>(glfwGetWindowUserPointer(window));

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !input_data->imgui_active)
    {
        if (first_mouse)
        {
            last_x = xpos;
            last_y = ypos;
            first_mouse = false;
        }

        if (xpos != last_x || ypos != last_y)
        {
            const float rotation_speed = 0.25f;

            const glm::vec3 va = get_arcball_vector(last_x, last_y);
            const glm::vec3 vb = get_arcball_vector(xpos, ypos);
            const float angle = acos(std::min(1.0f, glm::dot(va, vb))) * rotation_speed;
            const glm::vec3 axis_camera_coordinates = glm::cross(va, vb);

            const glm::mat3 camera_to_object = glm::inverse(glm::mat3(arcball_camera_matrix) * glm::mat3(arcball_model_matrix));

            const glm::vec3 axis_in_object_coord = camera_to_object * axis_camera_coordinates;

            arcball_model_matrix = glm::rotate(arcball_model_matrix, glm::degrees(angle), axis_in_object_coord);

            // Set last to current
            last_x = xpos;
            last_y = ypos;
        }
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
    {
        last_x = xpos;
        last_y = ypos;
    }
}

/**
 * Debug function that will be used internally by OpenGL to print out warnings, errors, etc.
 */
void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* user_param)
{
    auto const src_str = [source]() 
    {
        switch (source)
        {
        case GL_DEBUG_SOURCE_API: return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
        case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER: return "OTHER";
        }
    }();

    auto const type_str = [type]() 
    {
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR: return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER: return "MARKER";
        case GL_DEBUG_TYPE_OTHER: return "OTHER";
        }
    }();

    auto const severity_str = [severity]() 
    {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
        case GL_DEBUG_SEVERITY_LOW: return "LOW";
        case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
        case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
        }
    }();

    std::cout << src_str << ", " << type_str << ", " << severity_str << ", " << id << ": " << message << "\n";
}

/**
 * Draw the UI elements corresponding to the specified grid diagram (i.e. a matrix of x's and o's).
 */
void draw_diagram(const knot::Diagram& diagram)
{
    ImGui::Text("Grid diagram is %u x %u", diagram.get_number_of_rows(), diagram.get_number_of_cols());

    const auto text_size = ImGui::CalcTextSize("x");
    const auto button_dims = std::max(text_size.x, text_size.y) * 2;

    for (size_t i = 0; i < diagram.get_size(); ++i)
    {
        for (size_t j = 0; j < diagram.get_size(); ++j)
        {
            std::string label = utils::to_string(diagram.get_data()[i][j]);
            
            // Based on the current "edit" (i.e. Cromwell) mode, highlight certain grid cells
            bool pop_color = false;
            auto selectable_color = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
            selectable_color.x *= 0.75f;
            selectable_color.y *= 0.75f;
            selectable_color.z *= 0.75f;
            const auto selected_color = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];

            if (current_move == "Commutation")
            {
                ImGui::GetColorU32(ImGuiCol_ButtonHovered);
                if ((static_cast<knot::Axis>(commutation_row_or_col) == knot::Axis::ROW && commutation_index == i) ||
                    (static_cast<knot::Axis>(commutation_row_or_col) == knot::Axis::COL && commutation_index == j))
                {
                    pop_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Button, selected_color);
                }
            }
            else if (current_move == "Stabilization")
            {  
                if (stabilization_index_i == i && stabilization_index_j == j)
                {
                    pop_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Button, selected_color);
                }
                else if (label != " ")
                {
                    pop_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Button, selectable_color);
                }
            }
            else if (current_move == "Destabilization")
            {
                if ((destabilization_index_i == i + 0 && destabilization_index_j == j + 0) ||   // UL
                    (destabilization_index_i == i - 1 && destabilization_index_j == j + 0) ||   // LL
                    (destabilization_index_i == i + 0 && destabilization_index_j == j - 1) ||   // UR
                    (destabilization_index_i == i - 1 && destabilization_index_j == j - 1))     // LR
                {
                    pop_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Button, selected_color);
                }
            }

            // If this is the last row, we only want to remove the horizontal item spacing
            if (i != diagram.get_size() - 1)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
            }
            else
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, ImGui::GetStyle().ItemSpacing.y });
            }

            // Finally, draw the button
            ImGui::Button(label.c_str(), { button_dims, button_dims });

            if (pop_color)
            {
                ImGui::PopStyleColor();
            }

            if (j != diagram.get_size() - 1)
            {
                ImGui::SameLine();
            }

            ImGui::PopStyleVar();

        }
    }
}

/**
 * Initialize GLFW and the OpenGL context.
 */
void initialize()
{
    // Create and configure the GLFW window 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(window_w, window_h, "Grid Diagrams for Knots", nullptr, nullptr);

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetWindowUserPointer(window, &input_data);

    // Load function pointers from glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Setup initial OpenGL state
    {
#if defined(_DEBUG)
        // Debug logging
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(message_callback, nullptr);
#endif
        // Depth testing
        glEnable(GL_DEPTH_TEST);

        // Backface culling for optimization
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // Configure point and line size
        glLineWidth(8.0f);
        glEnable(GL_PROGRAM_POINT_SIZE);
    }
}

/**
 * Load all available knot .csv files from the "diagrams" folder.
 */
void load_csvs()
{
    for (auto& directory_entry : std::filesystem::directory_iterator("../diagrams")) 
    {
        if (directory_entry.path().extension() == ".csv")
        {
            std::cout << "Found knot diagram .csv at: " << directory_entry.path().generic_string() << std::endl;
            available_csvs.push_back(directory_entry.path().generic_string());
        }
    }
}

uint32_t vao_tube;
uint32_t vbo_tube_position;

uint32_t vao_curve;
uint32_t vbo_curve_position;
uint32_t vbo_curve_stuck;

uint32_t framebuffer_ui;
uint32_t texture_ui;
uint32_t renderbuffer_ui;

uint32_t framebuffer_depth;
uint32_t texture_depth;

/**
 * Build the VAOs and VBOs used for rendering.
 */
void build_vaos(const std::vector<glm::vec3>& tube_data,
                const std::vector<glm::vec3>& curve_data,
                const std::vector<int32_t>& stuck_data)
{
    // Initialize objects for rendering the tube mesh
    glCreateVertexArrays(1, &vao_tube);

    glCreateBuffers(1, &vbo_tube_position);
    glNamedBufferStorage(vbo_tube_position, sizeof(glm::vec3) * tube_data.size(), tube_data.data(), GL_DYNAMIC_STORAGE_BIT);

    glVertexArrayVertexBuffer(vao_tube, 0, vbo_tube_position, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(vao_tube, 0);
    glVertexArrayAttribFormat(vao_tube, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao_tube, 0, 0);

    // Initialize objects for rendering the curve mesh
    glCreateVertexArrays(1, &vao_curve);

    glCreateBuffers(1, &vbo_curve_position);
    glNamedBufferStorage(vbo_curve_position, sizeof(glm::vec3) * curve_data.size(), curve_data.data(), GL_DYNAMIC_STORAGE_BIT);

    glCreateBuffers(1, &vbo_curve_stuck);
    glNamedBufferStorage(vbo_curve_stuck, sizeof(int32_t) * stuck_data.size(), stuck_data.data(), GL_DYNAMIC_STORAGE_BIT);

    glVertexArrayVertexBuffer(vao_curve, 0, vbo_curve_position, 0, sizeof(glm::vec3));
    glVertexArrayVertexBuffer(vao_curve, 1, vbo_curve_stuck, 0, sizeof(int32_t)); 

    glEnableVertexArrayAttrib(vao_curve, 0);
    glVertexArrayAttribFormat(vao_curve, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao_curve, 0, 0);

    glEnableVertexArrayAttrib(vao_curve, 1);
    glVertexArrayAttribIFormat(vao_curve, 1, 1, GL_INT, 0); 
    glVertexArrayAttribBinding(vao_curve, 1, 1);
}

/**
 * Build the FBOs used for rendering.
 */
void build_fbos()
{
    // Create the offscreen framebuffer that we will render the S2 sphere into
    {
        glCreateFramebuffers(1, &framebuffer_ui);

        // Create a color attachment texture and associate it with the framebuffer
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_ui);
        glTextureStorage2D(texture_ui, 1, GL_RGBA8, window_w, window_h);
        glTextureParameteri(texture_ui, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture_ui, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(framebuffer_ui, GL_COLOR_ATTACHMENT0, texture_ui, 0);

        // Create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
        glCreateRenderbuffers(1, &renderbuffer_ui);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_ui);
        glNamedRenderbufferStorage(renderbuffer_ui, GL_DEPTH24_STENCIL8, window_w, window_h);
        glNamedFramebufferRenderbuffer(framebuffer_ui, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer_ui);

        // Now that we actually created the framebuffer and added all attachments we want to check if it is actually complete 
        if (glCheckNamedFramebufferStatus(framebuffer_ui, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Error: framebuffer is not complete\n";
        }
    }

    // Create the offscreen framebuffer that we will render depth into (for shadow mapping)
    {
        glCreateFramebuffers(1, &framebuffer_depth);

        // Create a color attachment texture and associate it with the framebuffer
        const float border[] = { 1.0, 1.0, 1.0, 1.0 };
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_depth);
        glTextureStorage2D(texture_depth, 1, GL_DEPTH_COMPONENT32F, depth_w, depth_h);
        glTextureParameteri(texture_depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture_depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture_depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texture_depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTextureParameterfv(texture_depth, GL_TEXTURE_BORDER_COLOR, border);
        glNamedFramebufferTexture(framebuffer_depth, GL_DEPTH_ATTACHMENT, texture_depth, 0);

        glNamedFramebufferDrawBuffer(framebuffer_depth, GL_NONE);
        glNamedFramebufferReadBuffer(framebuffer_depth, GL_NONE);

        // Now that we actually created the framebuffer and added all attachments we want to check if it is actually complete 
        if (glCheckNamedFramebufferStatus(framebuffer_depth, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Error: framebuffer is not complete\n";
        }
    }
}

int main()
{
    // Setup the GUI library + OpenGL, etc.
    initialize();

    // Load all of the grid diagram files
    load_csvs();
    if (available_csvs.size() == 0)
    {
        throw std::runtime_error("No .csv files found");
    }
    current_csv = available_csvs[0];

    // Initialize the grid diagram
    auto diagram = knot::Diagram{ current_csv };
    auto curve = diagram.generate_curve();
    auto knot = knot::Knot{ curve };
    const size_t warmup_iterations = 3;
    for (size_t i = 0; i < warmup_iterations; i++)
    {
        knot.relax();
    }
    auto tube = geom::generate_tube(knot.get_rope());

    // Command log history messages
    auto history = utils::History{};

    // Load all of the relevant shader programs
    auto shader_depth = graphics::Shader{ "../shaders/depth.vert", "../shaders/depth.frag" };
    auto shader_draw = graphics::Shader{ "../shaders/render.vert", "../shaders/render.frag" };
    auto shader_ui = graphics::Shader{ "../shaders/ui.vert", "../shaders/ui.frag" };

    // Create VAOs, VBOs, FBOs, textures, etc.
    build_vaos(tube, curve.get_vertices(), knot.get_stuck());
    build_fbos();

    while (!glfwWindowShouldClose(window))
    {
        // Update flag that denotes whether or not the user is interacting with ImGui
        ImGuiIO& io = ImGui::GetIO();
        input_data.imgui_active = io.WantCaptureMouse;

        // Poll regular GLFW window events and start the ImGui frame
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw the UI elements (buttons, sliders, etc.)
        {
            // If this flag gets set by any of the UI elements below, the knot will be rebuilt
            bool topology_needs_update = false;

            // Basic settings UI window
            {
                ImGui::Begin("Settings");

                // Colors and FPS
                ImGui::ColorEdit3("clear color", (float*)&clear_color);
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

                // Drop-down menu for selecting a Cromwell move
                ImGui::Separator();
                if (ImGui::BeginCombo("Grid Diagram File", current_csv.c_str()))
                {
                    for (size_t i = 0; i < available_csvs.size(); ++i)
                    {
                        bool is_selected = current_csv.c_str() == available_csvs[i];

                        if (ImGui::Selectable(available_csvs[i].c_str(), is_selected))
                        {
                            // If the .csv selection has changed, load a new diagram
                            current_csv = available_csvs[i];
                            diagram = knot::Diagram{ current_csv };

                            std::stringstream stream;
                            stream << "Loaded diagram: ";
                            stream << current_csv;

                            history.push(stream.str(), utils::MessageType::INFO);

                            topology_needs_update = true;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Image((void*)(intptr_t)texture_ui, ImVec2(256, 256), ImVec2(1, 1), ImVec2(0, 0));
                ImGui::Text("Number of Grid Points: %u", curve.get_number_of_vertices());
                
                // Knot relaxation physics simulation
                ImGui::Separator();
                ImGui::Text("Simulation");
                ImGui::Checkbox("Simulation Active", &simulation_active);
                if (ImGui::Button("Reset Simulation"))
                {
                    knot.reset();
                }
        
                // Simulation params
                ImGui::Separator();
                ImGui::Text("Simulation Parameters");
                ImGui::SliderFloat("Damping", &knot.get_simulation_params().damping, 0.1f, 0.75f);
                ImGui::SliderFloat("Anchor Weight", &knot.get_simulation_params().anchor_weight, 0.0f, 0.5f);
                ImGui::SliderFloat("Beta", &knot.get_simulation_params().beta, 1.0f, 5.0f);
                ImGui::SliderFloat("H", &knot.get_simulation_params().h, 0.0f, 15.0f);
                ImGui::SliderFloat("Alpha", &knot.get_simulation_params().alpha, 1.0f, 5.0f);
                ImGui::SliderFloat("K", &knot.get_simulation_params().k, 0.0f, 15.0f);

                // Console log information
                ImGui::Separator();
                ImGui::Text("Log");
                const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
                for (const auto& item : history.get_messages())
                {
                    // Check to see if this message was an error message
                    bool pop_color = false;
                    if (item.second == utils::MessageType::ERROR)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); 
                        pop_color = true;
                    }
                    
                    // Prefix each message with its type, i.e. "[Error]"
                    auto full_message = utils::to_string(item.second) + std::string(" ") + item.first;
                    ImGui::TextUnformatted(full_message.c_str());

                    if (pop_color)
                    {
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::EndChild();

                ImGui::End();
            }

            // Grid diagram settings UI window
            {
                ImGui::Begin("Grid Diagram");

                // Draw grid cell buttons
                draw_diagram(diagram);
                
                // Drop-down menu for selecting a Cromwell move
                ImGui::Text("Cromwell Moves");
                if (ImGui::BeginCombo("Cromwell", current_move.c_str()))
                {
                    for (size_t i = 0; i < cromwell_moves.size(); ++i)
                    {
                        bool is_selected = current_move.c_str() == cromwell_moves[i];

                        if (ImGui::Selectable(cromwell_moves[i].c_str(), is_selected))
                        {
                            current_move = cromwell_moves[i];
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                // Extra UI elements (based on current edit mode)
                if (current_move == "Translation")
                {
                    auto direction_message = "";

                    if (ImGui::Button("Up"))
                    {
                        diagram.apply_translation(knot::Direction::U);
                        topology_needs_update = true;
                        direction_message = utils::to_string(knot::Direction::U);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Down"))
                    {
                        diagram.apply_translation(knot::Direction::D);
                        topology_needs_update = true;
                        direction_message = utils::to_string(knot::Direction::D);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Left"))
                    {
                        diagram.apply_translation(knot::Direction::L);
                        topology_needs_update = true;
                        direction_message = utils::to_string(knot::Direction::L);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Right"))
                    {
                        diagram.apply_translation(knot::Direction::R);
                        topology_needs_update = true;
                        direction_message = utils::to_string(knot::Direction::R);
                    }

                    // Was one of the directional buttons actually pressed?
                    if (topology_needs_update && direction_message != "")
                    {
                        std::stringstream stream;
                        stream << "Applied translation ";
                        stream << direction_message;

                        history.push(stream.str(), utils::MessageType::INFO);
                    }
                }
                else if (current_move == "Commutation")
                {
                    // The user should select either "row" or "col" 
                    ImGui::RadioButton("Row", &commutation_row_or_col, 0); 
                    ImGui::SameLine();
                    ImGui::RadioButton("Col", &commutation_row_or_col, 1);

                    // The index of the row / col to switch
                    ImGui::SliderInt("Index", &commutation_index, 0, diagram.get_size() - 1);

                    // Run the actual commutation operation
                    if (ImGui::Button("Operate"))
                    {
                        try
                        {
                            diagram.apply_commutation(static_cast<knot::Axis>(commutation_row_or_col), commutation_index);
                            topology_needs_update = true;

                            std::stringstream stream;
                            stream << "Applied commutation on ";
                            stream << utils::to_string(static_cast<knot::Axis>(commutation_row_or_col));
                            stream << " " << commutation_index << "<->" << commutation_index + 1;

                            history.push(stream.str(), utils::MessageType::INFO);
                        }
                        catch (knot::CromwellException e)
                        {
                            history.push(e.get_message(), utils::MessageType::ERROR);
                        }
                    }
                }
                else if (current_move == "Stabilization")
                {
                    // The user should select one of the following cardinal directions
                    ImGui::RadioButton("NW", &stabilization_cardinal, 0); ImGui::SameLine();
                    ImGui::RadioButton("SW", &stabilization_cardinal, 1); ImGui::SameLine();
                    ImGui::RadioButton("NE", &stabilization_cardinal, 2); ImGui::SameLine();
                    ImGui::RadioButton("SE", &stabilization_cardinal, 3); 

                    // The indices of the cell to stabilize
                    ImGui::SliderInt("Row Index", &stabilization_index_i, 0, diagram.get_size() - 1);
                    ImGui::SliderInt("Col Index", &stabilization_index_j, 0, diagram.get_size() - 1);

                    // Run the actual stabilization operation
                    if (ImGui::Button("Operate"))
                    {
                        try
                        {
                            diagram.apply_stabilization(static_cast<knot::Cardinal>(stabilization_cardinal), stabilization_index_i, stabilization_index_j);
                            topology_needs_update = true;

                            std::stringstream stream;
                            stream << "Applied stabilization (";
                            stream << utils::to_string(static_cast<knot::Cardinal>(stabilization_cardinal)) << ") on ";
                            stream << "row: " << stabilization_index_i << ", ";
                            stream << "col: " << stabilization_index_j;

                            history.push(stream.str(), utils::MessageType::INFO);
                        }
                        catch (knot::CromwellException e)
                        {
                            history.push(e.get_message(), utils::MessageType::ERROR);
                        }
                    }
                }
                else if (current_move == "Destabilization")
                {
                    // The indices of the top-left corner of the 2x2 subgrid to destabilize
                    ImGui::SliderInt("Row Index", &destabilization_index_i, 0, diagram.get_size() - 2);
                    ImGui::SliderInt("Col Index", &destabilization_index_j, 0, diagram.get_size() - 2);

                    // Run the actual desabilization operation
                    if (ImGui::Button("Operate"))
                    {
                        try
                        {
                            diagram.apply_destabilization(destabilization_index_i, destabilization_index_j);
                            topology_needs_update = true;

                            std::stringstream stream;
                            stream << "Applied destabilization on subgrid with corner at ";
                            stream << "row: " << destabilization_index_i << ", ";
                            stream << "col: " << destabilization_index_j;

                            history.push(stream.str(), utils::MessageType::INFO);
                        }
                        catch (knot::CromwellException e)
                        {
                            history.push(e.get_message(), utils::MessageType::ERROR);
                        }
                    }
                }

                // Update draw data if necessary
                if (topology_needs_update)
                {
                    history.push("Updating knot...", utils::MessageType::INFO);

                    // Rebuild the curve that corresponds to this diagram
                    curve = diagram.generate_curve();

                    // Rebuild the knot
                    knot = knot::Knot{ curve };
                    if (!simulation_active)
                    {
                        for (size_t i = 0; i < warmup_iterations; ++i)
                        {
                            knot.relax();
                        }
                    }
                    tube = geom::generate_tube(knot.get_rope());

                    // Rebuild VAO / VBO for tube mesh
                    build_vaos(tube, curve.get_vertices(), knot.get_stuck());
                }

                ImGui::End();
            }
        }
        ImGui::Render();
        
        // Render 3D objects to UI (offscreen) framebuffer
        {
            glViewport(0, 0, window_w, window_h);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ui);

            const float clear_color_values[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            const float clear_depth_value = 1.0f;
            const uint32_t color_buffer_index = 0;
            glClearNamedFramebufferfv(framebuffer_ui, GL_COLOR, color_buffer_index, clear_color_values);
            glClearNamedFramebufferfv(framebuffer_ui, GL_DEPTH, 0, &clear_depth_value);

            // Set the projection matrix so that the entire grid is always visible (with some padding)
            const size_t padding = 2;
            const float size = diagram.get_size() + padding;
            const glm::mat4 projection = glm::ortho(size / 2, -size / 2, -size / 2, size / 2, -5.0f, 5.0f);

            const glm::mat4 view = glm::lookAt(
                glm::vec3{ 0.0f, 0.0f, 1.0f },
                glm::vec3{ 0.0f, 0.0f, 0.0f },
                glm::vec3{ 0.0f, 1.0f, 0.0f }
            );

            shader_ui.use();
            shader_ui.uniform_int("u_number_of_vertices", curve.get_number_of_vertices());
            shader_ui.uniform_mat4("u_projection", projection);
            shader_ui.uniform_mat4("u_view", view);
            shader_ui.uniform_mat4("u_model", glm::mat4{ 1.0f });
            glBindVertexArray(vao_curve);
            glDrawArrays(GL_LINE_LOOP, 0, curve.get_number_of_vertices());
            glDrawArrays(GL_POINTS, 0, curve.get_number_of_vertices());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Render 3D objects to default framebuffer
        {
            // Run physics simulation
            if (simulation_active)
            {
                knot.relax();

                tube = geom::generate_tube(knot.get_rope());
                glNamedBufferSubData(vbo_tube_position, 0, sizeof(glm::vec3) * tube.size(), tube.data());

                const auto stuck = knot.get_stuck();
                glNamedBufferSubData(vbo_curve_stuck, 0, sizeof(int32_t) * stuck.size(), stuck.data());
            }

            // Setup faux light position, projection matrix, etc.
            const glm::vec3 light_position{ 1.0f, 1.0f, 1.0f };
            const float near_plane = -10.0f;
            const float far_plane = 10.0f;
            const float ortho_width = 20.0f;
            const auto light_projection = glm::ortho(-ortho_width, ortho_width, -ortho_width, ortho_width, near_plane, far_plane);
            const auto light_view = glm::lookAt(light_position, glm::vec3{ 0.0f, 0.0f, 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
            const auto light_space_matrix = light_projection * light_view;

            // Make sure the knot is always centered
            const auto bounds = knot.get_rope().get_bounds();
            const auto size_of_bounds = bounds.get_size();
            const auto center_of_bounds = bounds.get_center();
            glm::mat4 translate_center = glm::mat4{ 1.0f };
            translate_center = glm::translate(translate_center, -center_of_bounds);

           // Render pass #1: render depth
           {
               glViewport(0, 0, depth_w, depth_h);
               glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_depth);

               // Clear the depth attachment (there are no color attachments)
               const float clear_depth_value = 1.0f;
               glClearNamedFramebufferfv(framebuffer_depth, GL_DEPTH, 0, &clear_depth_value);

               // Draw the knot
               shader_depth.use();
               shader_depth.uniform_mat4("u_light_space_matrix", light_space_matrix);
               shader_depth.uniform_mat4("u_model", arcball_model_matrix * translate_center);
               glBindVertexArray(vao_tube);
               glDrawArrays(GL_TRIANGLES, 0, tube.size());
               
               glBindFramebuffer(GL_FRAMEBUFFER, 0);
           }

           // Render pass #2: draw scene with shadows
           {
               glViewport(0, 0, window_w, window_h);
           
               glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
               glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

               glm::mat4 projection = glm::perspective(
                   glm::radians(zoom),
                   static_cast<float>(window_w) / static_cast<float>(window_h),
                   0.1f,
                   1000.0f
               );

               // Bind the depth map from the previous render pass
               glBindTextureUnit(0, texture_depth);

               // Draw the knot (with shadows)
               shader_draw.use();
               shader_draw.uniform_bool("u_display_shadows", true);
               shader_draw.uniform_mat4("u_light_space_matrix", light_space_matrix);
               shader_draw.uniform_float("u_time", glfwGetTime());
               shader_draw.uniform_mat4("u_projection", projection);
               shader_draw.uniform_mat4("u_view", arcball_camera_matrix);
               shader_draw.uniform_mat4("u_model", arcball_model_matrix * translate_center);
               shader_draw.uniform_vec3("u_size_of_bounds", size_of_bounds);
               glBindVertexArray(vao_tube);
               glDrawArrays(GL_TRIANGLES, 0, tube.size());
           }
        }

        // Draw the ImGui window
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Clean-up UI bits
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Delete OpenGL objects
    glDeleteVertexArrays(1, &vao_curve);
    glDeleteVertexArrays(1, &vao_tube);
    glDeleteBuffers(1, &vbo_curve_position);
    glDeleteBuffers(1, &vbo_curve_stuck);
    glDeleteBuffers(1, &vbo_tube_position);
    glDeleteTextures(1, &texture_depth);
    glDeleteTextures(1, &texture_ui);
    glDeleteFramebuffers(1, &framebuffer_depth);
    glDeleteFramebuffers(1, &framebuffer_ui);

    // Clean-up GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
}

