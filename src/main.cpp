#include <cstdlib>
#include <iostream>
#include <cmath>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
};

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log) - 1), nullptr, log);
        std::cerr << "Shader compile failed: " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LinkProgram(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetProgramInfoLog(program, static_cast<GLsizei>(sizeof(log) - 1), nullptr, log);
        std::cerr << "Program link failed: " << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
struct SaveData {
    int coins = 0;
    int damage_level = 0;
    int speed_level = 0;
    int max_health_level = 0;
    int attack_cooldown_level = 0;
    int skin_unlocked = 0;
};

bool LoadSave(const char* path, SaveData* out) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    SaveData data;
    file >> data.coins
         >> data.damage_level
         >> data.speed_level
         >> data.max_health_level
         >> data.attack_cooldown_level
         >> data.skin_unlocked;
    if (!file.fail()) {
        *out = data;
        return true;
    }
    return false;
}

void SaveProgress(const char* path, const SaveData& data) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return;
    }
    file << data.coins << ' '
         << data.damage_level << ' '
         << data.speed_level << ' '
         << data.max_health_level << ' '
         << data.attack_cooldown_level << ' '
         << data.skin_unlocked;
}

struct TextTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

TextTexture CreateTextTexture(TTF_Font* font, const std::string& text, SDL_Color color) {
    if (!font || text.empty()) {
        return {};
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        return {};
    }
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        return {};
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, converted->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    TextTexture result;
    result.id = texture;
    result.width = converted->w;
    result.height = converted->h;
    SDL_FreeSurface(converted);
    return result;
}

void DestroyTextTexture(TextTexture* texture) {
    if (texture && texture->id != 0) {
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
        texture->width = 0;
        texture->height = 0;
    }
}

TTF_Font* LoadUIFont(int size) {
    const char* candidates[] = {
        "assets/Roboto-Regular.ttf",
        "assets/Inter-Regular.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    for (const char* path : candidates) {
        TTF_Font* font = TTF_OpenFont(path, size);
        if (font) {
            return font;
        }
    }
    return nullptr;
}

std::vector<float> BuildSphereVertices(int stacks, int slices, float radius) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(stacks * slices * 6) * 3);
    for (int i = 0; i < stacks; ++i) {
        float v0 = static_cast<float>(i) / static_cast<float>(stacks);
        float v1 = static_cast<float>(i + 1) / static_cast<float>(stacks);
        float phi0 = glm::pi<float>() * (v0 - 0.5f);
        float phi1 = glm::pi<float>() * (v1 - 0.5f);
        float y0 = std::sin(phi0);
        float y1 = std::sin(phi1);
        float r0 = std::cos(phi0);
        float r1 = std::cos(phi1);

        for (int j = 0; j < slices; ++j) {
            float u0 = static_cast<float>(j) / static_cast<float>(slices);
            float u1 = static_cast<float>(j + 1) / static_cast<float>(slices);
            float theta0 = glm::two_pi<float>() * u0;
            float theta1 = glm::two_pi<float>() * u1;

            glm::vec3 p00(r0 * std::cos(theta0), y0, r0 * std::sin(theta0));
            glm::vec3 p10(r0 * std::cos(theta1), y0, r0 * std::sin(theta1));
            glm::vec3 p01(r1 * std::cos(theta0), y1, r1 * std::sin(theta0));
            glm::vec3 p11(r1 * std::cos(theta1), y1, r1 * std::sin(theta1));

            glm::vec3 tri1[3] = {p00, p11, p10};
            glm::vec3 tri2[3] = {p00, p01, p11};
            for (int k = 0; k < 3; ++k) {
                vertices.push_back(tri1[k].x * radius);
                vertices.push_back(tri1[k].y * radius);
                vertices.push_back(tri1[k].z * radius);
            }
            for (int k = 0; k < 3; ++k) {
                vertices.push_back(tri2[k].x * radius);
                vertices.push_back(tri2[k].y * radius);
                vertices.push_back(tri2[k].z * radius);
            }
        }
    }
    return vertices;
}

std::vector<float> BuildConeVertices(int slices, float radius, float height) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(slices * 6) * 3);
    glm::vec3 tip(0.0f, height * 0.5f, 0.0f);
    float base_y = -height * 0.5f;
    for (int j = 0; j < slices; ++j) {
        float u0 = static_cast<float>(j) / static_cast<float>(slices);
        float u1 = static_cast<float>(j + 1) / static_cast<float>(slices);
        float theta0 = glm::two_pi<float>() * u0;
        float theta1 = glm::two_pi<float>() * u1;
        glm::vec3 p0(radius * std::cos(theta0), base_y, radius * std::sin(theta0));
        glm::vec3 p1(radius * std::cos(theta1), base_y, radius * std::sin(theta1));

        // Side triangle
        glm::vec3 tri1[3] = {tip, p0, p1};
        // Base triangle
        glm::vec3 tri2[3] = {glm::vec3(0.0f, base_y, 0.0f), p1, p0};

        for (int k = 0; k < 3; ++k) {
            vertices.push_back(tri1[k].x);
            vertices.push_back(tri1[k].y);
            vertices.push_back(tri1[k].z);
        }
        for (int k = 0; k < 3; ++k) {
            vertices.push_back(tri2[k].x);
            vertices.push_back(tri2[k].y);
            vertices.push_back(tri2[k].z);
        }
    }
    return vertices;
}

std::vector<float> BuildCylinderVertices(int slices, float radius, float height) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(slices * 12) * 3);
    float half = height * 0.5f;
    for (int j = 0; j < slices; ++j) {
        float u0 = static_cast<float>(j) / static_cast<float>(slices);
        float u1 = static_cast<float>(j + 1) / static_cast<float>(slices);
        float theta0 = glm::two_pi<float>() * u0;
        float theta1 = glm::two_pi<float>() * u1;

        glm::vec3 p00(radius * std::cos(theta0), -half, radius * std::sin(theta0));
        glm::vec3 p01(radius * std::cos(theta0),  half, radius * std::sin(theta0));
        glm::vec3 p10(radius * std::cos(theta1), -half, radius * std::sin(theta1));
        glm::vec3 p11(radius * std::cos(theta1),  half, radius * std::sin(theta1));

        glm::vec3 side1[3] = {p00, p01, p11};
        glm::vec3 side2[3] = {p00, p11, p10};
        glm::vec3 top[3] = {glm::vec3(0.0f, half, 0.0f), p11, p01};
        glm::vec3 bottom[3] = {glm::vec3(0.0f, -half, 0.0f), p00, p10};

        for (int k = 0; k < 3; ++k) {
            vertices.push_back(side1[k].x);
            vertices.push_back(side1[k].y);
            vertices.push_back(side1[k].z);
        }
        for (int k = 0; k < 3; ++k) {
            vertices.push_back(side2[k].x);
            vertices.push_back(side2[k].y);
            vertices.push_back(side2[k].z);
        }
        for (int k = 0; k < 3; ++k) {
            vertices.push_back(top[k].x);
            vertices.push_back(top[k].y);
            vertices.push_back(top[k].z);
        }
        for (int k = 0; k < 3; ++k) {
            vertices.push_back(bottom[k].x);
            vertices.push_back(bottom[k].y);
            vertices.push_back(bottom[k].z);
        }
    }
    return vertices;
}

constexpr int kGridW = 10;
constexpr int kGridH = 10;
constexpr float kTileSize = 5.0f;
constexpr float kLevelStep = 5.0f;
// 0 = flat, 1 = ramp +X, 2 = ramp -X, 3 = ramp +Z, 4 = ramp -Z
static const int kHeightMap[kGridH][kGridW] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 1, 2, 2, 1, 0, 0, 0, 0},
    {0, 0, 1, 2, 2, 1, 0, 0, 1, 1},
    {0, 0, 1, 2, 2, 1, 0, 0, 2, 2},
    {0, 0, 1, 2, 2, 1, 0, 0, 2, 2},
    {0, 0, 1, 1, 1, 0, 0, 0, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

int TileLevel(int tx, int tz) {
    if (tx < 0 || tz < 0 || tx >= kGridW || tz >= kGridH) {
        return 0;
    }
    return kHeightMap[tz][tx];
}

int RampDirAt(int tx, int tz) {
    int level = TileLevel(tx, tz);
    if (TileLevel(tx + 1, tz) == level + 1) {
        return 1;
    }
    if (TileLevel(tx - 1, tz) == level + 1) {
        return 2;
    }
    if (TileLevel(tx, tz + 1) == level + 1) {
        return 3;
    }
    if (TileLevel(tx, tz - 1) == level + 1) {
        return 4;
    }
    return 0;
}

float TerrainHeight(float x, float z) {
    float half = (kGridW * kTileSize) * 0.5f;
    float local_x = x + half;
    float local_z = z + half;
    int tx = static_cast<int>(std::floor(local_x / kTileSize));
    int tz = static_cast<int>(std::floor(local_z / kTileSize));
    if (tx < 0 || tz < 0 || tx >= kGridW || tz >= kGridH) {
        return 0.0f;
    }
    float base = static_cast<float>(TileLevel(tx, tz)) * kLevelStep;
    float u = (local_x - tx * kTileSize) / kTileSize;
    float v = (local_z - tz * kTileSize) / kTileSize;
    int ramp = RampDirAt(tx, tz);
    if (ramp == 1) {
        return base + u * kLevelStep;
    }
    if (ramp == 2) {
        return base + (1.0f - u) * kLevelStep;
    }
    if (ramp == 3) {
        return base + v * kLevelStep;
    }
    if (ramp == 4) {
        return base + (1.0f - v) * kLevelStep;
    }

    // Soft ramp bands near edges when adjacent tile is higher.
    const float ramp_band = 0.25f;
    float height = base;
    int level = TileLevel(tx, tz);
    if (TileLevel(tx + 1, tz) == level + 1 && u > 1.0f - ramp_band) {
        float t = (u - (1.0f - ramp_band)) / ramp_band;
        height = base + t * kLevelStep;
    }
    if (TileLevel(tx - 1, tz) == level + 1 && u < ramp_band) {
        float t = (ramp_band - u) / ramp_band;
        height = base + t * kLevelStep;
    }
    if (TileLevel(tx, tz + 1) == level + 1 && v > 1.0f - ramp_band) {
        float t = (v - (1.0f - ramp_band)) / ramp_band;
        height = base + t * kLevelStep;
    }
    if (TileLevel(tx, tz - 1) == level + 1 && v < ramp_band) {
        float t = (ramp_band - v) / ramp_band;
        height = base + t * kLevelStep;
    }
    return height;
}

Mesh CreateMesh(const std::vector<float>& vertices) {
    Mesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    mesh.count = static_cast<GLsizei>(vertices.size() / 3);
    return mesh;
}
}  // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return EXIT_FAILURE;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "Vampire Diary Prototype",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    int window_width = 800;
    int window_height = 600;
    glViewport(0, 0, window_width, window_height);
    glEnable(GL_DEPTH_TEST);

    const char* vertex_source =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "out vec3 vColor;\n"
        "uniform mat4 uMVP;\n"
        "uniform int uUseVertexColor;\n"
        "uniform vec3 uColor;\n"
        "void main() {\n"
        "    vColor = (uUseVertexColor == 1) ? aColor : uColor;\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* fragment_source =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 FragColor;\n"
        "uniform float uAlpha;\n"
        "void main() {\n"
        "    FragColor = vec4(vColor, uAlpha);\n"
        "}\n";

    const char* text_vertex_source =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aUv;\n"
        "out vec2 vUv;\n"
        "uniform mat4 uMVP;\n"
        "void main() {\n"
        "    vUv = aUv;\n"
        "    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* text_fragment_source =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform vec4 uTint;\n"
        "void main() {\n"
        "    vec4 tex = texture(uTexture, vUv);\n"
        "    FragColor = tex * uTint;\n"
        "}\n";

    const char* ring_vertex_source =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec2 vLocal;\n"
        "uniform mat4 uMVP;\n"
        "void main() {\n"
        "    vLocal = aPos.xz;\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* ring_fragment_source =
        "#version 330 core\n"
        "in vec2 vLocal;\n"
        "out vec4 FragColor;\n"
        "uniform vec3 uColor;\n"
        "uniform float uPhase;\n"
        "uniform float uAlpha;\n"
        "void main() {\n"
        "    float dist = length(vLocal);\n"
        "    float rise = smoothstep(0.0, 0.45, uPhase);\n"
        "    float fall = 1.0 - smoothstep(0.55, 1.0, uPhase);\n"
        "    float pulse = rise * fall;\n"
        "    float radius = mix(0.35, 1.05, rise);\n"
        "    float thickness = mix(0.25, 0.10, rise);\n"
        "    float edge = abs(dist - radius);\n"
        "    float ring = smoothstep(thickness, 0.0, edge);\n"
        "    float alpha = ring * (0.25 + 0.75 * pulse) * uAlpha;\n"
        "    if (alpha <= 0.01) discard;\n"
        "    FragColor = vec4(uColor, alpha);\n"
        "}\n";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GLuint text_vs = CompileShader(GL_VERTEX_SHADER, text_vertex_source);
    GLuint text_fs = CompileShader(GL_FRAGMENT_SHADER, text_fragment_source);
    if (!text_vs || !text_fs) {
        glDeleteProgram(program);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GLuint text_program = LinkProgram(text_vs, text_fs);
    glDeleteShader(text_vs);
    glDeleteShader(text_fs);
    if (!text_program) {
        glDeleteProgram(program);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GLuint ring_vs = CompileShader(GL_VERTEX_SHADER, ring_vertex_source);
    GLuint ring_fs = CompileShader(GL_FRAGMENT_SHADER, ring_fragment_source);
    if (!ring_vs || !ring_fs) {
        glDeleteProgram(text_program);
        glDeleteProgram(program);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }
    GLuint ring_program = LinkProgram(ring_vs, ring_fs);
    glDeleteShader(ring_vs);
    glDeleteShader(ring_fs);
    if (!ring_program) {
        glDeleteProgram(text_program);
        glDeleteProgram(program);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::vector<float> ground_vertices;
    std::vector<glm::vec2> ground_tile_centers;
    const int ground_steps = kGridW;
    const float ground_extent = (kGridW * kTileSize) * 0.5f;
    const float ground_step = kTileSize;
    for (int x = 0; x < ground_steps; ++x) {
        for (int z = 0; z < ground_steps; ++z) {
            float x0 = -ground_extent + x * ground_step;
            float x1 = x0 + ground_step;
            float z0 = -ground_extent + z * ground_step;
            float z1 = z0 + ground_step;
            glm::vec3 p00(x0, TerrainHeight(x0, z0), z0);
            glm::vec3 p10(x1, TerrainHeight(x1, z0), z0);
            glm::vec3 p01(x0, TerrainHeight(x0, z1), z1);
            glm::vec3 p11(x1, TerrainHeight(x1, z1), z1);

            glm::vec3 tri1[3] = {p00, p11, p10};
            glm::vec3 tri2[3] = {p00, p01, p11};
            for (int k = 0; k < 3; ++k) {
                ground_vertices.push_back(tri1[k].x);
                ground_vertices.push_back(tri1[k].y);
                ground_vertices.push_back(tri1[k].z);
                ground_vertices.push_back(0.08f);
                ground_vertices.push_back(0.10f);
                ground_vertices.push_back(0.14f);
            }
            for (int k = 0; k < 3; ++k) {
                ground_vertices.push_back(tri2[k].x);
                ground_vertices.push_back(tri2[k].y);
                ground_vertices.push_back(tri2[k].z);
                ground_vertices.push_back(0.08f);
                ground_vertices.push_back(0.10f);
                ground_vertices.push_back(0.14f);
            }
            ground_tile_centers.push_back(glm::vec2((x0 + x1) * 0.5f, (z0 + z1) * 0.5f));
        }
    }

    float cube_vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    float ui_quad_vertices[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,

        0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    float ring_vertices[] = {
        -1.3f, 0.0f, -1.3f,
         1.3f, 0.0f, -1.3f,
         1.3f, 0.0f,  1.3f,

        -1.3f, 0.0f, -1.3f,
         1.3f, 0.0f,  1.3f,
        -1.3f, 0.0f,  1.3f
    };

    GLuint ground_vao = 0;
    GLuint ground_vbo = 0;
    glGenVertexArrays(1, &ground_vao);
    glGenBuffers(1, &ground_vbo);
    glBindVertexArray(ground_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
    glBufferData(GL_ARRAY_BUFFER, ground_vertices.size() * sizeof(float), ground_vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint cube_vao = 0;
    GLuint cube_vbo = 0;
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);
    glBindVertexArray(cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint ui_vao = 0;
    GLuint ui_vbo = 0;
    glGenVertexArrays(1, &ui_vao);
    glGenBuffers(1, &ui_vbo);
    glBindVertexArray(ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ui_quad_vertices), ui_quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint text_vao = 0;
    GLuint text_vbo = 0;
    glGenVertexArrays(1, &text_vao);
    glGenBuffers(1, &text_vbo);
    glBindVertexArray(text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint ring_vao = 0;
    GLuint ring_vbo = 0;
    glGenVertexArrays(1, &ring_vao);
    glGenBuffers(1, &ring_vbo);
    glBindVertexArray(ring_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ring_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ring_vertices), ring_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    Mesh sphere_mesh = CreateMesh(BuildSphereVertices(8, 12, 0.5f));
    Mesh cone_mesh = CreateMesh(BuildConeVertices(12, 0.5f, 1.0f));
    Mesh cylinder_mesh = CreateMesh(BuildCylinderVertices(12, 0.35f, 0.9f));

    GLint mvp_location = glGetUniformLocation(program, "uMVP");
    GLint color_location = glGetUniformLocation(program, "uColor");
    GLint alpha_location = glGetUniformLocation(program, "uAlpha");
    GLint use_vertex_color_location = glGetUniformLocation(program, "uUseVertexColor");
    GLint text_mvp_location = glGetUniformLocation(text_program, "uMVP");
    GLint text_tint_location = glGetUniformLocation(text_program, "uTint");
    GLint text_texture_location = glGetUniformLocation(text_program, "uTexture");
    GLint ring_mvp_location = glGetUniformLocation(ring_program, "uMVP");
    GLint ring_color_location = glGetUniformLocation(ring_program, "uColor");
    GLint ring_phase_location = glGetUniformLocation(ring_program, "uPhase");
    GLint ring_alpha_location = glGetUniformLocation(ring_program, "uAlpha");

    TTF_Font* ui_font = LoadUIFont(18);
    TTF_Font* ui_font_small = LoadUIFont(14);
    if (!ui_font) {
        std::cerr << "Failed to load a UI font (try adding one in assets/)." << std::endl;
    }

    struct Enemy {
        glm::vec3 position;
        float speed;
        int health;
        int damage;
        float scale;
        glm::vec3 color;
        float phase;
        int type;
        bool elite;
    };

    struct ExperienceGem {
        glm::vec3 position;
    };

    enum class PickupType {
        Coin,
        Health,
        Nuke,
        Freeze
    };

    struct Pickup {
        glm::vec3 position;
        PickupType type;
    };

    struct Obstacle {
        glm::vec2 min;
        glm::vec2 max;
    };

    struct Projectile {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        int damage;
    };

    struct Bomb {
        glm::vec3 position;
        glm::vec3 velocity;
        float timer;
    };

    struct Explosion {
        glm::vec3 position;
        float timer;
        float duration;
        float radius;
    };

    enum class ItemId {
        Fireball,
        FireballUpgrade,
        GarlicUpgrade,
        Bomb,
        BombUpgrade,
        MaxHealth,
        MoveSpeed,
        AttackCooldown
    };

    struct ItemChoice {
        ItemId id;
        const char* name;
        int weight;
    };

    enum class GameState {
        MainMenu,
        PowerUps,
        Running,
        Paused,
        LevelUp,
        GameOver
    };

    const char* save_path = "save.txt";
    SaveData save_data;
    LoadSave(save_path, &save_data);

    int coins = save_data.coins;
    int meta_damage_level = save_data.damage_level;
    int meta_speed_level = save_data.speed_level;
    int meta_max_health_level = save_data.max_health_level;
    int meta_attack_cooldown_level = save_data.attack_cooldown_level;
    bool skin_unlocked = save_data.skin_unlocked != 0;
    bool skin_selected = skin_unlocked;

    int main_menu_index = 0;
    int starter_weapon_index = 0;
    int powerup_menu_index = 0;
    GameState state = GameState::MainMenu;
    bool victory = false;
    float game_over_timer = 0.0f;
    float run_time = 0.0f;
    const float win_time = 300.0f;
    int enemies_killed = 0;
    int coins_earned = 0;

    glm::vec3 player_position(0.0f, 0.0f, 0.0f);
    int player_level = 1;
    int player_xp = 0;
    int player_health = 10;
    int player_max_health = 10;
    float player_speed = 4.0f;
    glm::vec3 player_aim_dir(0.0f, 0.0f, -1.0f);
    const glm::vec3 camera_offset(0.0f, 5.0f, 7.0f);
    float camera_yaw = glm::radians(180.0f);
    float camera_pitch = glm::radians(-20.0f);
    float camera_distance = 9.0f;
    const float play_area_extent = 25.0f;

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> side_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> edge_dist(-play_area_extent, play_area_extent);
    std::uniform_real_distribution<float> speed_dist(1.2f, 2.4f);
    std::uniform_real_distribution<float> unit_dist(0.0f, 1.0f);

    std::vector<Enemy> enemies;
    std::vector<ExperienceGem> gems;
    std::vector<Projectile> projectiles;
    std::vector<Bomb> bombs;
    std::vector<Explosion> explosions;
    std::vector<Pickup> pickups;
    std::vector<Obstacle> obstacles;
    float spawn_timer = 0.0f;
    int early_spawn_index = 0;
    const float base_spawn_interval = 2.0f;
    float attack_timer = 0.0f;
    float attack_interval = 1.0f;
    float attack_radius = 2.2f;
    int attack_damage = 1;
    float player_damage_timer = 0.0f;
    const float player_damage_interval = 0.7f;
    const float player_contact_radius = 0.9f;

    int garlic_level = 1;
    int fireball_level = 0;
    float fireball_timer = 0.0f;
    float fireball_cooldown = 1.5f;
    int fireball_damage = 2;
    int bomb_level = 0;
    float bomb_timer = 0.0f;
    float bomb_cooldown = 2.5f;
    float bomb_radius = 2.8f;
    int bomb_damage = 4;

    float freeze_timer = 0.0f;
    float nuke_flash_timer = 0.0f;
    float jump_timer = 0.0f;
    float jump_cooldown_timer = 0.0f;
    const float jump_duration = 0.35f;
    const float jump_cooldown = 0.8f;
    const float jump_height = 1.2f;

    std::vector<ItemChoice> current_choices;

    // Build obstacle list for city blocks.
    const int city_half = 4;
    const float block_size = 5.0f;
    const float block_half = 1.1f;
    obstacles.clear();
    for (int x = -city_half; x <= city_half; ++x) {
        for (int z = -city_half; z <= city_half; ++z) {
            if ((x + z) % 2 != 0) {
                continue;
            }
            if (x == 0 && z == 0) {
                continue;
            }
            float wx = static_cast<float>(x) * block_size;
            float wz = static_cast<float>(z) * block_size;
            int tx = static_cast<int>(std::floor((wx + (kGridW * kTileSize) * 0.5f) / block_size));
            int tz = static_cast<int>(std::floor((wz + (kGridH * kTileSize) * 0.5f) / block_size));
            if (tx < 0 || tz < 0 || tx >= kGridW || tz >= kGridH) {
                continue;
            }
            int ramp_type = RampDirAt(tx, tz);
            int tile_height = TileLevel(tx, tz);
            if (ramp_type != 0 || tile_height < 1) {
                continue;
            }
            glm::vec2 center(static_cast<float>(x) * block_size,
                             static_cast<float>(z) * block_size);
            Obstacle box;
            box.min = center - glm::vec2(block_half, block_half);
            box.max = center + glm::vec2(block_half, block_half);
            obstacles.push_back(box);
        }
    }

    auto spawn_enemy = [&]() {
        float side = side_dist(rng);
        glm::vec3 position(0.0f);
        if (side < 0.25f) {
            position = glm::vec3(-play_area_extent, 0.0f, edge_dist(rng));
        } else if (side < 0.5f) {
            position = glm::vec3(play_area_extent, 0.0f, edge_dist(rng));
        } else if (side < 0.75f) {
            position = glm::vec3(edge_dist(rng), 0.0f, -play_area_extent);
        } else {
            position = glm::vec3(edge_dist(rng), 0.0f, play_area_extent);
        }
        float minute = run_time / 60.0f;
        int roll = static_cast<int>(unit_dist(rng) * 100.0f);
        bool allow_fast = minute >= 1.0f;
        bool allow_tank = minute >= 2.0f;
        bool allow_plant = minute >= 3.0f;

        int health = 3 + player_level / 3;
        float speed = speed_dist(rng);
        int damage = 1;
        float scale = 0.7f;
        glm::vec3 color(0.35f, 0.55f, 0.85f);
        int type = 0;
        bool elite = false;

        if (run_time < 60.0f) {
            int cycle = early_spawn_index % 4;
            early_spawn_index += 1;
            if (cycle == 0) {
                // basic
            } else if (cycle == 1) {
                health = glm::max(2, health - 1);
                speed *= 1.6f;
                damage = 1;
                scale = 0.55f;
                color = glm::vec3(0.85f, 0.30f, 0.55f);
                type = 1;
            } else if (cycle == 2) {
                health += 5;
                speed *= 0.6f;
                damage = 2;
                scale = 1.0f;
                color = glm::vec3(0.45f, 0.60f, 0.22f);
                type = 2;
            } else {
                health += 4;
                speed *= 0.8f;
                damage = 1;
                scale = 0.75f;
                color = glm::vec3(0.30f, 0.70f, 0.35f);
                type = 3;
            }
        } else if (allow_plant && roll < 15) {
            health += 4;
            speed *= 0.8f;
            damage = 1;
            scale = 0.75f;
            color = glm::vec3(0.30f, 0.70f, 0.35f);
            type = 3;
        } else if (allow_tank && roll < 30) {
            health += 5;
            speed *= 0.6f;
            damage = 2;
            scale = 1.0f;
            color = glm::vec3(0.45f, 0.60f, 0.22f);
            type = 2;
        } else if (allow_fast && roll < 65) {
            health = glm::max(2, health - 1);
            speed *= 1.6f;
            damage = 1;
            scale = 0.55f;
            color = glm::vec3(0.85f, 0.30f, 0.55f);
            type = 1;
        }

        position.y = TerrainHeight(position.x, position.z);
        if (run_time >= 90.0f && unit_dist(rng) < 0.12f) {
            elite = true;
            health += 6;
            damage += 1;
            scale *= 1.15f;
            color = glm::vec3(0.95f, 0.80f, 0.25f);
        }

        float phase = unit_dist(rng) * glm::two_pi<float>();
        enemies.push_back(Enemy{position, speed, health, damage, scale, color, phase, type, elite});
    };

    auto save_progress = [&]() {
        save_data.coins = coins;
        save_data.damage_level = meta_damage_level;
        save_data.speed_level = meta_speed_level;
        save_data.max_health_level = meta_max_health_level;
        save_data.attack_cooldown_level = meta_attack_cooldown_level;
        save_data.skin_unlocked = skin_unlocked ? 1 : 0;
        SaveProgress(save_path, save_data);
    };

    auto start_run = [&]() {
        enemies.clear();
        gems.clear();
        projectiles.clear();
        bombs.clear();
        explosions.clear();
        current_choices.clear();
        pickups.clear();

        player_position = glm::vec3(0.0f, 0.0f, 0.0f);
        player_position.y = TerrainHeight(player_position.x, player_position.z);
        player_level = 1;
        player_xp = 0;
        player_max_health = 10 + meta_max_health_level;
        player_health = player_max_health;
        player_speed = 4.0f * (1.0f + 0.05f * static_cast<float>(meta_speed_level));
        attack_interval = glm::max(0.4f, 1.0f - 0.05f * static_cast<float>(meta_attack_cooldown_level));
        attack_radius = 2.2f;
        attack_damage = 1 + meta_damage_level;

        garlic_level = starter_weapon_index == 0 ? 1 : 0;
        fireball_level = starter_weapon_index == 1 ? 1 : 0;
        fireball_timer = 0.0f;
        fireball_cooldown = 1.5f;
        fireball_damage = 2 + meta_damage_level;
        bomb_level = starter_weapon_index == 2 ? 1 : 0;
        bomb_timer = 0.0f;
        bomb_cooldown = 2.5f;
        bomb_radius = 2.8f;
        bomb_damage = 4 + meta_damage_level;

        spawn_timer = 0.0f;
        early_spawn_index = 0;
        attack_timer = 0.0f;
        player_damage_timer = 0.0f;
        run_time = 0.0f;
        enemies_killed = 0;
        coins_earned = 0;
        victory = false;
        freeze_timer = 0.0f;
    };

    auto end_run = [&](bool won) {
        victory = won;
        game_over_timer = 0.0f;
        coins += coins_earned + (won ? 25 : 0) + static_cast<int>(run_time / 20.0f);
        save_progress();
        state = GameState::GameOver;
    };

    auto draw_ui_quad = [&](float x, float y, float w, float h, const glm::vec3& color,
                            const glm::mat4& projection) {
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(x, y, 0.0f));
        model = glm::scale(model, glm::vec3(w, h, 1.0f));
        glm::mat4 mvp = projection * model;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(color_location, color.r, color.g, color.b);
        glBindVertexArray(ui_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    };

    auto draw_text_with_font = [&](TTF_Font* font, float x, float y, const std::string& text,
                                   SDL_Color color, const glm::mat4& projection) {
        if (!font) {
            return;
        }
        TextTexture texture = CreateTextTexture(font, text, color);
        if (texture.id == 0) {
            return;
        }

        float xpos = x;
        float ypos = y;
        float w = static_cast<float>(texture.width);
        float h = static_cast<float>(texture.height);
        float vertices[6][4] = {
            {xpos,     ypos,     0.0f, 1.0f},
            {xpos + w, ypos,     1.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f},
            {xpos,     ypos,     0.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f},
            {xpos,     ypos + h, 0.0f, 0.0f}
        };

        glUseProgram(text_program);
        glUniformMatrix4fv(text_mvp_location, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform4f(text_tint_location,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glUniform1i(text_texture_location, 0);

        glBindVertexArray(text_vao);
        glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        DestroyTextTexture(&texture);
        glUseProgram(program);
        glUniform1f(alpha_location, 1.0f);
        glUniform1i(use_vertex_color_location, 0);
    };

    auto draw_text = [&](float x, float y, const std::string& text, SDL_Color color,
                         const glm::mat4& projection) {
        draw_text_with_font(ui_font, x, y, text, color, projection);
    };

    auto draw_text_centered = [&](float x, float y, float w, float h, const std::string& text,
                                  SDL_Color color, const glm::mat4& projection) {
        if (!ui_font) {
            return;
        }
        TextTexture texture = CreateTextTexture(ui_font, text, color);
        if (texture.id == 0) {
            return;
        }

        float xpos = x + (w - static_cast<float>(texture.width)) * 0.5f;
        float ypos = y + (h - static_cast<float>(texture.height)) * 0.5f;
        float vertices[6][4] = {
            {xpos,                     ypos,                     0.0f, 1.0f},
            {xpos + texture.width,     ypos,                     1.0f, 1.0f},
            {xpos + texture.width,     ypos + texture.height,    1.0f, 0.0f},
            {xpos,                     ypos,                     0.0f, 1.0f},
            {xpos + texture.width,     ypos + texture.height,    1.0f, 0.0f},
            {xpos,                     ypos + texture.height,    0.0f, 0.0f}
        };

        glUseProgram(text_program);
        glUniformMatrix4fv(text_mvp_location, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform4f(text_tint_location,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glUniform1i(text_texture_location, 0);

        glBindVertexArray(text_vao);
        glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        DestroyTextTexture(&texture);
        glUseProgram(program);
    };

    auto circle_intersects_aabb = [&](const glm::vec2& center, float radius, const Obstacle& box) {
        glm::vec2 closest = glm::clamp(center, box.min, box.max);
        glm::vec2 delta = center - closest;
        return glm::dot(delta, delta) < radius * radius;
    };

    Uint64 last_ticks = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        SDL_SetRelativeMouseMode(state == GameState::Running ? SDL_TRUE : SDL_FALSE);
        Uint64 current_ticks = SDL_GetPerformanceCounter();
        float delta_time = static_cast<float>(current_ticks - last_ticks) /
                           static_cast<float>(SDL_GetPerformanceFrequency());
        last_ticks = current_ticks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
                glViewport(0, 0, window_width, window_height);
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (state == GameState::MainMenu) {
                    if (key == SDLK_UP || key == SDLK_w) {
                        main_menu_index = (main_menu_index + 2) % 3;
                    } else if (key == SDLK_DOWN || key == SDLK_s) {
                        main_menu_index = (main_menu_index + 1) % 3;
                    } else if (key == SDLK_LEFT || key == SDLK_a) {
                        starter_weapon_index = (starter_weapon_index + 2) % 3;
                    } else if (key == SDLK_RIGHT || key == SDLK_d) {
                        starter_weapon_index = (starter_weapon_index + 1) % 3;
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (main_menu_index == 0) {
                            start_run();
                            state = GameState::Running;
                        } else if (main_menu_index == 1) {
                            powerup_menu_index = 0;
                            state = GameState::PowerUps;
                        } else {
                            running = false;
                        }
                    }
                } else if (state == GameState::PowerUps) {
                    int option_count = 5;
                    if (key == SDLK_UP || key == SDLK_w) {
                        powerup_menu_index = (powerup_menu_index + option_count - 1) % option_count;
                    } else if (key == SDLK_DOWN || key == SDLK_s) {
                        powerup_menu_index = (powerup_menu_index + 1) % option_count;
                    } else if (key == SDLK_ESCAPE) {
                        state = GameState::MainMenu;
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        int index = powerup_menu_index;
                        if (index == 0) {
                            int cost = 10 + 5 * meta_damage_level;
                            if (meta_damage_level < 10 && coins >= cost) {
                                coins -= cost;
                                meta_damage_level += 1;
                                save_progress();
                            }
                        } else if (index == 1) {
                            int cost = 8 + 4 * meta_speed_level;
                            if (meta_speed_level < 10 && coins >= cost) {
                                coins -= cost;
                                meta_speed_level += 1;
                                save_progress();
                            }
                        } else if (index == 2) {
                            int cost = 12 + 6 * meta_max_health_level;
                            if (meta_max_health_level < 20 && coins >= cost) {
                                coins -= cost;
                                meta_max_health_level += 1;
                                save_progress();
                            }
                        } else if (index == 3) {
                            int cost = 12 + 6 * meta_attack_cooldown_level;
                            if (meta_attack_cooldown_level < 10 && coins >= cost) {
                                coins -= cost;
                                meta_attack_cooldown_level += 1;
                                save_progress();
                            }
                        } else if (index == 4) {
                            if (!skin_unlocked) {
                                int cost = 60;
                                if (coins >= cost) {
                                    coins -= cost;
                                    skin_unlocked = true;
                                    skin_selected = true;
                                    save_progress();
                                }
                            } else {
                                skin_selected = !skin_selected;
                            }
                        }
                    }
                } else if (state == GameState::Running) {
                    if (key == SDLK_ESCAPE) {
                        state = GameState::Paused;
                    }
                } else if (state == GameState::Paused) {
                    if (key == SDLK_ESCAPE) {
                        state = GameState::Running;
                    } else if (key == SDLK_q) {
                        state = GameState::MainMenu;
                    }
                } else if (state == GameState::LevelUp) {
                    int index = -1;
                    if (key == SDLK_1) {
                        index = 0;
                    } else if (key == SDLK_2) {
                        index = 1;
                    } else if (key == SDLK_3) {
                        index = 2;
                    }
                    if (index >= 0 && index < static_cast<int>(current_choices.size())) {
                        ItemId choice = current_choices[index].id;
                        switch (choice) {
                            case ItemId::Fireball:
                                fireball_level = 1;
                                fireball_damage = 2 + meta_damage_level;
                                fireball_cooldown = 1.5f;
                                break;
                            case ItemId::FireballUpgrade:
                                fireball_level += 1;
                                fireball_damage += 1;
                                fireball_cooldown = glm::max(0.6f, fireball_cooldown - 0.1f);
                                break;
                        case ItemId::GarlicUpgrade:
                            garlic_level += 1;
                            attack_damage += 1;
                            attack_radius += 0.2f;
                            break;
                        case ItemId::Bomb:
                            bomb_level = 1;
                            bomb_damage = 4 + meta_damage_level;
                            bomb_cooldown = 2.5f;
                            bomb_radius = 2.8f;
                            break;
                        case ItemId::BombUpgrade:
                            bomb_level += 1;
                            bomb_damage += 2;
                            bomb_radius += 0.3f;
                            bomb_cooldown = glm::max(1.2f, bomb_cooldown - 0.2f);
                            break;
                            case ItemId::MaxHealth: {
                                int bonus = static_cast<int>(std::ceil(player_max_health * 0.2f));
                                player_max_health += bonus;
                                player_health += bonus;
                                break;
                            }
                            case ItemId::MoveSpeed:
                                player_speed *= 1.1f;
                                break;
                            case ItemId::AttackCooldown:
                                attack_interval = glm::max(0.4f, attack_interval - 0.1f);
                                break;
                        }
                        current_choices.clear();
                        state = GameState::Running;
                    }
                }
            } else if (event.type == SDL_MOUSEMOTION && state == GameState::Running) {
                const float sensitivity = 0.0018f;
                camera_yaw += static_cast<float>(event.motion.xrel) * sensitivity;
                camera_pitch += static_cast<float>(event.motion.yrel) * sensitivity;
                camera_pitch = glm::clamp(camera_pitch, glm::radians(-45.0f), glm::radians(10.0f));
            } else if (event.type == SDL_MOUSEWHEEL && state == GameState::Running) {
                camera_distance -= static_cast<float>(event.wheel.y) * 0.6f;
                camera_distance = glm::clamp(camera_distance, 4.0f, 16.0f);
            }
        }

        if (state == GameState::Running) {
            run_time += delta_time;
            if (run_time >= win_time) {
                end_run(true);
            }
            if (state == GameState::Running) {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            glm::vec3 input(0.0f);
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
                input.z += 1.0f;
            }
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
                input.z -= 1.0f;
            }
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
                input.x -= 1.0f;
            }
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
                input.x += 1.0f;
            }
            glm::vec3 forward = glm::normalize(glm::vec3(
                std::cos(camera_yaw), 0.0f, std::sin(camera_yaw)));
            glm::vec3 right = glm::normalize(glm::vec3(-forward.z, 0.0f, forward.x));
            glm::vec3 move = input.x * right + input.z * forward;
            if (glm::length(move) > 0.0f) {
                move = glm::normalize(move);
                player_aim_dir = forward;
            }
            if (jump_cooldown_timer > 0.0f) {
                jump_cooldown_timer = glm::max(0.0f, jump_cooldown_timer - delta_time);
            }
            if (jump_timer <= 0.0f && jump_cooldown_timer <= 0.0f &&
                (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_LSHIFT])) {
                jump_timer = jump_duration;
                jump_cooldown_timer = jump_cooldown;
            }
            glm::vec3 desired = player_position + move * player_speed * delta_time;
            desired.x = glm::clamp(desired.x, -play_area_extent, play_area_extent);
            desired.z = glm::clamp(desired.z, -play_area_extent, play_area_extent);
            desired.y = TerrainHeight(desired.x, desired.z);

            const float player_radius = 0.6f;
            glm::vec2 desired_xz(desired.x, desired.z);
            bool blocked = false;
            for (const Obstacle& box : obstacles) {
                if (circle_intersects_aabb(desired_xz, player_radius, box)) {
                    blocked = true;
                    break;
                }
            }
                if (!blocked) {
                    player_position.x = desired.x;
                    player_position.z = desired.z;
                    player_position.y = glm::mix(player_position.y, desired.y, 0.25f);
                } else {
                    glm::vec3 slide_x = player_position + glm::vec3(move.x, 0.0f, 0.0f) * player_speed * delta_time;
                    slide_x.x = glm::clamp(slide_x.x, -play_area_extent, play_area_extent);
                    slide_x.y = TerrainHeight(slide_x.x, slide_x.z);
                    glm::vec2 slide_xz(slide_x.x, player_position.z);
                    bool blocked_x = false;
                for (const Obstacle& box : obstacles) {
                    if (circle_intersects_aabb(slide_xz, player_radius, box)) {
                        blocked_x = true;
                        break;
                    }
                }
                    if (!blocked_x) {
                        player_position.x = slide_x.x;
                        player_position.y = glm::mix(player_position.y, slide_x.y, 0.25f);
                    }

                    glm::vec3 slide_z = player_position + glm::vec3(0.0f, 0.0f, move.z) * player_speed * delta_time;
                    slide_z.z = glm::clamp(slide_z.z, -play_area_extent, play_area_extent);
                    slide_z.y = TerrainHeight(slide_z.x, slide_z.z);
                    glm::vec2 slide_zz(player_position.x, slide_z.z);
                    bool blocked_z = false;
                for (const Obstacle& box : obstacles) {
                    if (circle_intersects_aabb(slide_zz, player_radius, box)) {
                        blocked_z = true;
                        break;
                    }
                }
                    if (!blocked_z) {
                        player_position.z = slide_z.z;
                        player_position.y = glm::mix(player_position.y, slide_z.y, 0.25f);
                    }
                }

            float spawn_interval = glm::max(0.5f, base_spawn_interval - player_level * 0.05f);
            spawn_interval = glm::max(0.35f, spawn_interval - run_time * 0.01f);
            spawn_timer += delta_time;
            if (spawn_timer >= spawn_interval) {
                spawn_timer = 0.0f;
                spawn_enemy();
                if (run_time >= 20.0f) {
                    spawn_enemy();
                }
                if (run_time >= 60.0f && unit_dist(rng) < 0.45f) {
                    spawn_enemy();
                }
            }

            if (freeze_timer > 0.0f) {
                freeze_timer -= delta_time;
            } else {
                for (Enemy& enemy : enemies) {
                    glm::vec3 to_player = player_position - enemy.position;
                    to_player.y = 0.0f;
                    float distance = glm::length(to_player);
                    if (distance > 0.001f) {
                        glm::vec3 direction = to_player / distance;
                        glm::vec3 desired_enemy = enemy.position + direction * enemy.speed * delta_time;
                        glm::vec2 desired_enemy_xz(desired_enemy.x, desired_enemy.z);
                        bool blocked_enemy = false;
                        for (const Obstacle& box : obstacles) {
                                if (circle_intersects_aabb(desired_enemy_xz, enemy.scale * 0.55f, box)) {
                                blocked_enemy = true;
                                break;
                            }
                        }
                        if (!blocked_enemy) {
                            enemy.position.x = desired_enemy.x;
                            enemy.position.z = desired_enemy.z;
                            enemy.position.y = glm::mix(enemy.position.y,
                                                       TerrainHeight(enemy.position.x, enemy.position.z), 0.25f);
                        } else {
                            glm::vec3 slide_x = enemy.position + glm::vec3(direction.x, 0.0f, 0.0f)
                                                * enemy.speed * delta_time;
                            slide_x.y = TerrainHeight(slide_x.x, slide_x.z);
                            glm::vec2 slide_xz(slide_x.x, enemy.position.z);
                            bool blocked_x = false;
                            for (const Obstacle& box : obstacles) {
                                if (circle_intersects_aabb(slide_xz, enemy.scale * 0.55f, box)) {
                                    blocked_x = true;
                                    break;
                                }
                            }
                            if (!blocked_x) {
                                enemy.position.x = slide_x.x;
                                enemy.position.y = glm::mix(enemy.position.y, slide_x.y, 0.25f);
                            } else {
                                glm::vec3 slide_z = enemy.position + glm::vec3(0.0f, 0.0f, direction.z)
                                                    * enemy.speed * delta_time;
                                slide_z.y = TerrainHeight(slide_z.x, slide_z.z);
                                glm::vec2 slide_zz(enemy.position.x, slide_z.z);
                                bool blocked_z = false;
                                for (const Obstacle& box : obstacles) {
                                if (circle_intersects_aabb(slide_zz, enemy.scale * 0.55f, box)) {
                                        blocked_z = true;
                                        break;
                                    }
                                }
                                if (!blocked_z) {
                                    enemy.position.z = slide_z.z;
                                    enemy.position.y = glm::mix(enemy.position.y, slide_z.y, 0.25f);
                                }
                            }
                        }
                    }
                }
            }

            if (jump_timer > 0.0f) {
                jump_timer = glm::max(0.0f, jump_timer - delta_time);
            }
            if (nuke_flash_timer > 0.0f) {
                nuke_flash_timer = glm::max(0.0f, nuke_flash_timer - delta_time);
            }

            if (garlic_level > 0) {
                attack_timer += delta_time;
                if (attack_timer >= attack_interval) {
                    attack_timer = 0.0f;
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - player_position;
                        delta.y = 0.0f;
                        if (glm::length(delta) <= attack_radius) {
                            enemy.health -= attack_damage;
                        }
                    }
                }
            }

            fireball_timer += delta_time;
            if (fireball_level > 0 && fireball_timer >= fireball_cooldown) {
                fireball_timer = 0.0f;
                int projectile_count = 1 + (fireball_level - 1) / 2;
                for (int p = 0; p < projectile_count; ++p) {
                    glm::vec3 direction = player_aim_dir;
                    if (!enemies.empty() && glm::length(direction) < 0.001f) {
                        float best_dist = std::numeric_limits<float>::max();
                        glm::vec3 best_dir(0.0f, 0.0f, -1.0f);
                        for (const Enemy& enemy : enemies) {
                            glm::vec3 to_enemy = enemy.position - player_position;
                            to_enemy.y = 0.0f;
                            float dist = glm::dot(to_enemy, to_enemy);
                            if (dist < best_dist) {
                                best_dist = dist;
                                best_dir = to_enemy;
                            }
                        }
                        direction = best_dir;
                    }
                    if (glm::length(direction) < 0.001f) {
                        direction = glm::vec3(0.0f, 0.0f, -1.0f);
                    }
                    direction = glm::normalize(direction);
                    projectiles.push_back(Projectile{
                        player_position + glm::vec3(0.0f, 0.6f, 0.0f),
                        direction * 7.0f,
                        3.0f,
                        fireball_damage});
                }
            }

            bomb_timer += delta_time;
            if (bomb_level > 0 && bomb_timer >= bomb_cooldown) {
                bomb_timer = 0.0f;
                glm::vec3 direction = player_aim_dir;
                if (glm::length(direction) < 0.001f) {
                    direction = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                direction = glm::normalize(direction);
                bombs.push_back(Bomb{
                    player_position + glm::vec3(0.0f, 0.6f, 0.0f),
                    direction * 5.0f,
                    0.8f});
            }

            for (size_t i = 0; i < bombs.size();) {
                bombs[i].position += bombs[i].velocity * delta_time;
                bombs[i].timer -= delta_time;
                if (bombs[i].timer <= 0.0f) {
                    explosions.push_back(Explosion{
                        bombs[i].position,
                        0.0f,
                        0.5f,
                        bomb_radius});
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - bombs[i].position;
                        delta.y = 0.0f;
                        if (glm::length(delta) <= bomb_radius) {
                            enemy.health -= bomb_damage;
                        }
                    }
                    bombs[i] = bombs.back();
                    bombs.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < projectiles.size();) {
                projectiles[i].position += projectiles[i].velocity * delta_time;
                projectiles[i].lifetime -= delta_time;
                if (projectiles[i].lifetime <= 0.0f) {
                    projectiles[i] = projectiles.back();
                    projectiles.pop_back();
                    continue;
                }

                bool hit = false;
                for (Enemy& enemy : enemies) {
                    glm::vec3 delta = enemy.position - projectiles[i].position;
                    delta.y = 0.0f;
                    if (glm::length(delta) <= 0.6f) {
                        enemy.health -= projectiles[i].damage;
                        hit = true;
                        break;
                    }
                }
                if (hit) {
                    projectiles[i] = projectiles.back();
                    projectiles.pop_back();
                } else {
                    ++i;
                }
            }

            bool player_contact = false;
            int contact_damage = 0;
            for (const Enemy& enemy : enemies) {
                glm::vec3 delta = enemy.position - player_position;
                delta.y = 0.0f;
                if (glm::length(delta) <= player_contact_radius) {
                    player_contact = true;
                    contact_damage = glm::max(contact_damage, enemy.damage);
                    break;
                }
            }
            player_damage_timer += delta_time;
            if (jump_timer <= 0.0f && player_contact && player_damage_timer >= player_damage_interval) {
                player_damage_timer = 0.0f;
                player_health -= glm::max(1, contact_damage);
            }

            for (size_t i = 0; i < enemies.size();) {
                if (enemies[i].health <= 0) {
                    gems.push_back(ExperienceGem{enemies[i].position});
                    enemies_killed += 1;
                    coins_earned += enemies[i].elite ? 3 : 1;
                    float drop_roll = unit_dist(rng);
                    if (drop_roll < 0.08f) {
                        pickups.push_back(Pickup{enemies[i].position, PickupType::Coin});
                    } else if (drop_roll < 0.11f) {
                        pickups.push_back(Pickup{enemies[i].position, PickupType::Health});
                    } else if (drop_roll < 0.125f) {
                        pickups.push_back(Pickup{enemies[i].position, PickupType::Freeze});
                    } else if (drop_roll < 0.13f) {
                        pickups.push_back(Pickup{enemies[i].position, PickupType::Nuke});
                    }
                    enemies[i] = enemies.back();
                    enemies.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < gems.size();) {
                glm::vec3 delta = gems[i].position - player_position;
                delta.y = 0.0f;
                if (glm::length(delta) <= 1.0f) {
                    player_xp += 1;
                    gems[i] = gems.back();
                    gems.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < pickups.size();) {
                glm::vec3 delta = pickups[i].position - player_position;
                delta.y = 0.0f;
                if (glm::length(delta) <= 1.1f) {
                    switch (pickups[i].type) {
                        case PickupType::Coin:
                            coins_earned += 5;
                            break;
                        case PickupType::Health:
                            player_health = glm::min(player_max_health, player_health + 4);
                            break;
                        case PickupType::Freeze:
                            freeze_timer = 4.0f;
                            break;
                        case PickupType::Nuke:
                            for (Enemy& enemy : enemies) {
                                enemy.health = 0;
                            }
                            nuke_flash_timer = 0.45f;
                            break;
                    }
                    pickups[i] = pickups.back();
                    pickups.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < explosions.size();) {
                explosions[i].timer += delta_time;
                if (explosions[i].timer >= explosions[i].duration) {
                    explosions[i] = explosions.back();
                    explosions.pop_back();
                } else {
                    ++i;
                }
            }

            int xp_needed = 5 + (player_level - 1) * 2;
            if (player_xp >= xp_needed) {
                player_xp -= xp_needed;
                player_level += 1;
                state = GameState::LevelUp;

                std::vector<ItemChoice> pool;
                if (garlic_level < 5) {
                    pool.push_back(ItemChoice{ItemId::GarlicUpgrade, "Garlic Aura +", 100});
                }
                if (fireball_level == 0) {
                    pool.push_back(ItemChoice{ItemId::Fireball, "Fireball", 80});
                } else if (fireball_level < 5) {
                    pool.push_back(ItemChoice{ItemId::FireballUpgrade, "Fireball +", 60});
                }
                if (bomb_level == 0) {
                    pool.push_back(ItemChoice{ItemId::Bomb, "Bomb", 70});
                } else if (bomb_level < 5) {
                    pool.push_back(ItemChoice{ItemId::BombUpgrade, "Bomb +", 50});
                }
                if (player_max_health < 40) {
                    pool.push_back(ItemChoice{ItemId::MaxHealth, "Max Health +20%", 80});
                }
                if (player_speed < 7.5f) {
                    pool.push_back(ItemChoice{ItemId::MoveSpeed, "Move Speed +10%", 60});
                }
                if (attack_interval > 0.5f) {
                    pool.push_back(ItemChoice{ItemId::AttackCooldown, "Attack Cooldown -", 50});
                }

                current_choices.clear();
                if (pool.empty()) {
                    state = GameState::Running;
                }
                for (int pick = 0; pick < 3 && !pool.empty(); ++pick) {
                    int total_weight = 0;
                    for (const ItemChoice& item : pool) {
                        total_weight += item.weight;
                    }
                    int roll = static_cast<int>(unit_dist(rng) * total_weight);
                    int running_weight = 0;
                    size_t chosen = 0;
                    for (size_t i = 0; i < pool.size(); ++i) {
                        running_weight += pool[i].weight;
                        if (roll < running_weight) {
                            chosen = i;
                            break;
                        }
                    }
                    current_choices.push_back(pool[chosen]);
                    pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(chosen));
                }
            }

            if (player_health <= 0) {
                end_run(false);
            }
            }
        }

        if (state == GameState::GameOver) {
            game_over_timer += delta_time;
            if (game_over_timer >= 3.0f) {
                state = GameState::MainMenu;
            }
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
            static_cast<float>(window_width) / static_cast<float>(window_height),
            0.1f,
            140.0f);
        float jump_view_phase = jump_timer > 0.0f ? (jump_duration - jump_timer) / jump_duration : 0.0f;
        float jump_view_offset = jump_timer > 0.0f ? std::sin(jump_view_phase * glm::pi<float>()) * jump_height : 0.0f;
        glm::vec3 cam_dir(
            std::cos(camera_pitch) * std::cos(camera_yaw),
            std::sin(camera_pitch),
            std::cos(camera_pitch) * std::sin(camera_yaw));
        glm::vec3 camera_pos = player_position - cam_dir * camera_distance;
        float min_cam_y = TerrainHeight(camera_pos.x, camera_pos.z) + 1.0f;
        if (camera_pos.y < min_cam_y) {
            camera_pos.y = min_cam_y;
        }
        glm::mat4 view = glm::lookAt(
            camera_pos,
            player_position + glm::vec3(0.0f, jump_view_offset, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Update ground colors based on nearby entities.
        for (size_t i = 0; i < ground_tile_centers.size(); ++i) {
            glm::vec2 center = ground_tile_centers[i];
            float player_dist = glm::length(center - glm::vec2(player_position.x, player_position.z));
            float player_glow = std::exp(-(player_dist * player_dist) / 120.0f);
            float enemy_glow = 0.0f;
            for (const Enemy& enemy : enemies) {
                glm::vec2 epos(enemy.position.x, enemy.position.z);
                float dist = glm::length(center - epos);
                if (dist < 4.0f) {
                    enemy_glow = glm::max(enemy_glow, 1.0f - dist / 4.0f);
                }
            }
            float pulse = 0.6f + 0.4f * std::sin(run_time * 1.5f + (center.x + center.y) * 0.2f);
            glm::vec3 base(0.12f, 0.16f, 0.12f);
            glm::vec3 glow_color = base
                + glm::vec3(0.08f, 0.20f, 0.10f) * player_glow
                + glm::vec3(0.35f, 0.12f, 0.10f) * enemy_glow * pulse;

            size_t vertex_index = i * 6 * 6;
            for (int v = 0; v < 6; ++v) {
                ground_vertices[vertex_index + v * 6 + 3] = glow_color.r;
                ground_vertices[vertex_index + v * 6 + 4] = glow_color.g;
                ground_vertices[vertex_index + v * 6 + 5] = glow_color.b;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, ground_vertices.size() * sizeof(float),
                        ground_vertices.data());

        glm::mat4 ground_model(1.0f);
        glm::mat4 ground_mvp = projection * view * ground_model;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(ground_mvp));
        glUniform1i(use_vertex_color_location, 1);
        glBindVertexArray(ground_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ground_vertices.size() / 6));
        glBindVertexArray(0);
        glUniform1i(use_vertex_color_location, 0);

        // Wireframe overlay for distinct triangles.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.0f);
        glUniform3f(color_location, 0.10f, 0.35f, 0.18f);
        glUniform1i(use_vertex_color_location, 0);
        glBindVertexArray(ground_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ground_vertices.size() / 6));
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_BLEND);

        // City blocks (wireframe skyline so enemies remain visible)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.5f);
        const int city_half = 4;
        const float block_size = 5.0f;
        for (int x = -city_half; x <= city_half; ++x) {
            for (int z = -city_half; z <= city_half; ++z) {
                if ((x + z) % 2 != 0) {
                    continue;
                }
                if (x == 0 && z == 0) {
                    continue;
                }
                float wx = static_cast<float>(x) * block_size;
                float wz = static_cast<float>(z) * block_size;
                int tx = static_cast<int>(std::floor((wx + (kGridW * kTileSize) * 0.5f) / block_size));
                int tz = static_cast<int>(std::floor((wz + (kGridH * kTileSize) * 0.5f) / block_size));
                int ramp_type = RampDirAt(tx, tz);
                int tile_height = TileLevel(tx, tz);
                if (ramp_type != 0 || tile_height < 1) {
                    continue;
                }
                float height = 1.5f + 0.6f * static_cast<float>((x * x + z * z) % 6);
                float base_height = TerrainHeight(wx, wz);
                glm::vec3 base(static_cast<float>(x) * block_size,
                               base_height + height * 0.5f,
                               static_cast<float>(z) * block_size);
                glm::mat4 block = glm::translate(glm::mat4(1.0f), base);
                block = glm::scale(block, glm::vec3(block_half * 2.0f, height, block_half * 2.0f));
                glm::mat4 block_mvp = projection * view * block;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(block_mvp));
                float glow = 0.55f + 0.45f * std::sin(run_time * 1.5f + (x + z) * 0.6f);
                glUniform3f(color_location, 0.25f * glow, 0.75f * glow, 0.95f * glow);
                glBindVertexArray(cube_vao);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_BLEND);

        if (state == GameState::Running && garlic_level > 0) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            glm::mat4 ring_model = glm::translate(
                glm::mat4(1.0f),
                player_position + glm::vec3(0.0f, 0.05f, 0.0f));
            ring_model = glm::scale(ring_model, glm::vec3(attack_radius, 1.0f, attack_radius));
            glm::mat4 ring_mvp = projection * view * ring_model;
            glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
            glUniform3f(ring_color_location, 0.30f, 0.95f, 0.40f);
            float phase = attack_interval > 0.0f ? glm::clamp(attack_timer / attack_interval, 0.0f, 1.0f) : 0.0f;
            glUniform1f(ring_phase_location, phase);
            glUniform1f(ring_alpha_location, 1.0f);
            glBindVertexArray(ring_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        glm::vec3 player_color = skin_selected
            ? glm::vec3(0.35f, 0.20f, 0.85f)
            : glm::vec3(0.95f, 0.55f, 0.10f);
        glUniform3f(color_location, player_color.r, player_color.g, player_color.b);

        float jump_phase = jump_timer > 0.0f ? (jump_duration - jump_timer) / jump_duration : 0.0f;
        float jump_offset = jump_timer > 0.0f ? std::sin(jump_phase * glm::pi<float>()) * jump_height : 0.0f;
        glm::vec3 base_pos = player_position + glm::vec3(0.0f, 0.35f + jump_offset, 0.0f);
        glm::mat4 body = glm::translate(glm::mat4(1.0f), base_pos);
        body = glm::scale(body, glm::vec3(0.9f, 0.6f, 1.1f));
        glm::mat4 body_mvp = projection * view * body;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(body_mvp));
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glBindVertexArray(0);

        glm::mat4 head = glm::translate(glm::mat4(1.0f), base_pos + glm::vec3(0.0f, 0.35f, 0.0f));
        head = glm::scale(head, glm::vec3(0.45f));
        glm::mat4 head_mvp = projection * view * head;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(head_mvp));
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);

        glm::mat4 tail = glm::translate(glm::mat4(1.0f), base_pos + glm::vec3(0.0f, -0.35f, 0.0f));
        tail = glm::scale(tail, glm::vec3(0.35f));
        glm::mat4 tail_mvp = projection * view * tail;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(tail_mvp));
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);

        for (const Enemy& enemy : enemies) {
            float bob = 0.08f * std::sin(run_time * 3.5f + enemy.phase);
            glm::vec3 base_pos = enemy.position + glm::vec3(0.0f, 0.35f + bob, 0.0f);
            if (enemy.elite) {
                float glow = 0.65f + 0.35f * std::sin(run_time * 5.0f + enemy.phase);
                glUniform3f(color_location, enemy.color.r * glow, enemy.color.g * glow, enemy.color.b * glow);
            } else {
                glUniform3f(color_location, enemy.color.r, enemy.color.g, enemy.color.b);
            }

            if (enemy.type == 0) {
                glm::mat4 body = glm::translate(glm::mat4(1.0f), base_pos);
                body = glm::scale(body, glm::vec3(enemy.scale * 0.9f, enemy.scale * 0.6f, enemy.scale * 1.1f));
                glm::mat4 body_mvp = projection * view * body;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(body_mvp));
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);

                glm::mat4 head = glm::translate(glm::mat4(1.0f), base_pos + glm::vec3(0.0f, 0.35f, 0.0f));
                head = glm::scale(head, glm::vec3(enemy.scale * 0.45f));
                glm::mat4 head_mvp = projection * view * head;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(head_mvp));
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);

                glm::mat4 tail = glm::translate(glm::mat4(1.0f), base_pos + glm::vec3(0.0f, -0.35f, 0.0f));
                tail = glm::scale(tail, glm::vec3(enemy.scale * 0.35f));
                glm::mat4 tail_mvp = projection * view * tail;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(tail_mvp));
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
            } else if (enemy.type == 1) {
                glm::mat4 enemy_model = glm::translate(glm::mat4(1.0f), base_pos);
                enemy_model = glm::scale(enemy_model, glm::vec3(enemy.scale * 0.7f,
                                                               enemy.scale * 0.45f,
                                                               enemy.scale * 1.4f));
                glm::mat4 enemy_mvp = projection * view * enemy_model;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(enemy_mvp));
                glBindVertexArray(cone_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                glBindVertexArray(0);
            } else if (enemy.type == 3) {
                glm::mat4 stem = glm::translate(glm::mat4(1.0f), base_pos);
                stem = glm::scale(stem, glm::vec3(enemy.scale * 0.6f,
                                                  enemy.scale * 1.1f,
                                                  enemy.scale * 0.6f));
                glm::mat4 stem_mvp = projection * view * stem;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(stem_mvp));
                glBindVertexArray(cone_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                glBindVertexArray(0);

                glm::mat4 bud = glm::translate(glm::mat4(1.0f),
                                               base_pos + glm::vec3(0.0f, 0.55f, 0.0f));
                bud = glm::scale(bud, glm::vec3(enemy.scale * 0.35f));
                glm::mat4 bud_mvp = projection * view * bud;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(bud_mvp));
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
            } else {
                glm::mat4 enemy_model = glm::translate(glm::mat4(1.0f), base_pos);
                enemy_model = glm::scale(enemy_model, glm::vec3(enemy.scale * 1.2f,
                                                               enemy.scale * 1.1f,
                                                               enemy.scale * 1.2f));
                glm::mat4 enemy_mvp = projection * view * enemy_model;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(enemy_mvp));
                glBindVertexArray(cube_vao);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        for (const ExperienceGem& gem : gems) {
            glm::mat4 gem_model = glm::translate(
                glm::mat4(1.0f),
                gem.position + glm::vec3(0.0f, 0.2f, 0.0f));
            gem_model = glm::scale(gem_model, glm::vec3(0.3f));
            glm::mat4 gem_mvp = projection * view * gem_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(gem_mvp));
            glUniform3f(color_location, 0.20f, 0.65f, 0.95f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        for (const Bomb& bomb : bombs) {
            glm::mat4 bomb_model = glm::translate(
                glm::mat4(1.0f),
                bomb.position);
            bomb_model = glm::scale(bomb_model, glm::vec3(0.25f));
            glm::mat4 bomb_mvp = projection * view * bomb_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(bomb_mvp));
            glUniform3f(color_location, 0.35f, 0.10f, 0.08f);
            glBindVertexArray(sphere_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
            glBindVertexArray(0);
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const Pickup& pickup : pickups) {
            glm::vec3 color(0.85f, 0.85f, 0.2f);
            float scale = 0.35f;
            switch (pickup.type) {
                case PickupType::Coin:
                    color = glm::vec3(0.95f, 0.78f, 0.20f);
                    scale = 0.22f;
                    break;
                case PickupType::Health:
                    color = glm::vec3(0.85f, 0.20f, 0.22f);
                    scale = 0.3f;
                    break;
                case PickupType::Freeze:
                    color = glm::vec3(0.25f, 0.75f, 0.95f);
                    scale = 0.3f;
                    break;
                case PickupType::Nuke:
                    color = glm::vec3(0.95f, 0.45f, 0.10f);
                    scale = 0.35f;
                    break;
            }
            glm::mat4 pickup_model = glm::translate(
                glm::mat4(1.0f),
                pickup.position + glm::vec3(0.0f, 0.25f, 0.0f));
            pickup_model = glm::scale(pickup_model, glm::vec3(scale));
            glm::mat4 pickup_mvp = projection * view * pickup_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(pickup_mvp));
            glUniform3f(color_location, color.r, color.g, color.b);
            if (pickup.type == PickupType::Health) {
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);
                glm::mat4 cap = glm::translate(glm::mat4(1.0f),
                                               pickup.position + glm::vec3(0.0f, 0.42f, 0.0f));
                cap = glm::scale(cap, glm::vec3(scale * 0.5f));
                glm::mat4 cap_mvp = projection * view * cap;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cap_mvp));
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
            } else if (pickup.type == PickupType::Coin) {
                float sparkle = 0.7f + 0.3f * std::sin(run_time * 4.0f + pickup.position.x * 3.0f);
                glUniform3f(color_location, color.r * sparkle, color.g * sparkle, color.b * sparkle);
                glm::mat4 coin = glm::scale(pickup_model, glm::vec3(1.0f, 0.22f, 1.0f));
                glm::mat4 coin_mvp = projection * view * coin;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(coin_mvp));
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);

                glm::mat4 coin2 = glm::translate(glm::mat4(1.0f),
                                                 pickup.position + glm::vec3(0.08f, 0.29f, 0.05f));
                coin2 = glm::scale(coin2, glm::vec3(scale * 0.9f, scale * 0.18f, scale * 0.9f));
                glm::mat4 coin2_mvp = projection * view * coin2;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(coin2_mvp));
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);

                glm::mat4 coin3 = glm::translate(glm::mat4(1.0f),
                                                 pickup.position + glm::vec3(-0.06f, 0.32f, -0.04f));
                coin3 = glm::scale(coin3, glm::vec3(scale * 0.7f, scale * 0.16f, scale * 0.7f));
                glm::mat4 coin3_mvp = projection * view * coin3;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(coin3_mvp));
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);
            } else if (pickup.type == PickupType::Freeze) {
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
            } else if (pickup.type == PickupType::Nuke) {
                glBindVertexArray(cone_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                glBindVertexArray(0);
            }
        }
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        if (!explosions.empty()) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            for (const Explosion& explosion : explosions) {
                float t = explosion.timer / glm::max(0.001f, explosion.duration);
                glm::mat4 ring_model = glm::translate(
                    glm::mat4(1.0f),
                    explosion.position + glm::vec3(0.0f, 0.05f, 0.0f));
                ring_model = glm::scale(ring_model, glm::vec3(explosion.radius, 1.0f, explosion.radius));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, 0.95f, 0.25f, 0.20f);
                glUniform1f(ring_phase_location, 1.0f);
                glUniform1f(ring_alpha_location, 1.0f - t);
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        for (const Projectile& projectile : projectiles) {
            glm::mat4 proj_model = glm::translate(
                glm::mat4(1.0f),
                projectile.position);
            proj_model = glm::scale(proj_model, glm::vec3(0.2f));
            glm::mat4 proj_mvp = projection * view * proj_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(proj_mvp));
            glUniform3f(color_location, 0.95f, 0.55f, 0.10f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 ui_projection = glm::ortho(
            0.0f,
            static_cast<float>(window_width),
            0.0f,
            static_cast<float>(window_height),
            -1.0f,
            1.0f);

        float font_height = ui_font ? static_cast<float>(TTF_FontHeight(ui_font)) : 16.0f;
        if (state == GameState::Running || state == GameState::Paused || state == GameState::LevelUp ||
            state == GameState::GameOver) {
            float margin = 12.0f;
            float bar_width = 220.0f;
            float bar_height = 14.0f;
            float health_ratio = player_max_health > 0 ?
                static_cast<float>(player_health) / static_cast<float>(player_max_health) : 0.0f;
            draw_ui_quad(margin, window_height - margin - bar_height, bar_width, bar_height,
                         glm::vec3(0.18f, 0.18f, 0.22f), ui_projection);
            draw_ui_quad(margin, window_height - margin - bar_height, bar_width * health_ratio, bar_height,
                         glm::vec3(0.88f, 0.18f, 0.22f), ui_projection);

            int xp_needed = 5 + (player_level - 1) * 2;
            float xp_ratio = xp_needed > 0 ? static_cast<float>(player_xp) / static_cast<float>(xp_needed) : 0.0f;
            draw_ui_quad(margin, window_height - margin - bar_height * 2.2f, bar_width, bar_height,
                         glm::vec3(0.18f, 0.18f, 0.22f), ui_projection);
            draw_ui_quad(margin, window_height - margin - bar_height * 2.2f, bar_width * xp_ratio, bar_height,
                         glm::vec3(0.20f, 0.65f, 0.95f), ui_projection);

            float timer_width = 180.0f;
            float timer_ratio = win_time > 0.0f ? glm::min(run_time / win_time, 1.0f) : 0.0f;
            draw_ui_quad(window_width - margin - timer_width, window_height - margin - bar_height,
                         timer_width, bar_height, glm::vec3(0.18f, 0.18f, 0.22f), ui_projection);
            draw_ui_quad(window_width - margin - timer_width, window_height - margin - bar_height,
                         timer_width * timer_ratio, bar_height, glm::vec3(0.85f, 0.72f, 0.25f), ui_projection);

            float icon_size = 10.0f;
            float icon_x = window_width - margin - icon_size;
            float icon_y = window_height - margin - bar_height * 3.0f;
            for (int i = 0; i < garlic_level; ++i) {
                draw_ui_quad(icon_x - i * (icon_size + 4.0f), icon_y, icon_size, icon_size,
                             glm::vec3(0.35f, 0.85f, 0.45f), ui_projection);
            }
            if (fireball_level > 0) {
                for (int i = 0; i < fireball_level; ++i) {
                    draw_ui_quad(icon_x - i * (icon_size + 4.0f),
                                 icon_y - icon_size - 6.0f,
                                 icon_size, icon_size,
                                 glm::vec3(0.95f, 0.55f, 0.10f), ui_projection);
                }
            }
            if (bomb_level > 0) {
                for (int i = 0; i < bomb_level; ++i) {
                    draw_ui_quad(icon_x - i * (icon_size + 4.0f),
                                 icon_y - (icon_size + 6.0f) * 2.0f,
                                 icon_size, icon_size,
                                 glm::vec3(0.85f, 0.25f, 0.20f), ui_projection);
                }
            }

            if (freeze_timer > 0.0f) {
                float freeze_ratio = glm::clamp(freeze_timer / 4.0f, 0.0f, 1.0f);
                float freeze_y = window_height - margin - bar_height * 3.6f;
                draw_ui_quad(margin, freeze_y, bar_width, bar_height * 0.6f,
                             glm::vec3(0.08f, 0.12f, 0.18f), ui_projection);
                draw_ui_quad(margin, freeze_y, bar_width * freeze_ratio, bar_height * 0.6f,
                             glm::vec3(0.20f, 0.65f, 0.95f), ui_projection);
            }
        }

        if (nuke_flash_timer > 0.0f) {
            float alpha = glm::clamp(nuke_flash_timer / 0.45f, 0.0f, 1.0f) * 0.35f;
            glUniform1f(alpha_location, alpha);
            draw_ui_quad(0.0f, 0.0f, static_cast<float>(window_width),
                         static_cast<float>(window_height), glm::vec3(0.85f, 0.20f, 0.15f), ui_projection);
            glUniform1f(alpha_location, 1.0f);
        }

        if (state == GameState::MainMenu) {
            float panel_w = 360.0f;
            float panel_h = 220.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.10f, 0.10f, 0.14f), ui_projection);
            for (int i = 0; i < 3; ++i) {
                float y = panel_y + panel_h - 60.0f - i * 48.0f;
                glm::vec3 color = (i == main_menu_index) ? glm::vec3(0.85f, 0.72f, 0.25f)
                                                         : glm::vec3(0.22f, 0.22f, 0.28f);
                draw_ui_quad(panel_x + 40.0f, y, panel_w - 80.0f, 28.0f, color, ui_projection);
            }
        } else if (state == GameState::PowerUps) {
            float panel_w = 420.0f;
            float panel_h = 300.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.10f, 0.10f, 0.14f), ui_projection);
            for (int i = 0; i < 5; ++i) {
                float y = panel_y + panel_h - 50.0f - i * 44.0f;
                glm::vec3 color = (i == powerup_menu_index) ? glm::vec3(0.20f, 0.65f, 0.95f)
                                                            : glm::vec3(0.22f, 0.22f, 0.28f);
                draw_ui_quad(panel_x + 40.0f, y, panel_w - 80.0f, 26.0f, color, ui_projection);
            }
        } else if (state == GameState::Paused) {
            float panel_w = 320.0f;
            float panel_h = 140.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.12f, 0.12f, 0.18f), ui_projection);
        } else if (state == GameState::LevelUp) {
            float panel_w = 420.0f;
            float panel_h = 200.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.12f, 0.12f, 0.18f), ui_projection);
            for (int i = 0; i < static_cast<int>(current_choices.size()); ++i) {
                float y = panel_y + panel_h - 60.0f - i * 44.0f;
                draw_ui_quad(panel_x + 40.0f, y, panel_w - 80.0f, 26.0f,
                             glm::vec3(0.85f, 0.45f, 0.18f), ui_projection);
            }
        } else if (state == GameState::GameOver) {
            float panel_w = 360.0f;
            float panel_h = 160.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            glm::vec3 color = victory ? glm::vec3(0.18f, 0.55f, 0.30f)
                                      : glm::vec3(0.55f, 0.18f, 0.20f);
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, color, ui_projection);
        }

        SDL_Color ui_white{235, 235, 235, 255};
        SDL_Color ui_dim{170, 170, 170, 255};
        SDL_Color ui_highlight{240, 200, 80, 255};
        SDL_Color ui_highlight_text{30, 26, 20, 255};

        if (state == GameState::MainMenu) {
            float panel_w = 360.0f;
            float panel_h = 220.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_text_centered(panel_x, panel_y + panel_h + 16.0f, panel_w, 32.0f, "Vampire Diary",
                               ui_white, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 60.0f, panel_w - 80.0f, 28.0f,
                               "Start Run", main_menu_index == 0 ? ui_highlight_text : ui_dim,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 108.0f, panel_w - 80.0f, 28.0f,
                               "Power Ups", main_menu_index == 1 ? ui_highlight_text : ui_dim,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 156.0f, panel_w - 80.0f, 28.0f,
                               "Quit", main_menu_index == 2 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 8.0f, panel_w, 20.0f,
                               std::string("Start Weapon: ") +
                                   (starter_weapon_index == 0 ? "Garlic" :
                                        (starter_weapon_index == 1 ? "Fireball" : "Bomb")),
                               ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 28.0f, panel_w, 18.0f,
                               "Use Left/Right to choose", ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 32.0f, panel_w, 24.0f,
                               "Coins: " + std::to_string(coins), ui_white, ui_projection);
        } else if (state == GameState::PowerUps) {
            int cost_damage = 10 + 5 * meta_damage_level;
            int cost_speed = 8 + 4 * meta_speed_level;
            int cost_health = 12 + 6 * meta_max_health_level;
            int cost_cooldown = 12 + 6 * meta_attack_cooldown_level;
            float panel_w = 420.0f;
            float panel_h = 300.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_text_centered(panel_x, panel_y + panel_h + 16.0f, panel_w, 28.0f,
                               "Power Ups (Coins " + std::to_string(coins) + ")", ui_white,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 58.0f, panel_w - 80.0f, 26.0f,
                               "Damage +" + std::to_string(meta_damage_level) + " (Cost " +
                                   std::to_string(cost_damage) + ")",
                               powerup_menu_index == 0 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 102.0f, panel_w - 80.0f, 26.0f,
                               "Move Speed +" + std::to_string(meta_speed_level) + " (Cost " +
                                   std::to_string(cost_speed) + ")",
                               powerup_menu_index == 1 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 146.0f, panel_w - 80.0f, 26.0f,
                               "Max Health +" + std::to_string(meta_max_health_level) + " (Cost " +
                                   std::to_string(cost_health) + ")",
                               powerup_menu_index == 2 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 190.0f, panel_w - 80.0f, 26.0f,
                               "Cooldown +" + std::to_string(meta_attack_cooldown_level) + " (Cost " +
                                   std::to_string(cost_cooldown) + ")",
                               powerup_menu_index == 3 ? ui_highlight_text : ui_dim, ui_projection);
            if (!skin_unlocked) {
                draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 234.0f, panel_w - 80.0f, 26.0f,
                                   "Unlock Skin (Cost 60)",
                                   powerup_menu_index == 4 ? ui_highlight_text : ui_dim, ui_projection);
            } else {
                draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 234.0f, panel_w - 80.0f, 26.0f,
                                   std::string("Toggle Skin (") + (skin_selected ? "On" : "Off") + ")",
                                   powerup_menu_index == 4 ? ui_highlight_text : ui_dim, ui_projection);
            }
            draw_text_centered(panel_x, panel_y - 32.0f, panel_w, 24.0f,
                               "ESC to return", ui_dim, ui_projection);
        } else if (state == GameState::LevelUp && !current_choices.empty()) {
            float panel_w = 420.0f;
            float panel_h = 200.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_text_centered(panel_x, panel_y + panel_h + 16.0f, panel_w, 24.0f,
                               "Level Up! Choose:", ui_white, ui_projection);
            for (size_t i = 0; i < current_choices.size(); ++i) {
                draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 58.0f - static_cast<float>(i) * 44.0f,
                                   panel_w - 80.0f, 26.0f,
                                   std::to_string(static_cast<int>(i + 1)) + ") " +
                                       current_choices[i].name,
                                   ui_highlight_text, ui_projection);
            }
        } else if (state == GameState::Paused) {
            float panel_w = 320.0f;
            float panel_h = 140.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_text_centered(panel_x, panel_y + panel_h - 70.0f, panel_w, 24.0f,
                               "Paused", ui_white, ui_projection);
            draw_text_centered(panel_x, panel_y + panel_h - 102.0f, panel_w, 20.0f,
                               "ESC: Resume", ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y + panel_h - 130.0f, panel_w, 20.0f,
                               "Q: Quit to Menu", ui_dim, ui_projection);
        } else if (state == GameState::GameOver) {
            float panel_w = 360.0f;
            float panel_h = 160.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_text_centered(panel_x, panel_y + panel_h - 76.0f, panel_w, 24.0f,
                               victory ? "Victory!" : "Game Over", ui_white, ui_projection);
            draw_text_centered(panel_x, panel_y + panel_h - 108.0f, panel_w, 20.0f,
                               "Time " + std::to_string(static_cast<int>(run_time)) + "s  Kills " +
                                   std::to_string(enemies_killed),
                               ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y + panel_h - 134.0f, panel_w, 20.0f,
                               "Coins +" + std::to_string(coins_earned),
                               ui_dim, ui_projection);
        } else {
            int xp_needed = 5 + (player_level - 1) * 2;
            float margin = 12.0f;
            float bar_width = 220.0f;
            float bar_height = 14.0f;
            float health_bar_y = window_height - margin - bar_height;
            float xp_bar_y = window_height - margin - bar_height * 2.2f;
            float health_text_y = health_bar_y + (bar_height - font_height) * 0.5f;
            float xp_text_y = xp_bar_y + (bar_height - font_height) * 0.5f;
            float time_text_y = window_height - margin - bar_height * 3.6f - font_height;
            draw_text(margin + 6.0f, health_text_y,
                      "HP " + std::to_string(player_health) + "/" +
                          std::to_string(player_max_health),
                      ui_white, ui_projection);
            draw_text(margin + 6.0f, xp_text_y,
                      "LV " + std::to_string(player_level) + "  XP " +
                          std::to_string(player_xp) + "/" + std::to_string(xp_needed),
                      ui_dim, ui_projection);
            draw_text(margin + 6.0f, time_text_y,
                      "Time " + std::to_string(static_cast<int>(run_time)) + "s",
                      ui_dim, ui_projection);
            if (freeze_timer > 0.0f) {
                draw_text(margin + 6.0f, time_text_y + font_height + 4.0f,
                          "Freeze " + std::to_string(static_cast<int>(std::ceil(freeze_timer))) + "s",
                          ui_highlight, ui_projection);
            }
        }

        if (state == GameState::Running) {
            for (const Pickup& pickup : pickups) {
                glm::vec3 delta = pickup.position - player_position;
                delta.y = 0.0f;
                if (glm::length(delta) > 6.0f) {
                    continue;
                }
                glm::vec4 clip = projection * view *
                                 glm::vec4(pickup.position + glm::vec3(0.0f, 0.7f, 0.0f), 1.0f);
                if (clip.w <= 0.0f) {
                    continue;
                }
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                float sx = (ndc.x * 0.5f + 0.5f) * window_width;
                float sy = (ndc.y * 0.5f + 0.5f) * window_height;
                std::string label;
                switch (pickup.type) {
                    case PickupType::Coin:
                        label = "Coin";
                        break;
                    case PickupType::Health:
                        label = "Potion";
                        break;
                    case PickupType::Freeze:
                        label = "Freeze";
                        break;
                    case PickupType::Nuke:
                        label = "Nuke";
                        break;
                }
                draw_text_with_font(ui_font_small, sx - 14.0f, sy + 6.0f, label,
                                    ui_white, ui_projection);
            }
        }

        std::string title;
        if (state == GameState::MainMenu) {
            title = "MAIN MENU | Coins " + std::to_string(coins) +
                    " | Start Run | Power Ups | Quit (Enter)";
        } else if (state == GameState::PowerUps) {
            int cost_damage = 10 + 5 * meta_damage_level;
            int cost_speed = 8 + 4 * meta_speed_level;
            int cost_health = 12 + 6 * meta_max_health_level;
            int cost_cooldown = 12 + 6 * meta_attack_cooldown_level;
            title = "POWER UPS | Coins " + std::to_string(coins) +
                    " | [1] Damage +" + std::to_string(meta_damage_level) +
                    " (" + std::to_string(cost_damage) + ")" +
                    " | [2] Speed +" + std::to_string(meta_speed_level) +
                    " (" + std::to_string(cost_speed) + ")" +
                    " | [3] Max HP +" + std::to_string(meta_max_health_level) +
                    " (" + std::to_string(cost_health) + ")" +
                    " | [4] Cooldown +" + std::to_string(meta_attack_cooldown_level) +
                    " (" + std::to_string(cost_cooldown) + ")";
            if (!skin_unlocked) {
                title += " | [5] Unlock Skin (60)";
            } else {
                title += " | [5] Toggle Skin";
            }
            title += " | ESC Back";
        } else if (state == GameState::LevelUp && !current_choices.empty()) {
            title = "LEVEL UP! 1) ";
            title += current_choices[0].name;
            if (current_choices.size() > 1) {
                title += "  2) ";
                title += current_choices[1].name;
            }
            if (current_choices.size() > 2) {
                title += "  3) ";
                title += current_choices[2].name;
            }
        } else if (state == GameState::Paused) {
            title = "PAUSED | ESC Resume | Q Quit to Menu";
        } else if (state == GameState::GameOver) {
            title = victory ? "VICTORY! " : "GAME OVER ";
            title += "Time " + std::to_string(static_cast<int>(run_time)) +
                     "s | Kills " + std::to_string(enemies_killed) +
                     " | Coins +" + std::to_string(coins_earned);
        } else {
            int xp_needed = 5 + (player_level - 1) * 2;
            title = "HP " + std::to_string(player_health) + "/" +
                    std::to_string(player_max_health) +
                    " | LV " + std::to_string(player_level) +
                    " XP " + std::to_string(player_xp) + "/" +
                    std::to_string(xp_needed) +
                    " | Coins +" + std::to_string(coins_earned) +
                    " | Enemies " + std::to_string(enemies.size()) +
                    " | ESC Pause";
        }
        SDL_SetWindowTitle(window, title.c_str());

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        SDL_GL_SwapWindow(window);
    }

    glDeleteVertexArrays(1, &ground_vao);
    glDeleteBuffers(1, &ground_vbo);
    glDeleteVertexArrays(1, &cube_vao);
    glDeleteBuffers(1, &cube_vbo);
    glDeleteVertexArrays(1, &ui_vao);
    glDeleteBuffers(1, &ui_vbo);
    glDeleteVertexArrays(1, &text_vao);
    glDeleteBuffers(1, &text_vbo);
    glDeleteVertexArrays(1, &ring_vao);
    glDeleteBuffers(1, &ring_vbo);
    glDeleteVertexArrays(1, &sphere_mesh.vao);
    glDeleteBuffers(1, &sphere_mesh.vbo);
    glDeleteVertexArrays(1, &cone_mesh.vao);
    glDeleteBuffers(1, &cone_mesh.vbo);
    glDeleteVertexArrays(1, &cylinder_mesh.vao);
    glDeleteBuffers(1, &cylinder_mesh.vbo);
    glDeleteProgram(program);
    glDeleteProgram(text_program);
    glDeleteProgram(ring_program);

    if (ui_font) {
        TTF_CloseFont(ui_font);
    }
    if (ui_font_small) {
        TTF_CloseFont(ui_font_small);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return EXIT_SUCCESS;
}
