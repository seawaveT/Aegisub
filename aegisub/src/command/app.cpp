// Copyright (c) 2005-2010, Niels Martin Hansen
// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
// Copyright (c) 2010, Amar Takhar
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

/// @file app.cpp
/// @brief app/ commands.
/// @ingroup command
///

#include "../config.h"

#include <wx/msgdlg.h>

#include "command.h"

#include <libaegisub/log.h>
#include <libaegisub/util.h>

#include "../audio_controller.h"
#include "../compat.h"
#include "../dialog_about.h"
#include "../dialog_detached_video.h"
#include "../dialog_log.h"
#include "../dialog_manager.h"
#include "../dialog_version_check.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../main.h"
#include "../options.h"
#include "../preferences.h"
#include "../utils.h"
#include "../video_context.h"

namespace {
	using cmd::Command;

/// @defgroup cmd-app Application related
/// @{

/// Launch about dialogue.
struct app_about : public Command {
	CMD_NAME("app/about")
	STR_MENU("&About")
	STR_DISP("About")
	STR_HELP("About Aegisub")

	void operator()(agi::Context *c) {
		AboutScreen(c->parent).ShowModal();
	}
};


/// Display audio and subtitles.
struct app_display_audio_subs : public Command {
	CMD_NAME("app/display/audio_subs")
	STR_MENU("&Audio+Subs View")
	STR_DISP("Audio+Subs View")
	STR_HELP("Display audio and subtitles only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *) {
		wxGetApp().frame->SetDisplayMode(0,1);
	}

	bool Validate(const agi::Context *c) {
		return c->audioController->IsAudioOpen();
	}

	bool IsActive(const agi::Context *) {
		return wxGetApp().frame->IsAudioShown() && !wxGetApp().frame->IsVideoShown();
	}
};


/// Display audio, video and subtitles.
struct app_display_full : public Command {
	CMD_NAME("app/display/full")
	STR_MENU("&Full view")
	STR_DISP("Full view")
	STR_HELP("Display audio, video and subtitles")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) {
		wxGetApp().frame->SetDisplayMode(1,1);
	}

	bool Validate(const agi::Context *c) {
		return c->audioController->IsAudioOpen() && c->videoController->IsLoaded() && !c->dialog->Get<DialogDetachedVideo>();
	}

	bool IsActive(const agi::Context *) {
		return wxGetApp().frame->IsAudioShown() && wxGetApp().frame->IsVideoShown();
	}
};


/// Display subtitles only.
struct app_display_subs : public Command {
	CMD_NAME("app/display/subs")
	STR_MENU("S&ubs Only View")
	STR_DISP("Subs Only View")
	STR_HELP("Display subtitles only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) {
		wxGetApp().frame->SetDisplayMode(0,0);
	}

	bool IsActive(const agi::Context *) {
		return !wxGetApp().frame->IsAudioShown() && !wxGetApp().frame->IsVideoShown();
	}
};


/// Display video and subtitles only.
struct app_display_video_subs : public Command {
	CMD_NAME("app/display/video_subs")
	STR_MENU("&Video+Subs View")
	STR_DISP("Video+Subs View")
	STR_HELP("Display video and subtitles only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) {
		wxGetApp().frame->SetDisplayMode(1,0);
	}

	bool Validate(const agi::Context *c) {
		return c->videoController->IsLoaded() && !c->dialog->Get<DialogDetachedVideo>();
	}

	bool IsActive(const agi::Context *) {
		return !wxGetApp().frame->IsAudioShown() && wxGetApp().frame->IsVideoShown();
	}
};


/// Exit the application.
struct app_exit : public Command {
	CMD_NAME("app/exit")
	STR_MENU("E&xit")
	STR_DISP("Exit")
	STR_HELP("Exit the application")

	void operator()(agi::Context *c) {
		wxGetApp().frame->Close();
	}
};


/// Select Aegisub interface language
struct app_language : public Command {
	CMD_NAME("app/language")
	STR_MENU("&Language...")
	STR_DISP("Language")
	STR_HELP("Select Aegisub interface language")

	void operator()(agi::Context *c) {
		// Get language
		wxString new_language = wxGetApp().locale.PickLanguage();
		if (!new_language) return;

		OPT_SET("App/Language")->SetString(from_wx(new_language));

		// Ask to restart program
		int result = wxMessageBox("Aegisub needs to be restarted so that the new language can be applied. Restart now?", "Restart Aegisub?", wxYES_NO | wxICON_QUESTION |  wxCENTER);
		if (result == wxYES) {
			// Restart Aegisub
			if (wxGetApp().frame->Close()) {
				RestartAegisub();
			}
		}
	}
};

/// Event log.
struct app_log : public Command {
	CMD_NAME("app/log")
	STR_MENU("&Log window")
	STR_DISP("Log window")
	STR_HELP("View the event log")

	void operator()(agi::Context *c) {
		c->dialog->Show<LogWindow>(c);
	}
};


/// Open a new application window.
struct app_new_window : public Command {
	CMD_NAME("app/new_window")
	STR_MENU("New &Window")
	STR_DISP("New Window")
	STR_HELP("Open a new application window")

	void operator()(agi::Context *) {
		RestartAegisub();
	}
};


/// Configure Aegisub.
struct app_options : public Command {
	CMD_NAME("app/options")
	STR_MENU("&Options...")
	STR_DISP("Options")
	STR_HELP("Configure Aegisub")

	void operator()(agi::Context *c) {
		try {
			while (Preferences(c->parent).ShowModal() < 0);
		} catch (agi::Exception& e) {
			LOG_E("config/init") << "Caught exception: " << e.GetName() << " -> " << e.GetMessage();
		}
	}
};

/// Toggle global override hotkeys (aka medusa mode).
struct app_toggle_global_hotkeys : public Command {
	CMD_NAME("app/toggle/global_hotkeys")
	STR_MENU("Toggle global hotkey overrides")
	STR_DISP("Toggle global hotkey overrides")
	STR_HELP("Toggle global hotkey overrides")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) {
		return OPT_GET("Audio/Medusa Timing Hotkeys")->GetBool();
	}

	void operator()(agi::Context *) {
		agi::OptionValue *opt = OPT_SET("Audio/Medusa Timing Hotkeys");
		opt->SetBool(!opt->GetBool());
	}
};

/// Toggle the main toolbar
struct app_toggle_toolbar : public Command {
	CMD_NAME("app/toggle/toolbar")
	STR_HELP("Toggle the main toolbar")
	CMD_TYPE(COMMAND_DYNAMIC_NAME)

	wxString StrMenu(const agi::Context *) const {
		return OPT_GET("App/Show Toolbar")->GetBool() ?
			_("Hide Toolbar") :
			_("Show Toolbar");
	}

	wxString StrDisplay(const agi::Context *) const {
		return StrMenu(0);
	}

	void operator()(agi::Context *) {
		agi::OptionValue *opt = OPT_SET("App/Show Toolbar");
		opt->SetBool(!opt->GetBool());
	}
};

/// Check to see if there is a new version of Aegisub available.
struct app_updates : public Command {
	CMD_NAME("app/updates")
	STR_MENU("&Check for Updates...")
	STR_DISP("Check for Updates")
	STR_HELP("Check to see if there is a new version of Aegisub available")

	void operator()(agi::Context *) {
		PerformVersionCheck(true);
	}
};

/// @}
}

namespace cmd {
	void init_app() {
		reg(agi::util::make_unique<app_about>());
		reg(agi::util::make_unique<app_display_audio_subs>());
		reg(agi::util::make_unique<app_display_full>());
		reg(agi::util::make_unique<app_display_subs>());
		reg(agi::util::make_unique<app_display_video_subs>());
		reg(agi::util::make_unique<app_exit>());
		reg(agi::util::make_unique<app_language>());
		reg(agi::util::make_unique<app_log>());
		reg(agi::util::make_unique<app_new_window>());
		reg(agi::util::make_unique<app_options>());
		reg(agi::util::make_unique<app_toggle_global_hotkeys>());
		reg(agi::util::make_unique<app_toggle_toolbar>());
#ifdef WITH_UPDATE_CHECKER
		reg(agi::util::make_unique<app_updates>());
#endif
	}
}
