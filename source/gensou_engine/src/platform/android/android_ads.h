#pragma once

#include "core/core.h"

namespace gs::ads {

	using ad_reward_fn = std::function<void(bool)>;

	enum class ad_state { not_init = 0, loading = 1, loaded, failed_loading };

	extern void load_all();

	extern void load_banner_ad();
	extern void show_banner_ad();

	extern void load_interstitial_ad();
	extern void show_interstitial_ad();

	extern void load_rewarded_video_ad();
	extern void show_rewarded_video_ad();

	extern ad_state get_rewarded_video_ad_state();

	extern void set_on_reward_action(ad_reward_fn action);
	extern void clear_on_reward_action();

}