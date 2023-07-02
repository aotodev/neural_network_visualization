#include "core/system.h"

#include "core/gensou_app.h"
#include "core/misc.h"
#include "core/runtime.h"

#include "renderer/renderer.h"

#include <filesystem>

#ifdef APP_ANDROID
#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#include "platform/android/android_platform.h"
#endif

namespace gs {

	////////////////////////////--STATICS--//////////////////////////////////

	void*									system::s_platform_data = nullptr;
	bool									system::s_rumbler_active = true;
	std::string								system::s_internal_data_path;
	std::shared_ptr<app_settings>			system::s_app_settings;

	system::render_thread					system::s_render_thread;
	system::loading_thread					system::s_loading_thread;
	system::thread_pool						system::s_thread_pool;
	
	std::thread::id							system::s_main_thread_id;
	std::thread::id							system::s_render_thread_id;
	std::thread::id							system::s_loading_thread_id;

	////////////////////////////////////////////////////////////////////////


	void system::init()
	{
		/* main_thread_id == 0 */
		s_render_thread.id = 1;
		s_loading_thread.id = 2;

		s_main_thread_id = std::this_thread::get_id();
		LOG_ENGINE(trace, "main thread id == %llX", s_main_thread_id);

		/* render thread */
		{
			s_render_thread.thread_name = "render";

			for(auto& cmdQueue : s_render_thread.command_queues)
				cmdQueue.resize(MiB >> 2);

			s_render_thread.thread = std::thread([&data = s_render_thread]()
			{
				LOG_ENGINE(trace, "starting %s thread | thread id == %llX", data.thread_name.c_str(), std::this_thread::get_id());

				while(data.is_alive)
				{
					std::unique_lock<std::mutex> lock(data.mutex);
					data.mutex_condition.wait(lock, [&data] { return data.active || !data.is_alive; });

					data.command_queues[data.current_frame].dequeue_all();
					data.active = false;

					data.promise.set_value();
					data.promise = std::promise<void>();
				}

				LOG_ENGINE(trace, "finishing %s thread | thread id == %llX", data.thread_name.c_str(), std::this_thread::get_id());
			});

			s_render_thread_id = s_render_thread.thread.get_id();
		}

		/* loading thread */
		{
			s_loading_thread.thread_name = "loading";
			s_loading_thread.task_queue.resize(256);

			s_loading_thread.thread = std::thread([&data = s_loading_thread]()
			{
				LOG_ENGINE(trace, "starting %s thread | thread id == %llX", data.thread_name.c_str(), std::this_thread::get_id());

				while(data.is_alive)
				{
					/* thread-safe when there is a single producer thread and single consumer thread */
					{
						std::unique_lock<std::mutex> lock(data.mutex);
						data.mutex_condition.wait(lock, [&data] { return data.has_work() || !data.is_alive; });

						if (!data.is_alive)
							break;
					}

					if (data.tail_ptr == data.task_queue.size())
						data.tail_ptr = 0;

					/* take ownership to free up a slot in the task queue */
					std::function<void(void)> function(std::move(data.task_queue[data.tail_ptr].function));
					std::promise<void> promise(std::move(data.task_queue[data.tail_ptr].promise));
					data.task_queue[data.tail_ptr].free = true;

					data.tail_ptr++;

					function();
					promise.set_value();
				}

				LOG_ENGINE(trace, "finishing %s thread | thread id == %llX", data.thread_name.c_str(), std::this_thread::get_id());
			});

			s_loading_thread_id = s_loading_thread.thread.get_id();
		}

		/* thread pool */
		for(uint32_t i = 0; i < thread_pool::thread_count; i++)
		{
			s_thread_pool.threads.push_back(std::thread([&pool = s_thread_pool]
			{
				while(pool.is_alive)
				{
					thread_pool::task nextTask;
					{
						std::unique_lock<std::mutex> lock(pool.mutex);
						pool.mutex_condition.wait(lock, [&pool]
						{ 
							return !pool.tasks_queue.empty() || !pool.is_alive;
						});

						if (!pool.is_alive) 
							break;

						nextTask = std::move(pool.tasks_queue.front());
						pool.tasks_queue.pop();
					}

					nextTask.function();
					nextTask.promise.set_value();
				}					
			}));
		}

		#ifdef APP_ANDROID
		s_internal_data_path = static_cast<android_app*>(s_platform_data)->activity->internalDataPath;
		#else
		s_internal_data_path = std::filesystem::absolute(std::filesystem::current_path()).string();
		#endif
	}

	void system::terminate()
	{
		s_thread_pool.terminate();

		/* terminate loading thread */
		{
			std::unique_lock<std::mutex> lock(s_loading_thread.mutex);
			s_loading_thread.is_alive = false;
			s_loading_thread.mutex_condition.notify_one();
		}

		/* terminate render thread */
		{
			std::unique_lock<std::mutex> lock(s_render_thread.mutex);
			s_render_thread.is_alive = false;
			s_render_thread.mutex_condition.notify_one();
		}

		s_loading_thread.thread.join();
		s_render_thread.thread.join();
	}

	void system::set_cursor_type(cursor_type cursorType)
	{
		gensou_app::get()->get_window()->set_cursor_type(cursorType);
	}

	void system::set_clear_value(const glm::vec4& color)
	{
		renderer::set_clear_value(color);
	}

	std::shared_ptr<app_settings> system::get_settings()
	{
		if (s_app_settings)
			return s_app_settings;

		s_app_settings = deserialize_settings();

		if (!s_app_settings)
		{
			s_app_settings = std::make_shared<app_settings>();

			s_app_settings->width = 0;
			s_app_settings->height = 0;
			s_app_settings->use_postprocess = true;
			s_app_settings->vsync = true;

			serialize_settings(s_app_settings);
		}

		return s_app_settings;
	}

	std::string system::make_path_from_internal_data(const std::string& path)
	{
		#ifdef APP_WINDOWS
		return std::string(s_internal_data_path + std::string("\\") + path);
		#else
		return std::string(s_internal_data_path + std::string("/") + path);
		#endif
	}

	void system::serialize_data(const std::string& path, const void* data, size_t dataSize)
	{
		#ifdef APP_WINDOWS
		std::string saveDir(s_internal_data_path + "\\save\\");
		std::string backupDir(s_internal_data_path + "\\save\\backup\\");
		#else
		std::string saveDir(s_internal_data_path + "/save/");
		std::string backupDir(s_internal_data_path + "/save/backup/");
		#endif

		if(!std::filesystem::exists(saveDir))
			std::filesystem::create_directories(saveDir);

		if(!std::filesystem::exists(backupDir))
			std::filesystem::create_directories(backupDir);

		std::string savePath(saveDir + path);

		/* backup file (copy working file before overriding it) */
		if(std::filesystem::exists(savePath))
		{
			std::string backupPath(backupDir + path);

			#ifdef APP_WINDOWS
			std::string backupCmd("copy " + savePath + " " + backupPath);
			#else
			std::string backupCmd("cp " + savePath + " " + backupPath);
			#endif

			std::system(backupCmd.c_str());
		}

		/* dataSize + headerSize + hashSize */
		size_t totalSize = dataSize + 12ULL + 4ULL;
		byte* writeData = new byte[totalSize];

		memset(writeData, 0, totalSize);

		/* gensou file header */
		writeData[0] = 0x41; writeData[1] = 0x4F; writeData[2] = 0x54; writeData[3] = 0x4F;
		*((uint64_t*)(&writeData[4])) = uuid();

		dword hash = get_hashcode_from_binary((const byte*)data, dataSize);
		memcpy(writeData + 12ULL, &hash, sizeof(dword));
		memcpy(writeData + 16ULL, data, dataSize);

		std::ofstream fout(savePath, std::ios::binary);

		fout.write(reinterpret_cast<char*>(writeData), totalSize);

		if (writeData)
			delete[] writeData;
	}

	std::shared_ptr<gensou_file> system::deserialize_data_internal(const std::string& path)
	{
		std::shared_ptr<gensou_file> outData;

		FILE* file = fopen(path.c_str(), "rb");

		if(!file)
		{
			LOG_ENGINE(error, "failed to load file from path '%s'", path.c_str());
			return outData;
		}

		LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

		fseek(file, 0, SEEK_END);
		size_t totaldDataSize = ftell(file);

		if(totaldDataSize < 16ULL)
		{
			LOG_ENGINE(error, "could not read file from path '%s', data may be corrupted or incomplete", path.c_str());
			fclose(file);
			return outData;
		}

		/* extract gensou header to assert it's our data type */
		{
			rewind(file);
			byte headerData[12];

			if(fread(headerData, 1, 12ULL, file) != 12ULL)
			{
				LOG_ENGINE(error, "failed to read file from path '%s', data may be corrupted or incomplete", path.c_str());
				fclose(file);
				return outData;
			}

			/* check magic number */
			if(headerData[0] != 0x41 || headerData[1] != 0x4F || headerData[2] != 0x54 || headerData[3] != 0x4F)
			{
				LOG_ENGINE(error, "file from path '%s' is not a gensou file", path.c_str());
				fclose(file);
				return outData;
			}

			outData = std::make_shared<gensou_file>();
			uint64_t* idPtr = (uint64_t*)(&headerData[4]);
			outData->m_id = uuid(*idPtr);
		}

		outData->m_size = totaldDataSize - 16ULL; /* minus header and hash size */
		outData->m_data = std::make_unique<byte[]>(outData->m_size);
		memset(outData->m_data.get(), 0, outData->m_size);

		dword correctHash = 0;

		/* read hash */
		if(fread(&correctHash, 1, sizeof(dword), file) != 4ULL)
		{
			outData.reset();
			fclose(file);

			LOG_ENGINE(error, "failed to read file from path '%s', data may be corrupted or incomplete", path.c_str());
			return outData;
		}

		if (fread(outData->m_data.get(), 1, outData->m_size, file) != outData->m_size)
		{
			outData.reset();
			fclose(file);

			LOG_ENGINE(error, "failed to read file from path '%s', data may be corrupted or incomplete", path.c_str());
			return outData;
		}

		/* compare hash to assert data integrity */
		{
			dword dataHash = get_hashcode_from_binary(outData->m_data.get(), outData->m_size);
			if(dataHash != correctHash)
			{
				LOG_ENGINE(error, "corrupted data | file from path '%s' failed hash check", path.c_str());
				outData.reset();
			}
		}

		fclose(file); 

		return outData;
	}

	std::shared_ptr<gensou_file> system::deserialize_data(const std::string& path)
	{
		#ifdef APP_WINDOWS
		auto savePath = std::string(s_internal_data_path + std::string("\\save\\" + path));
		#else
		auto savePath = std::string(s_internal_data_path + std::string("/save/" + path));
		#endif

		auto gFile = deserialize_data_internal(savePath);

		/* if failed to load main file, try backup */
		if(!gFile)
		{
			LOG_ENGINE(error, "failed to deserialize save file, trying backup...");

			#ifdef APP_WINDOWS
			std::string backupPath(s_internal_data_path + std::string("\\save\\backup\\" + path));
			#else
			std::string backupPath(s_internal_data_path + std::string("/save/backup/" + path));
			#endif

			gFile = deserialize_data_internal(backupPath);

			if(!gFile)
			{
				LOG_ENGINE(error, "failed to deserialize app settings backup");
			}
		}

		return gFile;
	}

	void system::serialize_settings(std::shared_ptr<app_settings> settings)
	{
		LOG_ENGINE(trace, "serializing application settings to the path");

		if(!s_app_settings)
		{
			app_settings outSettings{};
			outSettings.width = runtime::viewport().width;
			outSettings.width = runtime::viewport().height;
			outSettings.vsync = vsync();
			outSettings.use_postprocess = renderer::is_post_process_enabled();

			serialize_data("engine_settings", &outSettings, sizeof(app_settings));
		}
		else
		{
			serialize_data("engine_settings", s_app_settings.get(), sizeof(app_settings));
		}
	}

	std::shared_ptr<app_settings> system::deserialize_settings()
	{
		std::shared_ptr<app_settings> outSettings;

		auto gFile = deserialize_data("engine_settings");

		/* if failed to load main file, try backup */
		if(!gFile)
		{
			LOG_ENGINE(warn, "failed to deserialize app settings, creating a new one");
			return outSettings;
		}

		auto pSettings = gFile->get_data_as<app_settings>();

		outSettings = std::shared_ptr<app_settings>(std::move(pSettings));

		LOG_ENGINE(warn, "deserialized app settings from path");

		return outSettings;
	}

	void system::exit()
	{
#ifdef APP_ANDROID
		/* in android this can be called before our c++ application even exists */
		auto app = gensou_app::get();

		if(!app)
		{
			LOG_ENGINE(error, "main app was nullptr, exiting application forcibly");
			android_window::destroy_application();
			return;
		}
		
		auto pWindow = app->get_window();

		if(!pWindow)
		{
			LOG_ENGINE(error, "window was nullptr, exiting application forcibly");
			android_window::destroy_application();
			return;
		}

		/*  otherwise do all the necessary housekeeping before exiting */
		pWindow->request_destroy();
#else
		gensou_app::get()->get_window()->request_destroy();
#endif
	}

	bool system::supports_nonvsync_mode()
	{
		return gensou_app::get()->get_window()->supports_nonvsync_mode();
	}

	bool system::vsync()
	{
		return gensou_app::get()->get_window()->is_vsync();
	}

	void system::set_vsync(bool enabled)
	{
		gensou_app::get()->get_window()->set_vsync(enabled);
		s_app_settings->vsync = enabled;
	}

	uint32_t system::get_display_cutout_height()
	{
		if(auto app = gensou_app::get())
		{
			if(auto pWindow = app->get_window())
				return pWindow->get_display_cutout_height();
		}

		return 0;
	}


	static std::unordered_map<std::string, uuid> s_id_from_path_atlas;

	uuid system::get_cached_id_from_file(const std::string& filePath)
	{
		const auto mapIterator = s_id_from_path_atlas.find(filePath);

		if (mapIterator != s_id_from_path_atlas.end())
			return mapIterator->second;

		return uuid(0ULL);
	}

	std::vector<byte> system::load_internal_file(const std::string& path)
	{
		std::vector<byte> outData;

		#ifdef APP_WINDOWS
		auto internalPath = std::string(s_internal_data_path + std::string("\\") + path);
		#else
		std::string internalPath(s_internal_data_path + std::string("/") + path);
		#endif

		FILE* pFile = fopen(internalPath.c_str(), "rb");

		if(!pFile)
		{
			LOG_ENGINE(error, "failed to load file from path '%s'", internalPath.c_str());
			return outData;
		}

		LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

		fseek(pFile, 0, SEEK_END);

		size_t fileSize = ftell(pFile);
		if(!fileSize)
		{
			LOG_ENGINE(error, "file from path '%s' was empty", internalPath.c_str());
			return outData;
		}

		outData.resize(fileSize, 0);

		rewind(pFile);
		size_t readSize = fread(outData.data(), 1, fileSize, pFile);

		if (readSize != fileSize)
		{
			outData.clear();
			outData.shrink_to_fit();
			LOG_ENGINE(error, "failed to read file from path '%s', data may be corrupted or incomplete", internalPath.c_str());
		}

		fclose(pFile);

		return outData;
	}

	std::shared_ptr<gensou_file> system::load_file(const std::string& path)
	{
		std::shared_ptr<gensou_file> outData;

#ifdef APP_ANDROID
		if (!s_platform_data)
		{
			LOG_ENGINE(error, "android_app was nullptr");
			return outData;
		}

		AAsset* asset = AAssetManager_open(((android_app*)s_platform_data)->activity->assetManager, path.c_str(), AASSET_MODE_RANDOM); // AASSET_MODE_STREAMING ?
		
		if (asset)
		{
			LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

			if (size_t dataSize = (size_t)AAsset_getLength(asset))
			{
				if(dataSize < 12ULL)
				{
					LOG_ENGINE(error, "could not read file from path '%s', data incomplete", path.c_str());
					return outData;
				}

				/* extract gensou header to assert it's our data type */
				{
					byte headerData[12];
					auto readsize = AAsset_read(asset, headerData, 12ULL);
					if(readsize == 12ULL)
					{
						/* check magic number */
						if(headerData[0] == 0x41 && headerData[1] == 0x4F && headerData[2] == 0x54 && headerData[3] == 0x4F)
						{
							outData = std::make_shared<gensou_file>();
							uint64_t* idPtr = (uint64_t*)(&headerData[4]);
							outData->m_id = uuid(*idPtr);
						}
						else
						{
							LOG_ENGINE(error, "file from path '%s' is not a gensou file", path.c_str());
						}
					}
					else
					{
						LOG_ENGINE(error, "failed to parse file from path '%s'", path.c_str());
					}
				}

				if (outData)
				{
					outData->m_size = dataSize - 12ULL;
					outData->m_data = std::make_unique<byte[]>(outData->m_size);
					memset(outData->m_data.get(), 0, outData->m_size);

					AAsset_seek(asset, 12L, SEEK_SET);
					size_t readsize = AAsset_read(asset, outData->m_data.get(), outData->m_size);

					if (readsize != outData->m_size)
					{
						outData.reset();
						LOG_ENGINE(error, "failed to parse file from path '%s'", path.c_str());
					}
				}
			}
			else
			{
				LOG_ENGINE(error, "could not read file from path '%s'", path.c_str());
			}

			AAsset_close(asset);
		}
		else
		{
			LOG_ENGINE(error, "failed to load file from path '%s'", path.c_str());
		}

#else
		if (!std::filesystem::exists(path))
		{
			LOG_ENGINE(warn, "file with path '%s' does not exist", path.c_str());
			return outData;
		}

		FILE* file = nullptr;

		file = fopen(path.c_str(), "rb");

		if (file)
		{
			LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

			fseek(file, 0, SEEK_END);
			if (size_t totaldDataSize = ftell(file))
			{
				if(totaldDataSize < 12ULL)
				{
					LOG_ENGINE(error, "could not read file from path '%s', data incomplete", path.c_str());
					return outData;
				}

				/* extract gensou header to assert it's our data type */
				{
					rewind(file);
					byte headerData[12];
					size_t readsize = fread(headerData, 1, 12ULL, file);
					if(readsize == 12ULL)
					{
						/* check magic number */
						if(headerData[0] == 0x41 && headerData[1] == 0x4F && headerData[2] == 0x54 && headerData[3] == 0x4F)
						{
							outData = std::make_shared<gensou_file>();
							uint64_t* idPtr = (uint64_t*)(&headerData[4]);
							outData->m_id = uuid(*idPtr);
						}
						else
						{
							LOG_ENGINE(error, "file from path '%s' is not a gensou file", path.c_str());
						}
					}
					else
					{
						LOG_ENGINE(error, "failed to parse file from path '%s'", path.c_str());
					}
				}

				if (outData)
				{
					outData->m_size = totaldDataSize - 12ULL;
					outData->m_data = std::make_unique<byte[]>(outData->m_size);
					memset(outData->m_data.get(), 0, outData->m_size);

					fseek(file, 12L, SEEK_SET);
					size_t readsize = fread(outData->m_data.get(), 1, outData->m_size, file);

					if (readsize != outData->m_size)
					{
						outData.reset();
						LOG_ENGINE(error, "failed to parse file from path '%s'", path.c_str());
					}
				}
			}
			else
			{
				LOG_ENGINE(error, "could not read file from path '%s'", path.c_str());
			}

			fclose(file); 
		}
		else
		{
			LOG_ENGINE(error, "Failed to load file from path '%s'", path.c_str());
		}

#endif

		if(outData)
			s_id_from_path_atlas[path] = outData->m_id;

		return outData;
	}

	std::vector<byte> system::load_spv_file(const std::string& path)
	{
		std::vector<byte> outData;

#ifdef APP_ANDROID
		if (!s_platform_data)
		{
			LOG_ENGINE(error, "android_app was nullptr");
			return outData;
		}

		AAsset* asset = AAssetManager_open(((android_app*)s_platform_data)->activity->assetManager, path.c_str(), AASSET_MODE_BUFFER); //AASSET_MODE_RANDOM ?

		if (asset)
		{
			LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

			if (size_t dataSize = (size_t)AAsset_getLength(asset))
			{
				if(dataSize < 4ULL)
				{
					LOG_ENGINE(error, "could not read file from path '%s', data imcomplete", path.c_str());
					return outData;
				}

				/* magic number to assert it's a SPIR-V file */
				{
					byte magicNumber[4];
					size_t currentReadsize = AAsset_read(asset, magicNumber, 4);

					if (currentReadsize == 4ULL)
					{
						/* spir-v */
						if (*((uint32_t*)magicNumber) == 0x07230203)
						{
							outData.resize(dataSize, 0);

							AAsset_seek(asset, 0, SEEK_SET);
							size_t readsize = AAsset_read(asset, outData.data(), outData.size());

							if (readsize != dataSize)
							{
								outData.clear();
								outData.shrink_to_fit();
								LOG_ENGINE(error, "failed to read spv shader file from path '%s'", path.c_str());
							}
						}
						else
						{
							LOG_ENGINE(error, "file from path '%s' is not a spv shader file", path.c_str());
						}

					}
					else
					{
						LOG_ENGINE(error, "failed to parse spv shader file from path '%s'", path.c_str());
					}
				}
			}
			else
			{
				LOG_ENGINE(error, "could not read file from path '%s'", path.c_str());
			}

			AAsset_close(asset);
		}
		else
		{
			LOG_ENGINE(error, "failed to load file from path '%s'", path.c_str());
		}

#else
		FILE* file = nullptr;
		file = fopen(path.c_str(), "rb");

		if (file)
		{
			LOG_ENGINE(trace, "loading file from path '%s'", path.c_str());

			fseek(file, 0, SEEK_END);
			if (size_t totaldDataSize = ftell(file))
			{
				if (totaldDataSize < 4ULL)
				{
					LOG_ENGINE(error, "could not read file from path '%s' (data incomplete)", path.c_str());
					return outData;
				}

				/* magic number to assert it's a SPIR-V file */
				{
					rewind(file);
					byte magicNumber[4];

					size_t magicReadSize = fread(magicNumber, 1, 4ULL, file);
					if (magicReadSize == 4ULL)
					{
						/* spir-v */
						if (*((uint32_t*)magicNumber) == 0x07230203)
						{
							rewind(file);

							outData.resize(totaldDataSize, 0);

							size_t readSize = fread(outData.data(), 1, outData.size(), file);
							if (readSize != totaldDataSize)
							{
								outData.clear();
								outData.shrink_to_fit();
								LOG_ENGINE(error, "failed to read spv shader file from path '%s'", path.c_str());
							}
						}
						else
						{
							LOG_ENGINE(error, "file from path '%s' is not a spv shader file", path.c_str());
						}

					}
					else
					{
						LOG_ENGINE(error, "failed to parse spv shader file from path '%s'", path.c_str());
					}
				}
			}
			else
			{
				LOG_ENGINE(error, "could not read spv shaderfile from path '%s'", path.c_str());
			}

			fclose(file);
		}
		else
		{
			LOG_ENGINE(error, "Failed to load file from path '%s'", path.c_str());
		}
#endif
		return outData;
	}

}