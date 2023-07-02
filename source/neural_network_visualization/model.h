#pragma once

#include "activation_functions.hpp"

#include <gensou/core.h>
#include <gensou/scene_actor.h>

class model : public gs::scene_actor
{
public:

	virtual void on_init() override;

	void load(const std::string& path);

	uint32_t neuron_count() { return biases.size(); }
	uint32_t weights_count() { return weights.size(); }
	uint32_t input_count() { return layout[0].first; }
	uint32_t output_count() { return layout.back().second; }

	/* from layer 0 to layers.size + 1 for the output offset */
	uint32_t get_layer_offset(uint32_t layer) const { return m_neuron_offsets[layer]; }

	std::vector<float> biases;
	std::vector<float> weights;

	std::vector<std::pair<uint32_t, uint32_t>> layout;
	std::vector<uint32_t> m_neuron_offsets;

	relu activation_fn;	

};
