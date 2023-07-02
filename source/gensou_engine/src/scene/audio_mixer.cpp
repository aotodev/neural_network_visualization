#include "scene/audio_mixer.h"

#include "core/core.h"
#include "core/log.h"
#include "core/engine_events.h"

namespace gs {

    audio_mixer::audio_mixer(const std::string& mixerName)
        : m_mute(false), m_mixer_volume(1.0f)
    {
        auto result = ma_engine_init(NULL, &m_engine);
        if(result != MA_SUCCESS)
        {
            LOG_ENGINE(error, "failed to initialized audio mixer '%s' with error '%s'", mixerName.c_str(), ma_result_description(result));
            return;
        }

        engine_events::window_minimize.subscribe(BIND_MEMBER_FUNCTION(audio_mixer::on_window_minimized));

        LOG_ENGINE(trace, "audio mixer '%s' initialized", mixerName.c_str());
        m_mixer_name = mixerName;
    }

    audio_mixer::~audio_mixer()
    {
        if(!m_mixer_name.empty())
            LOG_ENGINE(trace, "destroying audio mixer with name '%s'", m_mixer_name.c_str());

        ma_engine_uninit(&m_engine);
    }

    void audio_mixer::set_volume(float volume)
    {
        m_mixer_volume = volume;

        if(!m_mute)
            ma_engine_set_volume(&m_engine, m_mixer_volume);
    }

    void audio_mixer::mute()
    {
        m_mute = true;
        ma_engine_set_volume(&m_engine, 0.0f);
    }

    void audio_mixer::unmute()
    {
        m_mute = false;
        ma_engine_set_volume(&m_engine, m_mixer_volume);
    }

    bool audio_mixer::add_audio_clip(ma_sound* pSound, void* audiobuffer)
    {
        auto result = ma_sound_init_from_data_source(&m_engine, audiobuffer, 0, NULL, pSound);

        if(result != MA_SUCCESS)
        {
            LOG_ENGINE(error, "audio mixer failed to create audio clip with error '%s'", ma_result_description(result));
            return false;
        }

        return true;
    }

    bool audio_mixer::is_mute()
    { 
        return m_mute;
    }

    float audio_mixer::get_volume()
    { 
        return m_mixer_volume;
    }

    std::string audio_mixer::get_name()
    { 
        return m_mixer_name;
    }

    uint64_t audio_mixer::get_engine_time() const 
    {
        return ma_engine_get_time_in_pcm_frames(&m_engine);
    }

    uint64_t audio_mixer::get_engine_time_milliseconds() const 
    {
        return ma_engine_get_time_in_milliseconds(&m_engine);
    }

    void audio_mixer::reset_engine_time()
    {
        ma_engine_set_time_in_milliseconds(&m_engine, 0);
    }

    uint64_t audio_mixer::get_sample_rate() const 
    {
        return ma_engine_get_sample_rate(&m_engine);
    }

    void audio_mixer::set_paused(bool pause)
    {
        m_paused = pause;

        if(!pause)
        {
            ma_engine_start(&m_engine);

            if(!m_mute)
                ma_engine_set_volume(&m_engine, m_mixer_volume);
        }
        else
        {
            ma_engine_stop(&m_engine);

            if(!m_mute)
                ma_engine_set_volume(&m_engine, 0.0f);
        }       
    }

    void audio_mixer::on_window_minimized(bool minimized)
    {
        LOG_ENGINE(warn, "calling audio_mixer::on_window_minimized with value %s", minimized ? "true" : "false");

        if(!minimized)
        {
            if(!m_paused)
            {
                ma_engine_start(&m_engine);

                if(!m_mute)
                    ma_engine_set_volume(&m_engine, m_mixer_volume);
            }
        }
        else
        {
            ma_engine_stop(&m_engine);

            if(!m_mute)
                ma_engine_set_volume(&m_engine, 0.0f);
        }
    }

}