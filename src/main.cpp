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
#include "mesh.h"
#include "shader.h"

// Data that will be associated with the GLFW window
struct InputData
{
    bool imgui_active = false;
};

// Viewport and camera settings
const uint32_t window_w = 1200;
const uint32_t window_h = 720;
bool first_mouse = true;
float last_x;
float last_y;
float zoom = 45.0f;
glm::mat4 arcball_camera_matrix = glm::lookAt(glm::vec3{ 0.0f, 0.0f, 20.0f }, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
glm::mat4 arcball_model_matrix = glm::mat4{ 1.0f };

// Global settings
// ...

// Appearance settings
ImVec4 clear_color = ImVec4(0.208f, 0.256f, 0.373f, 1.0f);
std::vector<std::string> cromwell_moves = { "Translation", "Commutation", "Stabilization", "Destabilization" };
std::string current_move = cromwell_moves[0];

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

#define TUBE 

std::vector<std::pair<size_t, size_t>> determine_selectable(const Diagram & diagram, const std::string & move)
{

    for (size_t i = 0; i < diagram.get_number_of_rows(); ++i)
    {
        for (size_t j = 0; j < diagram.get_number_of_cols(); ++j)
        {
            
        }
    }
}

int main()
{
    auto diagram = Diagram{ "../diagrams/kinoshita_terasaka.csv" };
    diagram.apply_stabilization(Cardinal::SW, 1, 3);
   // diagram.apply_commutation(Axis::ROW, 3);
    //diagram.apply_translation(Direction::U);

    auto curve = diagram.generate_curve();
    curve = curve.refine(0.75f);
    auto tube = geom::generate_tube(curve, 0.5f, 10);
    auto knot = Knot{ curve };

    std::cout << "Number of vertices: " << curve.get_number_of_vertices() << std::endl;
    std::cout << tube.size() << std::endl;

    // Create and configure the GLFW window 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* window = glfwCreateWindow(window_w, window_h, "Marching Cubes", nullptr, nullptr);

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
        ///glEnable(GL_CULL_FACE);
       // glCullFace(GL_BACK);
    }


    // Generate mesh primitives
    auto grid_data = graphics::Mesh::from_grid(10.0f, 10.0f, glm::vec3{ 0.0f, -5.0f, 0.0f });
    graphics::Mesh mesh_grid{ grid_data.first, grid_data.second };

    glLineWidth(1.0f);
    glPointSize(4.0f);

    uint32_t vao;
    uint32_t vbo;
    glCreateVertexArrays(1, &vao);

    glCreateBuffers(1, &vbo);
#ifdef TUBE
    glNamedBufferStorage(vbo, sizeof(glm::vec3) * tube.size(), tube.data(), GL_DYNAMIC_STORAGE_BIT);
#else
    glNamedBufferStorage(vbo, sizeof(glm::vec3) * curve.get_number_of_vertices(), curve.get_vertices().data(), GL_DYNAMIC_STORAGE_BIT);
#endif

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0); // Last item is the offset
    glVertexArrayAttribBinding(vao, 0, 0);

    auto shader_depth = graphics::Shader{ "../shaders/depth.vert", "../shaders/depth.frag" };
    auto shader_draw = graphics::Shader{ "../shaders/render.vert", "../shaders/render.frag" };

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
        input_data.imgui_active = io.WantCaptureMouse;

        // Poll regular GLFW window events
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            ImGui::Begin("Settings");                             
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            
            if (ImGui::Button("Reset"))
            {
                knot.reset();
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

            ImGui::Begin("Grid Diagram");
            const auto text_size = ImGui::CalcTextSize("x");
            const auto button_dims = std::max(text_size.x, text_size.y) * 2;
            for (size_t i = 0; i < diagram.get_size(); i++)
            {
                for (size_t j = 0; j < diagram.get_size(); j++)
                {
                    std::string label = " ";
                    if (diagram.get_data()[i][j] == Entry::X) label = "x";
                    if (diagram.get_data()[i][j] == Entry::O) label = "o";

                    if (current_move == "Stabilization" && diagram.get_data()[i][j] == Entry::X)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 1.0f });
                    }

                    if (ImGui::Button(label.c_str(), { button_dims, button_dims }))
                    {
                        std::cout << "Pressed\n";
                    }

                    ImGui::PopStyleColor();

                    if (j != diagram.get_size() - 1)
                    {
                        ImGui::SameLine();
                    }
                }
            }
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
            ImGui::End();
            
        }
        ImGui::Render();

        

        // Render 3D objects to default framebuffer
        {
           knot.relax();
#ifdef TUBE
           tube = geom::generate_tube(knot.get_rope());
           glNamedBufferSubData(vbo, 0, sizeof(glm::vec3) * tube.size(), tube.data());
#else
           glNamedBufferSubData(vbo, 0, sizeof(glm::vec3) * knot.get_rope().get_number_of_vertices(), knot.get_rope().get_vertices().data());
#endif

           glm::vec3 light_position{ -2.0f, 2.0f, 2.0f };
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
               glBindVertexArray(vao);
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

               glBindVertexArray(vao);
#ifdef TUBE
          //     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
               glDrawArrays(GL_TRIANGLES, 0, tube.size());
#else
               glDrawArrays(GL_LINE_LOOP, 0, curve.get_number_of_vertices());
               glDrawArrays(GL_POINTS, 0, curve.get_number_of_vertices());
#endif

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

