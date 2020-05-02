#include <algorithm>
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
#include "mesh.h"
#include "shader.h"

// Data that will be associated with the GLFW window
struct InputData
{
    bool imgui_active = false;
};

// Viewport and camera settings
const uint32_t window_w = 1200;
const uint32_t window_h = 800;
const uint32_t ui_w = 256;
const uint32_t ui_h = 256;
bool first_mouse = true;
float last_x;
float last_y;
float zoom = 45.0f;
glm::mat4 arcball_camera_matrix = glm::lookAt(glm::vec3{ 0.0f, 0.0f, 20.0f }, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
glm::mat4 arcball_model_matrix = glm::mat4{ 1.0f };

// Global settings
bool simulation_active = false;

// Appearance settings
ImVec4 clear_color = ImVec4(0.208f, 0.256f, 0.373f, 1.0f);

std::vector<std::string> cromwell_moves = { "Translation", "Commutation", "Stabilization", "Destabilization" };
std::string current_move = cromwell_moves[0];

int commutation_row_or_col = 0;
int commutation_index = 0;

int stabilization_cardinal = 0;
int stabilization_index_i = 0;
int stabilization_index_j = 0;

InputData input_data;

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

            glm::vec3 va = get_arcball_vector(last_x, last_y);
            glm::vec3 vb = get_arcball_vector(xpos, ypos);
            const float angle = acos(std::min(1.0f, glm::dot(va, vb))) * rotation_speed;
            const glm::vec3 axis_camera_coordinates = glm::cross(va, vb);

            glm::mat3 camera_to_object = glm::inverse(glm::mat3(arcball_camera_matrix) * glm::mat3(arcball_model_matrix));

            glm::vec3 axis_in_object_coord = camera_to_object * axis_camera_coordinates;

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
    auto const src_str = [source]() {
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

    auto const type_str = [type]() {
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

    auto const severity_str = [severity]() {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
        case GL_DEBUG_SEVERITY_LOW: return "LOW";
        case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
        case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
        }
    }();

    std::cout << src_str << ", " << type_str << ", " << severity_str << ", " << id << ": " << message << '\n';
}

std::vector<std::pair<size_t, size_t>> determine_selectable(const Diagram & diagram, const std::string & move)
{

    for (size_t i = 0; i < diagram.get_number_of_rows(); ++i)
    {
        for (size_t j = 0; j < diagram.get_number_of_cols(); ++j)
        {
            
        }
    }

    return {};
}


void draw_diagram(const Diagram& diagram)
{
    ImGui::Text("Grid diagram is %u x %u", diagram.get_number_of_rows(), diagram.get_number_of_cols());

    const auto text_size = ImGui::CalcTextSize("x");
    const auto button_dims = std::max(text_size.x, text_size.y) * 2;

    for (size_t i = 0; i < diagram.get_size(); i++)
    {
        for (size_t j = 0; j < diagram.get_size(); j++)
        {
            std::string label = to_string(diagram.get_data()[i][j]);

            //ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 1.0f });
            //ImGui::PopStyleColor();

            // If this is the last row, we only want to remove the horizontal item spacing
            if (i != diagram.get_size() - 1)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
            }
            else
            {
                auto& style = ImGui::GetStyle();
                auto default_item_spacing = style.ItemSpacing;
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, default_item_spacing.y });
            }

            if (ImGui::Button(label.c_str(), { button_dims, button_dims }))
            {
                std::cout << "Pressed" << std::endl;
            }

            if (j != diagram.get_size() - 1)
            {
                ImGui::SameLine();
            }

            ImGui::PopStyleVar();

        }
    }

    
    

    //const auto tilesetImagePos = ImGui::GetCursorScreenPos();

    //// draw grid
    //auto draw_list = ImGui::GetWindowDrawList();

    //const auto tileSize = static_cast<float>(8.0f);
    //const auto xTiles = static_cast<int>(diagram.get_number_of_cols());
    //const auto yTiles = static_cast<int>(diagram.get_number_of_rows());

    //// draw horizontal lines
    //for (int x = 0; x < xTiles + 1; ++x) 
    //{
    //    draw_list->AddLine(ImVec2(tilesetImagePos.x + x * tileSize, tilesetImagePos.y),
    //        ImVec2(tilesetImagePos.x + x * tileSize, tilesetImagePos.y + yTiles * tileSize),
    //        ImColor(255, 255, 255));
    //}

    //// draw vertical lines
    //for (int y = 0; y < yTiles + 1; ++y) 
    //{
    //    draw_list->AddLine(ImVec2(tilesetImagePos.x, tilesetImagePos.y + y * tileSize),
    //        ImVec2(tilesetImagePos.x + xTiles * tileSize, tilesetImagePos.y + y * tileSize),
    //        ImColor(255, 255, 255));
    //}

    // Check input
    if (ImGui::IsItemHovered()) 
    {
        if (ImGui::IsMouseClicked(0)) 
        {
           const auto mouse_position = ImGui::GetMousePos();
        }
    }
}


GLFWwindow* window;

void initialize()
{
    // Create and configure the GLFW window 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(window_w, window_h, "Marching Cubes", nullptr, nullptr);

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
        //glEnable(GL_DEBUG_OUTPUT);
        //glDebugMessageCallback(message_callback, nullptr);
#endif
        // Depth testing
        glEnable(GL_DEPTH_TEST);

        // Backface culling for optimization
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // Configure point and line size
        glLineWidth(8.0f);
        glPointSize(16.0f);
    }
}

int main()
{
    initialize();

    auto diagram = Diagram{ "../diagrams/kinoshita_terasaka.csv" };
    auto curve = diagram.generate_curve();
    curve = curve.refine(0.75f);

    auto tube = geom::generate_tube(curve, 0.5f, 10);
    auto knot = Knot{ curve };


    auto history = utils::History{};



    // Generate mesh primitives
    auto grid_data = graphics::Mesh::from_grid(10.0f, 10.0f, glm::vec3{ 0.0f, -5.0f, 0.0f });
    graphics::Mesh mesh_grid{ grid_data.first, grid_data.second };

    uint32_t vao_tube;
    uint32_t vbo_tube;
    {
        glCreateVertexArrays(1, &vao_tube);

        glCreateBuffers(1, &vbo_tube);
        glNamedBufferStorage(vbo_tube, sizeof(glm::vec3) * tube.size(), tube.data(), GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayVertexBuffer(vao_tube, 0, vbo_tube, 0, sizeof(glm::vec3));
        glEnableVertexArrayAttrib(vao_tube, 0);
        glVertexArrayAttribFormat(vao_tube, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vao_tube, 0, 0);
    }

    uint32_t vao_curve;
    uint32_t vbo_curve;
    {
        glCreateVertexArrays(1, &vao_curve);

        glCreateBuffers(1, &vbo_curve);
        glNamedBufferStorage(vbo_curve, sizeof(glm::vec3) * curve.get_number_of_vertices(), curve.get_vertices().data(), GL_DYNAMIC_STORAGE_BIT);

        glVertexArrayVertexBuffer(vao_curve, 0, vbo_curve, 0, sizeof(glm::vec3));
        glEnableVertexArrayAttrib(vao_curve, 0);
        glVertexArrayAttribFormat(vao_curve, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vao_curve, 0, 0);
    }


    auto shader_depth = graphics::Shader{ "../shaders/depth.vert", "../shaders/depth.frag" };
    auto shader_draw = graphics::Shader{ "../shaders/render.vert", "../shaders/render.frag" };
    auto shader_ui = graphics::Shader{ "../shaders/ui.vert", "../shaders/ui.frag" };

    // Create the offscreen framebuffer that we will render the S2 sphere into
    uint32_t framebuffer_ui;
    uint32_t texture_ui;
    uint32_t renderbuffer_ui;
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
    uint32_t framebuffer_depth;
    uint32_t texture_depth;
    {
        glCreateFramebuffers(1, &framebuffer_depth);

        // Create a color attachment texture and associate it with the framebuffer
        const float border[] = { 1.0, 1.0, 1.0, 1.0 };
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_depth);
        glTextureStorage2D(texture_depth, 1, GL_DEPTH_COMPONENT32F, window_w, window_h);
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

    while (!glfwWindowShouldClose(window))
    {
        // Update flag that denotes whether or not the user is interacting with ImGui
        ImGuiIO& io = ImGui::GetIO();
        input_data.imgui_active = io.WantCaptureMouse;

        // Poll regular GLFW window events
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            {
                ImGui::Begin("Settings");
                ImGui::ColorEdit3("clear color", (float*)&clear_color);
                if (ImGui::Button("Reset"))
                {
                    knot.reset();
                }

                // Add some basic performanace monitoring
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

                // Draw the (un-extruded) knot
                ImGui::Image((void*)(intptr_t)texture_ui, ImVec2(ui_w, ui_h), ImVec2(1, 1), ImVec2(0, 0));

                // Toggle the knot relaxation physics simulation
                ImGui::Checkbox("Simulation Active", &simulation_active);

                // Add console log information
                const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
                for (const auto& item : history.get_messages())
                {
                    bool pop_color = false;
                    if (item.second == utils::MessageType::ERROR)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); 
                        pop_color = true;
                    }
                  
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

            {
                ImGui::Begin("Grid Diagram");

                draw_diagram(diagram);
                
                // Create the drop-down menu for selecting a Cromwell move
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

                bool topology_needs_update = false;

                if (current_move == "Translation")
                {
                    auto direction_message = "";

                    if (ImGui::Button("Up"))
                    {
                        diagram.apply_translation(Direction::U);
                        topology_needs_update = true;
                        direction_message = to_string(Direction::U);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Down"))
                    {
                        diagram.apply_translation(Direction::D);
                        topology_needs_update = true;
                        direction_message = to_string(Direction::D);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Left"))
                    {
                        diagram.apply_translation(Direction::L);
                        topology_needs_update = true;
                        direction_message = to_string(Direction::L);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Right"))
                    {
                        diagram.apply_translation(Direction::R);
                        topology_needs_update = true;
                        direction_message = to_string(Direction::R);
                    }

                    // Was one of the directional buttons actually pressed?
                    if (topology_needs_update)
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
                    ImGui::RadioButton("Row", &commutation_row_or_col, 0); ImGui::SameLine();
                    ImGui::RadioButton("Col", &commutation_row_or_col, 1);

                    // The index of the row / col to switch
                    ImGui::SliderInt("Index", &commutation_index, 0, diagram.get_size() - 1);

                    // Run the actual commutation operation
                    if (ImGui::Button("Operate"))
                    {
                        try
                        {
                            diagram.apply_commutation(static_cast<Axis>(commutation_row_or_col), commutation_index);
                            topology_needs_update = true;

                            std::stringstream stream;
                            stream << "Applied commutation on ";
                            stream << to_string(static_cast<Axis>(commutation_row_or_col));
                            stream << " " << commutation_index << "<->" << commutation_index + 1;

                            history.push(stream.str(), utils::MessageType::INFO);
                        }
                        catch (CromwellException e)
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
                            diagram.apply_stabilization(static_cast<Cardinal>(stabilization_cardinal), stabilization_index_i, stabilization_index_j);
                            topology_needs_update = true;

                            std::stringstream stream;
                            stream << "Applied stabilization (";
                            stream << to_string(static_cast<Cardinal>(stabilization_cardinal)) << ") on ";
                            stream << "row: " << stabilization_index_i << ", ";
                            stream << "col: " << stabilization_index_j;

                            history.push(stream.str(), utils::MessageType::INFO);
                        }
                        catch (CromwellException e)
                        {
                            history.push(e.get_message(), utils::MessageType::ERROR);
                        }
                    }

                }

                // Update draw data if necessary
                if (topology_needs_update)
                {
                    std::cout << "Updating knot...\n";

                    // Rebuild the curve that corresponds to this diagram
                    curve = diagram.generate_curve();
                    curve = curve.refine(0.75f);

                    // Rebuild the knot
                    knot = Knot{ curve };
                    
                    // Rebuild VAO / VBO for tube mesh
                    tube = geom::generate_tube(knot.get_rope());
                    glCreateVertexArrays(1, &vao_tube);

                    glCreateBuffers(1, &vbo_tube);
                    glNamedBufferStorage(vbo_tube, sizeof(glm::vec3) * tube.size(), tube.data(), GL_DYNAMIC_STORAGE_BIT);

                    glVertexArrayVertexBuffer(vao_tube, 0, vbo_tube, 0, sizeof(glm::vec3));
                    glEnableVertexArrayAttrib(vao_tube, 0);
                    glVertexArrayAttribFormat(vao_tube, 0, 3, GL_FLOAT, GL_FALSE, 0); // Last item is the offset
                    glVertexArrayAttribBinding(vao_tube, 0, 0);

                    // Rebuild VAO / VBO for curve mesh
                    glCreateVertexArrays(1, &vao_curve);

                    glCreateBuffers(1, &vbo_curve);
                    glNamedBufferStorage(vbo_curve, sizeof(glm::vec3) * curve.get_number_of_vertices(), curve.get_vertices().data(), GL_DYNAMIC_STORAGE_BIT);

                    glVertexArrayVertexBuffer(vao_curve, 0, vbo_curve, 0, sizeof(glm::vec3));
                    glEnableVertexArrayAttrib(vao_curve, 0);
                    glVertexArrayAttribFormat(vao_curve, 0, 3, GL_FLOAT, GL_FALSE, 0);
                    glVertexArrayAttribBinding(vao_curve, 0, 0);
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
                glNamedBufferSubData(vbo_tube, 0, sizeof(glm::vec3)* tube.size(), tube.data());
            }

            // Setup faux light position, projection matrix, etc.
            glm::vec3 light_position{ -2.0f, 2.0f, 5.0f };
            const float near_plane = 0.0f;
            const float far_plane = 20.0f;
            const float ortho_width = 30.0f;
            const auto light_projection = glm::ortho(-ortho_width, ortho_width, -ortho_width, ortho_width, near_plane, far_plane);
            const auto light_view = glm::lookAt(light_position, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
            const auto light_space_matrix = light_projection * light_view;

           // Render pass #1: render depth
           {
               glViewport(0, 0, window_w, window_h);
               glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_depth);

               // Clear the depth attachment (there are no color attachments)
               const float clear_depth_value = 1.0f;
               glClearNamedFramebufferfv(framebuffer_depth, GL_DEPTH, 0, &clear_depth_value);

               shader_depth.use();
               shader_depth.uniform_mat4("u_light_space_matrix", light_space_matrix);

               // Draw the knot
               shader_depth.uniform_mat4("u_model", arcball_model_matrix);
               glBindVertexArray(vao_tube);
               glDrawArrays(GL_TRIANGLES, 0, tube.size());

               // Draw the floor plane
               shader_depth.uniform_mat4("u_model", glm::mat4{ 1.0f });
               mesh_grid.draw();
               
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

               shader_draw.use();
 
               glBindTextureUnit(0, texture_depth);
                //shader.uniform_bool("u_display_shadows", display_shadows);
               shader_draw.uniform_mat4("u_light_space_matrix", light_space_matrix);
               shader_draw.uniform_float("u_time", glfwGetTime());

               shader_draw.uniform_mat4("u_projection", projection);
               shader_draw.uniform_mat4("u_view", arcball_camera_matrix);
               shader_draw.uniform_mat4("u_model", arcball_model_matrix);

               glBindVertexArray(vao_tube);
               glDrawArrays(GL_TRIANGLES, 0, tube.size());


              //shader_draw.uniform_mat4("u_model", glm::mat4{ 1.0f });
              //mesh_grid.draw();
           }

        }

        // Draw the ImGui window
        {
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glfwSwapBuffers(window);
    }

    // Clean-up imgui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
       
    // Clean-up GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
}

