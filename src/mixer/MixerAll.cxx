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

#include "output/MultipleOutputs.hxx"
#include "MixerControl.hxx"
#include "MixerInternal.hxx"
#include "MixerList.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "pcm/Volume.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "Idle.hxx"

#include <cassert>

static constexpr Domain mixer_domain("mixer");

gcc_pure
static int
output_mixer_get_volume(const AudioOutputControl &ao) noexcept
{
	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return -1;

	/* software mixers are always considered, even if they are
	   disabled */
	if (!ao.IsEnabled() && !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	try {
		return mixer_get_volume(mixer);
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to read mixer for '{}': {}",
			 ao.GetName(), std::current_exception());
		return -1;
	}
}

int
MultipleOutputs::GetVolume() const noexcept
{
	unsigned ok = 0;
	int total = 0;

	for (const auto &ao : outputs) {
		int volume = output_mixer_get_volume(*ao);
		if (volume >= 0) {
			total += volume;
			++ok;
		}
	}

	if (ok == 0)
		return -1;

	return total / ok;
}

gcc_pure
static int
output_mixer_get_rg(const AudioOutputControl &ao) noexcept
{
	if (!ao.IsEnabled())
		return -1;

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return -1;

	try {
		return mixer_get_rg(mixer);
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to read mixer for '{}': {}",
			 ao.GetName(), std::current_exception());
		return -1;
	}
}

int
MultipleOutputs::GetRgScale() const noexcept
{
	unsigned okrg = 0;
	int totalrg = 0;

	for (const auto &ao : outputs) {
		int rg = output_mixer_get_rg(*ao);
		if (rg >= 0) {
			totalrg += rg;
			++okrg;
		}
	}

	if (okrg == 0)
		return -1;

	return totalrg / okrg;
}

enum class SetVolumeResult {
	NO_MIXER,
	DISABLED,
	ERROR,
	OK,
};

static SetVolumeResult
output_mixer_set_volume(AudioOutputControl &ao, unsigned volume)
{
	assert(volume <= 100);

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return SetVolumeResult::NO_MIXER;

	/* software mixers are always updated, even if they are
	   disabled */
	if (!mixer->IsPlugin(software_mixer_plugin) &&
	    /* "global" mixers can be used even if the output hasn't
	       been used yet */
	    !(mixer->IsGlobal() ? ao.IsEnabled() : ao.IsReallyEnabled()))
		return SetVolumeResult::DISABLED;

	try {
		mixer_set_volume(mixer, volume);
		return SetVolumeResult::OK;
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to set mixer for '{}': {}",
			 ao.GetName(), std::current_exception());
		std::throw_with_nested(std::runtime_error(fmt::format("Failed to set mixer for '{}'",
								      ao.GetName())));
	}
}

void
MultipleOutputs::SetVolume(unsigned volume)
{
	assert(volume <= 100);

	SetVolumeResult result = SetVolumeResult::NO_MIXER;
	std::exception_ptr error;

	for (const auto &ao : outputs) {
		try {
			auto r = output_mixer_set_volume(*ao, volume);
			if (r > result)
				result = r;
		} catch (...) {
			/* remember the first error */
			if (!error) {
				error = std::current_exception();
				result = SetVolumeResult::ERROR;
			}
		}
	}

	switch (result) {
	case SetVolumeResult::NO_MIXER:
		throw std::runtime_error{"No mixer"};

	case SetVolumeResult::DISABLED:
		throw std::runtime_error{"All outputs are disabled"};

	case SetVolumeResult::ERROR:
		std::rethrow_exception(error);

	case SetVolumeResult::OK:
		break;
	}
}

enum class SetRgResult {
	NO_MIXER,
	DISABLED,
	ERROR,
	OK,
};

static SetRgResult
output_mixer_set_rg(AudioOutputControl &ao, unsigned rg)
{
	assert(rg <= 999);

	if (!ao.IsEnabled())
		return SetRgResult::NO_MIXER;

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return SetRgResult::DISABLED;

	try {
		mixer_set_rg(mixer, rg);
		idle_add(IDLE_MIXER);
		return SetRgResult::OK;
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to set replay gain for '{}': {}",
			 ao.GetName(), std::current_exception());
			std::throw_with_nested(std::runtime_error(fmt::format("Failed to set mixer for '{}'",
								      ao.GetName())));
	}
}

void
MultipleOutputs::SetRg(unsigned rg)
{
	assert(rg <= 999);
	
    SetRgResult result = SetRgResult::NO_MIXER;
	std::exception_ptr error;

	for (const auto &ao : outputs) {
		try {
			auto r = output_mixer_set_rg(*ao, rg);
			if (r > result)
				result = r;
		} catch (...) {
			/* remember the first error */
			if (!error) {
				error = std::current_exception();
				result = SetRgResult::ERROR;
			}
		}
	}

	switch (result) {
	case SetRgResult::NO_MIXER:
		throw std::runtime_error{"No mixer"};

	case SetRgResult::DISABLED:
		throw std::runtime_error{"All outputs are disabled"};

	case SetRgResult::ERROR:
		std::rethrow_exception(error);

	case SetRgResult::OK:
		break;
	}
	
}

static int
output_mixer_get_software_volume(const AudioOutputControl &ao) noexcept
{
	if (!ao.IsEnabled())
		return -1;

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr || !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	return mixer_get_volume(mixer);
}

int
MultipleOutputs::GetSoftwareVolume() const noexcept
{
	unsigned ok = 0;
	int total = 0;

	for (const auto &ao : outputs) {
		int volume = output_mixer_get_software_volume(*ao);
		if (volume >= 0) {
			total += volume;
			++ok;
		}
	}

	if (ok == 0)
		return -1;

	return total / ok;
}

void
MultipleOutputs::SetSoftwareVolume(unsigned volume) noexcept
{
	assert(volume <= PCM_VOLUME_1);

	for (const auto &ao : outputs) {
		auto *mixer = ao->GetMixer();

		if (mixer != nullptr &&
		    (&mixer->plugin == &software_mixer_plugin ||
		     &mixer->plugin == &null_mixer_plugin))
			mixer_set_volume(mixer, volume);
	}
}
