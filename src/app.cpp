#include "app.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vks {
void Application::run() {
    glm::mat4 view =
        glm::lookAt(glm::vec3{30.0f, 30.0f, 30.0f}, glm::vec3{0.0f, 0.0f, 0.0f},
                    glm::vec3{0.0f, 0.0f, 1.0f});
    glm::mat4 proj =
        glm::perspective(glm::radians(60.0f), m_window.aspect(), 0.1f, 100.0f);
    glm::mat4 proj_view = proj * view;

    float angle{0.0f};
    float rotation_speed{15.0f};
    glm::vec3 rotation_axis{0.0f, 0.0f, 1.0f};

    std::chrono::steady_clock clock{};
    auto last_time = clock.now();

    auto model = m_models.begin()->second;
    auto pipeline = m_pipelines.begin()->second;
    while (m_running) {
        auto current_time = clock.now();
        float d_time =
            std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        angle += rotation_speed * d_time;
        glm::mat4 model_transform =
            glm::rotate(glm::mat4{1.0f}, glm::radians(angle), rotation_axis);

        m_window.poolEvents();
        m_context.beginFrame(proj_view);
        m_context.bindPipeline(pipeline);
        m_context.draw(model, model_transform);
        m_context.endFrame();
    }
}
}  // namespace vks