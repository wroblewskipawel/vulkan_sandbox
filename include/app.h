#pragma once

#include <iostream>
#include <unordered_map>

#include "resources.h"
#include "vk/context.h"
#include "vk/device.h"
#include "window.h"

using namespace std::string_literals;

namespace vks {

class Application {
   public:
    Application(uint32_t window_width, uint32_t window_height,
                const std::string& name,
                const std::string& graphics_pipeline_source)
        : m_window{window_width, window_height, name},
          m_device{m_window},
          m_context{m_device} {
        m_window.registerCloseCallback([this]() { m_running = false; });

        Resources resources;
        Model::load("assets/obj/viking_room/viking_room.obj"s, resources);

        std::cout << "\nRESOURCES:\n";
        for (auto& [name, model] : resources.models) {
            std::cout << "Model: " << name << std::endl;
        }

        m_models.merge(m_context.loadResources(
            {"viking_room.mesh_all1_Texture1_0.mat0"s}, resources));
        m_pipelines.emplace("diffuse"s,
                            m_context.loadPipeline("shaders/diffuse"s));
        std::cout << '\n';
        m_running = true;
    }
    void run();

   private:
    Window m_window;
    Device m_device;
    Context m_context;

    bool m_running;
    std::unordered_map<std::string, ModelHandle> m_models;
    std::unordered_map<std::string, PipelineHandle> m_pipelines;
};
}  // namespace vks