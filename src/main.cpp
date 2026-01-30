#include <cstdlib>
#include <iostream>
#include <cmath>
#include <fstream>
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
        "uniform mat4 uMVP;\n"
        "void main() {\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* fragment_source =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 uColor;\n"
        "void main() {\n"
        "    FragColor = vec4(uColor, 1.0);\n"
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

    float ground_vertices[] = {
        -10.0f, 0.0f, -10.0f,
         10.0f, 0.0f, -10.0f,
         10.0f, 0.0f,  10.0f,

        -10.0f, 0.0f, -10.0f,
         10.0f, 0.0f,  10.0f,
        -10.0f, 0.0f,  10.0f
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

    GLuint ground_vao = 0;
    GLuint ground_vbo = 0;
    glGenVertexArrays(1, &ground_vao);
    glGenBuffers(1, &ground_vbo);
    glBindVertexArray(ground_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ground_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ground_vertices), ground_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
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

    GLint mvp_location = glGetUniformLocation(program, "uMVP");
    GLint color_location = glGetUniformLocation(program, "uColor");
    GLint text_mvp_location = glGetUniformLocation(text_program, "uMVP");
    GLint text_tint_location = glGetUniformLocation(text_program, "uTint");
    GLint text_texture_location = glGetUniformLocation(text_program, "uTexture");

    TTF_Font* ui_font = LoadUIFont(18);
    if (!ui_font) {
        std::cerr << "Failed to load a UI font (try adding one in assets/)." << std::endl;
    }

    struct Enemy {
        glm::vec3 position;
        float speed;
        int health;
    };

    struct ExperienceGem {
        glm::vec3 position;
    };

    struct Projectile {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        int damage;
    };

    enum class ItemId {
        Fireball,
        FireballUpgrade,
        GarlicUpgrade,
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
    const glm::vec3 camera_offset(0.0f, 5.0f, 7.0f);
    const float play_area_extent = 12.0f;

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> side_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> edge_dist(-play_area_extent, play_area_extent);
    std::uniform_real_distribution<float> speed_dist(1.2f, 2.4f);
    std::uniform_real_distribution<float> unit_dist(0.0f, 1.0f);

    std::vector<Enemy> enemies;
    std::vector<ExperienceGem> gems;
    std::vector<Projectile> projectiles;
    float spawn_timer = 0.0f;
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

    std::vector<ItemChoice> current_choices;

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
        int health = 3 + player_level / 3;
        enemies.push_back(Enemy{position, speed_dist(rng), health});
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
        current_choices.clear();

        player_position = glm::vec3(0.0f, 0.0f, 0.0f);
        player_level = 1;
        player_xp = 0;
        player_max_health = 10 + meta_max_health_level;
        player_health = player_max_health;
        player_speed = 4.0f * (1.0f + 0.05f * static_cast<float>(meta_speed_level));
        attack_interval = glm::max(0.4f, 1.0f - 0.05f * static_cast<float>(meta_attack_cooldown_level));
        attack_radius = 2.2f;
        attack_damage = 1 + meta_damage_level;

        garlic_level = 1;
        fireball_level = 0;
        fireball_timer = 0.0f;
        fireball_cooldown = 1.5f;
        fireball_damage = 2 + meta_damage_level;

        spawn_timer = 0.0f;
        attack_timer = 0.0f;
        player_damage_timer = 0.0f;
        run_time = 0.0f;
        enemies_killed = 0;
        coins_earned = 0;
        victory = false;
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

    auto draw_text = [&](float x, float y, const std::string& text, SDL_Color color,
                         const glm::mat4& projection) {
        if (!ui_font) {
            return;
        }
        TextTexture texture = CreateTextTexture(ui_font, text, color);
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

    Uint64 last_ticks = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
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
            }
        }

        if (state == GameState::Running) {
            run_time += delta_time;
            if (run_time >= win_time) {
                end_run(true);
            }
            if (state == GameState::Running) {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            glm::vec3 move(0.0f);
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
                move.z -= 1.0f;
            }
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
                move.z += 1.0f;
            }
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
                move.x -= 1.0f;
            }
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
                move.x += 1.0f;
            }
            if (glm::length(move) > 0.0f) {
                move = glm::normalize(move);
            }
            player_position += move * player_speed * delta_time;
            player_position.x = glm::clamp(player_position.x, -play_area_extent, play_area_extent);
            player_position.z = glm::clamp(player_position.z, -play_area_extent, play_area_extent);

            float spawn_interval = glm::max(0.6f, base_spawn_interval - player_level * 0.05f);
            spawn_timer += delta_time;
            if (spawn_timer >= spawn_interval) {
                spawn_timer = 0.0f;
                spawn_enemy();
            }

            for (Enemy& enemy : enemies) {
                glm::vec3 to_player = player_position - enemy.position;
                to_player.y = 0.0f;
                float distance = glm::length(to_player);
                if (distance > 0.001f) {
                    glm::vec3 direction = to_player / distance;
                    enemy.position += direction * enemy.speed * delta_time;
                }
            }

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

            fireball_timer += delta_time;
            if (fireball_level > 0 && fireball_timer >= fireball_cooldown && !enemies.empty()) {
                fireball_timer = 0.0f;
                int projectile_count = 1 + (fireball_level - 1) / 2;
                for (int p = 0; p < projectile_count; ++p) {
                    size_t target_index = static_cast<size_t>(
                        std::floor(unit_dist(rng) * enemies.size()));
                    if (target_index >= enemies.size()) {
                        target_index = enemies.size() - 1;
                    }
                    glm::vec3 to_target = enemies[target_index].position - player_position;
                    to_target.y = 0.0f;
                    if (glm::length(to_target) < 0.001f) {
                        to_target = glm::vec3(0.0f, 0.0f, -1.0f);
                    }
                    glm::vec3 direction = glm::normalize(to_target);
                    projectiles.push_back(Projectile{
                        player_position + glm::vec3(0.0f, 0.6f, 0.0f),
                        direction * 7.0f,
                        3.0f,
                        fireball_damage});
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
            for (const Enemy& enemy : enemies) {
                glm::vec3 delta = enemy.position - player_position;
                delta.y = 0.0f;
                if (glm::length(delta) <= player_contact_radius) {
                    player_contact = true;
                    break;
                }
            }
            player_damage_timer += delta_time;
            if (player_contact && player_damage_timer >= player_damage_interval) {
                player_damage_timer = 0.0f;
                player_health -= 1;
            }

            for (size_t i = 0; i < enemies.size();) {
                if (enemies[i].health <= 0) {
                    gems.push_back(ExperienceGem{enemies[i].position});
                    enemies_killed += 1;
                    coins_earned += 1;
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
            100.0f);
        glm::mat4 view = glm::lookAt(
            player_position + camera_offset,
            player_position,
            glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 ground_model(1.0f);
        glm::mat4 ground_mvp = projection * view * ground_model;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(ground_mvp));
        glUniform3f(color_location, 0.12f, 0.16f, 0.18f);
        glBindVertexArray(ground_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glm::mat4 player_model = glm::translate(
            glm::mat4(1.0f),
            player_position + glm::vec3(0.0f, 0.5f, 0.0f));
        glm::mat4 player_mvp = projection * view * player_model;
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(player_mvp));
        if (skin_selected) {
            glUniform3f(color_location, 0.35f, 0.20f, 0.85f);
        } else {
            glUniform3f(color_location, 0.92f, 0.16f, 0.20f);
        }
        glBindVertexArray(cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        for (const Enemy& enemy : enemies) {
            glm::mat4 enemy_model = glm::translate(
                glm::mat4(1.0f),
                enemy.position + glm::vec3(0.0f, 0.35f, 0.0f));
            enemy_model = glm::scale(enemy_model, glm::vec3(0.7f));
            glm::mat4 enemy_mvp = projection * view * enemy_model;
            glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(enemy_mvp));
            glUniform3f(color_location, 0.75f, 0.38f, 0.18f);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
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
    glDeleteProgram(program);
    glDeleteProgram(text_program);

    if (ui_font) {
        TTF_CloseFont(ui_font);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return EXIT_SUCCESS;
}
