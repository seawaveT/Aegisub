// Copyright (c) 2005-2007, Rodrigo Braz Monteiro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file audio_player.cpp
/// @brief Baseclass for audio players
/// @ingroup audio_output
///

#include "config.h"

#include "include/aegisub/audio_player.h"
#include "audio_player_alsa.h"
#include "audio_player_dsound.h"
#include "audio_player_dsound2.h"
#include "audio_player_openal.h"
#include "audio_player_oss.h"
#include "audio_player_portaudio.h"
#include "audio_player_pulse.h"

#include "audio_controller.h"
#include "options.h"

AudioPlayer::AudioPlayer(AudioProvider *provider)
: provider(provider)
{
}

std::unique_ptr<AudioPlayer> AudioPlayerFactory::GetAudioPlayer(AudioProvider *provider) {
	std::vector<std::string> list = GetClasses(OPT_GET("Audio/Player")->GetString());
	if (list.empty()) throw agi::NoAudioPlayersError("No audio players are available.", 0);

	std::string error;
	for (auto const& factory_name : list) {
		try {
			return Create(factory_name, provider);
		}
		catch (agi::AudioPlayerOpenError const& err) {
			error += factory_name + " factory: " + err.GetChainedMessage() + "\n";
		}
	}
	throw agi::AudioPlayerOpenError(error, 0);
}

void AudioPlayerFactory::RegisterProviders() {
#ifdef WITH_ALSA
	Register<AlsaPlayer>("ALSA");
#endif
#ifdef WITH_DIRECTSOUND
	Register<DirectSoundPlayer>("DirectSound-old");
	Register<DirectSoundPlayer2>("DirectSound");
#endif
#ifdef WITH_OPENAL
	Register<OpenALPlayer>("OpenAL");
#endif
#ifdef WITH_PORTAUDIO
	Register<PortAudioPlayer>("PortAudio");
#endif
#ifdef WITH_LIBPULSE
	Register<PulseAudioPlayer>("PulseAudio");
#endif
#ifdef WITH_OSS
	Register<OSSPlayer>("OSS");
#endif
}
