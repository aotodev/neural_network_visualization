#pragma once

#include "core/core.h"

PUSH_IGNORE_WARNING
#include <miniaudio.h>
POP_IGNORE_WARNING

namespace gs {

    class audio_mixer
    {
        friend class scene;
    public:
        audio_mixer(const std::string& mixerName);
        ~audio_mixer();

        audio_mixer(const audio_mixer&) = delete;
        audio_mixer& operator=(const audio_mixer&) = delete;

        void set_volume(float volume);
        void mute();
        void unmute();

        /* unlike mute/unmute, this function will only do anything if the mixer is not mute
         * it will also pause/unpause execution
         */
        void set_paused(bool pause);

        bool is_mute();
        float get_volume();
        std::string get_name();

        /* get time in milliseconds */
        uint64_t get_engine_time() const;
        uint64_t get_engine_time_milliseconds() const;
        void reset_engine_time();
        
        uint64_t get_sample_rate() const;

        bool add_audio_clip(ma_sound* pSound, void* audiobuffer);

    private:
        void on_window_minimized(bool minimized);

    private:
    	ma_engine m_engine;

        float m_mixer_volume = 1.0f;// to cache latest volume for when unmuting
		bool m_mute = false;
        bool m_paused = false;

        std::string m_mixer_name;
    };

}