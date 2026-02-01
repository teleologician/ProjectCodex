#include <algorithm>
#include <array>
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
#include <noise/noise.h>

namespace {
struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
};

struct MeshBuilder {
    std::vector<glm::vec3> positions;

    void add_triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        positions.push_back(a);
        positions.push_back(b);
        positions.push_back(c);
    }

    void add_quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
        add_triangle(a, b, c);
        add_triangle(a, c, d);
    }
};

struct LitMeshBuilder {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;

    void add_triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        positions.push_back(a);
        positions.push_back(b);
        positions.push_back(c);
        normals.push_back(n);
        normals.push_back(n);
        normals.push_back(n);
    }

    void add_quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
        add_triangle(a, b, c);
        add_triangle(a, c, d);
    }
};

std::vector<float> FlattenPositions(const std::vector<glm::vec3>& positions) {
    std::vector<float> data;
    data.reserve(positions.size() * 3);
    for (const glm::vec3& p : positions) {
        data.push_back(p.x);
        data.push_back(p.y);
        data.push_back(p.z);
    }
    return data;
}

glm::vec3 TransformPoint(const glm::mat4& m, const glm::vec3& p) {
    glm::vec4 v = m * glm::vec4(p, 1.0f);
    return glm::vec3(v);
}

void AddBox(MeshBuilder& builder, const glm::vec3& half, const glm::mat4& transform) {
    glm::vec3 corners[8] = {
        {-half.x, -half.y, -half.z},
        { half.x, -half.y, -half.z},
        { half.x,  half.y, -half.z},
        {-half.x,  half.y, -half.z},
        {-half.x, -half.y,  half.z},
        { half.x, -half.y,  half.z},
        { half.x,  half.y,  half.z},
        {-half.x,  half.y,  half.z}
    };
    for (glm::vec3& c : corners) {
        c = TransformPoint(transform, c);
    }
    builder.add_quad(corners[0], corners[1], corners[2], corners[3]);
    builder.add_quad(corners[5], corners[4], corners[7], corners[6]);
    builder.add_quad(corners[4], corners[0], corners[3], corners[7]);
    builder.add_quad(corners[1], corners[5], corners[6], corners[2]);
    builder.add_quad(corners[3], corners[2], corners[6], corners[7]);
    builder.add_quad(corners[4], corners[5], corners[1], corners[0]);
}

void AddLitBox(LitMeshBuilder& builder, const glm::vec3& half, const glm::mat4& transform) {
    glm::vec3 corners[8] = {
        {-half.x, -half.y, -half.z},
        { half.x, -half.y, -half.z},
        { half.x,  half.y, -half.z},
        {-half.x,  half.y, -half.z},
        {-half.x, -half.y,  half.z},
        { half.x, -half.y,  half.z},
        { half.x,  half.y,  half.z},
        {-half.x,  half.y,  half.z}
    };
    for (glm::vec3& c : corners) {
        c = TransformPoint(transform, c);
    }
    builder.add_quad(corners[0], corners[1], corners[2], corners[3]);
    builder.add_quad(corners[5], corners[4], corners[7], corners[6]);
    builder.add_quad(corners[4], corners[0], corners[3], corners[7]);
    builder.add_quad(corners[1], corners[5], corners[6], corners[2]);
    builder.add_quad(corners[3], corners[2], corners[6], corners[7]);
    builder.add_quad(corners[4], corners[5], corners[1], corners[0]);
}

void AddLitRoof(LitMeshBuilder& builder, float half_x, float half_z, float height, const glm::mat4& transform) {
    glm::vec3 a(-half_x, 0.0f, -half_z);
    glm::vec3 b(half_x, 0.0f, -half_z);
    glm::vec3 c(half_x, 0.0f, half_z);
    glm::vec3 d(-half_x, 0.0f, half_z);
    glm::vec3 ridge_front(0.0f, height, -half_z);
    glm::vec3 ridge_back(0.0f, height, half_z);

    a = TransformPoint(transform, a);
    b = TransformPoint(transform, b);
    c = TransformPoint(transform, c);
    d = TransformPoint(transform, d);
    ridge_front = TransformPoint(transform, ridge_front);
    ridge_back = TransformPoint(transform, ridge_back);

    builder.add_quad(a, ridge_front, ridge_back, d);
    builder.add_quad(ridge_front, b, c, ridge_back);
    builder.add_triangle(a, b, ridge_front);
    builder.add_triangle(d, ridge_back, c);
}

void AddCylinder(MeshBuilder& builder, float radius, float height, int segments, const glm::mat4& transform) {
    float half = height * 0.5f;
    for (int i = 0; i < segments; ++i) {
        float a0 = glm::two_pi<float>() * (static_cast<float>(i) / segments);
        float a1 = glm::two_pi<float>() * (static_cast<float>(i + 1) / segments);
        glm::vec3 p0(radius * std::cos(a0), -half, radius * std::sin(a0));
        glm::vec3 p1(radius * std::cos(a1), -half, radius * std::sin(a1));
        glm::vec3 p2(radius * std::cos(a1), half, radius * std::sin(a1));
        glm::vec3 p3(radius * std::cos(a0), half, radius * std::sin(a0));
        builder.add_quad(TransformPoint(transform, p0),
                         TransformPoint(transform, p1),
                         TransformPoint(transform, p2),
                         TransformPoint(transform, p3));

        glm::vec3 top(0.0f, half, 0.0f);
        glm::vec3 bot(0.0f, -half, 0.0f);
        builder.add_triangle(TransformPoint(transform, top),
                             TransformPoint(transform, p3),
                             TransformPoint(transform, p2));
        builder.add_triangle(TransformPoint(transform, bot),
                             TransformPoint(transform, p1),
                             TransformPoint(transform, p0));
    }
}

void AddCone(MeshBuilder& builder, float radius, float height, int segments, const glm::mat4& transform) {
    glm::vec3 tip(0.0f, height * 0.5f, 0.0f);
    glm::vec3 base(0.0f, -height * 0.5f, 0.0f);
    for (int i = 0; i < segments; ++i) {
        float a0 = glm::two_pi<float>() * (static_cast<float>(i) / segments);
        float a1 = glm::two_pi<float>() * (static_cast<float>(i + 1) / segments);
        glm::vec3 p0(radius * std::cos(a0), -height * 0.5f, radius * std::sin(a0));
        glm::vec3 p1(radius * std::cos(a1), -height * 0.5f, radius * std::sin(a1));
        builder.add_triangle(TransformPoint(transform, tip),
                             TransformPoint(transform, p0),
                             TransformPoint(transform, p1));
        builder.add_triangle(TransformPoint(transform, base),
                             TransformPoint(transform, p1),
                             TransformPoint(transform, p0));
    }
}

void AddSphere(MeshBuilder& builder, float radius, int rings, int segments, const glm::mat4& transform,
               float theta_min = 0.0f, float theta_max = glm::pi<float>()) {
    for (int y = 0; y < rings; ++y) {
        float v0 = static_cast<float>(y) / rings;
        float v1 = static_cast<float>(y + 1) / rings;
        float t0 = glm::mix(theta_min, theta_max, v0);
        float t1 = glm::mix(theta_min, theta_max, v1);
        for (int x = 0; x < segments; ++x) {
            float u0 = static_cast<float>(x) / segments;
            float u1 = static_cast<float>(x + 1) / segments;
            float p0 = u0 * glm::two_pi<float>();
            float p1 = u1 * glm::two_pi<float>();
            glm::vec3 a(radius * std::sin(t0) * std::cos(p0), radius * std::cos(t0), radius * std::sin(t0) * std::sin(p0));
            glm::vec3 b(radius * std::sin(t0) * std::cos(p1), radius * std::cos(t0), radius * std::sin(t0) * std::sin(p1));
            glm::vec3 c(radius * std::sin(t1) * std::cos(p1), radius * std::cos(t1), radius * std::sin(t1) * std::sin(p1));
            glm::vec3 d(radius * std::sin(t1) * std::cos(p0), radius * std::cos(t1), radius * std::sin(t1) * std::sin(p0));
            builder.add_quad(TransformPoint(transform, a),
                             TransformPoint(transform, b),
                             TransformPoint(transform, c),
                             TransformPoint(transform, d));
        }
    }
}

void AddCapsule(MeshBuilder& builder, float radius, float height, int rings, int segments, const glm::mat4& transform) {
    float cyl_height = glm::max(0.0f, height - radius * 2.0f);
    glm::mat4 cyl = transform;
    AddCylinder(builder, radius, cyl_height, segments, cyl);
    glm::mat4 top = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, cyl_height * 0.5f, 0.0f));
    AddSphere(builder, radius, rings, segments, top, 0.0f, glm::half_pi<float>());
    glm::mat4 bottom = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -cyl_height * 0.5f, 0.0f));
    AddSphere(builder, radius, rings, segments, bottom, glm::half_pi<float>(), glm::pi<float>());
}

struct RampTile {
    glm::vec2 center;
    glm::vec2 half;
    int dir; // 1:+X, 2:-X, 3:+Z, 4:-Z
    float base_height;
    float height;
    int plateau_index = -1;
    bool active = true;
};

struct Plateau {
    glm::vec2 center;
    glm::vec2 half;
    float height;
    bool active = true;
};

static std::vector<Plateau> g_plateaus;
static std::vector<RampTile> g_ramps;
static std::vector<glm::vec3> g_rock_positions;
static std::vector<glm::vec3> g_tree_positions;
static std::vector<float> g_plateau_fall_offsets;
static constexpr float g_ground_extent = 95.0f;
static noise::module::Perlin g_base_noise;
static noise::module::Perlin g_detail_noise;
static noise::module::Perlin g_rock_noise;

enum class StarterWeaponId {
    Garlic,
    HolyBomb,
    Cross,
    Stake,
    Crossbow,
    Caltrops
};

struct StarterWeaponOption {
    StarterWeaponId id;
    const char* name;
};

std::vector<StarterWeaponOption> BuildStarterWeapons(bool unlock_cross, bool unlock_stick,
                                                     bool unlock_crossbow, bool unlock_poison) {
    std::vector<StarterWeaponOption> options;
    options.push_back({StarterWeaponId::Garlic, "Garlic"});
    options.push_back({StarterWeaponId::HolyBomb, "Holy Bomb"});
    if (unlock_cross) {
        options.push_back({StarterWeaponId::Cross, "Cross"});
    }
    if (unlock_stick) {
        options.push_back({StarterWeaponId::Stake, "Stake"});
    }
    if (unlock_crossbow) {
        options.push_back({StarterWeaponId::Crossbow, "Crossbow"});
    }
    if (unlock_poison) {
        options.push_back({StarterWeaponId::Caltrops, "Caltrops"});
    }
    return options;
}

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
    int magnet_level = 0;
    int unlock_cross = 0;
    int unlock_stick = 0;
    int unlock_crossbow = 0;
    int unlock_holywater = 0;
    int unlock_poison = 0;
    int unlock_bat = 0;
    int skin_unlocked = 0;
    int hair_style = 0;
    int hair_color = 0;
    int hair_unlock_mask = 1;
};

bool LoadSave(const char* path, SaveData* out) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::vector<int> values;
    int value = 0;
    while (file >> value) {
        values.push_back(value);
    }
    if (values.size() < 5) {
        return false;
    }
    SaveData data;
    data.coins = values[0];
    data.damage_level = values[1];
    data.speed_level = values[2];
    data.max_health_level = values[3];
    data.attack_cooldown_level = values[4];
    if (values.size() >= 16) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.unlock_holywater = values[9];
        data.unlock_poison = values[10];
        data.unlock_bat = values[11];
        data.skin_unlocked = values[12];
        data.hair_style = values[13];
        data.hair_color = values[14];
        data.hair_unlock_mask = values[15];
    } else if (values.size() == 15) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.unlock_holywater = values[9];
        data.unlock_poison = values[10];
        data.unlock_bat = values[11];
        data.skin_unlocked = values[12];
        data.hair_style = values[13];
        data.hair_color = values[14];
        data.hair_unlock_mask = 1;
    } else if (values.size() == 14) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.unlock_holywater = values[9];
        data.unlock_poison = values[10];
        data.unlock_bat = values[11];
        data.skin_unlocked = values[12];
        data.hair_style = values[13];
        data.hair_color = 0;
        data.hair_unlock_mask = 1;
    } else if (values.size() == 12) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.unlock_holywater = values[9];
        data.unlock_poison = values[10];
        data.skin_unlocked = values[11];
    } else if (values.size() == 11) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.unlock_holywater = values[9];
        data.skin_unlocked = values[10];
    } else if (values.size() == 10) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.unlock_crossbow = values[8];
        data.skin_unlocked = values[9];
    } else if (values.size() == 9) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.unlock_stick = values[7];
        data.skin_unlocked = values[8];
    } else if (values.size() == 8) {
        data.magnet_level = values[5];
        data.unlock_cross = values[6];
        data.skin_unlocked = values[7];
    } else if (values.size() == 7) {
        data.magnet_level = values[5];
        data.skin_unlocked = values[6];
    } else if (values.size() == 6) {
        data.magnet_level = 0;
        data.skin_unlocked = values[5];
    } else {
        data.magnet_level = 0;
        data.skin_unlocked = 0;
    }
    *out = data;
    return true;
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
         << data.magnet_level << ' '
         << data.unlock_cross << ' '
         << data.unlock_stick << ' '
         << data.unlock_crossbow << ' '
         << data.unlock_holywater << ' '
         << data.unlock_poison << ' '
         << data.unlock_bat << ' '
         << data.skin_unlocked << ' '
         << data.hair_style << ' '
         << data.hair_color << ' '
         << data.hair_unlock_mask;
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

constexpr float kTileSize = 5.0f;

bool PointInRect(const glm::vec2& p, const glm::vec2& center, const glm::vec2& half) {
    glm::vec2 min = center - half;
    glm::vec2 max = center + half;
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

bool IsOnRamp(const glm::vec2& p) {
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        if (PointInRect(p, ramp.center, ramp.half)) {
            return true;
        }
    }
    return false;
}

bool IsOnPlateau(const glm::vec2& p) {
    for (const Plateau& plateau : g_plateaus) {
        if (!plateau.active) {
            continue;
        }
        if (PointInRect(p, plateau.center, plateau.half)) {
            return true;
        }
    }
    return false;
}

const RampTile* FindRamp(const glm::vec2& p) {
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        if (PointInRect(p, ramp.center, ramp.half)) {
            return &ramp;
        }
    }
    return nullptr;
}

bool IsNearRampBase(const RampTile& ramp, const glm::vec2& p, float margin) {
    glm::vec2 min = ramp.center - ramp.half;
    glm::vec2 max = ramp.center + ramp.half;
    if (ramp.dir == 1) {
        return p.x >= min.x - margin && p.x <= min.x + margin &&
               p.y >= min.y && p.y <= max.y;
    }
    if (ramp.dir == 2) {
        return p.x >= max.x - margin && p.x <= max.x + margin &&
               p.y >= min.y && p.y <= max.y;
    }
    if (ramp.dir == 3) {
        return p.y >= min.y - margin && p.y <= min.y + margin &&
               p.x >= min.x && p.x <= max.x;
    }
    return p.y >= max.y - margin && p.y <= max.y + margin &&
           p.x >= min.x && p.x <= max.x;
}

bool RampAllowsTransition(const glm::vec2& from, const glm::vec2& to, float margin) {
    const RampTile* ramp_from = FindRamp(from);
    const RampTile* ramp_to = FindRamp(to);
    if (ramp_from && ramp_to) {
        return ramp_from == ramp_to;
    }
    if (ramp_to && !ramp_from) {
        return IsNearRampBase(*ramp_to, from, margin);
    }
    if (ramp_from && !ramp_to) {
        return true;
    }
    return false;
}

bool IsNearRampEdge(const glm::vec2& p, float margin) {
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        glm::vec2 min = ramp.center - ramp.half;
        glm::vec2 max = ramp.center + ramp.half;
        float dx = 0.0f;
        if (p.x < min.x) {
            dx = min.x - p.x;
        } else if (p.x > max.x) {
            dx = p.x - max.x;
        }
        float dz = 0.0f;
        if (p.y < min.y) {
            dz = min.y - p.y;
        } else if (p.y > max.y) {
            dz = p.y - max.y;
        }
        if (dx <= margin && dz <= margin) {
            return true;
        }
    }
    return false;
}

bool IsNearRampArea(const glm::vec2& p, float margin) {
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        glm::vec2 min = ramp.center - ramp.half - glm::vec2(margin, margin);
        glm::vec2 max = ramp.center + ramp.half + glm::vec2(margin, margin);
        if (p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y) {
            return true;
        }
    }
    return false;
}

struct RampEndpoints {
    glm::vec2 base;
    glm::vec2 top;
};

RampEndpoints GetRampEndpoints(const RampTile& ramp) {
    glm::vec2 min = ramp.center - ramp.half;
    glm::vec2 max = ramp.center + ramp.half;
    RampEndpoints out{};
    if (ramp.dir == 1) { // +X rise
        out.base = glm::vec2(min.x, ramp.center.y);
        out.top = glm::vec2(max.x, ramp.center.y);
    } else if (ramp.dir == 2) { // -X rise
        out.base = glm::vec2(max.x, ramp.center.y);
        out.top = glm::vec2(min.x, ramp.center.y);
    } else if (ramp.dir == 3) { // +Z rise
        out.base = glm::vec2(ramp.center.x, min.y);
        out.top = glm::vec2(ramp.center.x, max.y);
    } else { // -Z rise
        out.base = glm::vec2(ramp.center.x, max.y);
        out.top = glm::vec2(ramp.center.x, min.y);
    }
    return out;
}

float RampHeightAt(const RampTile& ramp, const glm::vec2& p) {
    glm::vec2 min = ramp.center - ramp.half;
    glm::vec2 max = ramp.center + ramp.half;
    float t = 0.0f;
    if (ramp.dir == 1) {
        t = (p.x - min.x) / (max.x - min.x);
    } else if (ramp.dir == 2) {
        t = (max.x - p.x) / (max.x - min.x);
    } else if (ramp.dir == 3) {
        t = (p.y - min.y) / (max.y - min.y);
    } else if (ramp.dir == 4) {
        t = (max.y - p.y) / (max.y - min.y);
    }
    t = glm::clamp(t, 0.0f, 1.0f);
    float height = ramp.base_height + t * ramp.height;
    if (ramp.plateau_index >= 0 &&
        ramp.plateau_index < static_cast<int>(g_plateau_fall_offsets.size())) {
        height -= g_plateau_fall_offsets[ramp.plateau_index];
    }
    return height;
}

float BaseNoiseHeight(float x, float z) {
    double n0 = g_base_noise.GetValue(x * 0.02, 0.0, z * 0.02);
    double n1 = g_detail_noise.GetValue(x * 0.06, 0.0, z * 0.06);
    float h = static_cast<float>(n0 * 1.6 + n1 * 0.4);
    h -= std::abs(static_cast<float>(n0)) * 0.3f;
    return h;
}

float TerrainHeight(float x, float z) {
    glm::vec2 p(x, z);
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        if (PointInRect(p, ramp.center, ramp.half)) {
            return RampHeightAt(ramp, p);
        }
    }
    for (int i = 0; i < static_cast<int>(g_plateaus.size()); ++i) {
        const Plateau& plateau = g_plateaus[i];
        if (!plateau.active) {
            continue;
        }
        if (PointInRect(p, plateau.center, plateau.half)) {
            float height = plateau.height;
            if (i < static_cast<int>(g_plateau_fall_offsets.size())) {
                height -= g_plateau_fall_offsets[i];
            }
            return height;
        }
    }
    return BaseNoiseHeight(x, z);
}

float PlateauHeightAt(const glm::vec2& p) {
    for (int i = 0; i < static_cast<int>(g_plateaus.size()); ++i) {
        const Plateau& plateau = g_plateaus[i];
        if (!plateau.active) {
            continue;
        }
        if (PointInRect(p, plateau.center, plateau.half)) {
            float height = plateau.height;
            if (i < static_cast<int>(g_plateau_fall_offsets.size())) {
                height -= g_plateau_fall_offsets[i];
            }
            return height;
        }
    }
    return 0.0f;
}

float TerrainHeightAt(float x, float z, float current_y) {
    glm::vec2 p(x, z);
    for (const RampTile& ramp : g_ramps) {
        if (!ramp.active) {
            continue;
        }
        if (PointInRect(p, ramp.center, ramp.half)) {
            return RampHeightAt(ramp, p);
        }
    }
    for (int i = 0; i < static_cast<int>(g_plateaus.size()); ++i) {
        const Plateau& plateau = g_plateaus[i];
        if (!plateau.active) {
            continue;
        }
        if (PointInRect(p, plateau.center, plateau.half)) {
            float height = plateau.height;
            if (i < static_cast<int>(g_plateau_fall_offsets.size())) {
                height -= g_plateau_fall_offsets[i];
            }
            float snap_threshold = height * 0.55f;
            if (current_y >= snap_threshold) {
                return height;
            }
            return BaseNoiseHeight(x, z);
        }
    }
    return BaseNoiseHeight(x, z);
}

bool RectsOverlap(const glm::vec2& a_min, const glm::vec2& a_max,
                  const glm::vec2& b_min, const glm::vec2& b_max,
                  float pad) {
    if (a_min.x >= b_max.x - pad || a_max.x <= b_min.x + pad) {
        return false;
    }
    if (a_min.y >= b_max.y - pad || a_max.y <= b_min.y + pad) {
        return false;
    }
    return true;
}

void GenerateTerrain(uint32_t seed) {
    g_plateaus.clear();
    g_ramps.clear();
    g_rock_positions.clear();
    g_tree_positions.clear();

    std::mt19937 rng(seed);
    g_base_noise.SetSeed(static_cast<int>(seed));
    g_base_noise.SetFrequency(0.015);
    g_detail_noise.SetSeed(static_cast<int>(seed) + 17);
    g_detail_noise.SetFrequency(0.045);
    g_rock_noise.SetSeed(static_cast<int>(seed) + 33);
    g_rock_noise.SetFrequency(1.2);
    std::vector<glm::vec2> cells;
    for (int gx = -2; gx <= 2; ++gx) {
        for (int gz = -2; gz <= 2; ++gz) {
            if (gx == 0 && gz == 0) {
                continue;
            }
            cells.push_back(glm::vec2(static_cast<float>(gx), static_cast<float>(gz)));
        }
    }
    std::shuffle(cells.begin(), cells.end(), rng);

    const float spacing = 34.0f;
    const int plateau_count = 8;
    for (int i = 0; i < plateau_count && i < static_cast<int>(cells.size()); ++i) {
        bool high = (rng() % 4) == 0;
        float height = high ? 10.0f : 6.0f;
        glm::vec2 half = high ? glm::vec2(11.0f, 11.0f) : glm::vec2(9.0f, 9.0f);
        glm::vec2 center = cells[i] * spacing;
        g_plateaus.push_back(Plateau{center, half, height, true});

        std::array<int, 4> dirs = {1, 2, 3, 4};
        std::shuffle(dirs.begin(), dirs.end(), rng);
        int ramp_count = high ? 3 : 2;
        float ramp_len = high ? 5.0f : 4.0f;
        float ramp_w = 4.0f;
        const float ramp_overlap = 0.35f;
        for (int r = 0; r < ramp_count; ++r) {
            int place_dir = dirs[r];
            int dir = place_dir;
            glm::vec2 ramp_half = (dir == 1 || dir == 2)
                ? glm::vec2(ramp_len, ramp_w)
                : glm::vec2(ramp_w, ramp_len);
            glm::vec2 ramp_center = center;
            if (place_dir == 1) { // place east of plateau
                ramp_center.x += half.x + ramp_half.x - ramp_overlap;
                dir = 2; // rise toward -X (toward plateau)
            } else if (place_dir == 2) { // place west
                ramp_center.x -= half.x + ramp_half.x - ramp_overlap;
                dir = 1; // rise toward +X
            } else if (place_dir == 3) { // place north (+Z)
                ramp_center.y += half.y + ramp_half.y - ramp_overlap;
                dir = 4; // rise toward -Z
            } else { // place south (-Z)
                ramp_center.y -= half.y + ramp_half.y - ramp_overlap;
                dir = 3; // rise toward +Z
            }

            glm::vec2 ramp_min = ramp_center - ramp_half;
            glm::vec2 ramp_max = ramp_center + ramp_half;
            bool overlap = false;
            for (const RampTile& other : g_ramps) {
                glm::vec2 other_min = other.center - other.half;
                glm::vec2 other_max = other.center + other.half;
                if (RectsOverlap(ramp_min, ramp_max, other_min, other_max, 0.2f)) {
                    overlap = true;
                    break;
                }
            }
            if (!overlap) {
                for (const Plateau& other : g_plateaus) {
                    if (other.center == center) {
                        continue;
                    }
                    glm::vec2 other_min = other.center - other.half;
                    glm::vec2 other_max = other.center + other.half;
                    if (RectsOverlap(ramp_min, ramp_max, other_min, other_max, 0.2f)) {
                        overlap = true;
                        break;
                    }
                }
            }
            if (overlap) {
                continue;
            }
            g_ramps.push_back(RampTile{ramp_center, ramp_half, dir, 0.0f, height, i, true});
        }
    }

    std::uniform_real_distribution<float> dist(-g_ground_extent, g_ground_extent);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const int rock_count = 300;
    const int tree_count = 200;
    for (int i = 0; i < rock_count; ++i) {
        glm::vec2 pos(dist(rng), dist(rng));
        bool blocked = false;
        for (const Plateau& plateau : g_plateaus) {
            if (PointInRect(pos, plateau.center, plateau.half + glm::vec2(2.0f))) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            for (const RampTile& ramp : g_ramps) {
                if (PointInRect(pos, ramp.center, ramp.half + glm::vec2(1.5f))) {
                    blocked = true;
                    break;
                }
            }
        }
        if (blocked) {
            continue;
        }
        float y = TerrainHeight(pos.x, pos.y);
        g_rock_positions.push_back(glm::vec3(pos.x, y, pos.y));
    }
    for (int i = 0; i < tree_count; ++i) {
        glm::vec2 pos(dist(rng), dist(rng));
        bool blocked = false;
        for (const Plateau& plateau : g_plateaus) {
            if (PointInRect(pos, plateau.center, plateau.half + glm::vec2(2.5f))) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            for (const RampTile& ramp : g_ramps) {
                if (PointInRect(pos, ramp.center, ramp.half + glm::vec2(2.0f))) {
                    blocked = true;
                    break;
                }
            }
        }
        if (blocked) {
            continue;
        }
        float y = TerrainHeight(pos.x, pos.y);
        g_tree_positions.push_back(glm::vec3(pos.x, y, pos.y));
    }
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

Mesh CreateLitMesh(const std::vector<glm::vec3>& positions,
                   const std::vector<glm::vec3>& normals) {
    Mesh mesh;
    if (positions.size() != normals.size()) {
        return mesh;
    }
    std::vector<float> data;
    data.reserve(positions.size() * 6);
    for (size_t i = 0; i < positions.size(); ++i) {
        data.push_back(positions[i].x);
        data.push_back(positions[i].y);
        data.push_back(positions[i].z);
        data.push_back(normals[i].x);
        data.push_back(normals[i].y);
        data.push_back(normals[i].z);
    }
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    mesh.count = static_cast<GLsizei>(positions.size());
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
        "layout (location = 2) in vec3 aNormal;\n"
        "out vec3 vColor;\n"
        "out vec3 vNormal;\n"
        "uniform mat4 uMVP;\n"
        "uniform int uUseVertexColor;\n"
        "uniform vec3 uColor;\n"
        "uniform int uOutlinePass;\n"
        "uniform float uOutlineSize;\n"
        "uniform int uUseNormals;\n"
        "void main() {\n"
        "    vColor = (uUseVertexColor == 1) ? aColor : uColor;\n"
        "    vNormal = (uUseNormals == 1) ? aNormal : normalize(aPos);\n"
        "    vec3 pos = aPos;\n"
        "    if (uOutlinePass == 1) {\n"
        "        pos += vNormal * uOutlineSize;\n"
        "    }\n"
        "    gl_Position = uMVP * vec4(pos, 1.0);\n"
        "}\n";

    const char* fragment_source =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "in vec3 vNormal;\n"
        "out vec4 FragColor;\n"
        "uniform float uAlpha;\n"
        "uniform vec3 uLightDir;\n"
        "uniform float uAmbient;\n"
        "uniform int uBands;\n"
        "uniform vec3 uRimColor;\n"
        "uniform float uRimPower;\n"
        "uniform float uRimStrength;\n"
        "uniform vec3 uOutlineColor;\n"
        "uniform int uOutlinePass;\n"
        "uniform float uAO;\n"
        "uniform vec3 uFogColor;\n"
        "uniform float uFogNear;\n"
        "uniform float uFogFar;\n"
        "void main() {\n"
        "    if (uOutlinePass == 1) {\n"
        "        FragColor = vec4(uOutlineColor, uAlpha);\n"
        "        return;\n"
        "    }\n"
        "    vec3 normal = normalize(vNormal);\n"
        "    vec3 lightDir = normalize(uLightDir);\n"
        "    float light = max(dot(normal, lightDir), 0.0);\n"
        "    if (uBands > 1) {\n"
        "        light = floor(light * float(uBands)) / float(uBands);\n"
        "    }\n"
        "    float shade = uAmbient + (1.0 - uAmbient) * light;\n"
        "    vec3 color = vColor * shade;\n"
        "    float ao = mix(1.0, 0.7, 1.0 - clamp(normal.y * 0.5 + 0.5, 0.0, 1.0));\n"
        "    color *= mix(1.0, ao, uAO);\n"
        "    float rim = pow(clamp(1.0 - abs(normal.y), 0.0, 1.0), uRimPower) * uRimStrength;\n"
        "    color += uRimColor * rim;\n"
        "    float spec = step(0.95, light) * 0.15;\n"
        "    color += vec3(spec);\n"
        "    float depth = gl_FragCoord.z;\n"
        "    float fog = smoothstep(uFogNear, uFogFar, depth);\n"
        "    color = mix(color, uFogColor, fog);\n"
        "    FragColor = vec4(color, uAlpha);\n"
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
    const float ground_extent = g_ground_extent;
    auto add_quad = [&](const glm::vec3& p00, const glm::vec3& p10,
                        const glm::vec3& p11, const glm::vec3& p01) {
        glm::vec3 tri1[3] = {p00, p11, p10};
        glm::vec3 tri2[3] = {p00, p01, p11};
        for (int k = 0; k < 3; ++k) {
            ground_vertices.push_back(tri1[k].x);
            ground_vertices.push_back(tri1[k].y);
            ground_vertices.push_back(tri1[k].z);
        }
        for (int k = 0; k < 3; ++k) {
            ground_vertices.push_back(tri2[k].x);
            ground_vertices.push_back(tri2[k].y);
            ground_vertices.push_back(tri2[k].z);
        }
    };

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
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    auto rebuild_ground = [&]() {
        ground_vertices.clear();
        const float step = 2.0f;
        for (float z = -ground_extent; z < ground_extent; z += step) {
            for (float x = -ground_extent; x < ground_extent; x += step) {
                glm::vec2 p(x + step * 0.5f, z + step * 0.5f);
                bool skip = false;
                for (const Plateau& plateau : g_plateaus) {
                    if (!plateau.active) {
                        continue;
                    }
                    if (PointInRect(p, plateau.center, plateau.half)) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    for (const RampTile& ramp : g_ramps) {
                        if (!ramp.active) {
                            continue;
                        }
                        if (PointInRect(p, ramp.center, ramp.half)) {
                            skip = true;
                            break;
                        }
                    }
                }
                if (skip) {
                    continue;
                }
                glm::vec3 p00(x, BaseNoiseHeight(x, z), z);
                glm::vec3 p10(x + step, BaseNoiseHeight(x + step, z), z);
                glm::vec3 p11(x + step, BaseNoiseHeight(x + step, z + step), z + step);
                glm::vec3 p01(x, BaseNoiseHeight(x, z + step), z + step);
                add_quad(p00, p10, p11, p01);
            }
        }

        auto plateau_fall_offset = [&](int plateau_index) {
            if (plateau_index >= 0 &&
                plateau_index < static_cast<int>(g_plateau_fall_offsets.size())) {
                return g_plateau_fall_offsets[plateau_index];
            }
            return 0.0f;
        };

        for (int i = 0; i < static_cast<int>(g_plateaus.size()); ++i) {
            const Plateau& plateau = g_plateaus[i];
            if (!plateau.active) {
                continue;
            }
            float plateau_y = plateau.height - plateau_fall_offset(i);
            glm::vec2 min = plateau.center - plateau.half;
            glm::vec2 max = plateau.center + plateau.half;
            add_quad(glm::vec3(min.x, plateau_y, min.y),
                     glm::vec3(max.x, plateau_y, min.y),
                     glm::vec3(max.x, plateau_y, max.y),
                     glm::vec3(min.x, plateau_y, max.y));
        }

        for (const RampTile& ramp : g_ramps) {
            if (!ramp.active) {
                continue;
            }
            glm::vec2 min = ramp.center - ramp.half;
            glm::vec2 max = ramp.center + ramp.half;
            glm::vec2 p00(min.x, min.y);
            glm::vec2 p10(max.x, min.y);
            glm::vec2 p11(max.x, max.y);
            glm::vec2 p01(min.x, max.y);
            float h00 = RampHeightAt(ramp, p00);
            float h10 = RampHeightAt(ramp, p10);
            float h11 = RampHeightAt(ramp, p11);
            float h01 = RampHeightAt(ramp, p01);
            add_quad(glm::vec3(p00.x, h00, p00.y),
                     glm::vec3(p10.x, h10, p10.y),
                     glm::vec3(p11.x, h11, p11.y),
                     glm::vec3(p01.x, h01, p01.y));
        }

        glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
        glBufferData(GL_ARRAY_BUFFER, ground_vertices.size() * sizeof(float),
                     ground_vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    std::vector<float> ramp_wall_vertices;
    std::vector<float> ramp_surface_vertices;
    std::vector<float> ramp_side_vertices;
    GLuint ramp_wall_vao = 0;
    GLuint ramp_wall_vbo = 0;
    GLuint ramp_surface_vao = 0;
    GLuint ramp_surface_vbo = 0;
    GLuint ramp_side_vao = 0;
    GLuint ramp_side_vbo = 0;
    glGenVertexArrays(1, &ramp_wall_vao);
    glGenBuffers(1, &ramp_wall_vbo);
    glBindVertexArray(ramp_wall_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ramp_wall_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &ramp_surface_vao);
    glGenBuffers(1, &ramp_surface_vbo);
    glBindVertexArray(ramp_surface_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ramp_surface_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &ramp_side_vao);
    glGenBuffers(1, &ramp_side_vbo);
    glBindVertexArray(ramp_side_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ramp_side_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    auto rebuild_ramp_walls = [&]() {
        ramp_wall_vertices.clear();
        ramp_surface_vertices.clear();
        ramp_side_vertices.clear();
        auto add_edge = [&](const glm::vec3& a, const glm::vec3& b) {
            ramp_wall_vertices.push_back(a.x);
            ramp_wall_vertices.push_back(a.y);
            ramp_wall_vertices.push_back(a.z);
            ramp_wall_vertices.push_back(b.x);
            ramp_wall_vertices.push_back(b.y);
            ramp_wall_vertices.push_back(b.z);
        };
        auto add_ramp_quad = [&](const glm::vec3& p00, const glm::vec3& p10,
                                 const glm::vec3& p11, const glm::vec3& p01) {
            glm::vec3 tri1[3] = {p00, p11, p10};
            glm::vec3 tri2[3] = {p00, p01, p11};
            for (int k = 0; k < 3; ++k) {
                ramp_surface_vertices.push_back(tri1[k].x);
                ramp_surface_vertices.push_back(tri1[k].y);
                ramp_surface_vertices.push_back(tri1[k].z);
            }
            for (int k = 0; k < 3; ++k) {
                ramp_surface_vertices.push_back(tri2[k].x);
                ramp_surface_vertices.push_back(tri2[k].y);
                ramp_surface_vertices.push_back(tri2[k].z);
            }
        };
        auto add_side_quad = [&](const glm::vec3& p00, const glm::vec3& p10,
                                 const glm::vec3& p11, const glm::vec3& p01) {
            glm::vec3 tri1[3] = {p00, p11, p10};
            glm::vec3 tri2[3] = {p00, p01, p11};
            for (int k = 0; k < 3; ++k) {
                ramp_side_vertices.push_back(tri1[k].x);
                ramp_side_vertices.push_back(tri1[k].y);
                ramp_side_vertices.push_back(tri1[k].z);
            }
            for (int k = 0; k < 3; ++k) {
                ramp_side_vertices.push_back(tri2[k].x);
                ramp_side_vertices.push_back(tri2[k].y);
                ramp_side_vertices.push_back(tri2[k].z);
            }
        };
        for (const RampTile& ramp : g_ramps) {
            if (!ramp.active) {
                continue;
            }
            glm::vec2 min = ramp.center - ramp.half;
            glm::vec2 max = ramp.center + ramp.half;
            glm::vec2 p00(min.x, min.y);
            glm::vec2 p10(max.x, min.y);
            glm::vec2 p11(max.x, max.y);
            glm::vec2 p01(min.x, max.y);
            float h00 = RampHeightAt(ramp, p00);
            float h10 = RampHeightAt(ramp, p10);
            float h11 = RampHeightAt(ramp, p11);
            float h01 = RampHeightAt(ramp, p01);
            glm::vec3 v00(p00.x, h00, p00.y);
            glm::vec3 v10(p10.x, h10, p10.y);
            glm::vec3 v11(p11.x, h11, p11.y);
            glm::vec3 v01(p01.x, h01, p01.y);
            add_ramp_quad(v00, v10, v11, v01);

            float base_y = ramp.base_height;
            if (ramp.dir == 1 || ramp.dir == 2) {
                glm::vec3 s0a(v00.x, base_y, v00.z);
                glm::vec3 s0b(v01.x, base_y, v01.z);
                glm::vec3 s1a(v10.x, base_y, v10.z);
                glm::vec3 s1b(v11.x, base_y, v11.z);
                add_edge(s0a, v00);
                add_edge(s0b, v01);
                add_edge(s1a, v10);
                add_edge(s1b, v11);
                add_edge(v00, v01);
                add_edge(v10, v11);

                add_side_quad(s0a, s1a, v10, v00);
                add_side_quad(s0b, s1b, v11, v01);
            } else {
                glm::vec3 s0a(v00.x, base_y, v00.z);
                glm::vec3 s0b(v10.x, base_y, v10.z);
                glm::vec3 s1a(v01.x, base_y, v01.z);
                glm::vec3 s1b(v11.x, base_y, v11.z);
                add_edge(s0a, v00);
                add_edge(s0b, v10);
                add_edge(s1a, v01);
                add_edge(s1b, v11);
                add_edge(v00, v10);
                add_edge(v01, v11);

                add_side_quad(s0a, s1a, v01, v00);
                add_side_quad(s0b, s1b, v11, v10);
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, ramp_wall_vbo);
        glBufferData(GL_ARRAY_BUFFER, ramp_wall_vertices.size() * sizeof(float),
                     ramp_wall_vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, ramp_surface_vbo);
        glBufferData(GL_ARRAY_BUFFER, ramp_surface_vertices.size() * sizeof(float),
                     ramp_surface_vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, ramp_side_vbo);
        glBufferData(GL_ARRAY_BUFFER, ramp_side_vertices.size() * sizeof(float),
                     ramp_side_vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

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

    auto build_enemy_mesh = [&](int type) {
        MeshBuilder builder;
        glm::mat4 base = glm::mat4(1.0f);
        if (type == 0) {
            AddCapsule(builder, 0.35f, 0.9f, 6, 12, base);
            glm::mat4 head = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.55f, 0.0f));
            AddSphere(builder, 0.22f, 6, 12, head);
        } else if (type == 1) {
            glm::mat4 body = glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            AddCapsule(builder, 0.22f, 0.9f, 6, 12, body);
            glm::mat4 fin_l = glm::translate(glm::mat4(1.0f), glm::vec3(-0.35f, 0.0f, -0.15f));
            fin_l = glm::rotate(fin_l, glm::radians(65.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            AddCone(builder, 0.18f, 0.45f, 10, fin_l);
            glm::mat4 fin_r = glm::translate(glm::mat4(1.0f), glm::vec3(0.35f, 0.0f, -0.15f));
            fin_r = glm::rotate(fin_r, glm::radians(-65.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            AddCone(builder, 0.18f, 0.45f, 10, fin_r);
        } else if (type == 2) {
            AddBox(builder, glm::vec3(0.6f, 0.35f, 0.45f), base);
            glm::mat4 shoulder_l = glm::translate(glm::mat4(1.0f), glm::vec3(-0.7f, 0.25f, 0.0f));
            AddBox(builder, glm::vec3(0.25f, 0.18f, 0.3f), shoulder_l);
            glm::mat4 shoulder_r = glm::translate(glm::mat4(1.0f), glm::vec3(0.7f, 0.25f, 0.0f));
            AddBox(builder, glm::vec3(0.25f, 0.18f, 0.3f), shoulder_r);
        } else {
            glm::mat4 stem = glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 1.1f, 0.6f));
            AddCone(builder, 0.45f, 1.2f, 12, stem);
            glm::mat4 bud = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.65f, 0.0f));
            AddSphere(builder, 0.28f, 6, 12, bud);
        }
        std::vector<float> verts = FlattenPositions(builder.positions);
        return CreateMesh(verts);
    };
    Mesh enemy_meshes[4] = {build_enemy_mesh(0), build_enemy_mesh(1), build_enemy_mesh(2), build_enemy_mesh(3)};

    auto build_rock_mesh = [&](int seed, int rings, int segments) {
        MeshBuilder builder;
        noise::module::Perlin local_noise;
        local_noise.SetSeed(seed);
        local_noise.SetFrequency(1.5);
        float radius = 0.45f;
        for (int y = 0; y < rings; ++y) {
            float v0 = static_cast<float>(y) / rings;
            float v1 = static_cast<float>(y + 1) / rings;
            float t0 = v0 * glm::pi<float>();
            float t1 = v1 * glm::pi<float>();
            for (int x = 0; x < segments; ++x) {
                float u0 = static_cast<float>(x) / segments;
                float u1 = static_cast<float>(x + 1) / segments;
                float p0 = u0 * glm::two_pi<float>();
                float p1 = u1 * glm::two_pi<float>();
                glm::vec3 a(radius * std::sin(t0) * std::cos(p0), radius * std::cos(t0), radius * std::sin(t0) * std::sin(p0));
                glm::vec3 b(radius * std::sin(t0) * std::cos(p1), radius * std::cos(t0), radius * std::sin(t0) * std::sin(p1));
                glm::vec3 c(radius * std::sin(t1) * std::cos(p1), radius * std::cos(t1), radius * std::sin(t1) * std::sin(p1));
                glm::vec3 d(radius * std::sin(t1) * std::cos(p0), radius * std::cos(t1), radius * std::sin(t1) * std::sin(p0));
                auto jitter = [&](glm::vec3 p) {
                    double n = local_noise.GetValue(p.x * 2.2, p.y * 2.2, p.z * 2.2);
                    float scale = 1.0f + static_cast<float>(n) * 0.25f;
                    return p * scale;
                };
                builder.add_quad(jitter(a), jitter(b), jitter(c), jitter(d));
            }
        }
        return CreateMesh(FlattenPositions(builder.positions));
    };
    Mesh rock_meshes[3] = {build_rock_mesh(1, 6, 10), build_rock_mesh(7, 6, 10), build_rock_mesh(13, 6, 10)};
    Mesh rock_meshes_low[3] = {build_rock_mesh(3, 4, 6), build_rock_mesh(9, 4, 6), build_rock_mesh(15, 4, 6)};

    auto build_tree_mesh = [&](bool low) {
        MeshBuilder builder;
        if (low) {
            AddCylinder(builder, 0.16f, 1.0f, 6, glm::mat4(1.0f));
            glm::mat4 crown = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.8f, 0.0f));
            AddCone(builder, 0.5f, 1.0f, 6, crown);
        } else {
            AddCylinder(builder, 0.18f, 1.1f, 10, glm::mat4(1.0f));
            glm::mat4 crown = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.9f, 0.0f));
            AddCone(builder, 0.6f, 1.2f, 10, crown);
            glm::mat4 crown2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.3f, 0.0f));
            AddCone(builder, 0.45f, 0.9f, 10, crown2);
        }
        return CreateMesh(FlattenPositions(builder.positions));
    };
    Mesh tree_mesh = build_tree_mesh(false);
    Mesh tree_mesh_low = build_tree_mesh(true);

    struct HouseVariantDef {
        glm::vec3 base_half;
        float roof_height;
        float roof_overhang;
        bool chimney;
    };
    const HouseVariantDef house_defs[] = {
        {glm::vec3(0.65f, 0.40f, 0.55f), 0.35f, 0.08f, true},
        {glm::vec3(0.75f, 0.45f, 0.45f), 0.30f, 0.06f, false},
        {glm::vec3(0.55f, 0.38f, 0.65f), 0.42f, 0.10f, true},
        {glm::vec3(0.85f, 0.48f, 0.55f), 0.28f, 0.05f, false},
        {glm::vec3(0.60f, 0.42f, 0.50f), 0.50f, 0.10f, true}
    };
    constexpr int kHouseVariantCount = static_cast<int>(sizeof(house_defs) / sizeof(house_defs[0]));
    std::array<Mesh, kHouseVariantCount> house_base_meshes;
    std::array<Mesh, kHouseVariantCount> house_roof_meshes;
    for (int i = 0; i < kHouseVariantCount; ++i) {
        const HouseVariantDef& def = house_defs[i];
        LitMeshBuilder base_builder;
        LitMeshBuilder roof_builder;
        glm::mat4 base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, def.base_half.y, 0.0f));
        AddLitBox(base_builder, def.base_half, base);
        glm::mat4 roof = glm::translate(glm::mat4(1.0f),
                                        glm::vec3(0.0f, def.base_half.y + def.roof_height * 0.5f, 0.0f));
        AddLitRoof(roof_builder,
                   def.base_half.x + def.roof_overhang,
                   def.base_half.z + def.roof_overhang,
                   def.roof_height,
                   roof);
        if (def.chimney) {
            glm::mat4 chimney = glm::translate(glm::mat4(1.0f),
                                               glm::vec3(def.base_half.x * 0.35f,
                                                         def.base_half.y + def.roof_height * 0.8f,
                                                         -def.base_half.z * 0.2f));
            chimney = glm::scale(chimney, glm::vec3(0.18f, 0.35f, 0.18f));
            AddLitBox(roof_builder, glm::vec3(0.5f), chimney);
        }
        house_base_meshes[i] = CreateLitMesh(base_builder.positions, base_builder.normals);
        house_roof_meshes[i] = CreateLitMesh(roof_builder.positions, roof_builder.normals);
    }

    LitMeshBuilder town_builder;
    glm::mat4 hall_base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.55f, 0.0f));
    AddLitBox(town_builder, glm::vec3(1.1f, 0.55f, 0.9f), hall_base);
    glm::mat4 hall_roof = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.15f, 0.0f));
    AddLitRoof(town_builder, 1.25f, 1.05f, 0.55f, hall_roof);
    glm::mat4 hall_tower = glm::translate(glm::mat4(1.0f), glm::vec3(0.65f, 1.55f, 0.0f));
    hall_tower = glm::scale(hall_tower, glm::vec3(0.35f, 1.2f, 0.35f));
    AddLitBox(town_builder, glm::vec3(0.5f), hall_tower);
    Mesh town_hall_mesh = CreateLitMesh(town_builder.positions, town_builder.normals);

    LitMeshBuilder barracks_builder;
    glm::mat4 barracks_base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.4f, 0.0f));
    AddLitBox(barracks_builder, glm::vec3(0.9f, 0.4f, 0.7f), barracks_base);
    glm::mat4 barracks_roof = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.9f, 0.0f));
    AddLitRoof(barracks_builder, 1.0f, 0.8f, 0.3f, barracks_roof);
    Mesh barracks_mesh = CreateLitMesh(barracks_builder.positions, barracks_builder.normals);

    LitMeshBuilder armory_builder;
    glm::mat4 armory_base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.45f, 0.0f));
    AddLitBox(armory_builder, glm::vec3(0.85f, 0.45f, 0.65f), armory_base);
    glm::mat4 armory_roof = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.95f, 0.0f));
    AddLitRoof(armory_builder, 0.95f, 0.75f, 0.32f, armory_roof);
    glm::mat4 forge = glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, 1.05f, 0.0f));
    forge = glm::scale(forge, glm::vec3(0.22f, 0.45f, 0.22f));
    AddLitBox(armory_builder, glm::vec3(0.5f), forge);
    Mesh armory_mesh = CreateLitMesh(armory_builder.positions, armory_builder.normals);

    LitMeshBuilder tower_builder;
    glm::mat4 tower_base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.75f, 0.0f));
    AddLitBox(tower_builder, glm::vec3(0.45f, 0.75f, 0.45f), tower_base);
    glm::mat4 tower_top = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.55f, 0.0f));
    AddLitRoof(tower_builder, 0.6f, 0.6f, 0.4f, tower_top);
    Mesh watchtower_mesh = CreateLitMesh(tower_builder.positions, tower_builder.normals);

    GLint mvp_location = glGetUniformLocation(program, "uMVP");
    GLint color_location = glGetUniformLocation(program, "uColor");
    GLint alpha_location = glGetUniformLocation(program, "uAlpha");
    GLint use_vertex_color_location = glGetUniformLocation(program, "uUseVertexColor");
    GLint use_normals_location = glGetUniformLocation(program, "uUseNormals");
    GLint light_dir_location = glGetUniformLocation(program, "uLightDir");
    GLint ambient_location = glGetUniformLocation(program, "uAmbient");
    GLint bands_location = glGetUniformLocation(program, "uBands");
    GLint rim_color_location = glGetUniformLocation(program, "uRimColor");
    GLint rim_power_location = glGetUniformLocation(program, "uRimPower");
    GLint rim_strength_location = glGetUniformLocation(program, "uRimStrength");
    GLint outline_color_location = glGetUniformLocation(program, "uOutlineColor");
    GLint outline_pass_location = glGetUniformLocation(program, "uOutlinePass");
    GLint outline_size_location = glGetUniformLocation(program, "uOutlineSize");
    GLint ao_location = glGetUniformLocation(program, "uAO");
    GLint fog_color_location = glGetUniformLocation(program, "uFogColor");
    GLint fog_near_location = glGetUniformLocation(program, "uFogNear");
    GLint fog_far_location = glGetUniformLocation(program, "uFogFar");
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

    const glm::vec3 palette_neutral(0.18f, 0.55f, 0.70f);
    const glm::vec3 palette_outline(0.06f, 0.08f, 0.12f);
    const glm::vec3 palette_rim(0.25f, 0.85f, 0.95f);
    const glm::vec3 palette_fog(0.04f, 0.05f, 0.08f);
    const glm::vec3 palette_light_dir = glm::normalize(glm::vec3(-0.3f, 0.9f, 0.2f));
    const glm::vec3 house_base_palette[] = {
        glm::vec3(0.55f, 0.40f, 0.28f),
        glm::vec3(0.60f, 0.62f, 0.66f),
        glm::vec3(0.45f, 0.50f, 0.56f)
    };
    const glm::vec3 house_roof_palette[] = {
        glm::vec3(0.42f, 0.18f, 0.12f),
        glm::vec3(0.35f, 0.38f, 0.42f),
        glm::vec3(0.70f, 0.52f, 0.28f)
    };

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
        float hit_flash = 0.0f;
    };

    struct ExperienceGem {
        glm::vec3 position;
    };

    enum class BuildingOwner {
        Neutral,
        Player,
        Enemy
    };

    enum class BuildingType {
        House,
        TownHall,
        Barracks,
        Armory,
        Watchtower
    };

    struct Building {
        glm::vec2 center;
        float size = 1.0f;
        float height = 2.0f;
        float base_height = 0.0f;
        BuildingOwner owner = BuildingOwner::Neutral;
        BuildingType type = BuildingType::House;
        bool spawner = false;
        int health = 10;
        int max_health = 10;
        float capture_timer = 0.0f;
        float veteran_timer = 0.0f;
        float spawn_timer = 0.0f;
        float alive_timer = 0.0f;
        bool destroyed = false;
        int platform_index = -1;
        int house_variant = 0;
        int house_material = 0;
        float house_rotation = 0.0f;
    };

    enum class UnitState {
        Home,
        Follow,
        Guard
    };

    enum class OrderType {
        None,
        CaptureHomes
    };

    enum class PlatformState {
        Active,
        Collapsing,
        Landed,
        ConvertedToTown
    };

    struct Platform {
        float base_height = 0.0f;
        float ground_height = 0.0f;
        glm::vec2 center = glm::vec2(0.0f);
        std::vector<int> home_indices;
        PlatformState state = PlatformState::Active;
        float collapse_timer = 0.0f;
        float landed_timer = 0.0f;
        float collapse_duration = 2.2f;
        float fall_distance = 6.0f;
        float fall_offset = 0.0f;
        int plateau_index = -1;
    };

    struct Town {
        glm::vec3 position = glm::vec3(0.0f);
        int upgrade_level = 0;
        float production_timer = 0.0f;
        float barracks_timer = 0.0f;
        float watchtower_timer = 0.0f;
        int barracks_level = 0;
        int armory_level = 0;
        int watchtower_level = 0;
        bool ui_open = false;
        int platform_index = -1;
    };

    struct Ally {
        glm::vec3 position;
        int health = 4;
        int max_health = 4;
        int id = 0;
        int home_index = -1;
        int guard_index = -1;
        UnitState state = UnitState::Home;
        OrderType order = OrderType::None;
        int order_target_home = -1;
        int order_platform_index = -1;
        bool gold = false;
        float attack_timer = 0.0f;
        float hurt_timer = 0.0f;
        float respawn_timer = 0.0f;
        bool alive = true;
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
    bool is_ramp_wall = false;
    float height = 0.0f;
    bool is_building = false;
    BuildingOwner owner = BuildingOwner::Neutral;
};

    struct Projectile {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        int damage;
        glm::vec3 color;
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

    struct SpawnPoof {
        glm::vec3 position;
        float timer = 0.0f;
        float duration = 0.6f;
    };

    struct CaptureWave {
        glm::vec3 position;
        float timer = 0.0f;
        float duration = 0.8f;
    };

    enum class GroundEffectType {
        Holy,
        Poison
    };

    struct GroundEffect {
        glm::vec3 position;
        float radius = 2.0f;
        float timer = 0.0f;
        float duration = 3.5f;
        float tick_timer = 0.0f;
        float tick_interval = 0.4f;
        int damage = 1;
        GroundEffectType type = GroundEffectType::Holy;
    };

    enum class ItemId {
        Fireball,
        FireballUpgrade,
        GarlicUpgrade,
        Magnet,
        Cross,
        CrossUpgrade,
        Stick,
        StickUpgrade,
        Crossbow,
        CrossbowUpgrade,
        HolyWater,
        HolyWaterUpgrade,
        PoisonBomb,
        PoisonBombUpgrade,
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
        Customization,
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
    int meta_magnet_level = save_data.magnet_level;
    bool unlock_cross = save_data.unlock_cross != 0;
    bool unlock_stick = save_data.unlock_stick != 0;
    bool unlock_crossbow = save_data.unlock_crossbow != 0;
    bool unlock_holywater = save_data.unlock_holywater != 0;
    bool unlock_poison = save_data.unlock_poison != 0;
    bool unlock_bat = save_data.unlock_bat != 0;
    bool skin_unlocked = save_data.skin_unlocked != 0;
    bool skin_selected = skin_unlocked;
    int hair_style = save_data.hair_style;
    int hair_color = save_data.hair_color;
    int hair_unlock_mask = save_data.hair_unlock_mask == 0 ? 1 : save_data.hair_unlock_mask;

    int main_menu_index = 0;
    int starter_weapon_index = 0;
    int powerup_menu_index = 0;
    int customize_menu_index = 0;
    constexpr int kHairStyleCount = 3;
    const char* hair_style_names[kHairStyleCount] = {"Mohawk", "Roman Crest", "Spikes"};
    const int hair_style_costs[kHairStyleCount] = {0, 80, 120};
    const glm::vec3 hair_colors[] = {
        glm::vec3(0.95f, 0.55f, 0.15f),
        glm::vec3(0.25f, 0.95f, 0.85f),
        glm::vec3(0.85f, 0.25f, 0.95f),
        glm::vec3(0.95f, 0.85f, 0.20f),
        glm::vec3(0.15f, 0.95f, 0.45f)
    };
    const int hair_color_count = static_cast<int>(sizeof(hair_colors) / sizeof(hair_colors[0]));
    hair_style = (hair_style % kHairStyleCount + kHairStyleCount) % kHairStyleCount;
    hair_color = (hair_color % hair_color_count + hair_color_count) % hair_color_count;
    GameState state = GameState::MainMenu;
    bool victory = false;
    float game_over_timer = 0.0f;
    float run_time = 0.0f;
    const float win_time = 300.0f;
    int enemies_killed = 0;
    int coins_earned = 0;
    int points = 0;

    glm::vec3 player_position(0.0f, 0.0f, 0.0f);
    glm::vec3 player_velocity(0.0f, 0.0f, 0.0f);
    float player_move_amount = 0.0f;
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
    float camera_height = 0.0f;
    bool camera_height_initialized = false;
    bool camera_look_active = false;
    int selected_ally_id = -1;
    bool house_wireframe_debug = false;
    bool spawn_debug = false;
    glm::vec3 last_spawn_debug_pos(0.0f);
    int last_spawn_debug_platform = -1;
    uint32_t terrain_seed = 1337;
    bool terrain_dirty = true;
    const float play_area_extent = 90.0f;

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
    std::vector<SpawnPoof> spawn_poofs;
    std::vector<CaptureWave> capture_waves;
    std::vector<GroundEffect> ground_effects;
    std::vector<Pickup> pickups;
    std::vector<Building> buildings;
    std::vector<Platform> platforms;
    std::vector<Ally> allies;
    std::vector<Town> towns;
    std::vector<Obstacle> obstacles;
    int ally_id_counter = 0;
    float spawn_timer = 0.0f;
    int early_spawn_index = 0;
    const float base_spawn_interval = 2.0f;
    float attack_timer = 0.0f;
    float attack_interval = 1.0f;
    float attack_radius = 2.2f;
    int attack_damage = 1;
    float player_damage_timer = 0.0f;
    const float player_damage_interval = 0.7f;
    float player_hurt_timer = 0.0f;
    const float player_contact_radius = 0.9f;

    int garlic_level = 1;
    int magnet_level = 0;
    int fireball_level = 0;
    float fireball_timer = 0.0f;
    float fireball_cooldown = 1.5f;
    int fireball_damage = 2;
    int crossbow_level = 0;
    float crossbow_timer = 0.0f;
    float crossbow_cooldown = 1.2f;
    int crossbow_damage = 2;
    int stick_level = 0;
    float stick_timer = 0.0f;
    float stick_flash_timer = 0.0f;
    float stick_cooldown = 0.9f;
    float stick_range = 1.8f;
    int stick_damage = 2;
    int cross_level = 0;
    float cross_timer = 0.0f;
    float cross_tick = 0.45f;
    float cross_orbit_radius = 1.6f;
    float cross_hit_radius = 0.7f;
    int cross_damage = 2;
    int holywater_level = 0;
    float holywater_timer = 0.0f;
    float holywater_cooldown = 2.4f;
    float holywater_radius = 2.0f;
    int holywater_damage = 1;
    int poison_level = 0;
    float poison_timer = 0.0f;
    float poison_cooldown = 3.0f;
    float poison_radius = 2.4f;
    int poison_damage = 1;
    int bomb_level = 0;
    float bomb_timer = 0.0f;
    float bomb_cooldown = 2.5f;
    float bomb_radius = 2.8f;
    int bomb_damage = 4;

    float freeze_timer = 0.0f;
    float nuke_flash_timer = 0.0f;
    float jump_timer = 0.0f;
    float jump_release_timer = 0.0f;
    float jump_cooldown_timer = 0.0f;
    const float jump_duration = 0.35f;
    const float jump_cooldown = 0.8f;
    const float jump_height = 1.2f;
    float enemy_spawner_timer = 0.0f;
    const float enemy_spawner_interval = 35.0f;
    float pickup_magnet_radius = 1.2f;

    std::vector<ItemChoice> current_choices;

    const int city_half = 16;
    const float block_size = kTileSize;
    const float block_half = 1.1f;
    const float wall_thickness = 0.3f;
    auto rebuild_buildings = [&]() {
        buildings.clear();
        platforms.clear();
        towns.clear();
        if (g_plateau_fall_offsets.size() != g_plateaus.size()) {
            g_plateau_fall_offsets.assign(g_plateaus.size(), 0.0f);
        }
        for (int x = -city_half; x <= city_half; ++x) {
            for (int z = -city_half; z <= city_half; ++z) {
                if ((x + z) % 2 != 0) {
                    continue;
                }
                if (x == 0 && z == 0) {
                    continue;
                }
                glm::vec2 center(static_cast<float>(x) * block_size,
                                 static_cast<float>(z) * block_size);
                if (!IsOnPlateau(center) || IsOnRamp(center) || IsNearRampArea(center, block_half + 0.8f)) {
                    continue;
                }
                float height = 1.5f + 0.6f * static_cast<float>((x * x + z * z) % 6);
                Building building;
                building.center = center;
                building.size = block_half * 2.0f;
                building.height = height;
                building.base_height = PlateauHeightAt(center);
                building.owner = BuildingOwner::Neutral;
                building.type = BuildingType::House;
                building.spawner = false;
                building.max_health = 10;
                building.health = building.max_health;
                uint32_t house_seed = terrain_seed;
                house_seed ^= static_cast<uint32_t>(x * 73856093);
                house_seed ^= static_cast<uint32_t>(z * 19349663);
                std::mt19937 house_rng(house_seed);
                std::uniform_int_distribution<int> variant_dist(0, kHouseVariantCount - 1);
                std::uniform_int_distribution<int> material_dist(0,
                    static_cast<int>(sizeof(house_base_palette) / sizeof(house_base_palette[0])) - 1);
                std::uniform_int_distribution<int> rot_dist(0, 3);
                building.house_variant = variant_dist(house_rng);
                building.house_material = material_dist(house_rng);
                building.house_rotation = static_cast<float>(rot_dist(house_rng)) * glm::half_pi<float>();
                buildings.push_back(building);
            }
        }

        std::vector<int> platform_index_by_plateau(g_plateaus.size(), -1);
        for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
            Building& building = buildings[i];
            int plateau_index = -1;
            for (int p = 0; p < static_cast<int>(g_plateaus.size()); ++p) {
                if (PointInRect(building.center, g_plateaus[p].center, g_plateaus[p].half)) {
                    plateau_index = p;
                    break;
                }
            }
            if (plateau_index < 0) {
                continue;
            }
            int platform_index = platform_index_by_plateau[plateau_index];
            if (platform_index == -1) {
                Platform platform;
                platform.base_height = g_plateaus[plateau_index].height;
                platform.center = g_plateaus[plateau_index].center;
                platform.plateau_index = plateau_index;
                platform.ground_height = BaseNoiseHeight(platform.center.x, platform.center.y);
                platform.fall_distance = glm::max(0.0f, platform.base_height - platform.ground_height);
                platforms.push_back(platform);
                platform_index = static_cast<int>(platforms.size()) - 1;
                platform_index_by_plateau[plateau_index] = platform_index;
            }
            platforms[platform_index].home_indices.push_back(i);
            building.platform_index = platform_index;
        }
    };
    auto rebuild_obstacles = [&]() {
        obstacles.clear();
        for (const Building& building : buildings) {
            Obstacle box;
            box.min = building.center - glm::vec2(block_half, block_half);
            box.max = building.center + glm::vec2(block_half, block_half);
            box.is_ramp_wall = false;
            box.height = building.base_height;
            box.is_building = true;
            box.owner = building.owner;
            obstacles.push_back(box);
        }
        for (const RampTile& ramp : g_ramps) {
            if (!ramp.active) {
                continue;
            }
            glm::vec2 min = ramp.center - ramp.half;
            glm::vec2 max = ramp.center + ramp.half;
            if (ramp.dir == 1 || ramp.dir == 2) {
                Obstacle wall_a;
                wall_a.min = glm::vec2(min.x, min.y - wall_thickness);
                wall_a.max = glm::vec2(max.x, min.y + wall_thickness);
                wall_a.is_ramp_wall = true;
                wall_a.height = ramp.height;
                wall_a.is_building = false;
                obstacles.push_back(wall_a);

                Obstacle wall_b;
                wall_b.min = glm::vec2(min.x, max.y - wall_thickness);
                wall_b.max = glm::vec2(max.x, max.y + wall_thickness);
                wall_b.is_ramp_wall = true;
                wall_b.height = ramp.height;
                wall_b.is_building = false;
                obstacles.push_back(wall_b);
            } else {
                Obstacle wall_a;
                wall_a.min = glm::vec2(min.x - wall_thickness, min.y);
                wall_a.max = glm::vec2(min.x + wall_thickness, max.y);
                wall_a.is_ramp_wall = true;
                wall_a.height = ramp.height;
                wall_a.is_building = false;
                obstacles.push_back(wall_a);

                Obstacle wall_b;
                wall_b.min = glm::vec2(max.x - wall_thickness, min.y);
                wall_b.max = glm::vec2(max.x + wall_thickness, max.y);
                wall_b.is_ramp_wall = true;
                wall_b.height = ramp.height;
                wall_b.is_building = false;
                obstacles.push_back(wall_b);
            }
        }
    };

    terrain_seed = static_cast<uint32_t>(rng());
    GenerateTerrain(terrain_seed);
    g_plateau_fall_offsets.assign(g_plateaus.size(), 0.0f);
    rebuild_ground();
    rebuild_ramp_walls();
    rebuild_buildings();
    rebuild_obstacles();
    terrain_dirty = false;

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

        position.y = TerrainHeightAt(position.x, position.z, position.y);
        if (run_time >= 90.0f && unit_dist(rng) < 0.12f) {
            elite = true;
            health += 6;
            damage += 1;
            scale *= 1.15f;
            color = glm::vec3(0.95f, 0.80f, 0.25f);
        }

        float phase = unit_dist(rng) * glm::two_pi<float>();
        enemies.push_back(Enemy{position, speed, health, damage, scale, color, phase, type, elite, 0.0f});
        spawn_poofs.push_back(SpawnPoof{position, 0.0f, 0.5f});
    };

    auto spawn_enemy_at = [&](const glm::vec3& origin, int health_mult, const glm::vec3& color_override, bool force_elite) {
        glm::vec3 position = origin;
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

        position.y = TerrainHeightAt(position.x, position.z, position.y);
        if (force_elite || (run_time >= 90.0f && unit_dist(rng) < 0.12f)) {
            elite = true;
            health += 6;
            damage += 1;
            scale *= 1.15f;
            color = glm::vec3(0.95f, 0.80f, 0.25f);
        }
        if (health_mult > 1) {
            health *= health_mult;
        }
        if (color_override.x >= 0.0f) {
            color = color_override;
        }

        float phase = unit_dist(rng) * glm::two_pi<float>();
        enemies.push_back(Enemy{position, speed, health, damage, scale, color, phase, type, elite, 0.0f});
        spawn_poofs.push_back(SpawnPoof{position, 0.0f, 0.5f});
    };

    auto assign_enemy_spawners = [&](int count) {
        std::vector<int> indices;
        indices.reserve(static_cast<int>(buildings.size()));
        for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
            indices.push_back(i);
        }
        std::shuffle(indices.begin(), indices.end(), rng);
        int assigned = 0;
        int base_tank_health = (3 + player_level / 3 + 5);
        int spawner_health = base_tank_health * 5;
        for (int idx : indices) {
            if (assigned >= count) {
                break;
            }
            Building& building = buildings[idx];
            if (building.owner != BuildingOwner::Neutral) {
                continue;
            }
            building.owner = BuildingOwner::Enemy;
            building.spawner = true;
            building.destroyed = false;
            building.max_health = spawner_health;
            building.health = spawner_health;
            building.capture_timer = 0.0f;
            building.spawn_timer = 0.0f;
            building.alive_timer = 0.0f;
            assigned += 1;
        }
    };

    auto get_platform_bounds = [&](int platform_index, glm::vec2& center, glm::vec2& half) {
        if (platform_index < 0 || platform_index >= static_cast<int>(platforms.size())) {
            return false;
        }
        int plateau_index = platforms[platform_index].plateau_index;
        if (plateau_index < 0 || plateau_index >= static_cast<int>(g_plateaus.size())) {
            return false;
        }
        center = g_plateaus[plateau_index].center;
        half = g_plateaus[plateau_index].half;
        return true;
    };

    auto clamp_to_platform = [&](int platform_index, const glm::vec2& p, float margin) {
        glm::vec2 center(0.0f);
        glm::vec2 half(0.0f);
        if (!get_platform_bounds(platform_index, center, half)) {
            return p;
        }
        glm::vec2 min = center - half + glm::vec2(margin);
        glm::vec2 max = center + half - glm::vec2(margin);
        glm::vec2 clamped = glm::clamp(p, min, max);
        return clamped;
    };

    auto pick_spawn_outside_building = [&](const Building& building, int platform_index) {
        float margin = 0.35f;
        float radius = building.size * 0.65f + margin;
        std::array<glm::vec2, 8> offsets = {
            glm::vec2(radius, 0.0f),
            glm::vec2(-radius, 0.0f),
            glm::vec2(0.0f, radius),
            glm::vec2(0.0f, -radius),
            glm::vec2(radius * 0.7f, radius * 0.7f),
            glm::vec2(-radius * 0.7f, radius * 0.7f),
            glm::vec2(radius * 0.7f, -radius * 0.7f),
            glm::vec2(-radius * 0.7f, -radius * 0.7f)
        };
        std::shuffle(offsets.begin(), offsets.end(), rng);
        glm::vec2 center = building.center;
        glm::vec2 half(building.size * 0.5f, building.size * 0.5f);
        for (const glm::vec2& offset : offsets) {
            glm::vec2 candidate = center + offset;
            candidate = clamp_to_platform(platform_index, candidate, margin);
            if (!PointInRect(candidate, center, half)) {
                return candidate;
            }
        }
        glm::vec2 fallback = clamp_to_platform(platform_index, center + glm::vec2(radius, 0.0f), margin);
        return fallback;
    };

    auto ally_spawn_position = [&](const Building& building) {
        glm::vec2 pos = pick_spawn_outside_building(building, building.platform_index);
        float y = TerrainHeightAt(pos.x, pos.y, building.base_height);
        if (spawn_debug) {
            last_spawn_debug_pos = glm::vec3(pos.x, y + 0.1f, pos.y);
            last_spawn_debug_platform = building.platform_index;
        }
        return glm::vec3(pos.x, y + 0.6f, pos.y);
    };

    auto ally_ring_offset = [&](int id, float radius) {
        float angle = static_cast<float>(id % 16) / 16.0f * glm::two_pi<float>() +
                      std::sin(static_cast<float>(id)) * 0.35f;
        return glm::vec2(std::cos(angle), std::sin(angle)) * radius;
    };

    auto find_nearest_player_building = [&](const glm::vec3& pos) {
        int best_index = -1;
        float best_dist = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
            if (buildings[i].owner != BuildingOwner::Player) {
                continue;
            }
            glm::vec2 delta = buildings[i].center - glm::vec2(pos.x, pos.z);
            float dist = glm::dot(delta, delta);
            if (dist < best_dist) {
                best_dist = dist;
                best_index = i;
            }
        }
        return best_index;
    };

    auto command_units = [&](UnitState state, int guard_index) {
        float command_radius = attack_radius * 1.3f;
        float command_radius_sq = command_radius * command_radius;
        for (Ally& ally : allies) {
            if (!ally.alive) {
                continue;
            }
            glm::vec2 delta = glm::vec2(ally.position.x - player_position.x,
                                        ally.position.z - player_position.z);
            if (glm::dot(delta, delta) > command_radius_sq) {
                continue;
            }
            ally.state = state;
            ally.guard_index = guard_index;
            ally.order = OrderType::None;
            ally.order_target_home = -1;
            ally.order_platform_index = -1;
        }
    };

    auto find_platform_index_for_position = [&](const glm::vec3& pos) {
        for (int p = 0; p < static_cast<int>(g_plateaus.size()); ++p) {
            if (PointInRect(glm::vec2(pos.x, pos.z), g_plateaus[p].center, g_plateaus[p].half)) {
                for (int i = 0; i < static_cast<int>(platforms.size()); ++i) {
                    if (platforms[i].plateau_index == p) {
                        return i;
                    }
                }
            }
        }
        return -1;
    };

    auto find_nearest_uncaptured_home = [&](int platform_index, const glm::vec3& pos) {
        if (platform_index < 0 || platform_index >= static_cast<int>(platforms.size())) {
            return -1;
        }
        const Platform& platform = platforms[platform_index];
        if (platform.state != PlatformState::Active) {
            return -1;
        }
        float best_dist = std::numeric_limits<float>::max();
        int best_index = -1;
        for (int index : platform.home_indices) {
            const Building& building = buildings[index];
            if (building.owner == BuildingOwner::Player) {
                continue;
            }
            glm::vec2 delta = building.center - glm::vec2(pos.x, pos.z);
            float dist = glm::dot(delta, delta);
            if (dist < best_dist) {
                best_dist = dist;
                best_index = index;
            }
        }
        return best_index;
    };

    auto spawn_town_for_platform = [&](int platform_index) {
        if (platform_index < 0 || platform_index >= static_cast<int>(platforms.size())) {
            return;
        }
        for (const Town& town : towns) {
            if (town.platform_index == platform_index) {
                return;
            }
        }
        const Platform& platform = platforms[platform_index];
        int hall_index = -1;
        float best_dist = std::numeric_limits<float>::max();
        for (int index : platform.home_indices) {
            const Building& building = buildings[index];
            glm::vec2 delta = building.center - platform.center;
            float dist = glm::dot(delta, delta);
            if (dist < best_dist) {
                best_dist = dist;
                hall_index = index;
            }
        }
        if (hall_index >= 0) {
            buildings[hall_index].type = BuildingType::TownHall;
            std::vector<int> remaining;
            remaining.reserve(platform.home_indices.size());
            for (int index : platform.home_indices) {
                if (index != hall_index) {
                    remaining.push_back(index);
                }
            }
            std::sort(remaining.begin(), remaining.end(), [&](int a, int b) {
                glm::vec2 da = buildings[a].center - buildings[hall_index].center;
                glm::vec2 db = buildings[b].center - buildings[hall_index].center;
                return glm::dot(da, da) < glm::dot(db, db);
            });
            if (!remaining.empty()) {
                buildings[remaining[0]].type = BuildingType::Barracks;
            }
            if (remaining.size() > 1) {
                buildings[remaining[1]].type = BuildingType::Armory;
            }
            if (remaining.size() > 2) {
                buildings[remaining[2]].type = BuildingType::Watchtower;
            }
        }
        Town town;
        glm::vec3 hall_pos = hall_index >= 0
            ? glm::vec3(buildings[hall_index].center.x, platform.ground_height + 0.35f, buildings[hall_index].center.y)
            : glm::vec3(platform.center.x, platform.ground_height + 0.35f, platform.center.y);
        town.position = hall_pos;
        town.platform_index = platform_index;
        towns.push_back(town);
    };

    auto save_progress = [&]() {
        save_data.coins = coins;
        save_data.damage_level = meta_damage_level;
        save_data.speed_level = meta_speed_level;
        save_data.max_health_level = meta_max_health_level;
        save_data.attack_cooldown_level = meta_attack_cooldown_level;
        save_data.magnet_level = meta_magnet_level;
        save_data.unlock_cross = unlock_cross ? 1 : 0;
        save_data.unlock_stick = unlock_stick ? 1 : 0;
        save_data.unlock_crossbow = unlock_crossbow ? 1 : 0;
        save_data.unlock_holywater = unlock_holywater ? 1 : 0;
        save_data.unlock_poison = unlock_poison ? 1 : 0;
        save_data.unlock_bat = unlock_bat ? 1 : 0;
        save_data.skin_unlocked = skin_unlocked ? 1 : 0;
        save_data.hair_style = hair_style;
        save_data.hair_color = hair_color;
        save_data.hair_unlock_mask = hair_unlock_mask;
        SaveProgress(save_path, save_data);
    };

    auto build_starter_weapons = [&]() {
        auto options = BuildStarterWeapons(unlock_cross, unlock_stick, unlock_crossbow, unlock_poison);
        if (options.empty()) {
            options.push_back({StarterWeaponId::Garlic, "Garlic"});
        }
        if (starter_weapon_index >= static_cast<int>(options.size())) {
            starter_weapon_index = 0;
        }
        return options;
    };

    auto start_run = [&]() {
        if (terrain_dirty) {
            GenerateTerrain(terrain_seed);
            rebuild_ground();
            rebuild_ramp_walls();
            rebuild_buildings();
            rebuild_obstacles();
            terrain_dirty = false;
        }
        enemies.clear();
        gems.clear();
        projectiles.clear();
        bombs.clear();
        explosions.clear();
        spawn_poofs.clear();
        capture_waves.clear();
        current_choices.clear();
        pickups.clear();
        allies.clear();
        rebuild_buildings();
        rebuild_obstacles();

        player_position = glm::vec3(0.0f, 0.0f, 0.0f);
        player_position.y = TerrainHeightAt(player_position.x, player_position.z, player_position.y);
        player_velocity = glm::vec3(0.0f);
        player_level = 1;
        player_xp = 0;
        player_max_health = 10 + meta_max_health_level;
        player_health = player_max_health;
        player_speed = 4.0f * (1.0f + 0.05f * static_cast<float>(meta_speed_level));
        attack_interval = glm::max(0.4f, 1.0f - 0.05f * static_cast<float>(meta_attack_cooldown_level));
        attack_damage = 1 + meta_damage_level;
        attack_radius = 2.2f;
        magnet_level = 0;
        pickup_magnet_radius = 1.2f * std::pow(1.15f,
            static_cast<float>(meta_magnet_level + magnet_level));

        auto starters = build_starter_weapons();
        StarterWeaponId starter_id = starters[starter_weapon_index].id;

        garlic_level = starter_id == StarterWeaponId::Garlic ? 1 : 0;
        fireball_level = 0;
        fireball_timer = 0.0f;
        fireball_cooldown = 1.5f;
        fireball_damage = 2 + meta_damage_level;
        crossbow_level = starter_id == StarterWeaponId::Crossbow ? 1 : 0;
        crossbow_timer = 0.0f;
        crossbow_cooldown = 1.2f;
        crossbow_damage = 2 + meta_damage_level;
        stick_level = starter_id == StarterWeaponId::Stake ? 1 : 0;
        stick_timer = 0.0f;
        stick_flash_timer = 0.0f;
        stick_cooldown = 0.9f;
        stick_range = 1.8f;
        stick_damage = 2 + meta_damage_level;
        cross_level = starter_id == StarterWeaponId::Cross ? 1 : 0;
        cross_timer = 0.0f;
        cross_tick = 0.45f;
        cross_orbit_radius = 1.6f;
        cross_hit_radius = 0.7f;
        cross_damage = 2 + meta_damage_level;
        holywater_level = 0;
        holywater_timer = 0.0f;
        holywater_cooldown = 2.4f;
        holywater_radius = 2.0f;
        holywater_damage = 1 + meta_damage_level / 2;
        poison_level = starter_id == StarterWeaponId::Caltrops ? 1 : 0;
        poison_timer = 0.0f;
        poison_cooldown = 3.0f;
        poison_radius = 2.4f;
        poison_damage = 1 + meta_damage_level / 2;
        bomb_level = starter_id == StarterWeaponId::HolyBomb ? 1 : 0;
        bomb_timer = 0.0f;
        bomb_cooldown = 2.5f;
        bomb_radius = 2.8f;
        bomb_damage = 4 + meta_damage_level;

        if (garlic_level > 0) {
            attack_radius = 2.2f * std::pow(1.1f, static_cast<float>(garlic_level - 1));
        }

        spawn_timer = 0.0f;
        early_spawn_index = 0;
        attack_timer = 0.0f;
        player_damage_timer = 0.0f;
        run_time = 0.0f;
        enemies_killed = 0;
        coins_earned = 0;
        points = 0;
        victory = false;
        freeze_timer = 0.0f;
        enemy_spawner_timer = 0.0f;
        assign_enemy_spawners(4);
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
    auto obstacle_active = [&](const Obstacle& box, float current_y) {
        if (box.is_ramp_wall) {
            return current_y <= box.height - 1.0f;
        }
        if (box.height <= 0.1f) {
            return true;
        }
        return current_y >= box.height - 1.0f;
    };
    const float max_step_height = 1.2f;
    const float ramp_entry_margin = 1.2f;
    auto can_step = [&](const glm::vec2& from, const glm::vec2& to, float max_step, float current_y) {
        float from_h = TerrainHeightAt(from.x, from.y, current_y);
        float to_h = TerrainHeightAt(to.x, to.y, current_y);
        if (to_h - from_h > max_step) {
            if (!RampAllowsTransition(from, to, ramp_entry_margin)) {
                return false;
            }
        }
        return true;
    };

    Uint64 last_ticks = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        bool want_relative = state == GameState::Running && camera_look_active;
        SDL_SetRelativeMouseMode(want_relative ? SDL_TRUE : SDL_FALSE);
        SDL_ShowCursor(want_relative ? SDL_DISABLE : SDL_ENABLE);
        Uint64 current_ticks = SDL_GetPerformanceCounter();
        float delta_time = static_cast<float>(current_ticks - last_ticks) /
                           static_cast<float>(SDL_GetPerformanceFrequency());
        last_ticks = current_ticks;

        auto project_to_screen = [&](const glm::vec3& world_pos, float& out_x, float& out_y) {
            glm::mat4 projection = glm::perspective(
                glm::radians(60.0f),
                static_cast<float>(window_width) / static_cast<float>(window_height),
                0.1f,
                140.0f);
            float jump_view_phase = jump_timer > 0.0f
                ? (jump_duration - jump_timer) / jump_duration
                : 0.0f;
            float jump_view_offset = jump_timer > 0.0f
                ? std::sin(jump_view_phase * glm::pi<float>()) * jump_height
                : 0.0f;
            glm::vec3 cam_dir(
                std::cos(camera_pitch) * std::cos(camera_yaw),
                std::sin(camera_pitch),
                std::cos(camera_pitch) * std::sin(camera_yaw));
            glm::vec3 camera_pos = player_position - cam_dir * camera_distance;
            float min_cam_y = TerrainHeightAt(camera_pos.x, camera_pos.z, camera_pos.y) + 1.0f;
            float target_cam_y = glm::max(camera_pos.y, min_cam_y);
            camera_pos.y = camera_height_initialized ? camera_height : target_cam_y;
            glm::mat4 view = glm::lookAt(
                camera_pos,
                player_position + glm::vec3(0.0f, jump_view_offset, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec4 clip = projection * view * glm::vec4(world_pos, 1.0f);
            if (clip.w <= 0.0f) {
                return false;
            }
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
                return false;
            }
            out_x = (ndc.x * 0.5f + 0.5f) * window_width;
            out_y = (ndc.y * 0.5f + 0.5f) * window_height;
            return true;
        };

        auto pick_friendly_unit = [&](int mx, int my) {
            const float pick_radius = 26.0f;
            float best_dist2 = pick_radius * pick_radius;
            int best_id = -1;
            float mouse_x = static_cast<float>(mx);
            float mouse_y = static_cast<float>(window_height - my);
            for (const Ally& ally : allies) {
                if (!ally.alive) {
                    continue;
                }
                float sx = 0.0f;
                float sy = 0.0f;
                if (!project_to_screen(ally.position + glm::vec3(0.0f, 0.45f, 0.0f), sx, sy)) {
                    continue;
                }
                float dx = sx - mouse_x;
                float dy = sy - mouse_y;
                float dist2 = dx * dx + dy * dy;
                if (dist2 <= best_dist2) {
                    best_dist2 = dist2;
                    best_id = ally.id;
                }
            }
            return best_id;
        };

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
                        main_menu_index = (main_menu_index + 3) % 4;
                    } else if (key == SDLK_DOWN || key == SDLK_s) {
                        main_menu_index = (main_menu_index + 1) % 4;
                    } else if (key == SDLK_LEFT || key == SDLK_a) {
                        auto starters = build_starter_weapons();
                        int count = static_cast<int>(starters.size());
                        starter_weapon_index = (starter_weapon_index + count - 1) % count;
                    } else if (key == SDLK_RIGHT || key == SDLK_d) {
                        auto starters = build_starter_weapons();
                        int count = static_cast<int>(starters.size());
                        starter_weapon_index = (starter_weapon_index + 1) % count;
                    } else if (key == SDLK_r) {
                        terrain_seed = static_cast<uint32_t>(rng());
                        GenerateTerrain(terrain_seed);
                        rebuild_ground();
                        rebuild_ramp_walls();
                        rebuild_buildings();
                        rebuild_obstacles();
                        terrain_dirty = false;
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (main_menu_index == 0) {
                            start_run();
                            state = GameState::Running;
                        } else if (main_menu_index == 1) {
                            powerup_menu_index = 0;
                            state = GameState::PowerUps;
                        } else if (main_menu_index == 2) {
                            customize_menu_index = 0;
                            state = GameState::Customization;
                        } else {
                            running = false;
                        }
                    }
                } else if (state == GameState::Customization) {
                    if (key == SDLK_UP || key == SDLK_w) {
                        customize_menu_index = (customize_menu_index + 2) % 3;
                    } else if (key == SDLK_DOWN || key == SDLK_s) {
                        customize_menu_index = (customize_menu_index + 1) % 3;
                    } else if (key == SDLK_LEFT || key == SDLK_a) {
                        if (customize_menu_index == 0) {
                            hair_style = (hair_style + kHairStyleCount - 1) % kHairStyleCount;
                        } else if (customize_menu_index == 1) {
                            hair_color = (hair_color + hair_color_count - 1) % hair_color_count;
                        }
                    } else if (key == SDLK_RIGHT || key == SDLK_d) {
                        if (customize_menu_index == 0) {
                            hair_style = (hair_style + 1) % kHairStyleCount;
                        } else if (customize_menu_index == 1) {
                            hair_color = (hair_color + 1) % hair_color_count;
                        }
                    } else if (key == SDLK_ESCAPE) {
                        save_progress();
                        state = GameState::MainMenu;
                    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        if (customize_menu_index == 0) {
                            int cost = hair_style_costs[hair_style];
                            int mask = 1 << hair_style;
                            if ((hair_unlock_mask & mask) != 0) {
                                save_progress();
                            } else if (coins >= cost) {
                                coins -= cost;
                                hair_unlock_mask |= mask;
                                save_progress();
                            }
                        } else if (customize_menu_index == 1) {
                            save_progress();
                        } else if (customize_menu_index == 2) {
                            state = GameState::MainMenu;
                        }
                    }
                } else if (state == GameState::PowerUps) {
                    int option_count = 11;
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
                            int cost = 10 + 5 * meta_magnet_level;
                            if (meta_magnet_level < 10 && coins >= cost) {
                                coins -= cost;
                                meta_magnet_level += 1;
                                save_progress();
                            }
                        } else if (index == 5) {
                            int cost = 80;
                            if (!unlock_cross && coins >= cost) {
                                coins -= cost;
                                unlock_cross = true;
                                save_progress();
                            }
                        } else if (index == 6) {
                            int cost = 60;
                            if (!unlock_stick && coins >= cost) {
                                coins -= cost;
                                unlock_stick = true;
                                save_progress();
                            }
                        } else if (index == 7) {
                            int cost = 70;
                            if (!unlock_crossbow && coins >= cost) {
                                coins -= cost;
                                unlock_crossbow = true;
                                save_progress();
                            }
                        } else if (index == 8) {
                            int cost = 90;
                            if (!unlock_poison && coins >= cost) {
                                coins -= cost;
                                unlock_poison = true;
                                save_progress();
                            }
                        } else if (index == 9) {
                            int cost = 50;
                            if (!unlock_bat && coins >= cost) {
                                coins -= cost;
                                unlock_bat = true;
                                save_progress();
                            }
                        } else if (index == 10) {
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
                    if (key == SDLK_v) {
                        command_units(UnitState::Follow, -1);
                    } else if (key == SDLK_h) {
                        command_units(UnitState::Home, -1);
                    } else if (key == SDLK_g) {
                        int guard_index = find_nearest_player_building(player_position);
                        if (guard_index >= 0) {
                            command_units(UnitState::Guard, guard_index);
                        }
                    } else if (key == SDLK_F1) {
                        house_wireframe_debug = !house_wireframe_debug;
                    } else if (key == SDLK_F2) {
                        spawn_debug = !spawn_debug;
                    } else if (key == SDLK_c) {
                        if (selected_ally_id != -1) {
                            int platform_index = find_platform_index_for_position(player_position);
                            for (Ally& ally : allies) {
                                if (!ally.alive || ally.id != selected_ally_id) {
                                    continue;
                                }
                                ally.order = OrderType::CaptureHomes;
                                ally.order_platform_index = platform_index;
                                ally.order_target_home = find_nearest_uncaptured_home(platform_index, ally.position);
                                if (ally.order_target_home == -1 && platform_index == -1) {
                                    float best_dist = 40.0f * 40.0f;
                                    int best_index = -1;
                                    for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
                                        if (buildings[i].owner == BuildingOwner::Player) {
                                            continue;
                                        }
                                        glm::vec2 delta = buildings[i].center - glm::vec2(ally.position.x, ally.position.z);
                                        float dist = glm::dot(delta, delta);
                                        if (dist < best_dist) {
                                            best_dist = dist;
                                            best_index = i;
                                        }
                                    }
                                    if (best_index >= 0) {
                                        ally.order_target_home = best_index;
                                        ally.order_platform_index = buildings[best_index].platform_index;
                                    }
                                }
                                if (ally.order_target_home == -1) {
                                    ally.order = OrderType::None;
                                    ally.order_platform_index = -1;
                                }
                                break;
                            }
                        }
                    } else if (key == SDLK_t) {
                        float best_dist = 2.8f * 2.8f;
                        int best_index = -1;
                        for (int i = 0; i < static_cast<int>(towns.size()); ++i) {
                            glm::vec3 delta = towns[i].position - player_position;
                            delta.y = 0.0f;
                            float dist = glm::dot(delta, delta);
                            if (dist < best_dist) {
                                best_dist = dist;
                                best_index = i;
                            }
                        }
                        if (best_index >= 0) {
                            bool next_state = !towns[best_index].ui_open;
                            for (Town& town : towns) {
                                town.ui_open = false;
                            }
                            towns[best_index].ui_open = next_state;
                        }
                    } else if (key == SDLK_1 || key == SDLK_2 || key == SDLK_3) {
                        int open_town_index = -1;
                        for (int i = 0; i < static_cast<int>(towns.size()); ++i) {
                            if (towns[i].ui_open) {
                                open_town_index = i;
                                break;
                            }
                        }
                        if (open_town_index >= 0) {
                            Town& town = towns[open_town_index];
                            int key_index = key == SDLK_1 ? 0 : (key == SDLK_2 ? 1 : 2);
                            int barracks_cost = 6 + town.barracks_level * 6;
                            int armory_cost = 8 + town.armory_level * 8;
                            int watch_cost = 10 + town.watchtower_level * 10;
                            if (key_index == 0 && points >= barracks_cost) {
                                points -= barracks_cost;
                                town.barracks_level += 1;
                            } else if (key_index == 1 && points >= armory_cost) {
                                points -= armory_cost;
                                town.armory_level += 1;
                            } else if (key_index == 2 && points >= watch_cost) {
                                points -= watch_cost;
                                town.watchtower_level += 1;
                            }
                        }
                    } else if (key == SDLK_ESCAPE) {
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
                            if (garlic_level <= 0) {
                                garlic_level = 1;
                                attack_radius = 2.2f;
                            } else {
                                garlic_level += 1;
                            }
                            attack_damage += 1;
                            attack_radius *= 1.1f;
                            break;
                        case ItemId::Magnet:
                            magnet_level += 1;
                            pickup_magnet_radius = 1.2f * std::pow(1.15f,
                                static_cast<float>(meta_magnet_level + magnet_level));
                            break;
                        case ItemId::Cross:
                            cross_level = 1;
                            cross_damage = 2 + meta_damage_level;
                            cross_tick = 0.45f;
                            cross_orbit_radius = 1.6f;
                            cross_hit_radius = 0.7f;
                            break;
                        case ItemId::CrossUpgrade:
                            cross_level += 1;
                            cross_damage += 1;
                            cross_orbit_radius += 0.15f;
                            break;
                        case ItemId::Stick:
                            stick_level = 1;
                            stick_damage = 2 + meta_damage_level;
                            stick_cooldown = 0.9f;
                            stick_range = 1.8f;
                            break;
                        case ItemId::StickUpgrade:
                            stick_level += 1;
                            stick_damage += 1;
                            stick_cooldown = glm::max(0.35f, stick_cooldown - 0.08f);
                            stick_range += 0.1f;
                            break;
                        case ItemId::Crossbow:
                            crossbow_level = 1;
                            crossbow_damage = 2 + meta_damage_level;
                            crossbow_cooldown = 1.2f;
                            break;
                        case ItemId::CrossbowUpgrade:
                            crossbow_level += 1;
                            crossbow_damage += 1;
                            crossbow_cooldown = glm::max(0.4f, crossbow_cooldown - 0.08f);
                            break;
                        case ItemId::HolyWater:
                            holywater_level = 1;
                            holywater_damage = 1 + meta_damage_level / 2;
                            holywater_radius = 2.0f;
                            holywater_cooldown = 2.4f;
                            break;
                        case ItemId::HolyWaterUpgrade:
                            holywater_level += 1;
                            holywater_damage += 1;
                            holywater_radius += 0.25f;
                            holywater_cooldown = glm::max(1.2f, holywater_cooldown - 0.15f);
                            break;
                        case ItemId::PoisonBomb:
                            poison_level = 1;
                            poison_damage = 1 + meta_damage_level / 2;
                            poison_radius = 2.4f;
                            poison_cooldown = 3.0f;
                            break;
                        case ItemId::PoisonBombUpgrade:
                            poison_level += 1;
                            poison_damage += 1;
                            poison_radius += 0.25f;
                            poison_cooldown = glm::max(1.3f, poison_cooldown - 0.18f);
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
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_RIGHT && state == GameState::Running) {
                    camera_look_active = true;
                } else if (event.button.button == SDL_BUTTON_LEFT &&
                           state == GameState::Running && !camera_look_active) {
                    selected_ally_id = pick_friendly_unit(event.button.x, event.button.y);
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    camera_look_active = false;
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (state == GameState::Running && camera_look_active) {
                    const float sensitivity = 0.0018f;
                    camera_yaw += static_cast<float>(event.motion.xrel) * sensitivity;
                    camera_pitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                    camera_pitch = glm::clamp(camera_pitch, glm::radians(-45.0f), glm::radians(10.0f));
                }
            } else if (event.type == SDL_MOUSEWHEEL && state == GameState::Running) {
                camera_distance -= static_cast<float>(event.wheel.y) * 0.6f;
                camera_distance = glm::clamp(camera_distance, 4.0f, 16.0f);
            }
        }

        if (state != GameState::Running) {
            camera_look_active = false;
            player_move_amount = 0.0f;
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
            float move_len = glm::length(move);
            if (move_len > 0.0f) {
                move = glm::normalize(move);
                player_aim_dir = move;
            } else {
                player_aim_dir = forward;
            }
            player_move_amount = glm::clamp(move_len, 0.0f, 1.0f);
            if (jump_cooldown_timer > 0.0f) {
                jump_cooldown_timer = glm::max(0.0f, jump_cooldown_timer - delta_time);
            }
            if (jump_timer <= 0.0f && jump_cooldown_timer <= 0.0f &&
                (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_LSHIFT])) {
                jump_timer = jump_duration;
                jump_cooldown_timer = jump_cooldown;
            }
            glm::vec3 prev_player_position = player_position;
            glm::vec3 desired = player_position + move * player_speed * delta_time;
            desired.x = glm::clamp(desired.x, -play_area_extent, play_area_extent);
            desired.z = glm::clamp(desired.z, -play_area_extent, play_area_extent);
            glm::vec2 current_xz(player_position.x, player_position.z);
            glm::vec2 desired_xz(desired.x, desired.z);
            desired.y = TerrainHeightAt(desired.x, desired.z, player_position.y);

            const float player_radius = 0.6f;
            float current_height = TerrainHeightAt(current_xz.x, current_xz.y, player_position.y);
            float desired_height = TerrainHeightAt(desired_xz.x, desired_xz.y, player_position.y);
            bool jump_active = jump_timer > 0.0f || jump_release_timer > 0.0f;
            bool blocked = !can_step(current_xz, desired_xz, max_step_height, player_position.y);
            if (blocked && jump_active) {
                float jump_step_height = 8.0f;
                bool near_edge = IsNearRampEdge(current_xz, 4.0f) || IsNearRampEdge(desired_xz, 4.0f);
                if (desired_height <= current_height + 0.5f) {
                    blocked = false;
                } else if (near_edge && can_step(current_xz, desired_xz, jump_step_height, player_position.y)) {
                    blocked = false;
                } else if (RampAllowsTransition(current_xz, desired_xz, ramp_entry_margin)) {
                    blocked = false;
                }
            }
            for (const Obstacle& box : obstacles) {
                if (!obstacle_active(box, player_position.y)) {
                    continue;
                }
                if (jump_active && box.is_ramp_wall) {
                    continue;
                }
                if (circle_intersects_aabb(desired_xz, player_radius, box)) {
                    blocked = true;
                    break;
                }
            }
                if (!blocked) {
                    player_position.x = desired.x;
                    player_position.z = desired.z;
                } else {
                    glm::vec3 slide_x = player_position + glm::vec3(move.x, 0.0f, 0.0f) * player_speed * delta_time;
                    slide_x.x = glm::clamp(slide_x.x, -play_area_extent, play_area_extent);
                    glm::vec2 slide_xz(slide_x.x, player_position.z);
                    float slide_x_height = TerrainHeightAt(slide_xz.x, slide_xz.y, player_position.y);
                    bool blocked_x = !can_step(current_xz, slide_xz, max_step_height, player_position.y);
                    if (blocked_x && jump_active) {
                        float jump_step_height = 8.0f;
                        bool near_edge = IsNearRampEdge(current_xz, 4.0f) || IsNearRampEdge(slide_xz, 4.0f);
                        if (slide_x_height <= current_height + 0.5f) {
                            blocked_x = false;
                        } else if (near_edge && can_step(current_xz, slide_xz, jump_step_height, player_position.y)) {
                            blocked_x = false;
                        } else if (RampAllowsTransition(current_xz, slide_xz, ramp_entry_margin)) {
                            blocked_x = false;
                        }
                    }
                for (const Obstacle& box : obstacles) {
                    if (!obstacle_active(box, player_position.y)) {
                        continue;
                    }
                    if (jump_timer > 0.0f && box.is_ramp_wall) {
                        continue;
                    }
                    if (circle_intersects_aabb(slide_xz, player_radius, box)) {
                        blocked_x = true;
                        break;
                    }
                }
                    if (!blocked_x) {
                        player_position.x = slide_x.x;
                    }

                    glm::vec3 slide_z = player_position + glm::vec3(0.0f, 0.0f, move.z) * player_speed * delta_time;
                    slide_z.z = glm::clamp(slide_z.z, -play_area_extent, play_area_extent);
                    glm::vec2 slide_zz(player_position.x, slide_z.z);
                    float slide_z_height = TerrainHeightAt(slide_zz.x, slide_zz.y, player_position.y);
                    bool blocked_z = !can_step(current_xz, slide_zz, max_step_height, player_position.y);
                    if (blocked_z && jump_active) {
                        float jump_step_height = 8.0f;
                        bool near_edge = IsNearRampEdge(current_xz, 4.0f) || IsNearRampEdge(slide_zz, 4.0f);
                        if (slide_z_height <= current_height + 0.5f) {
                            blocked_z = false;
                        } else if (near_edge && can_step(current_xz, slide_zz, jump_step_height, player_position.y)) {
                            blocked_z = false;
                        } else if (RampAllowsTransition(current_xz, slide_zz, ramp_entry_margin)) {
                            blocked_z = false;
                        }
                    }
                for (const Obstacle& box : obstacles) {
                    if (!obstacle_active(box, player_position.y)) {
                        continue;
                    }
                    if (jump_timer > 0.0f && box.is_ramp_wall) {
                        continue;
                    }
                    if (circle_intersects_aabb(slide_zz, player_radius, box)) {
                        blocked_z = true;
                        break;
                    }
                }
                    if (!blocked_z) {
                        player_position.z = slide_z.z;
                    }
                }
                float target_y = TerrainHeightAt(player_position.x, player_position.z, player_position.y);
                player_position.y = glm::mix(player_position.y, target_y, 0.35f);
                if (delta_time > 0.0f) {
                    player_velocity = (player_position - prev_player_position) / delta_time;
                } else {
                    player_velocity = glm::vec3(0.0f);
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

            enemy_spawner_timer += delta_time;
            if (enemy_spawner_timer >= enemy_spawner_interval) {
                enemy_spawner_timer = 0.0f;
                assign_enemy_spawners(1);
            }

            for (Building& building : buildings) {
                if (building.owner == BuildingOwner::Player) {
                    building.veteran_timer += delta_time;
                }
                if (building.owner == BuildingOwner::Enemy && building.spawner) {
                    building.alive_timer += delta_time;
                    building.spawn_timer += delta_time;
                    float spawn_interval_building = glm::max(2.5f, 4.8f - run_time * 0.02f);
                    if (building.spawn_timer >= spawn_interval_building) {
                        building.spawn_timer = 0.0f;
                        int spawn_count = building.alive_timer >= 120.0f ? 3 : 2;
                        float spawn_radius = building.size * 0.7f + 1.2f;
                        for (int s = 0; s < spawn_count; ++s) {
                            float angle = unit_dist(rng) * glm::two_pi<float>();
                            glm::vec2 offset(std::cos(angle), std::sin(angle));
                            glm::vec3 spawn_pos(building.center.x + offset.x * spawn_radius,
                                                building.base_height,
                                                building.center.y + offset.y * spawn_radius);
                    bool gold = building.alive_timer >= 120.0f;
                    glm::vec3 color = gold ? glm::vec3(0.95f, 0.85f, 0.30f)
                                           : glm::vec3(0.95f, 0.20f, 0.20f);
                            spawn_enemy_at(spawn_pos, gold ? 3 : 2, color, gold);
                        }
                    }
                }
            }

            {
                float capture_radius = glm::max(attack_radius, 2.2f);
                float capture_radius_sq = capture_radius * capture_radius;
                for (Building& building : buildings) {
                    if (building.owner == BuildingOwner::Player) {
                        continue;
                    }
                    if (building.platform_index < 0 ||
                        building.platform_index >= static_cast<int>(platforms.size()) ||
                        platforms[building.platform_index].state != PlatformState::Active) {
                        continue;
                    }
                    int contributors = 0;
                    if (std::abs(player_position.y - building.base_height) <= 2.0f) {
                        glm::vec2 delta = building.center - glm::vec2(player_position.x, player_position.z);
                        float dist_sq = glm::dot(delta, delta);
                        if (dist_sq <= capture_radius_sq) {
                            contributors += 1;
                        }
                    }
                    for (const Ally& ally : allies) {
                        if (!ally.alive) {
                            continue;
                        }
                        if (std::abs(ally.position.y - building.base_height) > 2.0f) {
                            continue;
                        }
                        glm::vec2 delta = building.center - glm::vec2(ally.position.x, ally.position.z);
                        float dist_sq = glm::dot(delta, delta);
                        if (dist_sq <= capture_radius_sq) {
                            contributors += 1;
                        }
                    }
                    if (contributors > 0) {
                        building.capture_timer += delta_time * static_cast<float>(contributors);
                    } else {
                        building.capture_timer = glm::max(0.0f, building.capture_timer - delta_time * 1.5f);
                    }
                    if (building.capture_timer >= 5.0f) {
                        building.owner = BuildingOwner::Player;
                        building.spawner = false;
                        building.capture_timer = 0.0f;
                        building.veteran_timer = 0.0f;
                        building.alive_timer = 0.0f;
                        building.max_health = 10;
                        building.health = building.max_health;
                        capture_waves.push_back(
                            CaptureWave{glm::vec3(building.center.x, building.base_height + 0.05f, building.center.y), 0.0f, 0.8f});
                        rebuild_obstacles();
                    }
                }
            }

            for (Platform& platform : platforms) {
                if (platform.state != PlatformState::Active) {
                    continue;
                }
                if (platform.home_indices.empty()) {
                    continue;
                }
                bool all_captured = true;
                for (int index : platform.home_indices) {
                    if (buildings[index].owner != BuildingOwner::Player) {
                        all_captured = false;
                        break;
                    }
                }
            if (all_captured) {
                platform.state = PlatformState::Collapsing;
                platform.collapse_timer = 0.0f;
                platform.landed_timer = 0.0f;
                platform.fall_offset = 0.0f;
            }
        }

            for (int p = 0; p < static_cast<int>(platforms.size()); ++p) {
                Platform& platform = platforms[p];
                if (platform.state == PlatformState::Collapsing) {
                    platform.collapse_timer += delta_time;
                    float t = glm::clamp(platform.collapse_timer / platform.collapse_duration, 0.0f, 1.0f);
                    float ease = t * t * (3.0f - 2.0f * t);
                    platform.fall_offset = platform.fall_distance * ease;
                    if (platform.plateau_index >= 0 &&
                        platform.plateau_index < static_cast<int>(g_plateau_fall_offsets.size())) {
                        g_plateau_fall_offsets[platform.plateau_index] = platform.fall_offset;
                    }
                    if (platform.collapse_timer >= platform.collapse_duration) {
                        platform.state = PlatformState::Landed;
                        platform.landed_timer = 0.0f;
                        platform.fall_offset = platform.fall_distance;
                        if (platform.plateau_index >= 0 &&
                            platform.plateau_index < static_cast<int>(g_plateau_fall_offsets.size())) {
                            g_plateau_fall_offsets[platform.plateau_index] = platform.fall_offset;
                        }
                        if (platform.plateau_index >= 0 &&
                            platform.plateau_index < static_cast<int>(g_plateaus.size())) {
                            g_plateaus[platform.plateau_index].active = false;
                            for (RampTile& ramp : g_ramps) {
                                if (ramp.plateau_index == platform.plateau_index) {
                                    ramp.active = false;
                                }
                            }
                            rebuild_ground();
                            rebuild_ramp_walls();
                            rebuild_obstacles();
                        }
                    }
                } else if (platform.state == PlatformState::Landed) {
                    platform.landed_timer += delta_time;
                    if (platform.landed_timer >= 0.35f) {
                        spawn_town_for_platform(p);
                        platform.state = PlatformState::ConvertedToTown;
                    }
                }
            }

            for (Town& town : towns) {
                town.production_timer += delta_time;
            }

            int total_armory_level = 0;
            for (const Town& town : towns) {
                total_armory_level += town.armory_level;
            }
            int ally_damage_bonus = total_armory_level;
            int ally_health_bonus = total_armory_level;
            for (Town& town : towns) {
                if (town.barracks_level > 0) {
                    town.barracks_timer += delta_time;
                    float interval = glm::max(4.0f, 12.0f - 2.0f * static_cast<float>(town.barracks_level - 1));
                    if (town.barracks_timer >= interval) {
                        town.barracks_timer = 0.0f;
                        Ally ally;
                        glm::vec2 spawn_center(town.position.x, town.position.z);
                        float spawn_y = town.position.y;
                        const Building* barracks_building = nullptr;
                        if (town.platform_index >= 0 &&
                            town.platform_index < static_cast<int>(platforms.size())) {
                            const Platform& platform = platforms[town.platform_index];
                            spawn_y = platform.state == PlatformState::ConvertedToTown
                                ? platform.ground_height
                                : platform.base_height - platform.fall_offset;
                            for (int index : platform.home_indices) {
                                if (buildings[index].type == BuildingType::Barracks) {
                                    spawn_center = buildings[index].center;
                                    barracks_building = &buildings[index];
                                    break;
                                }
                            }
                        }
                        glm::vec2 pos2d = spawn_center;
                        if (barracks_building) {
                            pos2d = pick_spawn_outside_building(*barracks_building, barracks_building->platform_index);
                        }
                        glm::vec3 spawn_pos(pos2d.x, spawn_y, pos2d.y);
                        spawn_pos.y = TerrainHeightAt(spawn_pos.x, spawn_pos.z, spawn_pos.y);
                        if (spawn_debug) {
                            last_spawn_debug_pos = glm::vec3(spawn_pos.x, spawn_pos.y + 0.1f, spawn_pos.z);
                            last_spawn_debug_platform = town.platform_index;
                        }
                        ally.position = spawn_pos;
                        ally.id = ally_id_counter++;
                        ally.home_index = -1;
                        ally.guard_index = -1;
                        ally.state = UnitState::Follow;
                        ally.order = OrderType::None;
                        ally.order_target_home = -1;
                        ally.order_platform_index = -1;
                        ally.max_health = 4 + ally_health_bonus;
                        ally.health = ally.max_health;
                        ally.gold = true;
                        allies.push_back(ally);
                    }
                }
                if (town.watchtower_level > 0) {
                    town.watchtower_timer += delta_time;
                    float interval = 1.1f / (1.0f + 0.35f * static_cast<float>(town.watchtower_level));
                    if (town.watchtower_timer >= interval) {
                        town.watchtower_timer = 0.0f;
                        float radius = 5.0f + 1.5f * static_cast<float>(town.watchtower_level);
                        float radius_sq = radius * radius;
                        int damage = 1 + town.watchtower_level;
                        glm::vec3 tower_pos = town.position;
                        if (town.platform_index >= 0 &&
                            town.platform_index < static_cast<int>(platforms.size())) {
                            const Platform& platform = platforms[town.platform_index];
                            for (int index : platform.home_indices) {
                                if (buildings[index].type == BuildingType::Watchtower) {
                                    float base_y = platform.state == PlatformState::ConvertedToTown
                                        ? platform.ground_height
                                        : platform.base_height - platform.fall_offset;
                                    tower_pos = glm::vec3(buildings[index].center.x, base_y, buildings[index].center.y);
                                    break;
                                }
                            }
                        }
                        Enemy* best_enemy = nullptr;
                        float best_dist = std::numeric_limits<float>::max();
                        for (Enemy& enemy : enemies) {
                            glm::vec3 delta = enemy.position - tower_pos;
                            delta.y = 0.0f;
                            float dist = glm::dot(delta, delta);
                            if (dist <= radius_sq && dist < best_dist) {
                                best_dist = dist;
                                best_enemy = &enemy;
                            }
                        }
                        if (best_enemy) {
                            glm::vec3 launch_pos = tower_pos + glm::vec3(0.0f, 1.6f, 0.0f);
                            glm::vec3 to_target = best_enemy->position + glm::vec3(0.0f, 0.3f, 0.0f) - launch_pos;
                            if (glm::length(to_target) < 0.001f) {
                                to_target = glm::vec3(0.0f, 0.0f, -1.0f);
                            }
                            glm::vec3 dir = glm::normalize(to_target);
                            projectiles.push_back(Projectile{
                                launch_pos,
                                dir * 10.0f,
                                2.2f,
                                damage,
                                glm::vec3(0.90f, 0.75f, 0.35f)});
                        }
                    }
                }
            }

            std::vector<int> alive_counts(buildings.size(), 0);
            std::vector<int> slot_counts(buildings.size(), 0);
            for (const Ally& ally : allies) {
                if (ally.home_index < 0 || ally.home_index >= static_cast<int>(buildings.size())) {
                    continue;
                }
                slot_counts[ally.home_index] += 1;
                if (ally.alive) {
                    alive_counts[ally.home_index] += 1;
                }
            }

            for (Ally& ally : allies) {
                if (ally.alive) {
                    continue;
                }
                if (ally.respawn_timer > 0.0f) {
                    ally.respawn_timer = glm::max(0.0f, ally.respawn_timer - delta_time);
                }
                if (ally.respawn_timer <= 0.0f &&
                    ally.home_index >= 0 &&
                    ally.home_index < static_cast<int>(buildings.size())) {
                    Building& home = buildings[ally.home_index];
                    if (home.platform_index < 0 ||
                        home.platform_index >= static_cast<int>(platforms.size()) ||
                        platforms[home.platform_index].state != PlatformState::Active) {
                        continue;
                    }
                    int max_units = home.veteran_timer >= 60.0f ? 2 : 1;
                    if (home.owner == BuildingOwner::Player &&
                        alive_counts[ally.home_index] < max_units) {
                        ally.alive = true;
                        ally.health = ally.max_health;
                        ally.position = ally_spawn_position(home);
                        ally.state = UnitState::Home;
                        ally.guard_index = -1;
                        alive_counts[ally.home_index] += 1;
                    }
                }
            }

            for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
                Building& building = buildings[i];
                if (building.owner != BuildingOwner::Player) {
                    continue;
                }
                if (building.platform_index < 0 ||
                    building.platform_index >= static_cast<int>(platforms.size()) ||
                    platforms[building.platform_index].state != PlatformState::Active) {
                    continue;
                }
                int max_units = building.veteran_timer >= 60.0f ? 2 : 1;
                while (slot_counts[i] < max_units) {
                    Ally ally;
                    ally.position = ally_spawn_position(building);
                    ally.id = ally_id_counter++;
                    ally.home_index = i;
                    ally.guard_index = -1;
                    ally.state = UnitState::Home;
                    ally.max_health = 4;
                    ally.health = ally.max_health;
                    allies.push_back(ally);
                    slot_counts[i] += 1;
                    alive_counts[i] += 1;
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
                        float player_height = TerrainHeightAt(player_position.x, player_position.z, player_position.y);
                        float enemy_height = TerrainHeightAt(enemy.position.x, enemy.position.z, enemy.position.y);
                        glm::vec3 target = player_position;
                        bool using_ramp = false;
                        const RampTile* chosen_ramp = nullptr;
                        if (std::abs(player_height - enemy_height) > 1.5f) {
                            float best_dist = std::numeric_limits<float>::max();
                            bool enemy_below = enemy_height < player_height;
                            for (const RampTile& ramp : g_ramps) {
                                RampEndpoints endpoints = GetRampEndpoints(ramp);
                                float ramp_top = ramp.base_height + ramp.height;
                                float ramp_base = ramp.base_height;
                                if (enemy_below) {
                                    if (std::abs(ramp_top - player_height) > 1.5f) {
                                        continue;
                                    }
                                    glm::vec2 enemy_to_base = endpoints.base - glm::vec2(enemy.position.x, enemy.position.z);
                                    glm::vec2 player_to_top = endpoints.top - glm::vec2(player_position.x, player_position.z);
                                    float score = glm::dot(enemy_to_base, enemy_to_base) +
                                                  0.6f * glm::dot(player_to_top, player_to_top);
                                    if (score < best_dist) {
                                        best_dist = score;
                                        target = glm::vec3(endpoints.base.x, enemy.position.y, endpoints.base.y);
                                        using_ramp = true;
                                        chosen_ramp = &ramp;
                                    }
                                } else {
                                    if (std::abs(ramp_base - player_height) > 1.5f) {
                                        continue;
                                    }
                                    glm::vec2 enemy_to_top = endpoints.top - glm::vec2(enemy.position.x, enemy.position.z);
                                    glm::vec2 player_to_base = endpoints.base - glm::vec2(player_position.x, player_position.z);
                                    float score = glm::dot(enemy_to_top, enemy_to_top) +
                                                  0.6f * glm::dot(player_to_base, player_to_base);
                                    if (score < best_dist) {
                                        best_dist = score;
                                        target = glm::vec3(endpoints.top.x, enemy.position.y, endpoints.top.y);
                                        using_ramp = true;
                                        chosen_ramp = &ramp;
                                    }
                                }
                            }
                            glm::vec2 to_target_2d = glm::vec2(target.x - enemy.position.x,
                                                               target.z - enemy.position.z);
                            if (chosen_ramp) {
                                RampEndpoints endpoints = GetRampEndpoints(*chosen_ramp);
                                if (enemy_below && glm::dot(to_target_2d, to_target_2d) <= 1.2f * 1.2f) {
                                    target = glm::vec3(endpoints.top.x, enemy.position.y, endpoints.top.y);
                                    using_ramp = true;
                                }
                            }
                            if (IsOnRamp(glm::vec2(enemy.position.x, enemy.position.z)) ||
                                std::abs(player_height - enemy_height) <= 1.5f) {
                                target = player_position;
                                using_ramp = false;
                            }
                        }
                        glm::vec3 to_target = target - enemy.position;
                        to_target.y = 0.0f;
                        float target_dist = glm::length(to_target);
                        if (target_dist <= 0.001f) {
                            continue;
                        }
                        glm::vec3 direction = to_target / target_dist;
                        glm::vec3 desired_enemy = enemy.position + direction * enemy.speed * delta_time;
                        glm::vec2 desired_enemy_xz(desired_enemy.x, desired_enemy.z);
                        glm::vec2 enemy_xz(enemy.position.x, enemy.position.z);
                        bool blocked_enemy = !can_step(enemy_xz, desired_enemy_xz, max_step_height, enemy.position.y);
                        for (const Obstacle& box : obstacles) {
                            if (!obstacle_active(box, enemy.position.y)) {
                                continue;
                            }
                            if (box.is_ramp_wall && (using_ramp || IsNearRampArea(desired_enemy_xz, 0.6f))) {
                                continue;
                            }
                            if (circle_intersects_aabb(desired_enemy_xz, enemy.scale * 0.55f, box)) {
                                blocked_enemy = true;
                                break;
                            }
                        }
                        if (!blocked_enemy) {
                            enemy.position.x = desired_enemy.x;
                            enemy.position.z = desired_enemy.z;
                        } else {
                            glm::vec3 slide_x = enemy.position + glm::vec3(direction.x, 0.0f, 0.0f)
                                                * enemy.speed * delta_time;
                            glm::vec2 slide_xz(slide_x.x, enemy.position.z);
                            bool blocked_x = !can_step(enemy_xz, slide_xz, max_step_height, enemy.position.y);
                            for (const Obstacle& box : obstacles) {
                                if (!obstacle_active(box, enemy.position.y)) {
                                    continue;
                                }
                                if (box.is_ramp_wall && (using_ramp || IsNearRampArea(slide_xz, 0.6f))) {
                                    continue;
                                }
                                if (circle_intersects_aabb(slide_xz, enemy.scale * 0.55f, box)) {
                                    blocked_x = true;
                                    break;
                                }
                            }
                            if (!blocked_x) {
                                enemy.position.x = slide_x.x;
                            } else {
                                glm::vec3 slide_z = enemy.position + glm::vec3(0.0f, 0.0f, direction.z)
                                                    * enemy.speed * delta_time;
                                glm::vec2 slide_zz(enemy.position.x, slide_z.z);
                                bool blocked_z = !can_step(enemy_xz, slide_zz, max_step_height, enemy.position.y);
                                for (const Obstacle& box : obstacles) {
                                    if (!obstacle_active(box, enemy.position.y)) {
                                        continue;
                                    }
                                    if (box.is_ramp_wall && (using_ramp || IsNearRampArea(slide_zz, 0.6f))) {
                                        continue;
                                    }
                                    if (circle_intersects_aabb(slide_zz, enemy.scale * 0.55f, box)) {
                                        blocked_z = true;
                                        break;
                                    }
                                }
                                if (!blocked_z) {
                                    enemy.position.z = slide_z.z;
                                }
                            }
                        }
                        float enemy_target_y = TerrainHeightAt(enemy.position.x, enemy.position.z, enemy.position.y);
                        enemy.position.y = glm::mix(enemy.position.y, enemy_target_y, 0.35f);
                    }
                }
            }

            for (Ally& ally : allies) {
                if (!ally.alive) {
                    continue;
                }
                glm::vec3 target = ally.position;
                float follow_jump_bonus = 0.0f;
                bool ally_jump_active = false;
                bool using_capture_order = false;
                if (ally.order == OrderType::CaptureHomes) {
                    if (ally.order_platform_index < 0 ||
                        ally.order_platform_index >= static_cast<int>(platforms.size()) ||
                        platforms[ally.order_platform_index].state != PlatformState::Active) {
                        ally.order = OrderType::None;
                        ally.order_target_home = -1;
                        ally.order_platform_index = -1;
                    } else {
                        if (ally.order_target_home < 0 ||
                            ally.order_target_home >= static_cast<int>(buildings.size()) ||
                            buildings[ally.order_target_home].owner == BuildingOwner::Player ||
                            buildings[ally.order_target_home].platform_index != ally.order_platform_index) {
                            ally.order_target_home =
                                find_nearest_uncaptured_home(ally.order_platform_index, ally.position);
                        }
                        if (ally.order_target_home >= 0) {
                            const Building& home = buildings[ally.order_target_home];
                            target = glm::vec3(home.center.x, ally.position.y, home.center.y);
                            using_capture_order = true;
                        } else {
                            ally.order = OrderType::None;
                            ally.order_platform_index = -1;
                        }
                    }
                }
                if (!using_capture_order) {
                    if (ally.state == UnitState::Follow) {
                        glm::vec2 offset = ally_ring_offset(ally.id, attack_radius * 0.75f);
                        target = player_position + glm::vec3(offset.x, 0.0f, offset.y);
                        follow_jump_bonus = jump_timer > 0.0f ? 1.0f : 0.0f;
                        ally_jump_active = (jump_timer > 0.0f || jump_release_timer > 0.0f);
                    } else if (ally.state == UnitState::Guard) {
                        if (ally.guard_index >= 0 &&
                            ally.guard_index < static_cast<int>(buildings.size()) &&
                            buildings[ally.guard_index].owner == BuildingOwner::Player) {
                            glm::vec2 offset = ally_ring_offset(ally.id, buildings[ally.guard_index].size * 0.65f);
                            target = glm::vec3(buildings[ally.guard_index].center.x + offset.x,
                                               ally.position.y,
                                               buildings[ally.guard_index].center.y + offset.y);
                        } else if (ally.home_index >= 0 &&
                                   ally.home_index < static_cast<int>(buildings.size())) {
                            glm::vec2 offset = ally_ring_offset(ally.id, buildings[ally.home_index].size * 0.65f);
                            target = glm::vec3(buildings[ally.home_index].center.x + offset.x,
                                               ally.position.y,
                                               buildings[ally.home_index].center.y + offset.y);
                            ally.state = UnitState::Home;
                        }
                    } else if (ally.home_index >= 0 &&
                               ally.home_index < static_cast<int>(buildings.size())) {
                        glm::vec2 offset = ally_ring_offset(ally.id, buildings[ally.home_index].size * 0.65f);
                        target = glm::vec3(buildings[ally.home_index].center.x + offset.x,
                                           ally.position.y,
                                           buildings[ally.home_index].center.y + offset.y);
                    }
                }

                bool militia = false;
                for (const Building& building : buildings) {
                    if (building.owner != BuildingOwner::Player) {
                        continue;
                    }
                    glm::vec2 delta = building.center - glm::vec2(ally.position.x, ally.position.z);
                    if (glm::dot(delta, delta) <= 36.0f) {
                        militia = true;
                        break;
                    }
                }

                float ally_speed = militia ? 3.6f : 3.0f;
                int ally_damage = (militia ? 3 : 2) + ally_damage_bonus;
                int desired_max_health = 4 + ally_health_bonus;
                if (ally.max_health < desired_max_health) {
                    int delta = desired_max_health - ally.max_health;
                    ally.max_health = desired_max_health;
                    ally.health += delta;
                }
                glm::vec3 to_target = target - ally.position;
                to_target.y = 0.0f;
                float dist = glm::length(to_target);
                if (dist > 0.1f) {
                    glm::vec3 dir = to_target / dist;
                    glm::vec3 desired = ally.position + dir * ally_speed * delta_time;
                    glm::vec2 desired_xz(desired.x, desired.z);
                    glm::vec2 ally_xz(ally.position.x, ally.position.z);
                    float step_limit = max_step_height + follow_jump_bonus;
                    bool blocked = !can_step(ally_xz, desired_xz, step_limit, ally.position.y);
                    for (const Obstacle& box : obstacles) {
                        if (!obstacle_active(box, ally.position.y)) {
                            continue;
                        }
                        if (box.is_building && box.owner == BuildingOwner::Player) {
                            continue;
                        }
                        if (ally_jump_active && box.is_ramp_wall) {
                            continue;
                        }
                        if (circle_intersects_aabb(desired_xz, 0.45f, box)) {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) {
                        ally.position.x = desired.x;
                        ally.position.z = desired.z;
                    }
                }
                float target_y = TerrainHeightAt(ally.position.x, ally.position.z, ally.position.y);
                if (follow_jump_bonus > 0.0f) {
                    target_y += 0.6f;
                }
                ally.position.y = glm::mix(ally.position.y, target_y, 0.35f);

                if (ally.attack_timer > 0.0f) {
                    ally.attack_timer = glm::max(0.0f, ally.attack_timer - delta_time);
                }
                if (ally.attack_timer <= 0.0f) {
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - ally.position;
                        if (glm::length(delta) <= 1.2f) {
                            enemy.health -= ally_damage;
                            ally.attack_timer = 0.6f;
                            break;
                        }
                    }
                }

                if (ally.hurt_timer > 0.0f) {
                    ally.hurt_timer = glm::max(0.0f, ally.hurt_timer - delta_time);
                }
                if (ally.hurt_timer <= 0.0f) {
                    for (const Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - ally.position;
                        if (glm::length(delta) <= 0.9f) {
                            ally.health -= enemy.damage;
                            ally.hurt_timer = 0.8f;
                            break;
                        }
                    }
                }
                if (ally.health <= 0) {
                    ally.alive = false;
                    ally.respawn_timer = 20.0f;
                }
            }
            if (selected_ally_id != -1) {
                bool selected_alive = false;
                for (const Ally& ally : allies) {
                    if (ally.alive && ally.id == selected_ally_id) {
                        selected_alive = true;
                        break;
                    }
                }
                if (!selected_alive) {
                    selected_ally_id = -1;
                }
            }

            float prev_jump_timer = jump_timer;
            if (jump_timer > 0.0f) {
                jump_timer = glm::max(0.0f, jump_timer - delta_time);
            }
            if (prev_jump_timer > 0.0f && jump_timer <= 0.0f) {
                jump_release_timer = 0.12f;
            }
            if (jump_release_timer > 0.0f) {
                jump_release_timer = glm::max(0.0f, jump_release_timer - delta_time);
            }
            if (nuke_flash_timer > 0.0f) {
                nuke_flash_timer = glm::max(0.0f, nuke_flash_timer - delta_time);
            }
            if (stick_flash_timer > 0.0f) {
                stick_flash_timer = glm::max(0.0f, stick_flash_timer - delta_time);
            }
            if (player_hurt_timer > 0.0f) {
                player_hurt_timer = glm::max(0.0f, player_hurt_timer - delta_time);
            }
            for (Enemy& enemy : enemies) {
                if (enemy.hit_flash > 0.0f) {
                    enemy.hit_flash = glm::max(0.0f, enemy.hit_flash - delta_time);
                }
            }

            auto damage_enemy = [&](Enemy& enemy, int dmg) {
                enemy.health -= dmg;
                enemy.hit_flash = 0.12f;
            };

            if (garlic_level > 0) {
                attack_timer += delta_time;
                if (attack_timer >= attack_interval) {
                    attack_timer = 0.0f;
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - player_position;
                        if (glm::length(delta) <= attack_radius) {
                            damage_enemy(enemy, attack_damage);
                        }
                    }
                    for (Building& building : buildings) {
                        if (building.owner != BuildingOwner::Enemy || !building.spawner) {
                            continue;
                        }
                        glm::vec2 delta = building.center - glm::vec2(player_position.x, player_position.z);
                        float dy = (building.base_height + 0.5f) - player_position.y;
                        if (glm::dot(delta, delta) + dy * dy <= attack_radius * attack_radius) {
                            building.health -= attack_damage;
                            if (building.health <= 0) {
                                building.owner = BuildingOwner::Neutral;
                                building.spawner = false;
                                building.health = building.max_health;
                                building.alive_timer = 0.0f;
                                building.capture_timer = 0.0f;
                                rebuild_obstacles();
                            }
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
                        fireball_damage,
                        glm::vec3(0.95f, 0.55f, 0.10f)});
                }
            }

            crossbow_timer += delta_time;
            if (crossbow_level > 0 && crossbow_timer >= crossbow_cooldown) {
                crossbow_timer = 0.0f;
                int projectile_count = 1 + (crossbow_level - 1) / 2;
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
                        direction * 8.5f,
                        2.5f,
                        crossbow_damage,
                        glm::vec3(0.70f, 0.45f, 0.20f)});
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
                glm::vec3 carry = glm::vec3(player_velocity.x, 0.0f, player_velocity.z);
                glm::vec3 launch_velocity = direction * 5.0f + carry * 0.75f;
                bombs.push_back(Bomb{
                    player_position + glm::vec3(0.0f, 0.6f, 0.0f),
                    launch_velocity,
                    0.8f});
            }

            stick_timer += delta_time;
            if (stick_level > 0 && stick_timer >= stick_cooldown) {
                stick_timer = 0.0f;
                stick_flash_timer = 0.12f;
                glm::vec3 forward = player_aim_dir;
                forward.y = 0.0f;
                if (glm::length(forward) < 0.001f) {
                    forward = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                forward = glm::normalize(forward);
                float cos_half = std::cos(glm::radians(35.0f));
                for (Enemy& enemy : enemies) {
                    glm::vec3 delta = enemy.position - player_position;
                    float dy = std::abs(delta.y);
                    glm::vec2 flat(delta.x, delta.z);
                    float dist = glm::length(flat);
                    if (dist <= stick_range && dy <= 1.2f) {
                        glm::vec3 dir = glm::normalize(glm::vec3(delta.x, 0.0f, delta.z));
                        if (glm::dot(dir, forward) >= cos_half) {
                            damage_enemy(enemy, stick_damage);
                        }
                    }
                }
            }

            cross_timer += delta_time;
            if (cross_level > 0 && cross_timer >= cross_tick) {
                cross_timer = 0.0f;
                for (int i = 0; i < cross_level; ++i) {
                    float angle = run_time * 2.2f + static_cast<float>(i) * glm::two_pi<float>() / glm::max(1, cross_level);
                    glm::vec3 orb = player_position + glm::vec3(std::cos(angle), 0.25f, std::sin(angle)) * cross_orbit_radius;
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - orb;
                        if (glm::length(delta) <= cross_hit_radius) {
                            damage_enemy(enemy, cross_damage);
                        }
                    }
                }
            }

            holywater_timer += delta_time;
            if (holywater_level > 0 && holywater_timer >= holywater_cooldown) {
                holywater_timer = 0.0f;
                glm::vec2 offset(std::cos(run_time * 1.3f), std::sin(run_time * 1.3f));
                glm::vec3 pos = player_position + glm::vec3(offset.x, 0.0f, offset.y) * 2.2f;
                pos.y = TerrainHeightAt(pos.x, pos.z, player_position.y) + 0.05f;
                ground_effects.push_back(GroundEffect{pos, holywater_radius, 0.0f, 3.5f, 0.0f, 0.35f,
                                                      holywater_damage, GroundEffectType::Holy});
            }

            poison_timer += delta_time;
            if (poison_level > 0 && poison_timer >= poison_cooldown) {
                poison_timer = 0.0f;
                glm::vec3 pos = player_position + player_aim_dir * 2.6f;
                pos.y = TerrainHeightAt(pos.x, pos.z, player_position.y) + 0.05f;
                ground_effects.push_back(GroundEffect{pos, poison_radius, 0.0f, 4.0f, 0.0f, 0.45f,
                                                      poison_damage, GroundEffectType::Poison});
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
                    ground_effects.push_back(GroundEffect{bombs[i].position, bomb_radius, 0.0f, 1.6f, 0.0f, 0.35f,
                                                          std::max(1, bomb_damage / 2), GroundEffectType::Holy});
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - bombs[i].position;
                        delta.y = 0.0f;
                        if (glm::length(delta) <= bomb_radius) {
                            damage_enemy(enemy, bomb_damage);
                        }
                    }
                    for (Building& building : buildings) {
                        if (building.owner != BuildingOwner::Enemy || !building.spawner) {
                            continue;
                        }
                        glm::vec2 delta = building.center - glm::vec2(bombs[i].position.x, bombs[i].position.z);
                        if (glm::dot(delta, delta) <= bomb_radius * bomb_radius) {
                            building.health -= bomb_damage;
                            if (building.health <= 0) {
                                building.owner = BuildingOwner::Neutral;
                                building.spawner = false;
                                building.health = building.max_health;
                                building.alive_timer = 0.0f;
                                building.capture_timer = 0.0f;
                                rebuild_obstacles();
                            }
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
                    if (glm::length(delta) <= 0.6f) {
                        damage_enemy(enemy, projectiles[i].damage);
                        hit = true;
                        break;
                    }
                }
                if (!hit) {
                    for (Building& building : buildings) {
                        if (building.owner != BuildingOwner::Enemy || !building.spawner) {
                            continue;
                        }
                        glm::vec2 delta = building.center - glm::vec2(projectiles[i].position.x, projectiles[i].position.z);
                        float dy = (building.base_height + 0.5f) - projectiles[i].position.y;
                        if (glm::dot(delta, delta) + dy * dy <= 0.6f * 0.6f) {
                            building.health -= projectiles[i].damage;
                            hit = true;
                            if (building.health <= 0) {
                                building.owner = BuildingOwner::Neutral;
                                building.spawner = false;
                                building.health = building.max_health;
                                building.alive_timer = 0.0f;
                                building.capture_timer = 0.0f;
                                rebuild_obstacles();
                            }
                            break;
                        }
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
                player_hurt_timer = 0.25f;
            }

            for (size_t i = 0; i < enemies.size();) {
                if (enemies[i].health <= 0) {
                    gems.push_back(ExperienceGem{enemies[i].position});
                    enemies_killed += 1;
                    coins_earned += enemies[i].elite ? 3 : 1;
                    points += enemies[i].elite ? 2 : 1;
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
                float dist = glm::length(delta);
                if (dist <= pickup_magnet_radius && dist > 0.001f) {
                    gems[i].position -= (delta / dist) * (4.0f + pickup_magnet_radius) * delta_time;
                }
                float gem_ground = TerrainHeightAt(gems[i].position.x, gems[i].position.z, gems[i].position.y);
                gems[i].position.y = glm::mix(gems[i].position.y, gem_ground + 0.15f, 0.25f);
                if (dist <= 1.0f) {
                    player_xp += 1;
                    gems[i] = gems.back();
                    gems.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < pickups.size();) {
                glm::vec3 delta = pickups[i].position - player_position;
                float dist = glm::length(delta);
                if (dist <= pickup_magnet_radius && dist > 0.001f) {
                    pickups[i].position -= (delta / dist) * (3.5f + pickup_magnet_radius) * delta_time;
                }
                float pickup_ground = TerrainHeightAt(pickups[i].position.x, pickups[i].position.z, pickups[i].position.y);
                pickups[i].position.y = glm::mix(pickups[i].position.y, pickup_ground + 0.1f, 0.25f);
                if (dist <= 1.1f) {
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

            for (size_t i = 0; i < spawn_poofs.size();) {
                spawn_poofs[i].timer += delta_time;
                if (spawn_poofs[i].timer >= spawn_poofs[i].duration) {
                    spawn_poofs[i] = spawn_poofs.back();
                    spawn_poofs.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < capture_waves.size();) {
                capture_waves[i].timer += delta_time;
                if (capture_waves[i].timer >= capture_waves[i].duration) {
                    capture_waves[i] = capture_waves.back();
                    capture_waves.pop_back();
                } else {
                    ++i;
                }
            }

            for (size_t i = 0; i < ground_effects.size();) {
                GroundEffect& effect = ground_effects[i];
                effect.timer += delta_time;
                effect.tick_timer += delta_time;
                if (effect.tick_timer >= effect.tick_interval) {
                    effect.tick_timer = 0.0f;
                    for (Enemy& enemy : enemies) {
                        glm::vec3 delta = enemy.position - effect.position;
                        if (glm::length(delta) <= effect.radius) {
                            damage_enemy(enemy, effect.damage);
                        }
                    }
                }
                if (effect.timer >= effect.duration) {
                    ground_effects[i] = ground_effects.back();
                    ground_effects.pop_back();
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
                pool.push_back(ItemChoice{ItemId::Magnet, "Magnet +", 70});
                if (unlock_cross) {
                    if (cross_level == 0) {
                        pool.push_back(ItemChoice{ItemId::Cross, "Cross Guardian", 75});
                    } else if (cross_level < 5) {
                        pool.push_back(ItemChoice{ItemId::CrossUpgrade, "Cross +", 60});
                    }
                }
                if (unlock_stick) {
                    if (stick_level == 0) {
                        pool.push_back(ItemChoice{ItemId::Stick, "Stake", 70});
                    } else if (stick_level < 5) {
                        pool.push_back(ItemChoice{ItemId::StickUpgrade, "Stake +", 55});
                    }
                }
                if (unlock_crossbow) {
                    if (crossbow_level == 0) {
                        pool.push_back(ItemChoice{ItemId::Crossbow, "Crossbow", 75});
                    } else if (crossbow_level < 5) {
                        pool.push_back(ItemChoice{ItemId::CrossbowUpgrade, "Crossbow +", 60});
                    }
                }
                if (unlock_poison) {
                    if (poison_level == 0) {
                        pool.push_back(ItemChoice{ItemId::PoisonBomb, "Caltrops", 65});
                    } else if (poison_level < 5) {
                        pool.push_back(ItemChoice{ItemId::PoisonBombUpgrade, "Caltrops +", 50});
                    }
                }
                if (fireball_level == 0) {
                    pool.push_back(ItemChoice{ItemId::Fireball, "Fireball", 80});
                } else if (fireball_level < 5) {
                    pool.push_back(ItemChoice{ItemId::FireballUpgrade, "Fireball +", 60});
                }
                if (bomb_level == 0) {
                    pool.push_back(ItemChoice{ItemId::Bomb, "Holy Bomb", 70});
                } else if (bomb_level < 5) {
                    pool.push_back(ItemChoice{ItemId::BombUpgrade, "Holy Bomb +", 50});
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
    glUniform1i(use_vertex_color_location, 0);
    glUniform1i(use_normals_location, 0);
    glUniform3f(light_dir_location, palette_light_dir.x, palette_light_dir.y, palette_light_dir.z);
        glUniform1f(ambient_location, 0.35f);
        glUniform1i(bands_location, 3);
        glUniform3f(rim_color_location, palette_rim.x, palette_rim.y, palette_rim.z);
        glUniform1f(rim_power_location, 2.0f);
        glUniform1f(rim_strength_location, 0.45f);
        glUniform3f(outline_color_location, palette_outline.x, palette_outline.y, palette_outline.z);
        glUniform1i(outline_pass_location, 0);
        glUniform1f(outline_size_location, 0.045f);
        glUniform1f(ao_location, 0.6f);
        glUniform3f(fog_color_location, palette_fog.x, palette_fog.y, palette_fog.z);
        glUniform1f(fog_near_location, 0.995f);
        glUniform1f(fog_far_location, 1.0f);

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
        float min_cam_y = TerrainHeightAt(camera_pos.x, camera_pos.z, camera_pos.y) + 1.0f;
        float target_cam_y = glm::max(camera_pos.y, min_cam_y);
        if (!camera_height_initialized) {
            camera_height = target_cam_y;
            camera_height_initialized = true;
        }
        float rise_speed = 8.0f;
        float fall_speed = 5.0f;
        float speed = target_cam_y > camera_height ? rise_speed : fall_speed;
        float cam_alpha = 1.0f - std::exp(-speed * delta_time);
        camera_height = glm::mix(camera_height, target_cam_y, cam_alpha);
        camera_pos.y = camera_height;
        glm::mat4 view = glm::lookAt(
            camera_pos,
            player_position + glm::vec3(0.0f, jump_view_offset, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 ground_model(1.0f);
        glm::mat4 ground_mvp = projection * view * ground_model;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(ground_mvp));
        glUniform1i(use_vertex_color_location, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.25f);
        glUniform3f(color_location, 0.10f, 0.75f, 0.25f);
        glUniform1f(alpha_location, 0.55f);
        glBindVertexArray(ground_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ground_vertices.size() / 3));
        glBindVertexArray(0);
        glUniform1f(alpha_location, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_BLEND);

        // Ramp surfaces to make ramps stand out.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform3f(color_location, 0.10f, 0.95f, 0.85f);
        glUniform1f(alpha_location, 0.18f);
        glBindVertexArray(ramp_surface_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ramp_surface_vertices.size() / 3));
        glBindVertexArray(0);
        glUniform1f(alpha_location, 1.0f);
        glDisable(GL_BLEND);

        // Ramp wall wiremesh to match collision bounds.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.6f);
        glUniform3f(color_location, 0.15f, 0.95f, 0.65f);
        glUniform1f(alpha_location, 0.75f);
        glBindVertexArray(ramp_wall_vao);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(ramp_wall_vertices.size() / 3));
        glBindVertexArray(0);
        glUniform1f(alpha_location, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_BLEND);

        // Ramp side faces so the walls are visible.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform3f(color_location, 0.12f, 0.85f, 0.75f);
        glUniform1f(alpha_location, 0.2f);
        glBindVertexArray(ramp_side_vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(ramp_side_vertices.size() / 3));
        glBindVertexArray(0);
        glUniform1f(alpha_location, 1.0f);
        glDisable(GL_BLEND);

        // Procedural rocks and trees (non-colliding props).
        for (const glm::vec3& pos : g_rock_positions) {
            int mesh_index = static_cast<int>(std::abs(g_rock_noise.GetValue(pos.x * 0.3, 0.0, pos.z * 0.3)) * 3.0) % 3;
            float scale = 0.6f + 0.4f * static_cast<float>(std::abs(g_rock_noise.GetValue(pos.x * 0.1, 1.0, pos.z * 0.1)));
            glm::mat4 rock = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0.0f, 0.2f, 0.0f));
            rock = glm::scale(rock, glm::vec3(scale));
            glm::mat4 rock_mvp = projection * view * rock;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(rock_mvp));
            glUniform3f(color_location, 0.45f, 0.42f, 0.38f);
            float dist = glm::length(glm::vec2(camera_pos.x - pos.x, camera_pos.z - pos.z));
            const Mesh& rock_mesh = dist > 60.0f ? rock_meshes_low[mesh_index] : rock_meshes[mesh_index];
            glBindVertexArray(rock_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, rock_mesh.count);
            glBindVertexArray(0);
        }

        for (const glm::vec3& pos : g_tree_positions) {
            float scale = 0.8f + 0.4f * static_cast<float>(std::abs(g_rock_noise.GetValue(pos.x * 0.05, 2.0, pos.z * 0.05)));
            glm::mat4 tree = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0.0f, 0.55f * scale, 0.0f));
            tree = glm::scale(tree, glm::vec3(scale));
            glm::mat4 tree_mvp = projection * view * tree;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(tree_mvp));
            glUniform3f(color_location, 0.20f, 0.55f, 0.28f);
            float dist = glm::length(glm::vec2(camera_pos.x - pos.x, camera_pos.z - pos.z));
            const Mesh& mesh = dist > 70.0f ? tree_mesh_low : tree_mesh;
            glBindVertexArray(mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, mesh.count);
            glBindVertexArray(0);
        }

        // Houses (solid shaded)
        glUseProgram(program);
        glUniform1i(use_vertex_color_location, 0);
        glUniform1i(use_normals_location, 1);
        for (const Building& building : buildings) {
            float base_y = building.base_height;
            if (building.platform_index >= 0 &&
                building.platform_index < static_cast<int>(platforms.size())) {
                base_y -= platforms[building.platform_index].fall_offset;
            }
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(building.center.x, base_y, building.center.y));
            model = glm::rotate(model, building.house_rotation, glm::vec3(0.0f, 1.0f, 0.0f));
            float house_scale = building.size * 0.8f;
            model = glm::scale(model, glm::vec3(house_scale, house_scale, house_scale));
            glm::mat4 mvp = projection * view * model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));

            glm::vec3 base_color = house_base_palette[building.house_material];
            glm::vec3 roof_color = house_roof_palette[building.house_material];
            if (building.owner == BuildingOwner::Enemy && building.spawner) {
                float danger_pulse = 0.6f + 0.4f * std::sin(run_time * 4.0f + building.center.x * 0.12f);
                glm::vec3 danger = building.alive_timer >= 120.0f
                    ? glm::vec3(1.0f, 0.65f, 0.10f)
                    : glm::vec3(1.0f, 0.15f, 0.20f);
                base_color = glm::mix(base_color, danger, danger_pulse * 0.55f);
                roof_color = glm::mix(roof_color, danger, danger_pulse * 0.35f);
            } else if (building.owner == BuildingOwner::Player) {
                glm::vec3 tint = building.veteran_timer >= 60.0f
                    ? glm::vec3(0.70f, 0.35f, 0.95f)
                    : glm::vec3(0.20f, 0.85f, 0.40f);
                base_color = glm::mix(base_color, tint, 0.35f);
                roof_color = glm::mix(roof_color, tint, 0.25f);
            }
            if (building.type == BuildingType::TownHall) {
                glUniform3f(color_location, 0.95f, 0.75f, 0.25f);
                glBindVertexArray(town_hall_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, town_hall_mesh.count);
                glBindVertexArray(0);
                glUniform1i(use_normals_location, 0);
                glm::vec3 beacon_pos(building.center.x, base_y + 2.2f * house_scale, building.center.y);
                glm::mat4 beacon = glm::translate(glm::mat4(1.0f), beacon_pos);
                beacon = glm::scale(beacon, glm::vec3(0.12f, 0.8f, 0.12f));
                glm::mat4 beacon_mvp = projection * view * beacon;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(beacon_mvp));
                glUniform3f(color_location, 0.95f, 0.90f, 0.35f);
                glBindVertexArray(cylinder_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
                glBindVertexArray(0);
                glm::mat4 beacon_top = glm::translate(glm::mat4(1.0f),
                                                     beacon_pos + glm::vec3(0.0f, 0.55f, 0.0f));
                beacon_top = glm::scale(beacon_top, glm::vec3(0.25f));
                glm::mat4 beacon_top_mvp = projection * view * beacon_top;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(beacon_top_mvp));
                glUniform3f(color_location, 0.98f, 0.95f, 0.45f);
                glBindVertexArray(cone_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                glBindVertexArray(0);
                glUniform1i(use_normals_location, 1);
            } else if (building.type == BuildingType::Barracks) {
                glUniform3f(color_location, 0.35f, 0.65f, 0.85f);
                glBindVertexArray(barracks_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, barracks_mesh.count);
                glBindVertexArray(0);
            } else if (building.type == BuildingType::Armory) {
                glUniform3f(color_location, 0.85f, 0.45f, 0.25f);
                glBindVertexArray(armory_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, armory_mesh.count);
                glBindVertexArray(0);
            } else if (building.type == BuildingType::Watchtower) {
                glUniform3f(color_location, 0.65f, 0.70f, 0.85f);
                glBindVertexArray(watchtower_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, watchtower_mesh.count);
                glBindVertexArray(0);
            } else {
                glUniform3f(color_location, base_color.r, base_color.g, base_color.b);
                const Mesh& base_mesh = house_base_meshes[building.house_variant];
                glBindVertexArray(base_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, base_mesh.count);
                glBindVertexArray(0);

                glUniform3f(color_location, roof_color.r, roof_color.g, roof_color.b);
                const Mesh& roof_mesh = house_roof_meshes[building.house_variant];
                glBindVertexArray(roof_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, roof_mesh.count);
                glBindVertexArray(0);
            }
        }
        glUniform1i(use_normals_location, 0);

        if (house_wireframe_debug) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.1f);
            glUniform3f(color_location, 0.12f, 0.18f, 0.25f);
            for (const Building& building : buildings) {
                float base_y = building.base_height;
                if (building.platform_index >= 0 &&
                    building.platform_index < static_cast<int>(platforms.size())) {
                    base_y -= platforms[building.platform_index].fall_offset;
                }
                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                                 glm::vec3(building.center.x, base_y, building.center.y));
                model = glm::rotate(model, building.house_rotation, glm::vec3(0.0f, 1.0f, 0.0f));
                float house_scale = building.size * 0.8f;
                model = glm::scale(model, glm::vec3(house_scale, house_scale, house_scale));
                glm::mat4 mvp = projection * view * model;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
                const Mesh& base_mesh = house_base_meshes[building.house_variant];
                glBindVertexArray(base_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, base_mesh.count);
                glBindVertexArray(0);
                const Mesh& roof_mesh = house_roof_meshes[building.house_variant];
                glBindVertexArray(roof_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, roof_mesh.count);
                glBindVertexArray(0);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_BLEND);
        }

        if (spawn_debug) {
            glUseProgram(program);
            glUniform1i(use_vertex_color_location, 0);
            glUniform1i(use_normals_location, 0);
            glm::mat4 marker = glm::translate(glm::mat4(1.0f), last_spawn_debug_pos);
            marker = glm::scale(marker, glm::vec3(0.25f));
            glm::mat4 marker_mvp = projection * view * marker;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(marker_mvp));
            glUniform3f(color_location, 0.95f, 0.35f, 0.20f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            if (last_spawn_debug_platform >= 0 &&
                last_spawn_debug_platform < static_cast<int>(platforms.size())) {
                int plateau_index = platforms[last_spawn_debug_platform].plateau_index;
                if (plateau_index >= 0 && plateau_index < static_cast<int>(g_plateaus.size())) {
                    glm::vec2 center = g_plateaus[plateau_index].center;
                    glm::vec2 half = g_plateaus[plateau_index].half;
                    float height = g_plateaus[plateau_index].height;
                    if (plateau_index < static_cast<int>(g_plateau_fall_offsets.size())) {
                        height -= g_plateau_fall_offsets[plateau_index];
                    }
                    glUseProgram(ring_program);
                    glm::mat4 ring_model = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(center.x, height + 0.05f, center.y));
                    ring_model = glm::scale(ring_model, glm::vec3(half.x, 1.0f, half.y));
                    glm::mat4 ring_mvp = projection * view * ring_model;
                    glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                    glUniform3f(ring_color_location, 0.95f, 0.40f, 0.20f);
                    glUniform1f(ring_phase_location, 0.0f);
                    glUniform1f(ring_alpha_location, 0.5f);
                    glBindVertexArray(ring_vao);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);
                    glUseProgram(program);
                }
            }
        }

        // Town visuals are now represented by upgraded buildings.

        if (state == GameState::Running) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            float capture_radius = glm::max(attack_radius, 2.2f);
            float capture_radius_sq = capture_radius * capture_radius;
            for (const Building& building : buildings) {
                if (building.owner == BuildingOwner::Player) {
                    continue;
                }
                if (building.platform_index < 0 ||
                    building.platform_index >= static_cast<int>(platforms.size()) ||
                    platforms[building.platform_index].state != PlatformState::Active) {
                    continue;
                }
                bool nearby = false;
                if (std::abs(player_position.y - building.base_height) <= 2.0f) {
                    glm::vec2 delta = building.center - glm::vec2(player_position.x, player_position.z);
                    float dist_sq = glm::dot(delta, delta);
                    nearby = dist_sq <= capture_radius_sq;
                }
                if (!nearby) {
                    for (const Ally& ally : allies) {
                        if (!ally.alive) {
                            continue;
                        }
                        if (std::abs(ally.position.y - building.base_height) > 2.0f) {
                            continue;
                        }
                        glm::vec2 delta = building.center - glm::vec2(ally.position.x, ally.position.z);
                        float dist_sq = glm::dot(delta, delta);
                        if (dist_sq <= capture_radius_sq) {
                            nearby = true;
                            break;
                        }
                    }
                }
                if (!nearby && building.capture_timer <= 0.01f) {
                    continue;
                }
                float progress = glm::clamp(building.capture_timer / 5.0f, 0.0f, 1.0f);
                float base_y = building.base_height;
                if (building.platform_index >= 0 && building.platform_index < static_cast<int>(platforms.size())) {
                    base_y -= platforms[building.platform_index].fall_offset;
                }
                glm::vec3 ring_pos(building.center.x, base_y + 0.05f, building.center.y);
                glm::mat4 ring_model = glm::translate(glm::mat4(1.0f), ring_pos);
                ring_model = glm::scale(ring_model, glm::vec3(1.8f, 1.0f, 1.8f));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, 0.35f, 0.75f, 1.0f);
                glUniform1f(ring_phase_location, progress);
                glUniform1f(ring_alpha_location, 0.65f);
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(program);
        }

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

        if (!spawn_poofs.empty()) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            for (const SpawnPoof& poof : spawn_poofs) {
                float t = glm::clamp(poof.timer / poof.duration, 0.0f, 1.0f);
                glm::mat4 ring_model = glm::translate(glm::mat4(1.0f),
                                                     poof.position + glm::vec3(0.0f, 0.05f, 0.0f));
                ring_model = glm::scale(ring_model, glm::vec3(0.6f + 1.6f * t, 1.0f, 0.6f + 1.6f * t));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, 0.85f, 0.45f, 0.95f);
                glUniform1f(ring_phase_location, t);
                glUniform1f(ring_alpha_location, 0.6f * (1.0f - t));
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        if (!capture_waves.empty()) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            for (const CaptureWave& wave : capture_waves) {
                float t = glm::clamp(wave.timer / wave.duration, 0.0f, 1.0f);
                glm::mat4 ring_model = glm::translate(glm::mat4(1.0f),
                                                     wave.position + glm::vec3(0.0f, 0.05f, 0.0f));
                ring_model = glm::scale(ring_model, glm::vec3(1.2f + 2.8f * t, 1.0f, 1.2f + 2.8f * t));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, 0.25f, 0.95f, 0.65f);
                glUniform1f(ring_phase_location, t);
                glUniform1f(ring_alpha_location, 0.7f * (1.0f - t));
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        if (state == GameState::Running && garlic_level == 0) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            glm::mat4 hover_model = glm::translate(
                glm::mat4(1.0f),
                player_position + glm::vec3(0.0f, 0.04f, 0.0f));
            hover_model = glm::scale(hover_model, glm::vec3(1.2f, 1.0f, 1.2f));
            glm::mat4 hover_mvp = projection * view * hover_model;
            glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(hover_mvp));
            glUniform3f(ring_color_location, 0.15f, 0.85f, 0.95f);
            float phase = 0.4f + 0.3f * std::sin(run_time * 2.0f);
            glUniform1f(ring_phase_location, phase);
            glUniform1f(ring_alpha_location, 0.45f);
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
        float player_yaw = camera_yaw + glm::half_pi<float>();
        player_yaw += glm::pi<float>();
        glm::mat4 base_transform = glm::translate(glm::mat4(1.0f), base_pos);
        base_transform = glm::rotate(base_transform, player_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 body = base_transform;
        body = glm::scale(body, glm::vec3(0.75f, 1.15f, 0.75f));
        glm::mat4 body_mvp = projection * view * body;
        glm::mat4 head = base_transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.28f, 0.0f));
        head = glm::scale(head, glm::vec3(0.32f));
        glm::mat4 head_mvp = projection * view * head;

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glUniform1i(outline_pass_location, 1);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(body_mvp));
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(head_mvp));
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);
        glUniform1i(outline_pass_location, 0);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(body_mvp));
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glBindVertexArray(0);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(head_mvp));
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);

        glm::mat4 cloak = base_transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.12f, -0.15f));
        cloak = glm::rotate(cloak, glm::radians(-6.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        cloak = glm::scale(cloak, glm::vec3(0.75f, 0.85f, 0.6f));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform1f(alpha_location, 0.55f);
        glm::mat4 cloak_glow = cloak;
        cloak_glow = glm::scale(cloak_glow, glm::vec3(1.06f, 1.04f, 1.06f));
        glm::mat4 cloak_glow_mvp = projection * view * cloak_glow;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cloak_glow_mvp));
        glUniform3f(color_location, 0.15f, 0.85f, 0.95f);
        glBindVertexArray(cone_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
        glm::mat4 cloak_mvp = projection * view * cloak;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cloak_mvp));
        glUniform3f(color_location, 0.10f, 0.10f, 0.16f);
        glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
        glBindVertexArray(0);
        glUniform1f(alpha_location, 1.0f);
        glDisable(GL_BLEND);

        glm::mat4 helmet = base_transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.55f, 0.0f));
        helmet = glm::scale(helmet, glm::vec3(0.38f, 0.32f, 0.38f));
        glm::mat4 helmet_mvp = projection * view * helmet;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(helmet_mvp));
        glUniform3f(color_location, 0.92f, 0.78f, 0.28f);
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glBindVertexArray(0);

        glm::vec3 hair_color_value = hair_colors[glm::clamp(hair_color, 0, hair_color_count - 1)];
        int render_style = (hair_unlock_mask & (1 << hair_style)) ? hair_style : 0;
        if (render_style == 0) {
            glm::mat4 crest = base_transform * glm::translate(glm::mat4(1.0f),
                                                             glm::vec3(0.0f, 0.72f, -0.05f));
            crest = glm::scale(crest, glm::vec3(0.12f, 0.3f, 0.4f));
            glm::mat4 crest_mvp = projection * view * crest;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(crest_mvp));
            glUniform3f(color_location, hair_color_value.r, hair_color_value.g, hair_color_value.b);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        } else if (render_style == 1) {
            glm::mat4 crest = base_transform * glm::translate(glm::mat4(1.0f),
                                                             glm::vec3(0.0f, 0.78f, -0.02f));
            crest = glm::scale(crest, glm::vec3(0.16f, 0.4f, 0.55f));
            glm::mat4 crest_mvp = projection * view * crest;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(crest_mvp));
            glUniform3f(color_location, hair_color_value.r, hair_color_value.g, hair_color_value.b);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        } else {
            for (int i = 0; i < 3; ++i) {
                float offset = (static_cast<float>(i) - 1.0f) * 0.14f;
                glm::mat4 spike = base_transform * glm::translate(glm::mat4(1.0f),
                                                                glm::vec3(offset, 0.72f, -0.05f));
                spike = glm::scale(spike, glm::vec3(0.12f, 0.3f, 0.12f));
                glm::mat4 spike_mvp = projection * view * spike;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(spike_mvp));
                glUniform3f(color_location, hair_color_value.r, hair_color_value.g, hair_color_value.b);
                glBindVertexArray(cone_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                glBindVertexArray(0);
            }
        }

        glm::mat4 pauldron = base_transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, 0.25f, 0.0f));
        pauldron = glm::scale(pauldron, glm::vec3(0.22f));
        glm::mat4 pauldron_mvp = projection * view * pauldron;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(pauldron_mvp));
        glUniform3f(color_location, 0.95f, 0.85f, 0.25f);
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);

        glm::mat4 pauldron_left = base_transform * glm::translate(glm::mat4(1.0f), glm::vec3(-0.38f, 0.25f, 0.0f));
        pauldron_left = glm::scale(pauldron_left, glm::vec3(0.22f));
        glm::mat4 pauldron_left_mvp = projection * view * pauldron_left;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(pauldron_left_mvp));
        glUniform3f(color_location, 0.95f, 0.85f, 0.25f);
        glBindVertexArray(sphere_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
        glBindVertexArray(0);

        glUniform3f(color_location, player_color.r, player_color.g, player_color.b);

        glm::vec3 aim_dir = glm::normalize(glm::vec3(std::cos(player_yaw), 0.0f, std::sin(player_yaw)));
        glm::vec3 right_dir = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), aim_dir));
        float arm_swing = std::sin(run_time * 4.0f) * 0.6f * player_move_amount;

        glm::mat4 hand_left = base_transform * glm::translate(glm::mat4(1.0f),
                                                              glm::vec3(-0.48f, 0.1f, 0.0f));
        hand_left = glm::rotate(hand_left, arm_swing, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 arm_left = hand_left;
        arm_left = glm::scale(arm_left, glm::vec3(0.16f, 0.45f, 0.16f));
        glm::mat4 arm_left_mvp = projection * view * arm_left;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(arm_left_mvp));
        glUniform3f(color_location, player_color.r, player_color.g, player_color.b);
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glBindVertexArray(0);

        glm::mat4 hand_right = base_transform * glm::translate(glm::mat4(1.0f),
                                                               glm::vec3(0.48f, 0.1f, 0.0f));
        hand_right = glm::rotate(hand_right, -arm_swing, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 arm_right = hand_right;
        arm_right = glm::scale(arm_right, glm::vec3(0.16f, 0.45f, 0.16f));
        glm::mat4 arm_right_mvp = projection * view * arm_right;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(arm_right_mvp));
        glUniform3f(color_location, player_color.r, player_color.g, player_color.b);
        glBindVertexArray(cylinder_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
        glBindVertexArray(0);

        if (crossbow_level > 0) {
            glm::mat4 bow_model = hand_left * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.35f));
            bow_model = glm::scale(bow_model, glm::vec3(0.5f, 0.14f, 0.14f));
            glm::mat4 bow_mvp = projection * view * bow_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(bow_mvp));
            glUniform3f(color_location, 0.55f, 0.38f, 0.22f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        if (stick_level > 0) {
            glm::mat4 stake_model = hand_right * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.42f));
            stake_model = glm::scale(stake_model, glm::vec3(0.6f, 0.14f, 0.12f));
            glm::mat4 stake_mvp = projection * view * stake_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(stake_mvp));
            glUniform3f(color_location, 0.62f, 0.40f, 0.18f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        if (stick_flash_timer > 0.0f) {
            float alpha = glm::clamp(stick_flash_timer / 0.12f, 0.0f, 1.0f);
            glUniform1f(alpha_location, 0.85f * alpha);
            glUniform3f(color_location, 0.95f, 0.90f, 0.35f);
            glm::vec3 strike_pos = player_position + aim_dir * (stick_range * 0.55f);
            strike_pos.y += 0.35f;
            glm::mat4 strike_model = glm::translate(glm::mat4(1.0f), strike_pos);
            strike_model = glm::rotate(strike_model, player_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            strike_model = glm::scale(strike_model, glm::vec3(stick_range, 0.08f, 0.12f));
            glm::mat4 strike_mvp = projection * view * strike_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(strike_mvp));
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glUniform1f(alpha_location, 1.0f);
        }

        if (state == GameState::Running) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            auto draw_decal = [&](const glm::vec3& pos, float radius, const glm::vec3& color,
                                  float alpha, float phase) {
                float y = TerrainHeightAt(pos.x, pos.z, pos.y) + 0.03f;
                glm::mat4 ring_model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, y, pos.z));
                ring_model = glm::scale(ring_model, glm::vec3(radius, 1.0f, radius));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, color.r, color.g, color.b);
                glUniform1f(ring_phase_location, phase);
                glUniform1f(ring_alpha_location, alpha);
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            };

            draw_decal(player_position, 0.75f, glm::vec3(0.05f, 0.05f, 0.08f), 0.35f, 0.35f);
            for (const Enemy& enemy : enemies) {
                draw_decal(enemy.position, 0.55f * enemy.scale, glm::vec3(0.05f, 0.05f, 0.08f), 0.35f, 0.35f);
            }
            if (selected_ally_id != -1) {
                for (const Ally& ally : allies) {
                    if (!ally.alive || ally.id != selected_ally_id) {
                        continue;
                    }
                    draw_decal(ally.position, 0.6f, glm::vec3(0.15f, 0.85f, 0.35f), 0.65f, 0.6f);
                    break;
                }
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
        }

        for (const Enemy& enemy : enemies) {
            float bob = 0.08f * std::sin(run_time * 3.5f + enemy.phase);
            glm::vec3 base_pos = enemy.position + glm::vec3(0.0f, 0.35f + bob, 0.0f);
            glm::vec3 base_color = enemy.color;
            glm::vec3 accent = enemy.elite
                ? glm::vec3(0.95f, 0.80f, 0.25f)
                : glm::vec3(0.85f, 0.25f, 0.65f);
            float pulse = 0.5f + 0.5f * std::sin(run_time * 2.4f + enemy.phase);
            glm::vec3 mix_color = glm::mix(base_color, accent, 0.35f * pulse);
            if (enemy.hit_flash > 0.0f) {
                float flash = glm::clamp(enemy.hit_flash / 0.12f, 0.0f, 1.0f);
                mix_color = glm::mix(mix_color, glm::vec3(0.95f, 0.95f, 0.95f), flash);
            }
            if (enemy.elite) {
                float glow = 0.65f + 0.35f * std::sin(run_time * 5.0f + enemy.phase);
                glUniform3f(color_location, mix_color.r * glow, mix_color.g * glow, mix_color.b * glow);
            } else {
                glUniform3f(color_location, mix_color.r, mix_color.g, mix_color.b);
            }

            glm::vec3 to_player = player_position - enemy.position;
            to_player.y = 0.0f;
            float enemy_yaw = std::atan2(to_player.x, to_player.z);
            glm::mat4 enemy_base = glm::translate(glm::mat4(1.0f), base_pos);
            enemy_base = glm::rotate(enemy_base, enemy_yaw, glm::vec3(0.0f, 1.0f, 0.0f));

            int mesh_index = enemy.type;
            if (mesh_index < 0 || mesh_index > 3) {
                mesh_index = 0;
            }
            glm::mat4 enemy_model = enemy_base;
            glm::vec3 scale(enemy.scale);
            if (enemy.type == 2) {
                float squash = 0.08f * std::sin(run_time * 3.0f + enemy.phase);
                scale = glm::vec3(enemy.scale * (1.05f - squash), enemy.scale * (0.95f + squash), enemy.scale * (1.05f - squash));
            }
            enemy_model = glm::scale(enemy_model, scale);
            glm::mat4 enemy_mvp = projection * view * enemy_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(enemy_mvp));
            glBindVertexArray(enemy_meshes[mesh_index].vao);
            glDrawArrays(GL_TRIANGLES, 0, enemy_meshes[mesh_index].count);
            glBindVertexArray(0);
        }

        for (const Ally& ally : allies) {
            if (!ally.alive) {
                continue;
            }
            glm::vec3 ally_color(0.25f, 0.95f, 0.55f);
            if (ally.state == UnitState::Follow) {
                ally_color = glm::vec3(0.35f, 0.85f, 0.95f);
            } else if (ally.state == UnitState::Guard) {
                ally_color = glm::vec3(0.85f, 0.85f, 0.30f);
            }
            if (ally.gold) {
                ally_color = glm::vec3(0.95f, 0.82f, 0.35f);
            }
            glUniform3f(color_location, ally_color.r, ally_color.g, ally_color.b);
            glm::vec3 base_pos = ally.position + glm::vec3(0.0f, 0.25f, 0.0f);
            glm::mat4 body = glm::translate(glm::mat4(1.0f), base_pos);
            body = glm::scale(body, glm::vec3(0.5f, 0.35f, 0.6f));
            glm::mat4 body_mvp = projection * view * body;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(body_mvp));
            glBindVertexArray(cylinder_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, cylinder_mesh.count);
            glBindVertexArray(0);

            glm::mat4 head = glm::translate(glm::mat4(1.0f), base_pos + glm::vec3(0.0f, 0.25f, 0.0f));
            head = glm::scale(head, glm::vec3(0.28f));
            glm::mat4 head_mvp = projection * view * head;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(head_mvp));
            glBindVertexArray(sphere_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
            glBindVertexArray(0);
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
            glUniform3f(color_location, 0.65f, 0.90f, 1.0f);
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
                float ring_scale = explosion.radius * (0.45f + 0.8f * t);
                glm::mat4 ring_model = glm::translate(
                    glm::mat4(1.0f),
                    explosion.position + glm::vec3(0.0f, 0.05f, 0.0f));
                ring_model = glm::scale(ring_model, glm::vec3(ring_scale, 1.0f, ring_scale));
                glm::mat4 ring_mvp = projection * view * ring_model;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                glUniform3f(ring_color_location, 0.95f, 0.25f, 0.20f);
                glUniform1f(ring_phase_location, glm::clamp(t, 0.0f, 1.0f));
                glUniform1f(ring_alpha_location, 1.0f - t);
                glBindVertexArray(ring_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                glm::mat4 ring_model_outer = glm::translate(
                    glm::mat4(1.0f),
                    explosion.position + glm::vec3(0.0f, 0.08f, 0.0f));
                ring_model_outer = glm::scale(ring_model_outer,
                                              glm::vec3(ring_scale * 1.35f, 1.0f, ring_scale * 1.35f));
                glm::mat4 ring_mvp_outer = projection * view * ring_model_outer;
                glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp_outer));
                glUniform3f(ring_color_location, 0.65f, 0.90f, 1.0f);
                glUniform1f(ring_phase_location, glm::clamp(t * 1.2f, 0.0f, 1.0f));
                glUniform1f(ring_alpha_location, 0.6f * (1.0f - t));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glUseProgram(program);
                glUniform1f(alpha_location, 0.45f * (1.0f - t));
                glm::mat4 blast = glm::translate(glm::mat4(1.0f),
                                                 explosion.position + glm::vec3(0.0f, 0.25f, 0.0f));
                blast = glm::scale(blast, glm::vec3(explosion.radius * (0.35f + 0.9f * t)));
                glm::mat4 blast_mvp = projection * view * blast;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(blast_mvp));
                glUniform3f(color_location, 0.65f, 0.90f, 1.0f);
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
                glUniform1f(alpha_location, 1.0f);
                glUseProgram(ring_program);
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        if (!ground_effects.empty()) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(ring_program);
            for (const GroundEffect& effect : ground_effects) {
                float t = effect.timer / glm::max(0.001f, effect.duration);
                if (effect.type == GroundEffectType::Holy) {
                    glm::mat4 ring_model = glm::translate(
                        glm::mat4(1.0f),
                        effect.position + glm::vec3(0.0f, 0.04f, 0.0f));
                    ring_model = glm::scale(ring_model, glm::vec3(effect.radius, 1.0f, effect.radius));
                    glm::mat4 ring_mvp = projection * view * ring_model;
                    glUniformMatrix4fv(ring_mvp_location, 1, GL_FALSE, glm::value_ptr(ring_mvp));
                    glUniform3f(ring_color_location, 0.65f, 0.90f, 1.0f);
                    glUniform1f(ring_phase_location, glm::clamp(t, 0.0f, 1.0f));
                    glUniform1f(ring_alpha_location, 0.7f * (1.0f - t * 0.5f));
                    glBindVertexArray(ring_vao);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);
                } else {
                    glUseProgram(program);
                    glUniform1f(alpha_location, 0.9f);
                    glUniform3f(color_location, 0.20f, 0.85f, 0.35f);
                    for (int i = 0; i < 6; ++i) {
                        float angle = static_cast<float>(i) * glm::two_pi<float>() / 6.0f + t * 0.6f;
                        float radius = effect.radius * 0.35f;
                        glm::vec3 pos = effect.position +
                            glm::vec3(std::cos(angle), 0.05f, std::sin(angle)) * radius;
                        glm::mat4 spike = glm::translate(glm::mat4(1.0f), pos);
                        spike = glm::scale(spike, glm::vec3(0.18f, 0.55f, 0.18f));
                        glm::mat4 spike_mvp = projection * view * spike;
                        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(spike_mvp));
                        glBindVertexArray(cone_mesh.vao);
                        glDrawArrays(GL_TRIANGLES, 0, cone_mesh.count);
                    }
                    glBindVertexArray(0);
                    glUseProgram(ring_program);
                }
            }
            glUseProgram(program);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        for (const Projectile& projectile : projectiles) {
            glm::vec3 dir = projectile.velocity;
            dir.y = 0.0f;
            float len = glm::length(dir);
            if (len > 0.001f) {
                dir /= len;
                float yaw = std::atan2(dir.x, dir.z);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUniform1f(alpha_location, 0.5f);
                glm::mat4 trail = glm::translate(glm::mat4(1.0f),
                                                 projectile.position - glm::vec3(dir.x, 0.0f, dir.z) * 0.4f);
                trail = glm::rotate(trail, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                trail = glm::scale(trail, glm::vec3(0.12f, 0.12f, 0.8f));
                glm::mat4 trail_mvp = projection * view * trail;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(trail_mvp));
                glUniform3f(color_location, projectile.color.r, projectile.color.g, projectile.color.b);
                glBindVertexArray(cube_vao);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
                glUniform1f(alpha_location, 1.0f);
                glDisable(GL_BLEND);
            }
            glm::mat4 proj_model = glm::translate(
                glm::mat4(1.0f),
                projectile.position);
            proj_model = glm::scale(proj_model, glm::vec3(0.2f));
            glm::mat4 proj_mvp = projection * view * proj_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(proj_mvp));
            glUniform3f(color_location, projectile.color.r, projectile.color.g, projectile.color.b);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        if (cross_level > 0) {
            for (int i = 0; i < cross_level; ++i) {
                float angle = run_time * 2.2f + static_cast<float>(i) * glm::two_pi<float>() / glm::max(1, cross_level);
                glm::vec3 orb = player_position + glm::vec3(std::cos(angle), 0.25f, std::sin(angle)) * cross_orbit_radius;
                glUniform3f(color_location, 0.85f, 0.95f, 0.95f);
                glm::mat4 cross_a = glm::translate(glm::mat4(1.0f), orb);
                cross_a = glm::scale(cross_a, glm::vec3(0.6f, 0.15f, 0.15f));
                glm::mat4 cross_a_mvp = projection * view * cross_a;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cross_a_mvp));
                glBindVertexArray(cube_vao);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glm::mat4 cross_b = glm::translate(glm::mat4(1.0f), orb);
                cross_b = glm::scale(cross_b, glm::vec3(0.15f, 0.15f, 0.6f));
                glm::mat4 cross_b_mvp = projection * view * cross_b;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(cross_b_mvp));
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
        }

        if (unlock_bat) {
            glm::vec3 bat_pos = player_position +
                glm::vec3(std::cos(run_time * 2.6f), 0.9f + 0.15f * std::sin(run_time * 3.2f),
                          std::sin(run_time * 2.6f)) * 1.4f;
            float flap = std::sin(run_time * 6.0f) * 0.6f;
            glUniform3f(color_location, 0.20f, 0.80f, 0.95f);
            glm::mat4 bat_body = glm::translate(glm::mat4(1.0f), bat_pos);
            bat_body = glm::scale(bat_body, glm::vec3(0.45f, 0.2f, 0.65f));
            glm::mat4 bat_mvp = projection * view * bat_body;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(bat_mvp));
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);

            glm::mat4 wing_left = glm::translate(glm::mat4(1.0f), bat_pos + glm::vec3(-0.45f, 0.02f, 0.0f));
            wing_left = glm::rotate(wing_left, flap, glm::vec3(0.0f, 0.0f, 1.0f));
            wing_left = glm::scale(wing_left, glm::vec3(0.55f, 0.05f, 0.25f));
            glm::mat4 wing_left_mvp = projection * view * wing_left;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(wing_left_mvp));
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);

            glm::mat4 wing_right = glm::translate(glm::mat4(1.0f), bat_pos + glm::vec3(0.45f, 0.02f, 0.0f));
            wing_right = glm::rotate(wing_right, -flap, glm::vec3(0.0f, 0.0f, 1.0f));
            wing_right = glm::scale(wing_right, glm::vec3(0.55f, 0.05f, 0.25f));
            glm::mat4 wing_right_mvp = projection * view * wing_right;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(wing_right_mvp));
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);

            glm::mat4 eye_left = glm::translate(glm::mat4(1.0f),
                                                bat_pos + glm::vec3(-0.12f, 0.05f, 0.25f));
            eye_left = glm::scale(eye_left, glm::vec3(0.08f));
            glm::mat4 eye_left_mvp = projection * view * eye_left;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(eye_left_mvp));
            glUniform3f(color_location, 0.98f, 0.98f, 1.0f);
            glBindVertexArray(sphere_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
            glBindVertexArray(0);

            glm::mat4 eye_right = glm::translate(glm::mat4(1.0f),
                                                 bat_pos + glm::vec3(0.12f, 0.05f, 0.25f));
            eye_right = glm::scale(eye_right, glm::vec3(0.08f));
            glm::mat4 eye_right_mvp = projection * view * eye_right;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(eye_right_mvp));
            glUniform3f(color_location, 0.98f, 0.98f, 1.0f);
            glBindVertexArray(sphere_mesh.vao);
            glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
            glBindVertexArray(0);
        }

        if (state == GameState::Running) {
            int follow_count = 0;
            for (const Ally& ally : allies) {
                if (ally.alive && ally.state == UnitState::Follow) {
                    follow_count += 1;
                }
            }
            if (follow_count >= 5) {
                glm::vec3 sun_pos = player_position + glm::vec3(0.0f, 2.2f, 0.0f);
                glUniform3f(color_location, 0.95f, 0.95f, 0.90f);
                glm::mat4 sun = glm::translate(glm::mat4(1.0f), sun_pos);
                sun = glm::scale(sun, glm::vec3(0.5f));
                glm::mat4 sun_mvp = projection * view * sun;
                glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(sun_mvp));
                glBindVertexArray(sphere_mesh.vao);
                glDrawArrays(GL_TRIANGLES, 0, sphere_mesh.count);
                glBindVertexArray(0);
            }
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

        if (player_hurt_timer > 0.0f) {
            float t = glm::clamp(player_hurt_timer / 0.25f, 0.0f, 1.0f);
            glUniform1f(alpha_location, 0.25f * t);
            draw_ui_quad(0.0f, 0.0f,
                         static_cast<float>(window_width),
                         static_cast<float>(window_height),
                         glm::vec3(0.9f, 0.2f, 0.25f),
                         ui_projection);
            glUniform1f(alpha_location, 1.0f);
        }

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
            SDL_Color ui_hint{160, 170, 185, 255};
            draw_text(window_width - margin - timer_width,
                      window_height - margin - bar_height * 2.2f,
                      "V Follow  H Home  G Guard",
                      ui_hint,
                      ui_projection);

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
            float panel_h = 250.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.10f, 0.10f, 0.14f), ui_projection);
            for (int i = 0; i < 4; ++i) {
                float y = panel_y + panel_h - 60.0f - i * 48.0f;
                glm::vec3 color = (i == main_menu_index) ? glm::vec3(0.85f, 0.72f, 0.25f)
                                                         : glm::vec3(0.22f, 0.22f, 0.28f);
                draw_ui_quad(panel_x + 40.0f, y, panel_w - 80.0f, 28.0f, color, ui_projection);
            }
        } else if (state == GameState::Customization) {
            float panel_w = 420.0f;
            float panel_h = 260.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.10f, 0.10f, 0.14f), ui_projection);
            for (int i = 0; i < 3; ++i) {
                float y = panel_y + panel_h - 60.0f - i * 52.0f;
                glm::vec3 color = (i == customize_menu_index) ? glm::vec3(0.20f, 0.65f, 0.95f)
                                                              : glm::vec3(0.22f, 0.22f, 0.28f);
                draw_ui_quad(panel_x + 40.0f, y, panel_w - 80.0f, 30.0f, color, ui_projection);
            }
        } else if (state == GameState::PowerUps) {
            float panel_w = 420.0f;
            float panel_h = 600.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            draw_ui_quad(panel_x, panel_y, panel_w, panel_h, glm::vec3(0.10f, 0.10f, 0.14f), ui_projection);
            for (int i = 0; i < 11; ++i) {
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
            float panel_h = 250.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            auto starters = build_starter_weapons();
            const char* starter_name = starters[starter_weapon_index].name;
            draw_text_centered(panel_x, panel_y + panel_h + 16.0f, panel_w, 32.0f, "Vampire Diary",
                               ui_white, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 60.0f, panel_w - 80.0f, 28.0f,
                               "Start Run", main_menu_index == 0 ? ui_highlight_text : ui_dim,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 108.0f, panel_w - 80.0f, 28.0f,
                               "Power Ups", main_menu_index == 1 ? ui_highlight_text : ui_dim,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 156.0f, panel_w - 80.0f, 28.0f,
                               "Customize", main_menu_index == 2 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 204.0f, panel_w - 80.0f, 28.0f,
                               "Quit", main_menu_index == 3 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 96.0f, panel_w, 22.0f,
                               "Coins: " + std::to_string(coins), ui_white, ui_projection);
            draw_text_centered(panel_x, panel_y - 72.0f, panel_w, 20.0f,
                               std::string("Start Weapon: ") + starter_name,
                               ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 52.0f, panel_w, 18.0f,
                               "Use Left/Right to choose", ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 32.0f, panel_w, 18.0f,
                               "Seed: " + std::to_string(terrain_seed), ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 14.0f, panel_w, 18.0f,
                               "Press R to reroll", ui_dim, ui_projection);
        } else if (state == GameState::Customization) {
            float panel_w = 420.0f;
            float panel_h = 260.0f;
            float panel_x = (window_width - panel_w) * 0.5f;
            float panel_y = (window_height - panel_h) * 0.5f;
            int style_mask = 1 << hair_style;
            bool style_unlocked = (hair_unlock_mask & style_mask) != 0;
            std::string style_label = std::string(hair_style_names[hair_style]) +
                (style_unlocked ? " (Unlocked)" : " (Cost " + std::to_string(hair_style_costs[hair_style]) + ")");
            draw_text_centered(panel_x, panel_y + panel_h + 16.0f, panel_w, 28.0f,
                               "Customize", ui_white, ui_projection);
            draw_text_centered(panel_x, panel_y - 6.0f, panel_w, 20.0f,
                               "Coins: " + std::to_string(coins), ui_white, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 62.0f, panel_w - 80.0f, 26.0f,
                               "Hairstyle: " + style_label,
                               customize_menu_index == 0 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 114.0f, panel_w - 80.0f, 26.0f,
                               std::string("Mohawk Color #") + std::to_string(hair_color + 1),
                               customize_menu_index == 1 ? ui_highlight_text : ui_dim,
                               ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 166.0f, panel_w - 80.0f, 26.0f,
                               "Back", customize_menu_index == 2 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x, panel_y - 22.0f, panel_w, 20.0f,
                               "Left/Right to adjust", ui_dim, ui_projection);
        } else if (state == GameState::PowerUps) {
            int cost_damage = 10 + 5 * meta_damage_level;
            int cost_speed = 8 + 4 * meta_speed_level;
            int cost_health = 12 + 6 * meta_max_health_level;
            int cost_cooldown = 12 + 6 * meta_attack_cooldown_level;
            int cost_magnet = 10 + 5 * meta_magnet_level;
            int cost_cross = 80;
            int cost_stick = 60;
            int cost_crossbow = 70;
            int cost_poison = 90;
            int cost_bat = 50;
            float panel_w = 420.0f;
            float panel_h = 600.0f;
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
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 234.0f, panel_w - 80.0f, 26.0f,
                               "Magnet +" + std::to_string(meta_magnet_level) + " (Cost " +
                                   std::to_string(cost_magnet) + ")",
                               powerup_menu_index == 4 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 278.0f, panel_w - 80.0f, 26.0f,
                               unlock_cross ? "Cross (Unlocked)" : "Unlock Cross (Cost 80)",
                               powerup_menu_index == 5 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 322.0f, panel_w - 80.0f, 26.0f,
                               unlock_stick ? "Stake (Unlocked)" : "Unlock Stake (Cost 60)",
                               powerup_menu_index == 6 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 366.0f, panel_w - 80.0f, 26.0f,
                               unlock_crossbow ? "Crossbow (Unlocked)" : "Unlock Crossbow (Cost 70)",
                               powerup_menu_index == 7 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 410.0f, panel_w - 80.0f, 26.0f,
                               unlock_poison ? "Caltrops (Unlocked)" : "Unlock Caltrops (Cost 90)",
                               powerup_menu_index == 8 ? ui_highlight_text : ui_dim, ui_projection);
            draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 454.0f, panel_w - 80.0f, 26.0f,
                               unlock_bat ? "Bat Pet (Unlocked)" : "Unlock Bat Pet (Cost 50)",
                               powerup_menu_index == 9 ? ui_highlight_text : ui_dim, ui_projection);
            if (!skin_unlocked) {
                draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 498.0f, panel_w - 80.0f, 26.0f,
                                   "Unlock Skin (Cost 60)",
                                   powerup_menu_index == 10 ? ui_highlight_text : ui_dim, ui_projection);
            } else {
                draw_text_centered(panel_x + 40.0f, panel_y + panel_h - 498.0f, panel_w - 80.0f, 26.0f,
                                   std::string("Toggle Skin (") + (skin_selected ? "On" : "Off") + ")",
                                   powerup_menu_index == 10 ? ui_highlight_text : ui_dim, ui_projection);
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
            draw_text(margin + 6.0f, time_text_y - font_height - 6.0f,
                      "Points " + std::to_string(points),
                      ui_white, ui_projection);
            if (freeze_timer > 0.0f) {
                draw_text(margin + 6.0f, time_text_y + font_height + 4.0f,
                          "Freeze " + std::to_string(static_cast<int>(std::ceil(freeze_timer))) + "s",
                          ui_highlight, ui_projection);
            }
            int nearby_town = -1;
            float town_dist_sq = 2.8f * 2.8f;
            for (int i = 0; i < static_cast<int>(towns.size()); ++i) {
                glm::vec3 delta = towns[i].position - player_position;
                delta.y = 0.0f;
                float dist = glm::dot(delta, delta);
                if (dist < town_dist_sq) {
                    nearby_town = i;
                    town_dist_sq = dist;
                }
            }
            if (nearby_town >= 0) {
                std::string label = towns[nearby_town].ui_open ? "Town Open (T to close)"
                                                               : "Press T to open Town";
                draw_text(margin + 6.0f, time_text_y - font_height * 2.0f - 12.0f,
                          label, ui_white, ui_projection);
            }
            int open_town_index = -1;
            for (int i = 0; i < static_cast<int>(towns.size()); ++i) {
                if (towns[i].ui_open) {
                    open_town_index = i;
                    break;
                }
            }
            if (open_town_index >= 0) {
                Town& town = towns[open_town_index];
                int barracks_cost = 6 + town.barracks_level * 6;
                int armory_cost = 8 + town.armory_level * 8;
                int watch_cost = 10 + town.watchtower_level * 10;
                float panel_w = 300.0f;
                float panel_h = 170.0f;
                float panel_x = margin;
                float panel_y = time_text_y - panel_h - font_height;
                glUniform1f(alpha_location, 0.75f);
                draw_ui_quad(panel_x, panel_y, panel_w, panel_h,
                             glm::vec3(0.05f, 0.07f, 0.10f), ui_projection);
                glUniform1f(alpha_location, 1.0f);

                float text_x = panel_x + 10.0f;
                float y = panel_y + panel_h - 28.0f;
                draw_text(text_x, y, "Town Upgrades", ui_white, ui_projection);
                draw_text_with_font(ui_font_small, text_x + 170.0f, y + 4.0f,
                                    "Points " + std::to_string(points),
                                    ui_white, ui_projection);
                y -= 26.0f;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "1) Barracks", ui_white, ui_projection);
                y -= 18.0f;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Level " + std::to_string(town.barracks_level) +
                                        "  Spawns every " +
                                        std::to_string(static_cast<int>(
                                            glm::max(4.0f, 12.0f - 2.0f * static_cast<float>(town.barracks_level)))) + "s",
                                    ui_white, ui_projection);
                y -= 18.0f;
                SDL_Color cost_color = points >= barracks_cost ? ui_white : ui_dim;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Cost " + std::to_string(barracks_cost), cost_color, ui_projection);
                y -= 24.0f;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "2) Armory", ui_white, ui_projection);
                y -= 18.0f;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Level " + std::to_string(town.armory_level) +
                                        "  +DMG/HP " + std::to_string(town.armory_level),
                                    ui_white, ui_projection);
                y -= 18.0f;
                cost_color = points >= armory_cost ? ui_white : ui_dim;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Cost " + std::to_string(armory_cost), cost_color, ui_projection);
                y -= 24.0f;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "3) Watchtower", ui_white, ui_projection);
                y -= 18.0f;
                float wt_interval = 1.1f / (1.0f + 0.35f * static_cast<float>(town.watchtower_level));
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Level " + std::to_string(town.watchtower_level) +
                                        "  " + std::to_string(static_cast<int>(std::ceil(1.0f / glm::max(0.01f, wt_interval)))) + " shots/s",
                                    ui_white, ui_projection);
                y -= 18.0f;
                cost_color = points >= watch_cost ? ui_white : ui_dim;
                draw_text_with_font(ui_font_small, text_x, y,
                                    "Cost " + std::to_string(watch_cost), cost_color, ui_projection);
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
                    " | Start Run | Power Ups | Customize | Quit (Enter)";
        } else if (state == GameState::Customization) {
            title = "CUSTOMIZE | Left/Right Change | Enter Buy/Select | ESC Back";
        } else if (state == GameState::PowerUps) {
            int cost_damage = 10 + 5 * meta_damage_level;
            int cost_speed = 8 + 4 * meta_speed_level;
            int cost_health = 12 + 6 * meta_max_health_level;
            int cost_cooldown = 12 + 6 * meta_attack_cooldown_level;
            int cost_magnet = 10 + 5 * meta_magnet_level;
            title = "POWER UPS | Coins " + std::to_string(coins) +
                    " | [1] Damage +" + std::to_string(meta_damage_level) +
                    " (" + std::to_string(cost_damage) + ")" +
                    " | [2] Speed +" + std::to_string(meta_speed_level) +
                    " (" + std::to_string(cost_speed) + ")" +
                    " | [3] Max HP +" + std::to_string(meta_max_health_level) +
                    " (" + std::to_string(cost_health) + ")" +
                    " | [4] Cooldown +" + std::to_string(meta_attack_cooldown_level) +
                    " (" + std::to_string(cost_cooldown) + ")" +
                    " | [5] Magnet +" + std::to_string(meta_magnet_level) +
                    " (" + std::to_string(cost_magnet) + ")" +
                    " | [6] Cross " + std::string(unlock_cross ? "(Unlocked)" : "(80)") +
                    " | [7] Stake " + std::string(unlock_stick ? "(Unlocked)" : "(60)") +
                    " | [8] Crossbow " + std::string(unlock_crossbow ? "(Unlocked)" : "(70)") +
                    " | [9] Caltrops " + std::string(unlock_poison ? "(Unlocked)" : "(90)") +
                    " | [10] Bat " + std::string(unlock_bat ? "(Unlocked)" : "(50)") +
                    " | [11] Skin";
            if (!skin_unlocked) {
                title += " (60)";
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
    glDeleteVertexArrays(1, &ramp_wall_vao);
    glDeleteBuffers(1, &ramp_wall_vbo);
    glDeleteVertexArrays(1, &ramp_surface_vao);
    glDeleteBuffers(1, &ramp_surface_vbo);
    glDeleteVertexArrays(1, &ramp_side_vao);
    glDeleteBuffers(1, &ramp_side_vbo);
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
    for (Mesh& mesh : enemy_meshes) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
    }
    for (Mesh& mesh : rock_meshes_low) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
    }
    for (Mesh& mesh : rock_meshes) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
    }
    glDeleteVertexArrays(1, &town_hall_mesh.vao);
    glDeleteBuffers(1, &town_hall_mesh.vbo);
    glDeleteVertexArrays(1, &barracks_mesh.vao);
    glDeleteBuffers(1, &barracks_mesh.vbo);
    glDeleteVertexArrays(1, &armory_mesh.vao);
    glDeleteBuffers(1, &armory_mesh.vbo);
    glDeleteVertexArrays(1, &watchtower_mesh.vao);
    glDeleteBuffers(1, &watchtower_mesh.vbo);
    for (Mesh& mesh : house_base_meshes) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
    }
    for (Mesh& mesh : house_roof_meshes) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
    }
    glDeleteVertexArrays(1, &tree_mesh.vao);
    glDeleteBuffers(1, &tree_mesh.vbo);
    glDeleteVertexArrays(1, &tree_mesh_low.vao);
    glDeleteBuffers(1, &tree_mesh_low.vbo);
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
