// Copyright (c) 2006, Rodrigo Braz Monteiro
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

/// @file base_grid.h
/// @see base_grid.cpp
/// @ingroup main_ui
///




////////////
// Includes
#ifndef AGI_PRE
#include <list>
#include <vector>

#include <wx/grid.h>
#include <wx/scrolbar.h>
#endif


//////////////
// Prototypes
class AssEntry;
class AssDialogue;
class SubsEditBox;
class FrameMain;

/// DOCME
typedef std::list<AssEntry*>::iterator entryIter;



/// DOCME
/// @class BaseGrid
/// @brief DOCME
///
/// DOCME
class BaseGrid : public wxWindow, public BaseSubtitleSelectionController {
private:

	/// DOCME
	int lineHeight;

	/// DOCME
	int colWidth[16];

	/// DOCME
	int lastRow;

	/// DOCME
	int extendRow;

	/// DOCME
	bool holding;

	/// DOCME
	wxFont font;

	/// DOCME
	wxScrollBar *scrollBar;

	/// DOCME
	wxBitmap *bmp;

	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnScroll(wxScrollEvent &event);
	void OnMouseEvent(wxMouseEvent &event);
	void OnKeyPress(wxKeyEvent &event);

	void DrawImage(wxDC &dc);

protected:

	/// DOCME
	FrameMain *parentFrame;

	/// DOCME
	bool showCol[16];


	/// @brief DOCME
	/// @param alternate
	///
	virtual void OnPopupMenu(bool alternate=false) {}
	void ScrollTo(int y);

	/// DOCME
	int yPos;


public:
	// SubtitleSelectionController interface
	virtual void SetActiveLine(AssDialogue *new_line);
	virtual AssDialogue * GetActiveLine() const;
	virtual void SetSelectedSet(const SubtitleSelection &new_selection);
	virtual void GetSelectedSet(SubtitleSelection &selection) const;
	virtual void NextLine();
	virtual void PrevLine();
	// AddSelectionListener implemented in BaseSubtitleSelectionController
	// RemoveSelectionListener implemented in BaseSubtitleSelectionController

public:

	/// DOCME
	SubsEditBox *editBox;

	/// Called by SubsEditBox when the active line changes, to announce to selection listeners
	void AnnounceActiveLineChanged();
	/// Called from various places when the selection is updated
	void AnnounceSelectedSetChanged();


	/// DOCME
	bool byFrame;

	/// DOCME
	std::vector<entryIter> diagMap;

	/// DOCME
	std::vector<AssDialogue *> diagPtrMap;

	/// DOCME
	std::vector<bool> selMap;

	void AdjustScrollbar();
	void SetColumnWidths();
	void BeginBatch();
	void EndBatch();
	void SetByFrame (bool state);

	void SelectRow(int row, bool addToSelected = false, bool select=true);
	void ClearSelection();
	bool IsInSelection(int row, int col=0) const;
	bool IsDisplayed(AssDialogue *line);
	int GetNumberSelection();
	int GetFirstSelRow();
	int GetLastSelRow();
	void SelectVisible();
	wxArrayInt GetSelection(bool *continuous=NULL);

	void Clear();
	void UpdateMaps();
	void UpdateStyle();

	int GetRows() const;
	wxArrayInt GetRangeArray(int n1,int n2);
	void MakeCellVisible(int row, int col,bool center=true);

	AssDialogue *GetDialogue(int n) const;

	BaseGrid(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxWANTS_CHARS, const wxString& name = wxPanelNameStr);
	~BaseGrid();

	DECLARE_EVENT_TABLE()
};


///////
// IDs
enum {

	/// DOCME
	GRID_SCROLLBAR = 1730
};

