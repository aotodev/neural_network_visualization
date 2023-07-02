#pragma once

#include "scene_camera.h"
#include <future>
#include <gensou/core.h>
#include <gensou/scene.h>
#include <gensou/components.h>
#include <gensou/scene_actor.h>
#include <gensou/renderer/lines.h>

class application_scene : public gs::scene
{
public:
    application_scene() { set_loading_scene_min_duration(1.0f); }

    virtual void on_init() override;
    virtual void on_start() override;
    virtual void on_update(float dt) override;
    virtual void on_terminate() override;

    void next_data_point();

    void turn_off_base_connections() { m_line_renderer.set_invisible(); }
    void turn_on_base_connections() { m_line_renderer.set_visible(); }

    void set_camera_orbiting(bool b) { if(m_camera) m_camera->set_orbit(b); }

private:
    void generate_ann_model();

    /* will set the weight's intensity based on their neuron's intensity */
    void forward_pass(uint32_t dataPoint);

    void turn_off();
    void turn_on_layer(uint32_t layer);
    void turn_on_neurons(uint32_t layer);

    void swap_neuron_frame(uint32_t activeFrame);

private:
    std::shared_ptr<class model> m_model;
    std::shared_ptr<scene_camera> m_camera;
    gs::game_object m_line_renderer; // base conections

    /* computation of the next batch will be done asynchronously. we will have 2 copies of the data and 
     * alternate between them in the same way we do with vulkan resources
     */
    std::array<std::vector<gs::game_object>, 2> m_weights;
    std::array<std::vector<gs::game_object>, 2> m_neurons;
    std::array<std::future<void>, 2> m_futures;

    uint32_t m_local_frame = 0;
    uint32_t m_frame_count = 2;

    /* to hold the outputs during computation */
    std::array<std::vector<float>, 2> m_neuron_outputs;
    std::vector<float> m_soybean_data;

    uint32_t m_current_data_point = 0;
    uint32_t m_last_data_point = 2048;

    glm::vec4 m_base_synapses_color = gs::normalized_color<0x57, 0xA0, 0xD3>;
    glm::vec4 m_base_segment_color = { 0.2f, 0.2f, 0.2f, 0.4f };

    struct animation_data
    {
        float counter = 0.0f, per_layer_duration = 0.4f, lit_duration = 1.2f;
        uint32_t current_layer = 0, layer_count = 2;
        bool animating = false, waiting = true;

    } m_animation;
};
