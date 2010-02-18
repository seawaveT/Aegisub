// Copyright (c) 2005, Rodrigo Braz Monteiro
// Copyright (c) 2009-2010, Niels Martin Hansen
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
//
// $Id$

/// @file audio_display.cpp
/// @brief Display audio in the main UI
/// @ingroup audio_ui
///


///////////
// Headers
#include "config.h"

#ifndef AGI_PRE
#include <algorithm>

#include <wx/dcclient.h>
#endif

#include "ass_time.h"
#include "audio_controller.h"
#include "audio_display.h"
#include "block_cache.h"
#include "audio_renderer.h"
#include "audio_renderer_spectrum.h"
#include "include/aegisub/audio_provider.h"
#include "options.h"


#undef min
#undef max


class AudioDisplayScrollbar : public AudioDisplayInteractionObject {
	static const int height = 10;

	wxRect bounds;
	wxRect thumb;

	bool dragging;   // user is dragging with the primary mouse button

	int data_length; // total amount of data in control
	int page_length; // amount of data in one page
	int position;    // first item displayed

	int sel_start;   // first data item in selection
	int sel_length;  // number of data items in selection

	AudioDisplay *display;

	// Recalculate thumb bounds from position and length data
	void RecalculateThumb()
	{
		thumb.width = std::max((height+1)/2, bounds.width * page_length / data_length);
		thumb.height = height;
		thumb.x = bounds.width * position / data_length;
		thumb.y = bounds.y;
	}

public:

	AudioDisplayScrollbar(AudioDisplay *_display)
		: dragging(false)
		, data_length(1)
		, page_length(1)
		, position(0)
		, sel_start(-1)
		, sel_length(0)
		, display(_display)
	{
	}

	virtual ~AudioDisplayScrollbar()
	{
	}

	// The audio display has changed size
	void SetDisplaySize(const wxSize &display_size)
	{
		bounds.x = 0;
		bounds.y = display_size.y - height;
		bounds.width = display_size.x;
		bounds.height = height;
		page_length = display_size.x;

		RecalculateThumb();
	}


	const wxRect & GetBounds() const
	{
		return bounds;
	}

	int GetPosition() const
	{
		return position;
	}

	int SetPosition(int new_position)
	{
		// These two conditionals can't be swapped, otherwise the position can become
		// negative if the entire data is shorter than one page.
		if (new_position + page_length >= data_length)
			new_position = data_length - page_length - 1;
		if (new_position < 0)
			new_position = 0;

		// This check is required to avoid mutual recursion with the display
		if (new_position != position)
		{
			position = new_position;
			RecalculateThumb();
			display->ScrollPixelToLeft(position);
		}

		return position;
	}

	void SetSelection(int new_start, int new_length)
	{
		sel_start = new_start;
		sel_length = new_length;
	}

	void ChangeLengths(int new_data_length, int new_page_length)
	{
		data_length = new_data_length;
		page_length = new_page_length;

		RecalculateThumb();
	}

	bool OnMouseEvent(wxMouseEvent &event)
	{
		if (event.LeftIsDown())
		{
			const int thumb_left = event.GetPosition().x - thumb.width/2;
			const int data_length_less_page = data_length - page_length;
			const int shaft_length_less_thumb = bounds.width - thumb.width;

			SetPosition(data_length_less_page * thumb_left / shaft_length_less_thumb);

			dragging = true;
		}
		else if (event.LeftUp())
		{
			dragging = false;
		}

		return dragging;
	}

	void Paint(wxDC &dc, bool has_focus)
	{
		wxColour light(89, 145, 220);
		wxColour dark(8, 4, 13);
		wxColour sel(65, 34, 103);

		if (has_focus)
		{
			light.Set(205, 240, 226);
			sel.Set(82, 107, 213);
		}

		dc.SetPen(wxPen(light));
		dc.SetBrush(wxBrush(dark));
		dc.DrawRectangle(bounds);

		if (sel_length > 0 && sel_start >= 0)
		{
			wxRect r;
			r.x = sel_start * bounds.width / data_length;
			r.y = bounds.y;
			r.width = sel_length * bounds.width / data_length;
			r.height = bounds.height;

			dc.SetPen(wxPen(sel));
			dc.SetBrush(wxBrush(sel));
			dc.DrawRectangle(r);
		}

		dc.SetPen(wxPen(light));
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle(bounds);

		dc.SetPen(wxPen(light));
		dc.SetBrush(wxBrush(light));
		dc.DrawRectangle(thumb);
	}
};



class AudioDisplayTimeline : public AudioDisplayInteractionObject {
	int64_t num_samples;
	int samplerate;
	int samples_per_pixel;
	int pixel_left;

	wxRect bounds;

	wxPoint drag_lastpos;
	bool dragging;

	enum Scale {
		Sc_Millisecond,
		Sc_Centisecond,
		Sc_Decisecond,
		Sc_Second,
		Sc_Decasecond,
		Sc_Minute,
		Sc_Decaminute,
		Sc_Hour,
		Sc_Decahour, // If anyone needs this they should reconsider their project
		Sc_MAX = Sc_Decahour
	};
	Scale scale_minor;
	int scale_major_modulo; // If minor_scale_mark_index % scale_major_modulo == 0 the mark is a major mark
	double scale_minor_divisor; // Absolute scale-mark index multiplied by this number gives sample index for scale mark

	AudioDisplay *display;

public:

	AudioDisplayTimeline(AudioDisplay *_display)
		: num_samples(0)
		, samplerate(44100)
		, samples_per_pixel(1)
		, pixel_left(0)
		, display(_display)
		, dragging(false)
	{
	}

	virtual ~AudioDisplayTimeline()
	{
	}

	int GetHeight() const
	{
		int width, height;
		display->GetTextExtent(_T("0123456789:."), &width, &height);
		return height + 4;
	}

	void SetDisplaySize(const wxSize &display_size)
	{
		// The size is without anything that goes below the timeline (like scrollbar)
		bounds.width = display_size.x;
		bounds.height = GetHeight();
		bounds.x = 0;
		bounds.y = 0;
	}

	const wxRect & GetBounds() const
	{
		return bounds;
	}

	void ChangeAudio(int64_t new_length, int new_samplerate)
	{
		num_samples = new_length;
		samplerate = new_samplerate;
	}

	void ChangeZoom(int new_pixel_samples)
	{
		samples_per_pixel = new_pixel_samples;

		// Pixels per second
		double px_sec = (double)samplerate / (double)samples_per_pixel;

		if (px_sec > 3000) {
			scale_minor = Sc_Millisecond;
			scale_minor_divisor = (double)samplerate / 1000;
			scale_major_modulo = 10;
		} else if (px_sec > 300) {
			scale_minor = Sc_Centisecond;
			scale_minor_divisor = (double)samplerate / 100;
			scale_major_modulo = 10;
		} else if (px_sec > 30) {
			scale_minor = Sc_Decisecond;
			scale_minor_divisor = (double)samplerate / 10;
			scale_major_modulo = 10;
		} else if (px_sec > 3) {
			scale_minor = Sc_Second;
			scale_minor_divisor = (double)samplerate;
			scale_major_modulo = 10;
		} else if (px_sec > 1.0/3.0) {
			scale_minor = Sc_Decasecond;
			scale_minor_divisor = (double)samplerate * 10;
			scale_major_modulo = 6;
		} else if (px_sec > 1.0/9.0) {
			scale_minor = Sc_Minute;
			scale_minor_divisor = (double)samplerate * 60;
			scale_major_modulo = 10;
		} else if (px_sec > 1.0/90.0) {
			scale_minor = Sc_Decaminute;
			scale_minor_divisor = (double)samplerate * 600;
			scale_major_modulo = 6;
		} else {
			scale_minor = Sc_Hour;
			scale_minor_divisor = (double)samplerate * 3600;
			scale_major_modulo = 10;
		}
	}

	void SetPosition(int new_pixel_left)
	{
		if (new_pixel_left < 0)
			new_pixel_left = 0;

		if (new_pixel_left != pixel_left)
		{
			pixel_left = new_pixel_left;
			display->ScrollPixelToLeft(pixel_left);
		}

	}

	bool OnMouseEvent(wxMouseEvent &event)
	{
		if (event.LeftDown())
		{
			drag_lastpos = event.GetPosition();
			dragging = true;
		}
		else if (event.LeftIsDown())
		{
			SetPosition(pixel_left - event.GetPosition().x + drag_lastpos.x);

			drag_lastpos = event.GetPosition();
			dragging = true;
		}
		else if (event.LeftUp())
		{
			dragging = false;
		}

		return dragging;
	}

	void Paint(wxDC &dc)
	{
		wxColour light(89, 145, 220);
		wxColour dark(8, 4, 13);

		int bottom = bounds.y + bounds.height;

		// Background
		dc.SetPen(wxPen(dark));
		dc.SetBrush(wxBrush(dark));
		dc.DrawRectangle(bounds);

		// Top line
		dc.SetPen(wxPen(light));
		dc.DrawLine(bounds.x, bottom-1, bounds.x+bounds.width, bottom-1);

		// Prepare for writing text
		dc.SetTextBackground(dark);
		dc.SetTextForeground(light);

		// Figure out the first scale mark to show
		int64_t sample_left = pixel_left * samples_per_pixel;
		int next_scale_mark = (int)(sample_left / scale_minor_divisor);
		if (next_scale_mark * scale_minor_divisor < sample_left)
			next_scale_mark += 1;
		assert(next_scale_mark * scale_minor_divisor >= sample_left);

		// Draw scale marks
		int next_scale_mark_pos;
		int last_text_right = -1;
		int last_hour = -1, last_minute = -1;
		if (num_samples / samplerate < 3600) last_hour = 0; // Trick to only show hours if audio is longer than 1 hour
		do {
			next_scale_mark_pos = (int)(next_scale_mark * scale_minor_divisor / samples_per_pixel) - pixel_left;
			bool mark_is_major = next_scale_mark % scale_major_modulo == 0;

			if (mark_is_major)
				dc.DrawLine(next_scale_mark_pos, bottom-6, next_scale_mark_pos, bottom);
			else
				dc.DrawLine(next_scale_mark_pos, bottom-4, next_scale_mark_pos, bottom);

			// Print time labels on major scale marks
			if (mark_is_major && next_scale_mark_pos > last_text_right)
			{
				double mark_time = next_scale_mark * scale_minor_divisor / samplerate;
				int mark_hour = (int)(mark_time / 3600);
				int mark_minute = (int)(mark_time / 60) % 60;
				double mark_second = mark_time - mark_hour*3600 - mark_minute*60;

				wxString time_string;
				bool changed_hour = mark_hour != last_hour;
				bool changed_minute = mark_minute != last_minute;

				if (changed_hour)
				{
					time_string = wxString::Format(_T("%d:%02d:"), mark_hour, mark_minute);
					last_hour = mark_hour;
					last_minute = mark_minute;
				}
				else if (changed_minute)
				{
					time_string = wxString::Format(_T("%d:"), mark_minute);
					last_minute = mark_minute;
				}
				if (scale_minor >= Sc_Decisecond)
					time_string += wxString::Format(_T("%02d"), (int)mark_second);
				else if (scale_minor == Sc_Centisecond)
					time_string += wxString::Format(_T("%02.1f"), mark_second);
				else
					time_string += wxString::Format(_T("%02.2f"), mark_second);

				int tw, th;
				dc.GetTextExtent(time_string, &tw, &th);
				last_text_right = next_scale_mark_pos + tw;

				dc.DrawText(time_string, next_scale_mark_pos, 0);
			}

			next_scale_mark += 1;

		} while (next_scale_mark_pos < bounds.width);
	}
};



class AudioMarkerInteractionObject : public AudioDisplayInteractionObject {
	// Object-pair being intracted with
	AudioMarker *marker;
	AudioTimingController *timing_controller;
	// From the audio display
	int scroll_left;
	int pixel_samples;
	// Mouse button used to initiate the drag
	wxMouseButton button_used;

public:
	AudioMarkerInteractionObject(AudioMarker *marker, AudioTimingController *timing_controller, int scroll_left, int pixel_samples, wxMouseButton button_used)
		: marker(marker)
		, timing_controller(timing_controller)
		, scroll_left(scroll_left)
		, pixel_samples(pixel_samples)
		, button_used(button_used)
	{
	}

	virtual ~AudioMarkerInteractionObject()
	{
	}

	virtual bool OnMouseEvent(wxMouseEvent &event)
	{
		if (event.Dragging())
		{
			int64_t sample_pos = (scroll_left + event.GetPosition().x) * pixel_samples;
			timing_controller->OnMarkerDrag(marker, sample_pos);
		}

		// We lose the marker drag if the button used to initiate it goes up
		return !event.ButtonUp(button_used);
	}
};




AudioDisplay::AudioDisplay(wxWindow *parent, AudioController *_controller)
: wxWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS|wxBORDER_SIMPLE)
, controller(_controller)
, provider(0)
, dragged_object(0)
, old_selection(0, 0)
{
	scrollbar = new AudioDisplayScrollbar(this);
	timeline = new AudioDisplayTimeline(this);

	audio_renderer = new AudioRenderer;
	audio_spectrum_renderer = new AudioSpectrumRenderer;

	scroll_left = 0;
	pixel_audio_width = 0;
	scale_amplitude = 1.0;

	track_cursor_pos = -1;

	audio_renderer->SetRenderer(audio_spectrum_renderer);
	audio_renderer->SetAmplitudeScale(scale_amplitude);

	controller->AddAudioListener(this);
	controller->AddTimingListener(this);

	SetZoomLevel(0);

	ReloadRenderingSettings();

	SetMinClientSize(wxSize(-1, 70));
	SetBackgroundStyle(wxBG_STYLE_CUSTOM); // intended to be wxBG_STYLE_PAINT but that doesn't exist for me
	SetThemeEnabled(false);
}


AudioDisplay::~AudioDisplay()
{
	delete audio_renderer;
	delete audio_spectrum_renderer;

	delete timeline;
	delete scrollbar;

	controller->RemoveAudioListener(this);
	controller->RemoveTimingListener(this);
}


void AudioDisplay::ScrollBy(int pixel_amount)
{
	ScrollPixelToLeft(scroll_left + pixel_amount);
}


void AudioDisplay::ScrollPixelToLeft(int pixel_position)
{
	const int client_width = GetClientRect().GetWidth();

	if (pixel_position + client_width >= pixel_audio_width)
		pixel_position = pixel_audio_width - client_width;
	if (pixel_position < 0)
		pixel_position = 0;

	// This check is required to avoid needless redraws, but more importantly to
	// avoid mutual recursion with the scrollbar and timeline.
	if (pixel_position != scroll_left)
	{
		scroll_left = pixel_position;
		scrollbar->SetPosition(scroll_left);
		timeline->SetPosition(scroll_left);
		Refresh();
	}
}


void AudioDisplay::ScrollPixelToCenter(int pixel_position)
{
	ScrollPixelToLeft(pixel_position - GetClientRect().GetWidth()/2);
}


void AudioDisplay::ScrollSampleToLeft(int64_t sample_position)
{
	ScrollPixelToLeft(sample_position / pixel_samples);
}


void AudioDisplay::ScrollSampleToCenter(int64_t sample_position)
{
	ScrollPixelToCenter(sample_position / pixel_samples);
}


void AudioDisplay::ScrollSampleRangeInView(const AudioController::SampleRange &range)
{
	int client_width = GetClientRect().GetWidth();
	int range_begin = range.begin() / pixel_samples;
	int range_end = range.end() / pixel_samples;
	int range_len = range_end - range_begin;

	// Is everything already in view?
	if (range_begin >= scroll_left && range_end <= scroll_left+client_width)
		return;

	// For the rest of the calculation, remove 5 % from each side of the client area.
	// The leftadjust is the amount to subtract from the final scroll_left value.
	int leftadjust = client_width / 20;
	client_width = client_width * 9 / 10;

	// The entire range can fit inside the view, center it
	if (range_len < client_width)
	{
		ScrollPixelToLeft(range_begin - (client_width-range_len)/2 - leftadjust);
	}

	// Range doesn't fit in view and we're viewing a middle part of it, just leave it alone
	else if (range_begin < scroll_left+leftadjust && range_end > scroll_left+leftadjust+client_width)
	{
		// nothing
	}

	// Right edge is in view, scroll it as far to the right as possible
	else if (range_end >= scroll_left+leftadjust && range_end < scroll_left+leftadjust+client_width)
	{
		ScrollPixelToLeft(range_end - client_width - leftadjust);
	}

	// Nothing is in view or the left edge is in view, scroll left edge as far to the left as possible
	else
	{
		ScrollPixelToLeft(range_begin - leftadjust);
	}
}


void AudioDisplay::SetZoomLevel(int new_zoom_level)
{
	zoom_level = new_zoom_level;

	if (!provider)
		return;

	const int samples_per_second = provider ? provider->GetSampleRate() : 48000;
	const int base_pixels_per_second = 50; /// @todo Make this customisable
	const int base_samples_per_pixel = samples_per_second / base_pixels_per_second;

	const int factor = GetZoomLevelFactor(zoom_level);

	const int new_samples_per_pixel = std::max(1, 100 * base_samples_per_pixel / factor);

	if (pixel_samples != new_samples_per_pixel)
	{
		pixel_samples = new_samples_per_pixel;
		audio_renderer->SetSamplesPerPixel(pixel_samples);

		if (provider)
			pixel_audio_width = provider->GetNumSamples() / pixel_samples + 1;
		else
			pixel_audio_width = 1;

		scrollbar->ChangeLengths(pixel_audio_width, GetClientSize().GetWidth());
		timeline->ChangeZoom(pixel_samples);

		Refresh();
	}
}


int AudioDisplay::GetZoomLevel() const
{
	return zoom_level;
}


wxString AudioDisplay::GetZoomLevelDescription(int level) const
{
	const int factor = GetZoomLevelFactor(level);
	const int base_pixels_per_second = 50; /// @todo Make this customisable along with the above
	const int second_pixels = 100 * base_pixels_per_second / factor;

	return wxString::Format(_("%d%%, %d pixel/second"), factor, second_pixels);
}


int AudioDisplay::GetZoomLevelFactor(int level)
{
	int factor = 100;

	if (level > 0)
	{
		factor += 25 * level;
	}
	else if (level < 0)
	{
		if (level >= -5)
			factor += 10 * level;
		else if (level >= -11)
			factor = 50 + (level+5) * 5;
		else
			factor = 20 + level + 11;
		if (factor <= 0)
			factor = 1;
	}

	return factor;
}


void AudioDisplay::SetAmplitudeScale(float scale)
{
	audio_renderer->SetAmplitudeScale(scale);
	Refresh();
}


float AudioDisplay::GetAmplitudeScale() const
{
	return audio_renderer->GetAmplitudeScale();
}


void AudioDisplay::ReloadRenderingSettings()
{
	int spectrum_quality = Options.AsInt(_T("Audio Spectrum Quality"));
#ifdef WITH_FFTW
	// FFTW is so fast we can afford to upgrade quality by two levels
	spectrum_quality += 2;
#endif
	if (spectrum_quality < 0) spectrum_quality = 0;
	if (spectrum_quality > 5) spectrum_quality = 5;

	// Quality indexes:        0  1  2  3   4   5
	int spectrum_width[]    = {8, 9, 9, 9, 10, 11};
	int spectrum_distance[] = {8, 8, 7, 6,  6,  5};

	audio_spectrum_renderer->SetResolution(
		spectrum_width[spectrum_quality],
		spectrum_distance[spectrum_quality]);
	audio_renderer->Invalidate();

	Refresh();
}



BEGIN_EVENT_TABLE(AudioDisplay, wxWindow)
    EVT_MOUSE_EVENTS(AudioDisplay::OnMouseEvent)
    EVT_PAINT(AudioDisplay::OnPaint)
	EVT_SIZE(AudioDisplay::OnSize)
	EVT_SET_FOCUS(AudioDisplay::OnFocus)
	EVT_KILL_FOCUS(AudioDisplay::OnFocus)
END_EVENT_TABLE()



void AudioDisplay::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);

	if (!provider)
	{
		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();
		return;
	}

	int client_width, client_height;
	GetClientSize(&client_width, &client_height);

	wxRect audio_bounds(0, audio_top, client_width, audio_height);
	const wxRect &scrollbar_bounds = scrollbar->GetBounds();
	const wxRect &timeline_bounds = timeline->GetBounds();
	bool redraw_scrollbar = false;
	bool redraw_timeline = false;

	AudioController::SampleRange sel_samples(controller->GetSelection());
	int selection_start = (int)(sel_samples.begin() / pixel_samples);
	int selection_end = (int)(sel_samples.end() / pixel_samples);

	// Draw the tracking cursor first, to avoid it ever disappearing, because that
	// causes optical flicker. It looks better to have two cursors momentarily than
	// having no cursor momentarily.
	if (track_cursor_pos >= 0)
	{
		wxDCPenChanger penchanger(dc, wxPen(*wxWHITE));
		dc.DrawLine(track_cursor_pos-scroll_left, audio_top, track_cursor_pos-scroll_left, audio_top+audio_height);
	}

	wxRegionIterator region(GetUpdateRegion());
	while (region)
	{
		wxRect updrect = region.GetRect();

		redraw_scrollbar |= scrollbar_bounds.Intersects(updrect);
		redraw_timeline |= timeline_bounds.Intersects(updrect);

		if (audio_bounds.Intersects(updrect))
		{
			int p1, p2, p3, p4;
			// p1 -> p2 = before selection
			// p2 -> p3 = in selection
			// p3 -> p4 = after selection
			p1 = scroll_left + updrect.x;
			p2 = selection_start;
			p3 = selection_end;
			p4 = p1 + updrect.width;

			struct subregion {
				int x1, x2;
				bool selected;
				subregion(int x1, int x2, bool selected) : x1(x1), x2(x2), selected(selected) { }
			};
			std::vector<subregion> subregions;

			if (p1 < p2)
				subregions.push_back(subregion(p1, std::min(p2, p4), false));
			if (p4 > p2 && p1 < p3)
				subregions.push_back(subregion(std::max(p1, p2), std::min(p3, p4), true));
			if (p4 > p3)
				subregions.push_back(subregion(std::max(p1, p3), p4, false));

			int x = updrect.x;

			for (std::vector<subregion>::iterator sr = subregions.begin(); sr != subregions.end(); ++sr)
			{
				// Check if the track cursor is inside this redraw region and draw around it, to avoid overdraw
				if (track_cursor_pos >= sr->x1 && track_cursor_pos < sr->x2)
				{
					int tcp_x = track_cursor_pos - sr->x1;
					if (tcp_x > 0)
						audio_renderer->Render(dc, wxPoint(x, audio_top), sr->x1, tcp_x, sr->selected);
					if (track_cursor_pos < sr->x2 - 1)
						audio_renderer->Render(dc, wxPoint(x+tcp_x+1, audio_top), track_cursor_pos+1, sr->x2-track_cursor_pos-1, sr->selected);
				}
				else
				{
					audio_renderer->Render(dc, wxPoint(x, audio_top), sr->x1, sr->x2 - sr->x1, sr->selected);
				}

				x += sr->x2 - sr->x1;
			}

			// Draw markers on top of it all
			AudioMarkerVector markers;
			const int foot_size = 6;
			AudioController::SampleRange updrectsamples(
				(updrect.x+scroll_left-foot_size)*pixel_samples,
				(updrect.x+updrect.width+scroll_left+foot_size)*pixel_samples);
			controller->GetMarkers(updrectsamples, markers);
			if (controller->GetTimingController())
				controller->GetTimingController()->GetMarkers(updrectsamples, markers);
			wxDCPenChanger pen_retainer(dc, wxPen());
			wxDCBrushChanger brush_retainer(dc, wxBrush());
			for (AudioMarkerVector::iterator marker_i = markers.begin(); marker_i != markers.end(); ++marker_i)
			{
				const AudioMarker *marker = *marker_i;
				dc.SetPen(marker->GetStyle());
				int marker_x = marker->GetPosition() / pixel_samples - scroll_left;
				dc.DrawLine(marker_x, audio_top, marker_x, audio_top+audio_height);
				dc.SetBrush(wxBrush(marker->GetStyle().GetColour()));
				dc.SetPen(*wxTRANSPARENT_PEN);
				if (marker->GetFeet() & AudioMarker::Feet_Left)
				{
					wxPoint foot_top[3] = { wxPoint(-foot_size, 0), wxPoint(0, 0), wxPoint(0, foot_size) };
					wxPoint foot_bot[3] = { wxPoint(-foot_size, 0), wxPoint(0, -foot_size), wxPoint(0, 0) };
					dc.DrawPolygon(3, foot_top, marker_x, audio_top);
					dc.DrawPolygon(3, foot_bot, marker_x, audio_top+audio_height);
				}
				if (marker->GetFeet() & AudioMarker::Feet_Right)
				{
					wxPoint foot_top[3] = { wxPoint(foot_size, 0), wxPoint(0, 0), wxPoint(0, foot_size) };
					wxPoint foot_bot[3] = { wxPoint(foot_size, 0), wxPoint(0, -foot_size), wxPoint(0, 0) };
					dc.DrawPolygon(3, foot_top, marker_x, audio_top);
					dc.DrawPolygon(3, foot_bot, marker_x, audio_top+audio_height);
				}
			}
		}

		region++;
	}

	if (redraw_scrollbar)
		scrollbar->Paint(dc, HasFocus());
	if (redraw_timeline)
		timeline->Paint(dc);
}


void AudioDisplay::SetDraggedObject(AudioDisplayInteractionObject *new_obj)
{
	// Special case for audio markers being dragged: they use a temporary wrapper object
	// which must be deleted when it is no longer used.
	AudioMarkerInteractionObject *dragged_marker = dynamic_cast<AudioMarkerInteractionObject*>(dragged_object);
	if (dragged_marker)
		delete dragged_marker;

	dragged_object = new_obj;

	if (dragged_object && !HasCapture())
		CaptureMouse();
	else if (!dragged_object && HasCapture())
		ReleaseMouse();
}


void AudioDisplay::SetTrackCursor(int new_pos, bool show_time)
{
	if (new_pos != track_cursor_pos)
	{
		int old_pos = track_cursor_pos;
		track_cursor_pos = new_pos;

		RefreshRect(wxRect(old_pos - scroll_left - 0, audio_top, 1, audio_height));
		RefreshRect(wxRect(new_pos - scroll_left - 0, audio_top, 1, audio_height));

		AssTime new_label_time;
		new_label_time.SetMS(track_cursor_pos * pixel_samples);
		track_cursor_label = new_label_time.GetASSFormated();
		/// @todo Figure out a sensible way to handle the label bounding rect
	}
}


void AudioDisplay::RemoveTrackCursor()
{
	SetTrackCursor(-1, false);
}


void AudioDisplay::OnMouseEvent(wxMouseEvent& event)
{
	// Check for mouse wheel scrolling
	if (event.GetWheelRotation() != 0)
	{
		// First check if the cursor is inside or outside the display.
		// If it's outside, we want to send the event to the control it's over instead.
		/// @todo Factor this into a reusable function
		{
			wxWindow *targetwindow = wxFindWindowAtPoint(event.GetPosition());
			if (targetwindow && targetwindow != this)
			{
				targetwindow->GetEventHandler()->ProcessEvent(event);
				event.Skip(false);
				return;
			}
		}

		bool zoom = event.CmdDown();
		if (Options.AsBool(_T("Audio Wheel Default To Zoom"))) zoom = !zoom;

		if (!zoom)
		{
			int amount = -event.GetWheelRotation();
			// If the user did a horizontal scroll the amount should be inverted
			// for it to be natural.
			if (event.GetWheelAxis() == 1) amount = -amount;

			// Reset any accumulated zoom
			mouse_zoom_accum = 0;

			ScrollBy(amount);
		}
		else if (event.GetWheelAxis() == 0)
		{
			mouse_zoom_accum += event.GetWheelRotation();
			int zoom_delta = mouse_zoom_accum / event.GetWheelDelta();
			mouse_zoom_accum %= event.GetWheelDelta();
			SetZoomLevel(GetZoomLevel() + zoom_delta);
			/// @todo This has to update the trackbar in the audio box... maybe move handling mouse zoom to
			/// the audio box instead to avoid messing with friend classes?
		}

		// Scroll event processed
		return;
	}

	if (event.IsButton())
		SetFocus();

	// Handle any ongoing drag
	if (dragged_object && HasCapture())
	{
		if (!dragged_object->OnMouseEvent(event))
		{
			SetDraggedObject(0);
			SetCursor(wxNullCursor);
		}
		return;
	}
	else
	{
		// Something is wrong, we might have lost capture somehow.
		// Fix state and pretend it didn't happen.
		SetDraggedObject(0);
		SetCursor(wxNullCursor);
	}

	wxPoint mousepos = event.GetPosition();

	// Check for scrollbar action
	if (scrollbar->GetBounds().Contains(mousepos))
	{
		if (!controller->IsPlaying())
			RemoveTrackCursor();
		if (scrollbar->OnMouseEvent(event))
			SetDraggedObject(scrollbar);
		return;
	}

	// Check for timeline action
	if (timeline->GetBounds().Contains(mousepos))
	{
		SetCursor(wxCursor(wxCURSOR_SIZEWE));
		if (!controller->IsPlaying())
			RemoveTrackCursor();
		if (timeline->OnMouseEvent(event))
			SetDraggedObject(timeline);
		return;
	}

	AudioTimingController *timing = controller->GetTimingController();
	int drag_sensitivity = pixel_samples*3; /// @todo Make this depend on configuration

	// Not scrollbar, not timeline, no button action
	if (event.Moving())
	{
		if (timing)
		{
			int64_t samplepos = (scroll_left + mousepos.x) * pixel_samples;

			if (timing->IsNearbyMarker(samplepos, drag_sensitivity))
				SetCursor(wxCursor(wxCURSOR_SIZEWE));
			else
				SetCursor(wxNullCursor);
		}

		if (!controller->IsPlaying())
			SetTrackCursor(scroll_left + mousepos.x, true);
		else
			RemoveTrackCursor();
	}

	if (event.LeftDown() && timing)
	{
		int64_t samplepos = (scroll_left + mousepos.x) * pixel_samples;
		AudioMarker *marker = timing->OnLeftClick(samplepos, drag_sensitivity);

		if (marker)
		{
			RemoveTrackCursor();
			SetDraggedObject(new AudioMarkerInteractionObject(
				marker, timing,
				scroll_left, pixel_samples,
				wxMOUSE_BTN_LEFT));
			return;
		}
	}

	if (event.RightDown() && timing)
	{
		int64_t samplepos = (scroll_left + mousepos.x) * pixel_samples;
		AudioMarker *marker = timing->OnRightClick(samplepos, drag_sensitivity);

		if (marker)
		{
			RemoveTrackCursor();
			SetDraggedObject(new AudioMarkerInteractionObject(
				marker, timing,
				scroll_left, pixel_samples,
				wxMOUSE_BTN_RIGHT));
			return;
		}
	}

	/// @todo Handle middle click to seek video
}


void AudioDisplay::OnSize(wxSizeEvent &event)
{
	// We changed size, update the sub-controls' internal data and redraw
	wxSize size = GetClientSize();

	scrollbar->SetDisplaySize(size);
	timeline->SetDisplaySize(wxSize(size.x, scrollbar->GetBounds().y));

	audio_height = size.GetHeight();
	audio_height -= scrollbar->GetBounds().GetHeight();
	audio_height -= timeline->GetHeight();
	audio_renderer->SetHeight(audio_height);

	audio_top = timeline->GetHeight();

	Refresh();
}


void AudioDisplay::OnFocus(wxFocusEvent &event)
{
	// The scrollbar indicates focus so repaint that
	RefreshRect(scrollbar->GetBounds());
}


void AudioDisplay::OnAudioOpen(AudioProvider *_provider)
{
	provider = _provider;

	audio_renderer->SetAudioProvider(provider);
	audio_renderer->SetCacheMaxSize(Options.AsInt(_T("Audio Spectrum Memory Max")) * 1024 * 1024);

	SetZoomLevel(zoom_level);

	Refresh();
}


void AudioDisplay::OnAudioClose()
{
	OnAudioOpen(0);
}


void AudioDisplay::OnPlaybackPosition(int64_t sample_position)
{
	SetTrackCursor(sample_position / pixel_samples, false);
}


void AudioDisplay::OnPlaybackStop()
{
	RemoveTrackCursor();
}


void AudioDisplay::OnMarkersMoved()
{
	Refresh();
}


void AudioDisplay::OnSelectionChanged()
{
	AudioController::SampleRange sel(controller->GetSelection());
	scrollbar->SetSelection(sel.begin() / pixel_samples, sel.length() / pixel_samples);

	if (sel.overlaps(old_selection))
	{
		// Only redraw the parts of the selection that changed, to avoid flicker
		int s1 = sel.begin() / pixel_samples - scroll_left;
		int e1 = sel.end() / pixel_samples - scroll_left;
		int s2 = old_selection.begin() / pixel_samples - scroll_left;
		int e2 = old_selection.end() / pixel_samples - scroll_left;
		if (s1 != s2)
			RefreshRect(wxRect(std::min(s1, s2)-10, audio_top, abs(s1-s2)+20, audio_height));
		if (e1 != e2)
			RefreshRect(wxRect(std::min(e1, e2)-10, audio_top, abs(e1-e2)+20, audio_height));
	}
	else
	{
		RefreshRect(wxRect(0, audio_top, GetClientSize().GetX(), audio_height));
	}

	old_selection = sel;
}


void AudioDisplay::OnTimingControllerChanged()
{
	Refresh();
	/// @todo Do something more about the new timing controller?
}
