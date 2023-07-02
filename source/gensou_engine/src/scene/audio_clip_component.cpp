#include "scene/audio_clip_component.h"
#include "scene/audio_mixer.h"

#include "core/log.h"
#include "core/system.h"
#include <memory>

PUSH_IGNORE_WARNING
#include <stb_vorbis.c>
POP_IGNORE_WARNING

#define USE_ENGINE_DECODER 0

namespace gs {

    std::unordered_map<std::string, std::weak_ptr<vorbis_audio_data>> vorbis_audio_data::s_audio_atlas;

    std::shared_ptr<vorbis_audio_data> vorbis_audio_data::create(const std::string& path)
    {
        const auto mapIterator = s_audio_atlas.find(path);
        if (mapIterator != s_audio_atlas.end())
        {
            auto audioWeakPtr = mapIterator->second;
            if (!audioWeakPtr.expired())
            {
                LOG_ENGINE(trace, "texture with path '%s' found", path.c_str());
                return std::shared_ptr<vorbis_audio_data>(audioWeakPtr);
            }
        }

        auto outData = std::make_shared<vorbis_audio_data>(path);
        if(outData->valid())
        {
            s_audio_atlas[path] = std::weak_ptr<vorbis_audio_data>(outData);
            outData->path = path;
            return outData;
        }

        outData.reset();
        return outData;
    }

    static uint32_t decode_vorbis(const uint8_t* pData, size_t dataSize, uint32_t& channels, uint32_t& sampleRate, int16_t** ppOutData)
    {
        assert(pData && dataSize);
        int32_t error;

        auto pStbVorbis = stb_vorbis_open_memory(pData, int(dataSize), &error, NULL);

        if (!pStbVorbis)
            return 0;

        channels = pStbVorbis->channels;
        sampleRate = pStbVorbis->sample_rate;

        size_t bufferSize = dataSize * 10;
        int16_t* shortBuffer = new int16_t[bufferSize];

        uint32_t samplesPerChannel = 0;
        int32_t n;
        size_t offset = 0, frameLimit = channels * 4096;

        do
        {
            n = stb_vorbis_get_frame_short_interleaved(pStbVorbis, channels, shortBuffer + offset, bufferSize - offset);
            samplesPerChannel += (uint32_t)n;
            offset += n * channels;

            /* check if buffer needs to resize */
            if(frameLimit + offset > bufferSize)
            {
                LOG_ENGINE(trace, "decode vorbis, resize buffer");
                bufferSize = size_t(float(bufferSize) * 1.5f);

                int16_t* temp = new int16_t[bufferSize];
                std::swap(temp, shortBuffer);
                delete[] temp;
            }

        } while(n != 0);

        *ppOutData = shortBuffer;
        stb_vorbis_close(pStbVorbis);

        return samplesPerChannel;
    }

    vorbis_audio_data::vorbis_audio_data(const std::string& path)
    {
        if(auto gensouFile = system::load_file(path))
        {   
            #if USE_ENGINE_DECODER         
            uint32_t len = decode_vorbis(
                gensouFile->data(),
                gensouFile->size(),
                channels,
                sample_rate,
                &data);
#else
            int len = stb_vorbis_decode_memory(
                gensouFile->data(),
                (int)gensouFile->size(),
                &channels,
                &sample_rate,
                &data);
#endif
            if(len <= 0)
            {
                LOG_ENGINE(error, "failed to decode audio file from path '%s'", path.c_str());
                return;
            }


            samples = len;
            data_size = (size_t)(len * channels * sizeof(int16_t));
            LOG_ENGINE(trace, "sample rate == %d, total file size == %zu, decoded size == %zu", sample_rate, gensouFile->size(), data_size);
        }
    }

    vorbis_audio_data::~vorbis_audio_data()
    {
        #if USE_ENGINE_DECODER
        if(data)
        {
            delete[] data;
            data = nullptr;
        }
        #else
        /* stb_vorbis is a c library and allocates memory with malloc, so free with its corresponding function */
        if(data)
            free(data);
        #endif

        data = nullptr;
        data_size = 0;
        samples = 0;

		if (!path.empty())
		{
			const auto mapIterator = s_audio_atlas.find(path);
			if(mapIterator != s_audio_atlas.end())
			{
				if(mapIterator->second.use_count() == 0)
				{
					s_audio_atlas.erase(path);
            		LOG_ENGINE(trace, "Erasing vorbis_audio_data from atlas with path '%s'", path.c_str());
				}
			}
		}
    }

    vorbis_audio_data::vorbis_audio_data(vorbis_audio_data&& other) noexcept
    {
        data = other.data;
        data_size = other.data_size;
        samples = other.samples;

        other.data = nullptr;
        other.data_size = 0;
        other.samples = 0;
    }

    vorbis_audio_data& vorbis_audio_data::operator=(vorbis_audio_data&& other) noexcept
    {
        data = other.data;
        data_size = other.data_size;
        samples = other.samples;

        other.data = nullptr;
        other.data_size = 0;
        other.samples = 0;

        return *this;
    }

    bool vorbis_audio_data::valid() const { return data && data_size; }
    vorbis_audio_data::operator bool() const { return valid(); }

    bool audio_clip_component::valid() const
    { 
        return m_object && m_owner_mixer && m_audio_data && m_audio_data->valid();
    }

    audio_clip_component::operator bool () const { return valid(); }

    audio_clip_component::audio_clip_component(game_object gObject, const std::string& ownerMixerName)
        : m_object(gObject)
    {
        if(!m_object)
        {
            assert(false);
            return;
        }

        attach_to_mixer(ownerMixerName);
    }

    audio_clip_component::~audio_clip_component()
    {
        clear_audio_clip();
    }


    audio_clip_component::audio_clip_component(audio_clip_component&& other) noexcept
    {
        m_sound = other.m_sound;
        m_volume = other.m_volume;
        m_mute = other.m_mute;
		m_audio_data = std::move(other.m_audio_data);
        m_owner_mixer = other.m_owner_mixer;
        m_object = other.m_object;

        other.m_volume = 1.0f;
        other.m_mute = 1.0f;
        other.m_owner_mixer.reset();
        other.m_object.reset();

        other.m_audio_data.reset();
    }

    audio_clip_component& audio_clip_component::operator=(audio_clip_component&& other) noexcept
    {
        m_sound = other.m_sound;
        m_volume = other.m_volume;
        m_mute = other.m_mute;
		m_audio_data = std::move(other.m_audio_data);
        m_owner_mixer = other.m_owner_mixer;
        m_object = other.m_object;

        other.m_volume = 1.0f;
        other.m_mute = 1.0f;
        other.m_owner_mixer.reset();
        other.m_object.reset();
        other.m_audio_data.reset();

        return *this;
    }

    void audio_clip_component::attach_to_mixer(const std::string& mixerName)
    {
        auto pScene = m_object.get_scene();

        if(!(m_owner_mixer = pScene->get_audio_mixer(mixerName)))
        {
            LOG_ENGINE(error, "failed to initialized audio_clip_component. owner mixer with name '%s' does not exist", mixerName.c_str());
        }
    }

    void audio_clip_component::set_audio_clip(const std::string& path, bool loop)
    {
        if(!m_owner_mixer)
        {
            LOG_ENGINE(error, "could not set audio clip with path '%s' because the current audio clip is not attached to a valid audio mixer", path.c_str());
            return;
        }

        if(valid())
        {
            LOG_ENGINE(warn, "audio clip already set, overriding with new clip from path '%s'", path.c_str());
            clear_audio_clip();
        }

        m_audio_data = vorbis_audio_data::create(path);
        if(m_audio_data)
        {            
            ma_audio_buffer_config config = ma_audio_buffer_config_init(
            ma_format_s16,
            m_audio_data->channels,
            (uint64_t)m_audio_data->samples,
            (void*)m_audio_data->data,
            NULL);

            config.sampleRate = m_audio_data->sample_rate;

            auto result = ma_audio_buffer_init(&config, &m_audio_buffer);
            if(result != MA_SUCCESS)
            {
                LOG_ENGINE(error, "failed to initialize ma_audio_buffer with error '%s'", ma_result_description(result));
                ma_audio_buffer_uninit(&m_audio_buffer);
                m_audio_data.reset();

                return;
            }

            LOG_ENGINE(trace, "audio file successfully loaded from path '%s'", path.c_str());
            if(m_owner_mixer->add_audio_clip(&m_sound, &m_audio_buffer))
            {
                ma_sound_set_pitch(&m_sound, m_pitch);
                ma_sound_set_volume(&m_sound, m_mute ? 0.0f : m_volume);
                ma_sound_set_looping(&m_sound, loop);
            }
            else
            {
                LOG_ENGINE(error, "failed to add audio clip to mixer");
                clear_audio_clip();
            }
        }
        else
        {
            LOG_ENGINE(error, "could not load audio clip from path '%s'", path.c_str());
        }
    }

    void audio_clip_component::clear_audio_clip()
    {
        if(valid())
        {
            ma_audio_buffer_uninit(&m_audio_buffer);
            ma_sound_uninit(&m_sound);

            m_audio_data.reset();
        }
    }

    void audio_clip_component::play(uint64_t fadeLegthMilli)
    {
        if(!m_audio_data)
        {
            LOG_ENGINE(error, "trying to play an empty or invalid audio_clip. try calling the method set_audio_clip(path)");
            return;
        }

        if(ma_sound_is_playing(&m_sound))
        {
            LOG_ENGINE(warn, "PLAY | sound is already playing with volume %.3f", m_volume);
            ma_sound_stop(&m_sound);
        }

        /* if the sound was stoped with fade, its current volume will be 0.0f */
        if(!m_mute)
        {
            if(fadeLegthMilli)
            {
                auto engineTime = m_owner_mixer->get_engine_time();
                ma_sound_set_fade_in_milliseconds(&m_sound, 0.0f, m_volume, fadeLegthMilli);
            }
            else
            {
                ma_sound_set_volume(&m_sound, m_volume);
            }
        }

        auto result = ma_sound_start(&m_sound);
        if(result != MA_SUCCESS)
        {
            LOG_ENGINE(error, "failed to start sound with error '%s'", ma_result_description(result));
        }
    }

    void audio_clip_component::stop(uint64_t fadeLegthMilli /*= 128 */)
    {
        if(!m_audio_data)
        {
            LOG_ENGINE(error, "trying to stop an empty or invalid audio_clip. try calling the method set_audio_clip(path)");
            return;
        }

        if(!ma_sound_is_playing(&m_sound))
            return;

        if(fadeLegthMilli)
        {
            ma_sound_stop(&m_sound);

            /* although this will work, the device won't play anything after calling set_stop_time for some reason */
            #if 0
            ma_sound_set_fade_in_milliseconds(&m_sound, m_volume, 0.0f, fadeLegthMilli);
            auto engineTime = m_owner_mixer->get_engine_time();
            ma_sound_set_stop_time_in_milliseconds(&m_sound, engineTime + fadeLegthMilli);
            #endif
        }
        else
        {
            ma_sound_stop(&m_sound);
        }
    }

    void audio_clip_component::set_volume(float volume, bool fade)
    {
        LOG_ENGINE(info, "SETTING audio volume to %.3f", volume);
        if(valid() && !m_mute)
        {
            if(fade)
            {
                ma_sound_set_fade_in_milliseconds(&m_sound, m_volume, volume, 512);
            }
            else
            {
                ma_sound_set_volume(&m_sound, volume);
            }
        }

        m_volume = volume;
    }

    void audio_clip_component::mute()
    {
        m_mute = true;

        if(valid())
            ma_sound_set_volume(&m_sound, 0.0f);
    }

    void audio_clip_component::unmute()
    {
        m_mute = false;

        if(valid())
            ma_sound_set_volume(&m_sound, m_volume);
    }

    void audio_clip_component::set_pitch(float pitch)
    {
        m_pitch = pitch;

        if(valid())
            ma_sound_set_pitch(&m_sound, pitch);
    }

    void audio_clip_component::set_looping(bool loop)
    {
        if(valid())
            ma_sound_set_looping(&m_sound, loop);
    }

    void audio_clip_component::rewind()
    {
        if(valid())
            ma_data_source_seek_to_pcm_frame(m_sound.pDataSource, 0);
    }

    float audio_clip_component::get_volume() const
    {
         return m_volume;
    }

    float audio_clip_component::get_pitch() const
    { 
        return m_pitch; 
    }

    bool audio_clip_component::is_mute() const
    { 
        return m_mute;
    }

}