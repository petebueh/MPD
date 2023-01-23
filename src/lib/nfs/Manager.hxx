/*
 * Copyright 2003-2022 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_NFS_MANAGER_HXX
#define MPD_NFS_MANAGER_HXX

#include "Connection.hxx"
#include "event/IdleEvent.hxx"
#include "util/IntrusiveList.hxx"

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager final {
	class ManagedConnection final
		: public NfsConnection,
		  public IntrusiveListHook<>
	{
		NfsManager &manager;

	public:
		ManagedConnection(NfsManager &_manager, EventLoop &_loop,
				  const char *_server,
				  const char *_export_name) noexcept
			:NfsConnection(_loop, _server, _export_name),
			 manager(_manager) {}

	protected:
		/* virtual methods from NfsConnection */
		void OnNfsConnectionError(std::exception_ptr &&e) noexcept override;
	};

	using List = IntrusiveList<ManagedConnection>;

	List connections;

	/**
	 * A list of "garbage" connection objects.  Their destruction
	 * is postponed because they were thrown into the garbage list
	 * when callers on the stack were still using them.
	 */
	List garbage;

	IdleEvent idle_event;

public:
	explicit NfsManager(EventLoop &_loop) noexcept
		:idle_event(_loop, BIND_THIS_METHOD(OnIdle)) {}

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsManager() noexcept;

	auto &GetEventLoop() const noexcept {
		return idle_event.GetEventLoop();
	}

	[[gnu::pure]]
	NfsConnection &GetConnection(const char *server,
				     const char *export_name) noexcept;

private:
	void ScheduleDelete(ManagedConnection &c) noexcept {
		connections.erase(connections.iterator_to(c));
		garbage.push_front(c);
		idle_event.Schedule();
	}

	/**
	 * Delete all connections on the #garbage list.
	 */
	void CollectGarbage() noexcept;

	/* virtual methods from IdleMonitor */
	void OnIdle() noexcept;
};

#endif
