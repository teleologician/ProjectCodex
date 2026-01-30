#include <cstdlib>
#include <iostream>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <SDL2/SDL.h>
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
}  // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
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
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
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

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GLuint program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
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

    GLint mvp_location = glGetUniformLocation(program, "uMVP");
    GLint color_location = glGetUniformLocation(program, "uColor");

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

    bool menu_open = false;
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
            } else if (event.type == SDL_KEYDOWN && menu_open) {
                int index = -1;
                if (event.key.keysym.sym == SDLK_1) {
                    index = 0;
                } else if (event.key.keysym.sym == SDLK_2) {
                    index = 1;
                } else if (event.key.keysym.sym == SDLK_3) {
                    index = 2;
                }
                if (index >= 0 && index < static_cast<int>(current_choices.size())) {
                    ItemId choice = current_choices[index].id;
                    switch (choice) {
                        case ItemId::Fireball:
                            fireball_level = 1;
                            fireball_damage = 2;
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
                    menu_open = false;
                }
            }
        }

        if (player_health <= 0) {
            running = false;
        }

        if (!menu_open) {
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
                menu_open = true;

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
                    menu_open = false;
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
        glUniform3f(color_location, 0.92f, 0.16f, 0.20f);
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

        if (menu_open && !current_choices.empty()) {
            std::string title = "LEVEL UP! 1) ";
            title += current_choices[0].name;
            if (current_choices.size() > 1) {
                title += "  2) ";
                title += current_choices[1].name;
            }
            if (current_choices.size() > 2) {
                title += "  3) ";
                title += current_choices[2].name;
            }
            SDL_SetWindowTitle(window, title.c_str());
        } else {
            int xp_needed = 5 + (player_level - 1) * 2;
            std::string hud = "HP " + std::to_string(player_health) + "/" +
                              std::to_string(player_max_health) +
                              " | LV " + std::to_string(player_level) +
                              " XP " + std::to_string(player_xp) + "/" +
                              std::to_string(xp_needed) +
                              " | Enemies " + std::to_string(enemies.size());
            SDL_SetWindowTitle(window, hud.c_str());
        }

        SDL_GL_SwapWindow(window);
    }

    glDeleteVertexArrays(1, &ground_vao);
    glDeleteBuffers(1, &ground_vbo);
    glDeleteVertexArrays(1, &cube_vao);
    glDeleteBuffers(1, &cube_vbo);
    glDeleteProgram(program);

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
