#pragma once

#include "core/core.h"
#include "scene/audio_mixer.h"
#include "scene/game_object.h"

PUSH_IGNORE_WARNING
#include <miniaudio.h>
POP_IGNORE_WARNING

/* for now only vorbis is supported
 * other formats will be available in the future (WAV, mp3, etc)
 */

namespace gs {

    struct vorbis_audio_data
    {
        friend class audio_clip_component;

        /* may return null. if not null, it's guaranteed to be valid */
        static std::shared_ptr<vorbis_audio_data> create(const std::string& path);

        bool valid() const;
        operator bool() const;

        vorbis_audio_data(const std::string& path);
        ~vorbis_audio_data();

        vorbis_audio_data(const vorbis_audio_data&) = delete;
        vorbis_audio_data& operator=(const vorbis_audio_data&) = delete;

        vorbis_audio_data(vorbis_audio_data&&) noexcept;
        vorbis_audio_data& operator=(vorbis_audio_data&&) noexcept;

    private:
        int32_t channels = 2, sample_rate = 44100; // 48000
        int32_t samples = 0;
        int16_t* data = nullptr;
        size_t data_size = 0;

        std::string path;

        static std::unordered_map<std::string, std::weak_ptr<vorbis_audio_data>> s_audio_atlas;
    };

    /* only vorbis is supported for the moment */
    class audio_clip_component
    {
        friend class game_object;

    public:
        audio_clip_component(game_object gObject, const std::string& ownerMixerName);
        ~audio_clip_component();

        audio_clip_component(audio_clip_component&&) noexcept;
        audio_clip_component& operator=(audio_clip_component&&) noexcept;

        void attach_to_mixer(const std::string& mixerName);

        void set_audio_clip(const std::string& path, bool loop = false);
        void clear_audio_clip();

        void play(uint64_t fadeLegthMilli = 0);

        /* 0.0f for no fade */
        void stop(uint64_t fadeLegthMilli = 512);

        void set_volume(float volume, bool fade = false);
        void mute();
        void unmute();

        void set_pitch(float pitch);

        void set_looping(bool loop);
        void rewind();

        bool valid() const;

        /* to allow if (auto audio = obj.get_component...) */
        operator bool () const;

        float get_volume() const;
        float get_pitch() const;
        bool is_mute() const;

    private:
        ma_sound m_sound;
        float m_volume = 1.0f;
        float m_pitch = 1.0f;
        bool m_mute = false;

        /* mini audio does not copy the data, so we should keep a copy of it */
        std::shared_ptr<vorbis_audio_data> m_audio_data;

        ma_audio_buffer m_audio_buffer;

        std::shared_ptr<class audio_mixer> m_owner_mixer;
        game_object m_object;
    };

}