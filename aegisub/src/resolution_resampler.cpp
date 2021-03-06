// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include "config.h"

#include "resolution_resampler.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_style.h"

#include <libaegisub/of_type_adaptor.h>
#include <libaegisub/split.h>

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <functional>

enum {
	LEFT = 0,
	RIGHT = 1,
	TOP = 2,
	BOTTOM = 3
};

namespace {
	std::string transform_drawing(std::string const& drawing, int shift_x, int shift_y, double scale_x, double scale_y) {
		bool is_x = true;
		std::string final;
		final.reserve(drawing.size());

		for (auto const& cur : agi::Split(drawing, ' ')) {
			if (std::all_of(begin(cur), end(cur), isdigit)) {
				int val = boost::lexical_cast<int>(agi::str(cur));
				if (is_x)
					val = (int)((val + shift_x) * scale_x + .5);
				else
					val = (int)((val + shift_y) * scale_y + .5);
				final += std::to_string(val);
				final += ' ';
			}
			else if (cur.size() == 1) {
				char c = tolower(cur[0]);
				if (c == 'm' || c == 'n' || c == 'l' || c == 'b' || c == 's' || c == 'p' || c == 'c') {
					is_x = true;
					final += c;
					final += ' ';
				}
			}
		}

		final.pop_back();
		return final;
	}

	struct resample_state {
		const int *margin;
		double rx;
		double ry;
		double ar;
	};

	void resample_tags(std::string const& name, AssOverrideParameter *cur, void *ud) {
		resample_state *state = static_cast<resample_state *>(ud);

		double resizer = 1.0;
		int shift = 0;

		switch (cur->classification) {
			case AssParameterClass::ABSOLUTE_SIZE:
				resizer = state->ry;
				break;

			case AssParameterClass::ABSOLUTE_POS_X:
				resizer = state->rx;
				shift = state->margin[LEFT];
				break;

			case AssParameterClass::ABSOLUTE_POS_Y:
				resizer = state->ry;
				shift = state->margin[TOP];
				break;

			case AssParameterClass::RELATIVE_SIZE_X:
				resizer = state->ar;
				break;

			case AssParameterClass::RELATIVE_SIZE_Y:
				break;

			case AssParameterClass::DRAWING: {
				cur->Set(transform_drawing(
					cur->Get<std::string>(),
					state->margin[LEFT], state->margin[TOP], state->rx, state->ry));
				return;
			}

			default:
				return;
		}

		VariableDataType curType = cur->GetType();
		if (curType == VariableDataType::FLOAT)
			cur->Set((cur->Get<double>() + shift) * resizer);
		else if (curType == VariableDataType::INT)
			cur->Set<int>((cur->Get<int>() + shift) * resizer + 0.5);
	}

	void resample_line(resample_state *state, AssEntry &line) {
		AssDialogue *diag = dynamic_cast<AssDialogue*>(&line);
		if (diag && !(diag->Comment && (boost::starts_with(diag->Effect.get(), "template") || boost::starts_with(diag->Effect.get(), "code")))) {
			boost::ptr_vector<AssDialogueBlock> blocks(diag->ParseTags());

			for (auto block : blocks | agi::of_type<AssDialogueBlockOverride>())
				block->ProcessParameters(resample_tags, state);

			for (auto drawing : blocks | agi::of_type<AssDialogueBlockDrawing>())
				drawing->text = transform_drawing(drawing->text, state->margin[LEFT], state->margin[TOP], state->rx, state->ry);

			for (size_t i = 0; i < 3; ++i)
				diag->Margin[i] = int((diag->Margin[i] + state->margin[i]) * (i < 2 ? state->rx : state->ry) + 0.5);

			diag->UpdateText(blocks);
		}
		else if (AssStyle *style = dynamic_cast<AssStyle*>(&line)) {
			style->fontsize = int(style->fontsize * state->ry + 0.5);
			style->outline_w *= state->ry;
			style->shadow_w *= state->ry;
			style->spacing *= state->rx;
			style->scalex *= state->ar;
			for (int i = 0; i < 3; i++)
				style->Margin[i] = int((style->Margin[i] + state->margin[i]) * (i < 2 ? state->rx : state->ry) + 0.5);
			style->UpdateData();
		}
	}
}

void ResampleResolution(AssFile *ass, ResampleSettings const& settings) {
	int src_x, src_y;
	ass->GetResolution(src_x, src_y);

	// Add margins to original resolution
	src_x += settings.margin[LEFT] + settings.margin[RIGHT];
	src_y += settings.margin[TOP] + settings.margin[BOTTOM];

	resample_state state = {
		settings.margin,
		double(settings.script_x) / double(src_x),
		double(settings.script_y) / double(src_y),
		1.0
	};

	if (settings.change_ar)
		state.ar = state.rx / state.ry;

	for (auto& line : ass->Line)
		resample_line(&state, line);

	ass->SetScriptInfo("PlayResX", std::to_string(settings.script_x));
	ass->SetScriptInfo("PlayResY", std::to_string(settings.script_y));

	ass->Commit(_("resolution resampling"), AssFile::COMMIT_SCRIPTINFO | AssFile::COMMIT_DIAG_FULL);
}
