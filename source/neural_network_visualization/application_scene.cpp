#include "application_scene.h"
#include "core/misc.h"
#include "scene/components.h"
#include "scene_camera.h"
#include "model.h"
#include "soybean_data.h"
#include "widgets.h"
#include "simd.hpp"

#include <gensou/game_statics.h>
#include <limits>


static std::random_device s_randomDevice;
static std::default_random_engine s_randomEngine(s_randomDevice());

void application_scene::on_init()
{
    has_physics = false;
    set_const_base_unit(32.0f);

    /* scene camera */
    {
        auto [obj, instance] = gs::game_statics::create_game_object<scene_camera>("application camera", {});
        m_camera = instance;
    }

    /* Widgets */
    gs::game_statics::create_game_object<application_widgets>("application widgets", {});

    /* Model */
    {
        auto [obj, instance] = gs::game_statics::create_game_object<model>("ann model", {});
        m_model = instance;
        
        #ifdef APP_ANDROID
        m_model->load("resources/model.gsasset");
        #else
        m_model->load("resources/model_1.gsasset");
        #endif
    }

    m_animation.layer_count = m_model->layout.size();
    generate_ann_model();

    m_soybean_data = load_soybean_series("resources/soybean.csv.gsasset", true, 2057);

    /* set first set */
    forward_pass(0);
    turn_off();
}

void application_scene::on_start()
{
    gs::system::set_clear_value({ 0.0f, 0.0f, 0.05f, 1.0f });

    m_animation.animating = true;
    m_animation.waiting = false;
    next_data_point();
}

void application_scene::on_update(float dt)
{
    if(m_animation.waiting)
    {
        /* check if the next batch is ready */
        if(!gs::is_future_ready(m_futures[m_local_frame]))
            return;

        /* turn off current buffer */
        turn_off();
        m_local_frame = (m_local_frame + 1) % m_frame_count;

        swap_neuron_frame(m_local_frame);

        /* turn on new frame's buffers */
        turn_on_neurons(m_animation.current_layer);
        turn_on_layer(m_animation.current_layer);

        m_animation.waiting = false;
        m_animation.animating = true;

        /* start computing next batch async */
        next_data_point();
    }

    m_animation.counter += dt;

    if(m_animation.animating)
    {
        auto& lines = m_weights[m_local_frame][m_animation.current_layer].get_component<gs::line_renderer_component>();
        lines.edge_range.x -= dt * (2.0f / m_animation.per_layer_duration);

        if(m_animation.counter >= m_animation.per_layer_duration)
        {
            m_animation.counter = 0.0f;
            lines.edge_range.x = 0.0f;
            m_animation.current_layer++;

            /* next layer */
            turn_on_neurons(m_animation.current_layer);
            turn_on_layer(m_animation.current_layer);

            /* model completely lit */
            if(m_animation.current_layer == m_animation.layer_count)
            {
                m_animation.animating = false;
                m_animation.current_layer = 0;
            }
        }
        return;
    }

    /* start next batch or wait for it to finish computing */
    if(m_animation.counter >= m_animation.lit_duration)
    {
        m_animation.waiting = true;
        m_animation.counter = 0.0f;
    }
}

void application_scene::on_terminate()
{
}

void application_scene::generate_ann_model()
{
    //--------------------------------------------------------------------------------------
    // Procedurally generate model based on its layout
    //--------------------------------------------------------------------------------------

    auto viewport = get_scene_viewport();
    float baseUnit = get_base_unit_in_pixels();

    /*------------------------------prepare-neuron-resources--------------------------------*/

    uint32_t totalCount = m_model->neuron_count() + m_model->input_count();
    std::vector<glm::vec3> neuronPositions;
    neuronPositions.reserve(totalCount);

    m_neurons[0].reserve(totalCount);
    m_neuron_outputs[0].resize(totalCount, 0.0f);
    m_neuron_outputs[1].resize(totalCount, 0.0f);

    /*------------------------------base-model-positions--------------------------------*/

    const float scale = viewport.x * 0.08f;
    const float strideX = scale * 0.2f * 2.5f, strideY = scale * 0.05f * 2.5f, strideZ = scale * 0.05f * 2.5f;

    /* so that neurons are not perfectly next to each other */
    std::uniform_real_distribution<float> uniform_distribution(0.0f, scale * 0.1f * 2.5f);

    /* get layer with biggest number of neurons to use as base */
    uint32_t squareMaxSide = m_model->input_count();
    for(auto [rows, columns] : m_model->layout)
        squareMaxSide = std::max(squareMaxSide, columns);

	uint32_t maxHeight = squareMaxSide / std::sqrt(squareMaxSide); // may not be a perfect square
	const float baseZ  = float(maxHeight/2) * strideZ;

    auto& camera = m_camera->get_game_object().get_component<gs::camera_component>();
    camera.set_look_at(0.0f, 0.0f, baseZ * baseUnit);

    float x = -strideX * float(m_model->layout.size() / 2);
    uint32_t count = 0;

    /*------------------------generate neurons and their positions-----------------------*/

    /* first generate all neurons for frame in flight 1, then deep copy everything into flight 2
     * the last thing we want is to have their resources intertwined in the components buffer
     */

    for(auto [rows, columns] : m_model->layout)
    {
        uint32_t height = std::sqrt(rows);
        uint32_t width = rows / height; // may not be a perfect square
        float initialZ = baseZ + float(width / 2) * strideZ;

        float z = initialZ;
        float y = (height / 2) * -strideY;

        auto& neurons = m_neurons[0];

        for(uint32_t i = 0; i < rows; i++)
        {
            auto& obj = neurons.emplace_back(create_object("neuron " + std::to_string(count)));
            obj.add_component<gs::cube_component>();

            float offX = uniform_distribution(s_randomEngine) * 2.0f;
            float offY = uniform_distribution(s_randomEngine) / 2.0f;
            float offZ = uniform_distribution(s_randomEngine);

            auto& transform = obj.get_component<gs::transform_component>();
            transform.translation.x = x - offX;
            transform.translation.y = y + offY;
            transform.translation.z = z - offZ;

            transform.scale *= scale;
            neuronPositions.push_back(transform.translation);

            z -= strideZ;
            count++;

            if((i + 1) % width == 0)
            {
                z = initialZ;
                y += strideY;
            }
        }

        float threshold = std::sqrt(rows + columns);
        if(threshold >= 16)
            x += strideX;

        x += strideX;
        z = initialZ;
    }

    /* output neuron(s) */
    {
        uint32_t localSize = m_model->output_count();
        uint32_t height = std::sqrt(localSize);
        uint32_t width = localSize / height;
        float initialZ = baseZ + float(width / 2) * strideZ;

        float z = initialZ;
        float y = (height / 2) * strideY;

        auto& neurons = m_neurons[0];

        for(uint32_t i = 0; i < localSize; i++)
        {
            auto& obj = neurons.emplace_back(create_object("neuron " + std::to_string(count)));
            obj.add_component<gs::cube_component>();

            auto& transform = obj.get_component<gs::transform_component>();
            transform.translation.x = x;
            transform.translation.y = y;
            transform.translation.z = z;

            transform.scale *= scale;
            neuronPositions.push_back(transform.translation);

            z -= strideZ;
            count++;

            if((i + 1) % width == 0)
            {
                z = initialZ;
                y += strideY;
            }           
        }
    }

    /*------------------------------set line data--------------------------------*/
    /* two sets, one for the base connections and one for the synapses */

    /* base connections */
    m_line_renderer = create_object("line renderer");
    auto& lineRenderer = m_line_renderer.add_component<gs::line_renderer_component>();
    lineRenderer.lines.reserve(m_model->weights.size());

    /* synapses */
    /* one game_object per layer */
    m_weights[0].reserve(m_model->layout.size());

    uint32_t row_i = 0, column_j = 0;
    for(auto [rows, columns] : m_model->layout)
    {
        auto& lineObj = m_weights[0].emplace_back(create_object("layer weight lines set 0"));

        auto& layerLines = lineObj.add_component<gs::line_renderer_component>();
        layerLines.lines.reserve(rows * columns);
        layerLines.edge_range.x = 2.0f;
        layerLines.edge_range.y = 0.05f;

        column_j += rows;
        for(uint32_t i = 0; i < rows; i++)
        {
            for(uint32_t j = 0; j < columns; j++)
            {
                auto& line = lineRenderer.lines.emplace_back();

                line.p1.position = neuronPositions[row_i + i] * baseUnit;
                line.p2.position = neuronPositions[column_j + j] * baseUnit;
                line.p1.color = m_base_segment_color;
                line.p2.color = m_base_segment_color;

                auto& layerLine = layerLines.lines.emplace_back();
                layerLine.p1.position = line.p1.position;
                layerLine.p2.position = line.p2.position;
                layerLine.p1.color = m_base_synapses_color;
                layerLine.p2.color = m_base_synapses_color;
            }
        }
        row_i += rows;
    }

    /* copy everything into their respective duplicate */
    /*-----------------------------------------------*/

    m_neurons[1].reserve(m_neurons[0].size());
    for(auto& obj : m_neurons[0])
    {
        auto& nObj = m_neurons[1].emplace_back(create_object("neuron " + std::to_string(count)));
        nObj.add_component<gs::cube_component>();

        auto& secondTransform = nObj.get_component<gs::transform_component>();
        auto& firstTransform = obj.get_component<gs::transform_component>();
        secondTransform = firstTransform;

        nObj.set_invisible();
    }

    m_weights[1].reserve(m_weights[0].size());
    for(auto& objLayer : m_weights[0])
    {
        auto& lineObj = m_weights[1].emplace_back(create_object("layer weight lines set 1"));
        auto& secondLines = lineObj.add_component<gs::line_renderer_component>();

        /* deep copy */
        auto& firstLines = objLayer.get_component<gs::line_renderer_component>();
        secondLines = firstLines;

        lineObj.set_invisible();
        secondLines.edge_range.x = 2.0f;
    }
}

void application_scene::forward_pass(uint32_t dataPoint)
{
    auto& neuronOutputs = m_neuron_outputs[m_local_frame];
    simd::set_to_zero(neuronOutputs.data(), neuronOutputs.size());

    /*--------------------input data------------------------------------------*/
    uint32_t inputsize = m_model->input_count();
    for(uint32_t i = 0; i < inputsize; i++)
        neuronOutputs[i] = std::max(m_soybean_data[dataPoint + i], 0.11f);

    /*--------------------offsets---------------------------------------------*/
	size_t biasesOffset = 0, weightsOffset = 0, outputOffset = inputsize;
    uint32_t row_i = 0;

    const float* input = &m_soybean_data[dataPoint];
    uint32_t layerIndex = 0;
    /*------------------------------------------------------------------------*/
    
    for (auto& [rowCount, columnCount] : m_model->layout)
    {
        simd::vec_mat_mul(input, &m_model->weights[weightsOffset], &neuronOutputs[outputOffset], rowCount, columnCount);
        m_model->activation_fn.add_bias_activation(&neuronOutputs[outputOffset], &m_model->biases[biasesOffset], columnCount);

        input = neuronOutputs.data() + outputOffset;

        outputOffset += columnCount;
        biasesOffset += columnCount;
        weightsOffset += columnCount * rowCount;

        auto& layerLines = m_weights[m_local_frame][layerIndex].get_component<gs::line_renderer_component>();

        for(uint32_t i = 0; i < rowCount; i++)
        {
            float output = neuronOutputs[row_i + i];
            output = std::clamp(output, 0.0f, 1.0f);

            /* update current layer's synapses */
            for(uint32_t j = 0; j < columnCount; j++)
            {
                auto& segment = layerLines.lines[i * columnCount + j];

                auto color = glm::vec3(m_base_synapses_color);
                segment.p1.color = glm::vec4(color, output * 2.0f);
                segment.p2.color = glm::vec4(color, output * 2.0f);
            }
        }

        row_i += rowCount;
        layerIndex++;
    }
}

void application_scene::next_data_point()
{
    m_current_data_point = (m_current_data_point + 8) % m_last_data_point;

    m_futures[m_local_frame] = gs::system::run_async([this]()
    {
        forward_pass(m_current_data_point);
    });
}

void application_scene::turn_off()
{
    for(auto obj : m_neurons[m_local_frame])
    {
        auto& cube = obj.get_component<gs::cube_component>();
        cube.color = { 0.2f, 0.2f, 0.2f, 0.16f };
    }

    for(auto obj : m_weights[m_local_frame])
    {
        obj.set_invisible();
        auto& lines= obj.get_component<gs::line_renderer_component>();
        lines.edge_range.x = 2.0f;
    }
}

void application_scene::turn_on_layer(uint32_t layer)
{
    auto& weights = m_weights[m_local_frame];

    if(layer > 0)
    {
        weights[layer - 1].set_invisible();
    }

    if(layer >= m_model->layout.size())
        return;

    weights[layer].set_visible();
    auto& layerLines = weights[layer].get_component<gs::line_renderer_component>();
    layerLines.edge_range.x = 2.0f;
}

void application_scene::turn_on_neurons(uint32_t layer)
{
    uint32_t count = layer < m_model->layout.size() ?
         m_model->layout[layer].first : m_model->layout[layer - 1].second;
         
    uint32_t offset = m_model->get_layer_offset(layer);

    for(uint32_t i = offset; i < offset + count; i++)
    {
        auto& cube = m_neurons[m_local_frame][i].get_component<gs::cube_component>();
        float output = std::clamp(m_neuron_outputs[m_local_frame][i], 0.0f, 1.0f);
        cube.color = { output + 0.2f, output + 0.2f, output + 0.2f, output + 0.1 };
    }
}

void application_scene::swap_neuron_frame(uint32_t activeFrame)
{
    uint32_t inactiveFrame = (activeFrame + 1) % m_frame_count;
    for(auto& obj : m_neurons[activeFrame])
        obj.set_visible();

    for(auto& obj : m_neurons[inactiveFrame])
        obj.set_invisible();
}