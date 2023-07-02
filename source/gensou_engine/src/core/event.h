#pragma once

#include "core/uuid.h"
#include "core/log.h"

// TODO: change the base data structure to a sparse set (instead of a dynamic array). subscribe and broadcast operations are very fast,
// but unsubscribe and clear tombstones (currently O(n)) could get particularly slow as the size of the List gets bigger


namespace gs {

	struct listener_id
	{
		bool operator==(listener_id other) { return other.id == id; }
		bool operator!=(listener_id other) { return other.id != id; }

		operator uint64_t() const { return id; }

	private:
		uuid id;
	};

	template<typename T>
	class event
	{
		enum listener_state : uint32_t { active = 0, inactive, deleted };
		struct event_block
		{
			event_block(const T& deleg) : state(active), fdelegate(deleg) {}

			listener_id id;
			listener_state state;
			T fdelegate;
		};

	public:
		template<typename... Args>
		listener_id subscribe(Args&&... args)
		{
			auto& newEvent = m_listeners_list.emplace_back(std::forward<decltype(args)>(args)...);
			return newEvent.id;
		}

		void unsubscribe(listener_id listenerID)
		{
			for (auto& block : m_listeners_list)
			{
				if ((block.state != deleted) && (block.id == listenerID))
				{
					block.state == deleted;
					m_tombstone_count++;

					if (m_tombstone_count >= m_max_tombstone_count)
						clear_tombstones();

					LOG_ENGINE(trace, "unsubscribed event 0x%zx", (uint64_t)listenerID);

					return;
				}
			}

			LOG_ENGINE(warn, "tried to unsubscribe event 0x%zx which was not subscribed", (uint64_t)listenerID);
		}

		/* pop events like a queue */
		template<typename... Args>
		void broadcast(Args&&... args)
		{
			for (const auto& listener : m_listeners_list)
			{
				if(listener.state == active)
					listener.fdelegate(std::forward<decltype(args)>(args)...);
			}
		}

		/* pop events like a stack */
		template<typename... Args>
		void broadcast_reverse(Args&&... args)
		{
			for (auto it = m_listeners_list.rbegin(); it != m_listeners_list.rend(); it++)
			{
				if(it->state == active)
					it->fdelegate(std::forward<decltype(args)>(args)...);
			}
		}

		void set_listener_active(listener_id id)
		{
			for (auto& listener : m_listeners_list)
			{
				if (listener.id == id)
					listener.state = active;
			}
		}

		void set_listener_inactive(listener_id id)
		{
			for (auto& listener : m_listeners_list)
			{
				if (listener.id == id)
					listener.state = inactive;
			}
		}

		void set_max_tombstone_count(uint32_t count) { m_max_tombstone_count = count; }

		void clear_listeners_list(bool dealocate)
		{
			m_listeners_list.clear();

			if(dealocate)
				m_listeners_list.shrink_to_fit();

			m_tombstone_count = 0;
		}

	private:
		void clear_tombstones()
		{
			LOG_ENGINE(warn, "clearing tombstones");

			std::vector<event_block> tempList;
			tempList.reserve(m_listeners_list.capacity());

			for (size_t i = 0; i < m_listeners_list.size(); i++)
			{
				if (m_listeners_list[i].state != deleted)
				{
					tempList.emplace_back(std::move(m_listeners_list[i]));
				}
			}

			clear_listeners_list(true);
			m_listeners_list = std::move(tempList);
		}

	private:
		std::vector<event_block> m_listeners_list;

		uint32_t m_tombstone_count = 0;
		uint32_t m_max_tombstone_count = 16;
	};

}