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

#pragma once

#include "MixerPlugin.hxx"
#include "thread/Mutex.hxx"

#include <exception>

class MixerListener;

class Mixer {
	const MixerPlugin &plugin;

public:
	/* this field needs to be public for the workaround in
	   ReplayGainFilter::Update() - TODO eliminate this kludge */
	MixerListener &listener;

private:
	/**
	 * This mutex protects all of the mixer struct, including its
	 * implementation, so plugins don't have to deal with that.
	 */
	Mutex mutex;

	/**
	 * Contains error details if this mixer has failed.  If set,
	 * it should not be reopened automatically.
	 */
	std::exception_ptr failure;

	/**
	 * Is the mixer device currently open?
	 */
	bool open = false;

public:
	explicit Mixer(const MixerPlugin &_plugin,
		       MixerListener &_listener) noexcept
		:plugin(_plugin), listener(_listener) {}

	Mixer(const Mixer &) = delete;

	virtual ~Mixer() = default;

	bool IsPlugin(const MixerPlugin &other) const noexcept {
		return &plugin == &other;
	}

	bool IsGlobal() const noexcept {
		return plugin.global;
	}

	/**
	 * Throws on error.
	 */
	void LockOpen();

	void LockClose() noexcept;

	/**
	 * Close the mixer unless the plugin's "global" flag is set.
	 * This is called when the #AudioOutput is closed.
	 */
	void LockAutoClose() noexcept {
		if (!IsGlobal())
			LockClose();
	}

	/**
	 * Throws on error.
	 */
	int LockGetVolume();
	
	int LockGetVolume();

	/**
	 * Throws on error.
	 */
	void LockSetVolume(unsigned volume);
	
	void LockSetVolume(unsigned rg);

private:
	void _Open();
	void _Close() noexcept;

protected:
	/**
	 * Open mixer device
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 */
	virtual void Open() = 0;

	/**
	 * Close mixer device
	 *
	 * Caller must lock the mutex.
	 */
	virtual void Close() noexcept = 0;

	/**
	 * Reads the current volume.
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @return the current volume (0..100 including) or -1 if
	 * unavailable
	 */
	virtual int GetVolume() = 0;
	
	virtual int GetReplayGain() = 0;

	/**
	 * Sets the volume.
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param volume the new volume (0..100 including)
	 */
	virtual void SetVolume(unsigned volume) = 0;
	
	virtual void SetReplayGain(unsigned rg) = 0;
};
