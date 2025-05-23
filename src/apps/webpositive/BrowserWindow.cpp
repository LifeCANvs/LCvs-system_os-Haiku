/*
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2007, 2010 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
 * Copyright (C) 2010 Rene Gollent <rene@gollent.com>
 * Copyright 2013-2015 Haiku, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BrowserWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <Clipboard.h>
#include <ControlLook.h>
#include <Debug.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <GridLayoutBuilder.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <IconMenuItem.h>
#include <Keymap.h>
#include <LayoutBuilder.h>
#include <Locale.h>
#include <ObjectList.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <SeparatorView.h>
#include <Size.h>
#include <SpaceLayoutItem.h>
#include <StatusBar.h>
#include <StringView.h>
#include <TextControl.h>
#include <UnicodeChar.h>
#include <Url.h>

#include <map>
#include <stdio.h>

#include "AuthenticationPanel.h"
#include "BaseURL.h"
#include "BitmapButton.h"
#include "BookmarkBar.h"
#include "BrowserApp.h"
#include "BrowsingHistory.h"
#include "CredentialsStorage.h"
#include "IconButton.h"
#include "NavMenu.h"
#include "SettingsKeys.h"
#include "SettingsMessage.h"
#include "TabManager.h"
#include "URLInputGroup.h"
#include "WebPage.h"
#include "WebView.h"
#include "WebViewConstants.h"
#include "WindowIcon.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "WebPositive Window"


enum {
	OPEN_LOCATION								= 'open',
	SAVE_PAGE									= 'save',
	GO_BACK										= 'goba',
	GO_FORWARD									= 'gofo',
	STOP										= 'stop',
	HOME										= 'home',
	GOTO_URL									= 'goul',
	RELOAD										= 'reld',
	SHOW_HIDE_BOOKMARK_BAR						= 'shbb',
	CLEAR_HISTORY								= 'clhs',

	CREATE_BOOKMARK								= 'crbm',
	SHOW_BOOKMARKS								= 'shbm',

	ZOOM_FACTOR_INCREASE						= 'zfin',
	ZOOM_FACTOR_DECREASE						= 'zfdc',
	ZOOM_FACTOR_RESET							= 'zfrs',
	ZOOM_TEXT_ONLY								= 'zfto',

	TOGGLE_FULLSCREEN							= 'tgfs',
	TOGGLE_AUTO_HIDE_INTERFACE_IN_FULLSCREEN	= 'tgah',
	CHECK_AUTO_HIDE_INTERFACE					= 'cahi',

	SHOW_PAGE_SOURCE							= 'spgs',

	EDIT_SHOW_FIND_GROUP						= 'sfnd',
	EDIT_HIDE_FIND_GROUP						= 'hfnd',
	EDIT_FIND_NEXT								= 'fndn',
	EDIT_FIND_PREVIOUS							= 'fndp',
	FIND_TEXT_CHANGED							= 'ftxt',

	SELECT_TAB									= 'sltb',
	CYCLE_TABS									= 'ctab',
};


static const int32 kModifiers = B_SHIFT_KEY | B_COMMAND_KEY
	| B_CONTROL_KEY | B_OPTION_KEY | B_MENU_KEY;


static const char* kHandledProtocols[] = {
	"http",
	"https",
	"file",
	"about",
	"data",
	"gopher"
};

static const char* kBookmarkBarSubdir = "Bookmark bar";

static BLayoutItem*
layoutItemFor(BView* view)
{
	BLayout* layout = view->Parent()->GetLayout();
	int32 index = layout->IndexOfView(view);
	return layout->ItemAt(index);
}


class BookmarkMenu : public BNavMenu {
public:
	BookmarkMenu(const char* title, BHandler* target, const entry_ref* navDir)
		:
		BNavMenu(title, B_REFS_RECEIVED, target)
	{
		// Add these items here already, so the shortcuts work even when
		// the menu has never been opened yet.
		_AddStaticItems();

		SetNavDir(navDir);
	}

	virtual void AttachedToWindow()
	{
		RemoveItems(0, CountItems(), true);
		ForceRebuild();
		BNavMenu::AttachedToWindow();
		if (CountItems() > 0)
			AddItem(new BSeparatorItem(), 0);
		_AddStaticItems();
		DoLayout();
	}

private:
	void _AddStaticItems()
	{
		AddItem(new BMenuItem(B_TRANSLATE("Manage bookmarks"),
			new BMessage(SHOW_BOOKMARKS), 'M'), 0);
		AddItem(new BMenuItem(B_TRANSLATE("Bookmark this page"),
			new BMessage(CREATE_BOOKMARK), 'B'), 0);
	}
};


class PageUserData : public BWebView::UserData {
public:
	PageUserData(BView* focusedView)
		:
		fFocusedView(focusedView),
		fPageIcon(NULL),
		fURLInputSelectionStart(-1),
		fURLInputSelectionEnd(-1)
	{
	}

	~PageUserData()
	{
		delete fPageIcon;
	}

	void SetFocusedView(BView* focusedView)
	{
		fFocusedView = focusedView;
	}

	BView* FocusedView() const
	{
		return fFocusedView;
	}

	void SetPageIcon(const BBitmap* icon)
	{
		delete fPageIcon;
		if (icon)
			fPageIcon = new BBitmap(icon);
		else
			fPageIcon = NULL;
	}

	const BBitmap* PageIcon() const
	{
		return fPageIcon;
	}

	void SetURLInputContents(const char* text)
	{
		fURLInputContents = text;
	}

	const BString& URLInputContents() const
	{
		return fURLInputContents;
	}

	void SetURLInputSelection(int32 selectionStart, int32 selectionEnd)
	{
		fURLInputSelectionStart = selectionStart;
		fURLInputSelectionEnd = selectionEnd;
	}

	int32 URLInputSelectionStart() const
	{
		return fURLInputSelectionStart;
	}

	int32 URLInputSelectionEnd() const
	{
		return fURLInputSelectionEnd;
	}

private:
	BView*		fFocusedView;
	BBitmap*	fPageIcon;
	BString		fURLInputContents;
	int32		fURLInputSelectionStart;
	int32		fURLInputSelectionEnd;
};


class CloseButton : public BButton {
public:
	CloseButton(BMessage* message)
		:
		BButton("close button", NULL, message),
		fOverCloseRect(false)
	{
		// Button is 16x16 regardless of font size
		SetExplicitMinSize(BSize(15, 15));
		SetExplicitMaxSize(BSize(15, 15));
	}

	virtual void Draw(BRect updateRect)
	{
		BRect frame = Bounds();
		BRect closeRect(frame.InsetByCopy(4, 4));
		rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
		float tint = B_DARKEN_1_TINT;

		if (fOverCloseRect)
			tint *= 1.4;
		else
			tint *= 1.2;

		if (Value() == B_CONTROL_ON && fOverCloseRect) {
			// Draw the button frame
			be_control_look->DrawButtonFrame(this, frame, updateRect,
				base, base, BControlLook::B_ACTIVATED
					| BControlLook::B_BLEND_FRAME);
			be_control_look->DrawButtonBackground(this, frame,
				updateRect, base, BControlLook::B_ACTIVATED);
			closeRect.OffsetBy(1, 1);
			tint *= 1.2;
		} else {
			SetHighColor(base);
			FillRect(updateRect);
		}

		// Draw the ×
		base = tint_color(base, tint);
		SetHighColor(base);
		SetPenSize(2);
		StrokeLine(closeRect.LeftTop(), closeRect.RightBottom());
		StrokeLine(closeRect.LeftBottom(), closeRect.RightTop());
		SetPenSize(1);
	}

	virtual void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage)
	{
		switch (transit) {
			case B_ENTERED_VIEW:
				fOverCloseRect = true;
				Invalidate();
				break;
			case B_EXITED_VIEW:
				fOverCloseRect = false;
				Invalidate();
				break;
			case B_INSIDE_VIEW:
				fOverCloseRect = true;
				break;
			case B_OUTSIDE_VIEW:
				fOverCloseRect = false;
				break;
		}

		BButton::MouseMoved(where, transit, dragMessage);
	}

private:
	bool fOverCloseRect;
};


// #pragma mark - BrowserWindow


BrowserWindow::BrowserWindow(BRect frame, SettingsMessage* appSettings, const BString& url,
	BPrivate::Network::BUrlContext* context, uint32 interfaceElements, BWebView* webView,
	uint32 workspaces)
	:
	BWebWindow(frame, kApplicationName, B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS, workspaces),
	fIsFullscreen(false),
	fInterfaceVisible(false),
	fMenusRunning(false),
	fPulseRunner(NULL),
	fVisibleInterfaceElements(interfaceElements),
	fContext(context),
	fAppSettings(appSettings),
	fZoomTextOnly(false),
	fShowTabsIfSinglePageOpen(true),
	fAutoHideInterfaceInFullscreenMode(false),
	fAutoHidePointer(false),
	fBookmarkBar(NULL)
{
	// Begin listening to settings changes and read some current values.
	fAppSettings->AddListener(BMessenger(this));
	fZoomTextOnly = fAppSettings->GetValue("zoom text only", fZoomTextOnly);
	fShowTabsIfSinglePageOpen = fAppSettings->GetValue(
		kSettingsKeyShowTabsIfSinglePageOpen, fShowTabsIfSinglePageOpen);

	fAutoHidePointer = fAppSettings->GetValue(kSettingsKeyAutoHidePointer,
		fAutoHidePointer);

	fNewWindowPolicy = fAppSettings->GetValue(kSettingsKeyNewWindowPolicy,
		(uint32)OpenStartPage);
	fNewTabPolicy = fAppSettings->GetValue(kSettingsKeyNewTabPolicy,
		(uint32)OpenBlankPage);
	fStartPageURL = fAppSettings->GetValue(kSettingsKeyStartPageURL,
		kDefaultStartPageURL);
	fSearchPageURL = fAppSettings->GetValue(kSettingsKeySearchPageURL,
		kDefaultSearchPageURL);

	// Create the interface elements
	BMessage* newTabMessage = new BMessage(NEW_TAB);
	newTabMessage->AddString("url", "");
	newTabMessage->AddPointer("window", this);
	newTabMessage->AddBool("select", true);
	fTabManager = new TabManager(BMessenger(this), newTabMessage);

	// Menu
#if INTEGRATE_MENU_INTO_TAB_BAR
	BMenu* mainMenu = new BMenu("≡");
#else
	BMenu* mainMenu = new BMenuBar("Main menu");
#endif
	BMenu* menu = new BMenu(B_TRANSLATE("Window"));
	BMessage* newWindowMessage = new BMessage(NEW_WINDOW);
	newWindowMessage->AddString("url", "");
	BMenuItem* newItem = new BMenuItem(B_TRANSLATE("New window"),
		newWindowMessage, 'N');
	menu->AddItem(newItem);
	newItem->SetTarget(be_app);
	newItem = new BMenuItem(B_TRANSLATE("New tab"),
		new BMessage(*newTabMessage), 'T');
	menu->AddItem(newItem);
	newItem->SetTarget(be_app);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Open location"),
		new BMessage(OPEN_LOCATION), 'L'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Close window"),
		new BMessage(B_QUIT_REQUESTED), 'W', B_SHIFT_KEY));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Close tab"),
		new BMessage(CLOSE_TAB), 'W'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Save page as" B_UTF8_ELLIPSIS),
		new BMessage(SAVE_PAGE), 'S'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Downloads"),
		new BMessage(SHOW_DOWNLOAD_WINDOW), 'D'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Settings"),
		new BMessage(SHOW_SETTINGS_WINDOW), ','));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Cookie manager"),
		new BMessage(SHOW_COOKIE_WINDOW)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Script console"),
		new BMessage(SHOW_CONSOLE_WINDOW)));
	BMenuItem* aboutItem = new BMenuItem(B_TRANSLATE("About"),
		new BMessage(B_ABOUT_REQUESTED));
	menu->AddItem(aboutItem);
	aboutItem->SetTarget(be_app);
	menu->AddSeparatorItem();
	BMenuItem* quitItem = new BMenuItem(B_TRANSLATE("Quit"),
		new BMessage(B_QUIT_REQUESTED), 'Q');
	menu->AddItem(quitItem);
	quitItem->SetTarget(be_app);
	mainMenu->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Edit"));
	menu->AddItem(fCutMenuItem = new BMenuItem(B_TRANSLATE("Cut"),
		new BMessage(B_CUT), 'X'));
	menu->AddItem(fCopyMenuItem = new BMenuItem(B_TRANSLATE("Copy"),
		new BMessage(B_COPY), 'C'));
	menu->AddItem(fPasteMenuItem = new BMenuItem(B_TRANSLATE("Paste"),
		new BMessage(B_PASTE), 'V'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Find"),
		new BMessage(EDIT_SHOW_FIND_GROUP), 'F'));
	menu->AddItem(fFindPreviousMenuItem
		= new BMenuItem(B_TRANSLATE("Find previous"),
		new BMessage(EDIT_FIND_PREVIOUS), 'G', B_SHIFT_KEY));
	menu->AddItem(fFindNextMenuItem = new BMenuItem(B_TRANSLATE("Find next"),
		new BMessage(EDIT_FIND_NEXT), 'G'));
	mainMenu->AddItem(menu);
	fFindPreviousMenuItem->SetEnabled(false);
	fFindNextMenuItem->SetEnabled(false);

	menu = new BMenu(B_TRANSLATE("View"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Reload"), new BMessage(RELOAD),
		'R'));
	// the label will be replaced with the appropriate text later on
	fBookmarkBarMenuItem = new BMenuItem(B_TRANSLATE("Show bookmark bar"),
		new BMessage(SHOW_HIDE_BOOKMARK_BAR));
	menu->AddItem(fBookmarkBarMenuItem);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Increase size"),
		new BMessage(ZOOM_FACTOR_INCREASE), '+'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Decrease size"),
		new BMessage(ZOOM_FACTOR_DECREASE), '-'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Reset size"),
		new BMessage(ZOOM_FACTOR_RESET), '0'));
	fZoomTextOnlyMenuItem = new BMenuItem(B_TRANSLATE("Zoom text only"),
		new BMessage(ZOOM_TEXT_ONLY));
	fZoomTextOnlyMenuItem->SetMarked(fZoomTextOnly);
	menu->AddItem(fZoomTextOnlyMenuItem);

	menu->AddSeparatorItem();
	fFullscreenItem = new BMenuItem(B_TRANSLATE("Full screen"),
		new BMessage(TOGGLE_FULLSCREEN), B_RETURN);
	menu->AddItem(fFullscreenItem);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Page source"),
		new BMessage(SHOW_PAGE_SOURCE), 'U'));
	mainMenu->AddItem(menu);

	fHistoryMenu = new BMenu(B_TRANSLATE("History"));
	fHistoryMenu->AddItem(fBackMenuItem = new BMenuItem(B_TRANSLATE("Back"),
		new BMessage(GO_BACK), B_LEFT_ARROW));
	fHistoryMenu->AddItem(fForwardMenuItem
		= new BMenuItem(B_TRANSLATE("Forward"), new BMessage(GO_FORWARD),
		B_RIGHT_ARROW));
	fHistoryMenu->AddSeparatorItem();
	fHistoryMenuFixedItemCount = fHistoryMenu->CountItems();
	mainMenu->AddItem(fHistoryMenu);

	BPath bookmarkPath;
	entry_ref bookmarkRef;
	if (_BookmarkPath(bookmarkPath) == B_OK
		&& get_ref_for_path(bookmarkPath.Path(), &bookmarkRef) == B_OK) {
		BMenu* bookmarkMenu
			= new BookmarkMenu(B_TRANSLATE("Bookmarks"), this, &bookmarkRef);
		mainMenu->AddItem(bookmarkMenu);

		BDirectory barDir(&bookmarkRef);
		BEntry bookmarkBar(&barDir, kBookmarkBarSubdir);
		entry_ref bookmarkBarRef;
		// TODO we could also check if the folder is empty here.
		if (bookmarkBar.Exists() && bookmarkBar.GetRef(&bookmarkBarRef)
				== B_OK) {
			fBookmarkBar = new BookmarkBar("Bookmarks", this, &bookmarkBarRef);
			fBookmarkBarMenuItem->SetEnabled(true);
		} else
			fBookmarkBarMenuItem->SetEnabled(false);
	} else
		fBookmarkBarMenuItem->SetEnabled(false);

	// Back, Forward, Stop & Home buttons
	fBackButton = new BIconButton("Back", NULL, new BMessage(GO_BACK));
	fBackButton->SetIcon(201);
	fBackButton->TrimIcon();

	fForwardButton = new BIconButton("Forward", NULL, new BMessage(GO_FORWARD));
	fForwardButton->SetIcon(202);
	fForwardButton->TrimIcon();

	fStopButton = new BIconButton("Stop", NULL, new BMessage(STOP));
	fStopButton->SetIcon(204);
	fStopButton->TrimIcon();

	fHomeButton = new BIconButton("Home", NULL, new BMessage(HOME));
	fHomeButton->SetIcon(206);
	fHomeButton->TrimIcon();
	if (!fAppSettings->GetValue(kSettingsKeyShowHomeButton, true))
		fHomeButton->Hide();

	// URL input group
	fURLInputGroup = new URLInputGroup(new BMessage(GOTO_URL));

	// Status Bar
	fStatusText = new BStringView("status", "");
	fStatusText->SetAlignment(B_ALIGN_LEFT);
	fStatusText->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fStatusText->SetExplicitMinSize(BSize(150, 12));
		// Prevent the window from growing to fit a long status message...
	BFont font(be_plain_font);
	font.SetSize(ceilf(font.Size() * 0.8));
	fStatusText->SetFont(&font, B_FONT_SIZE);

	// Loading progress bar
	fLoadingProgressBar = new BStatusBar("progress");
	fLoadingProgressBar->SetMaxValue(100);
	fLoadingProgressBar->Hide();
	font_height height;
	font.GetHeight(&height);
	fLoadingProgressBar->SetBarHeight(height.ascent + height.descent);

	const float kInsetSpacing = 3;
	const float kElementSpacing = 5;

	// Find group
	fFindCloseButton = new CloseButton(new BMessage(EDIT_HIDE_FIND_GROUP));
	fFindTextControl = new BTextControl("find", B_TRANSLATE("Find:"), "", NULL);
	fFindTextControl->SetModificationMessage(new BMessage(FIND_TEXT_CHANGED));
	fFindPreviousButton = new BButton(B_TRANSLATE("Previous"),
		new BMessage(EDIT_FIND_PREVIOUS));
	fFindPreviousButton->SetToolTip(
		B_TRANSLATE_COMMENT("Find previous occurrence of search terms",
			"find bar previous button tooltip"));
	fFindNextButton = new BButton(B_TRANSLATE("Next"),
		new BMessage(EDIT_FIND_NEXT));
	fFindNextButton->SetToolTip(
		B_TRANSLATE_COMMENT("Find next occurrence of search terms",
			"find bar next button tooltip"));
	fFindCaseSensitiveCheckBox = new BCheckBox(B_TRANSLATE("Match case"));
	BGroupLayout* findGroup = BLayoutBuilder::Group<>(B_VERTICAL, 0.0)
		.Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
		.Add(BGroupLayoutBuilder(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fFindCloseButton)
			.Add(fFindTextControl)
			.Add(fFindPreviousButton)
			.Add(fFindNextButton)
			.Add(fFindCaseSensitiveCheckBox)
			.SetInsets(kInsetSpacing, kInsetSpacing,
				kInsetSpacing, kInsetSpacing)
		)
	;

	// Navigation group
	BGroupLayout* navigationGroup = BLayoutBuilder::Group<>(B_VERTICAL, 0.0)
		.Add(BLayoutBuilder::Group<>(B_HORIZONTAL, kElementSpacing)
			.Add(fBackButton)
			.Add(fForwardButton)
			.Add(fStopButton)
			.Add(fHomeButton)
			.Add(fURLInputGroup)
			.SetInsets(kInsetSpacing, kInsetSpacing, kInsetSpacing,
				kInsetSpacing)
		)
		.Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
	;

	// Status bar group
	BGroupLayout* statusGroup = BLayoutBuilder::Group<>(B_VERTICAL, 0.0)
		.Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
		.Add(BLayoutBuilder::Group<>(B_HORIZONTAL, kElementSpacing)
			.Add(fStatusText)
			.Add(fLoadingProgressBar, 0.2)
			.AddStrut(12 - kElementSpacing)
			.SetInsets(kInsetSpacing, 0, kInsetSpacing, 0)
		)
	;

	BBitmapButton* toggleFullscreenButton = new BBitmapButton(kWindowIconBits,
		kWindowIconWidth, kWindowIconHeight, kWindowIconFormat,
		new BMessage(TOGGLE_FULLSCREEN));
	toggleFullscreenButton->SetBackgroundMode(BBitmapButton::MENUBAR_BACKGROUND);

#if !INTEGRATE_MENU_INTO_TAB_BAR
	BMenu* mainMenuItem = mainMenu;
	fMenuGroup = (new BGroupView(B_HORIZONTAL, 0))->GroupLayout();
#else
	BMenu* mainMenuItem = new BMenuBar("Main menu");
	mainMenuItem->AddItem(mainMenu);
	fMenuGroup = fTabManager->MenuContainerLayout();
#endif
	BLayoutBuilder::Group<>(fMenuGroup)
		.Add(mainMenuItem)
		.Add(toggleFullscreenButton, 0.0f)
	;

	if (fAppSettings->GetValue(kSettingsShowBookmarkBar, true))
		_ShowBookmarkBar(true);
	else
		_ShowBookmarkBar(false);

	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), NULL, 0,
		false);

	// Layout
	BGroupView* topView = new BGroupView(B_VERTICAL, 0.0);

#if !INTEGRATE_MENU_INTO_TAB_BAR
	topView->AddChild(fMenuGroup);
#endif
	topView->AddChild(fTabManager->TabGroup());
	topView->AddChild(navigationGroup);
	if (fBookmarkBar != NULL)
		topView->AddChild(fBookmarkBar);
	topView->AddChild(fTabManager->ContainerView());
	topView->AddChild(findGroup);
	topView->AddChild(statusGroup);

	AddChild(topView);

	fURLInputGroup->MakeFocus(true);

	fTabGroup = fTabManager->TabGroup()->GetLayout();
	fNavigationGroup = navigationGroup;
	fFindGroup = findGroup;
	fStatusGroup = statusGroup;
	fToggleFullscreenButton = layoutItemFor(toggleFullscreenButton);

	fFindGroup->SetVisible(false);
	fToggleFullscreenButton->SetVisible(false);

	CreateNewTab(url, true, webView);
	_ShowInterface(true);
	_SetAutoHideInterfaceInFullscreen(fAppSettings->GetValue(
		kSettingsKeyAutoHideInterfaceInFullscreenMode,
		fAutoHideInterfaceInFullscreenMode));

	AddShortcut('F', B_COMMAND_KEY | B_SHIFT_KEY,
		new BMessage(EDIT_HIDE_FIND_GROUP));
	// TODO: Should be a different shortcut, H is usually for Find selection.
	AddShortcut('H', B_COMMAND_KEY,	new BMessage(HOME));

	// Add shortcuts to select a particular tab
	for (int32 i = 1; i <= 9; i++) {
		BMessage* selectTab = new BMessage(SELECT_TAB);
		selectTab->AddInt32("tab index", i - 1);
		char numStr[2];
		snprintf(numStr, sizeof(numStr), "%d", (int) i);
		AddShortcut(numStr[0], B_COMMAND_KEY, selectTab);
	}

	// Add shortcut to cycle through tabs like in every other web browser
	AddShortcut(B_TAB, B_COMMAND_KEY, new BMessage(CYCLE_TABS));

	BKeymap keymap;
	keymap.SetToCurrent();
	BStringList unmodified(3);
	if (keymap.GetModifiedCharacters("+", B_SHIFT_KEY, 0, unmodified)
			== B_OK) {
		int32 count = unmodified.CountStrings();
		for (int32 i = 0; i < count; i++) {
			uint32 key = BUnicodeChar::FromUTF8(unmodified.StringAt(i));
			if (!HasShortcut(key, 0)) {
				// Add semantic zoom in shortcut, bug #7428
				AddShortcut(key, B_COMMAND_KEY,
					new BMessage(ZOOM_FACTOR_INCREASE));
			}
		}
	}
	unmodified.MakeEmpty();

	be_app->PostMessage(WINDOW_OPENED);
}


BrowserWindow::~BrowserWindow()
{
	fAppSettings->RemoveListener(BMessenger(this));
	delete fTabManager;
	delete fPulseRunner;
	delete fSavePanel;
}


void
BrowserWindow::DispatchMessage(BMessage* message, BHandler* target)
{
	const char* bytes;
	int32 modifierKeys;
	if ((message->what == B_KEY_DOWN || message->what == B_UNMAPPED_KEY_DOWN)
		&& message->FindString("bytes", &bytes) == B_OK
		&& message->FindInt32("modifiers", &modifierKeys) == B_OK) {
		if (bytes[0] == B_FUNCTION_KEY) {
			// Some function key Firefox compatibility
			int32 key;
			if (message->FindInt32("key", &key) == B_OK) {
				switch (key) {
					case B_F5_KEY:
						PostMessage(RELOAD);
						break;

					case B_F11_KEY:
						PostMessage(TOGGLE_FULLSCREEN);
						break;

					default:
						break;
				}
			}
		} else if (target == fURLInputGroup->TextView()) {
			// Handle B_RETURN in the URL text control. This is the easiest
			// way to react *only* when the user presses the return key in the
			// address bar, as opposed to trying to load whatever is in there
			// when the text control just goes out of focus.
			if (bytes[0] == B_RETURN) {
				// Do it in such a way that the user sees the Go-button go down.
				_InvokeButtonVisibly(fURLInputGroup->GoButton());
				return;
			} else if (bytes[0] == B_ESCAPE) {
				// Replace edited text with the current URL.
				fURLInputGroup->LockURLInput(false);
				fURLInputGroup->SetText(CurrentWebView()->MainFrameURL());
			}
		} else if (target == fFindTextControl->TextView()) {
			// Handle B_RETURN when the find text control has focus.
			if (bytes[0] == B_RETURN) {
				if ((modifierKeys & B_SHIFT_KEY) != 0)
					_InvokeButtonVisibly(fFindPreviousButton);
				else
					_InvokeButtonVisibly(fFindNextButton);
				return;
			} else if (bytes[0] == B_ESCAPE) {
				_InvokeButtonVisibly(fFindCloseButton);
				return;
			}
		} else if (bytes[0] == B_ESCAPE && !fMenusRunning) {
			if (modifierKeys == B_COMMAND_KEY)
				_ShowInterface(true);
			else {
				// Default escape key behavior:
				PostMessage(STOP);
				return;
			}
		}
	}

	if (message->what == B_MOUSE_MOVED || message->what == B_MOUSE_DOWN
		|| message->what == B_MOUSE_UP) {
		message->FindPoint("where", &fLastMousePos);
		if (message->FindInt64("when", &fLastMouseMovedTime) != B_OK)
			fLastMouseMovedTime = system_time();
		_CheckAutoHideInterface();
	}

	if (message->what == B_MOUSE_WHEEL_CHANGED) {
		BPoint where;
		uint32 buttons;
		CurrentWebView()->GetMouse(&where, &buttons, false);
		// Only do this when the mouse is over the web view
		if (CurrentWebView()->Bounds().Contains(where)) {
			// Zoom and unzoom text on Command + mouse wheel.
			// This could of course (and maybe should be) implemented in the
			// WebView, but there would need to be a way for the WebView to
			// know the setting of the fZoomTextOnly member here. Plus other
			// clients of the API may not want this feature.
			if ((modifiers() & B_COMMAND_KEY) != 0) {
				float deltaY;
				if (message->FindFloat("be:wheel_delta_y", &deltaY) == B_OK) {
					if (deltaY < 0)
						CurrentWebView()->IncreaseZoomFactor(fZoomTextOnly);
					else
						CurrentWebView()->DecreaseZoomFactor(fZoomTextOnly);

					return;
				}
			}
		} else {
			// Also don't scroll up and down if the mouse is not over the
			// web view
			return;
		}
	}

	BWebWindow::DispatchMessage(message, target);
}


void
BrowserWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case OPEN_LOCATION:
			_ShowInterface(true);
			if (fURLInputGroup->TextView()->IsFocus())
				fURLInputGroup->TextView()->SelectAll();
			else
				fURLInputGroup->MakeFocus(true);
			break;

		case RELOAD:
			CurrentWebView()->Reload();
			break;

		case SHOW_HIDE_BOOKMARK_BAR:
			_ShowBookmarkBar(fBookmarkBar->IsHidden());
			break;

		case GOTO_URL:
		{
			BString url;
			if (message->FindString("url", &url) != B_OK)
				url = fURLInputGroup->Text();

			_SetPageIcon(CurrentWebView(), NULL);
			_SmartURLHandler(url);

			break;
		}

		case SAVE_PAGE:
		{
			fSavePanel->SetSaveText(CurrentWebView()->MainFrameTitle());
			fSavePanel->Show();
			break;
		}

		case B_SAVE_REQUESTED:
		{
			entry_ref ref;
			BString name;

			if (message->FindRef("directory", &ref) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BDirectory dir(&ref);
				BFile output(&dir, name,
					B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
				CurrentWebView()->WebPage()->GetContentsAsMHTML(output);
			}

			break;
		}

		case GO_BACK:
			CurrentWebView()->GoBack();
			break;

		case GO_FORWARD:
			CurrentWebView()->GoForward();
			break;

		case STOP:
			CurrentWebView()->StopLoading();
			break;

		case HOME:
			CurrentWebView()->LoadURL(fStartPageURL);
			break;

		case CLEAR_HISTORY: {
			BrowsingHistory* history = BrowsingHistory::DefaultInstance();
			if (history->CountItems() == 0)
				break;
			BAlert* alert = new BAlert(B_TRANSLATE("Confirmation"),
				B_TRANSLATE("Do you really want to "
				"clear the browsing history?"), B_TRANSLATE("Clear"),
				B_TRANSLATE("Cancel"));
			alert->SetShortcut(1, B_ESCAPE);

			if (alert->Go() == 0)
				history->Clear();
			break;
		}

		case CREATE_BOOKMARK:
			_CreateBookmark();
			break;

		case SHOW_BOOKMARKS:
			_ShowBookmarks();
			break;

		case B_REFS_RECEIVED:
		{
			// Currently the only source of these messages is the bookmarks
			// menu.
			// Filter refs into URLs, this also gets rid of refs for folders.
			// For clicks on sub-folders in the bookmarks menu, we have Tracker
			// open the corresponding folder.
			entry_ref ref;
			uint32 addedCount = 0;
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
				BEntry entry(&ref);
				uint32 addedSubCount = 0;
				if (entry.IsDirectory()) {
					BDirectory directory(&entry);
					_AddBookmarkURLsRecursively(directory, message,
						addedSubCount);
				} else {
					BFile file(&ref, B_READ_ONLY);
					BString url;
					if (_ReadURLAttr(file, url)) {
						message->AddString("url", url.String());
						addedSubCount++;
					}
				}
				if (addedSubCount == 0) {
					// Don't know what to do with this entry, just pass it
					// on to the system to handle. Note that this may result
					// in us opening other supported files via the application
					// mechanism.
					be_roster->Launch(&ref);
				}
				addedCount += addedSubCount;
			}
			message->RemoveName("refs");
			if (addedCount > 10) {
				BString string(B_TRANSLATE_COMMENT("Do you want to open "
					"%addedCount bookmarks all at once?", "Don't translate "
					"variable %addedCount."));
				string.ReplaceFirst("%addedCount", BString() << addedCount);

				BAlert* alert = new BAlert(
					B_TRANSLATE("Open bookmarks confirmation"),
					string.String(), B_TRANSLATE("Cancel"),
					B_TRANSLATE("Open all"));
				alert->SetShortcut(0, B_ESCAPE);
				if (alert->Go() == 0)
					break;
			}
			message->AddPointer("window", this);
			be_app->PostMessage(message);
			break;
		}

		case B_SIMPLE_DATA:
		{
			const char* filetype = message->GetString("be:filetypes");
			if (filetype != NULL
				&& strcmp(filetype, "application/x-vnd.Be-bookmark") == 0
				&& LastMouseMovedView() == fBookmarkBar) {
				// Something that can be made into a bookmark (e.g. the page icon)
				// was dragged and dropped on the bookmark bar.
				BPath path;
				if (_BookmarkPath(path) == B_OK && path.Append(kBookmarkBarSubdir) == B_OK) {
					entry_ref ref;
					if (BEntry(path.Path()).GetRef(&ref) == B_OK) {
						message->AddRef("directory", &ref);
							// Add under the same name that Tracker would use, if
							// the ref had been added by dragging and dropping to Tracker.
						_CreateBookmark(message);
					}
				}
				break;
			}

			// User possibly dropped files on this window.
			// If there is more than one entry_ref, let the app handle it
			// (open one new page per ref). If there is one ref, open it in
			// this window.
			type_code type;
			int32 countFound;
			if (message->GetInfo("refs", &type, &countFound) != B_OK
				|| type != B_REF_TYPE) {
				break;
			}
			if (countFound > 1) {
				message->what = B_REFS_RECEIVED;
				be_app->PostMessage(message);
				break;
			}
			entry_ref ref;
			if (message->FindRef("refs", &ref) != B_OK)
				break;
			BEntry entry(&ref, true);
			BPath path;
			if (!entry.Exists() || entry.GetPath(&path) != B_OK)
				break;

			BUrl url(path);
			CurrentWebView()->LoadURL(url);
			break;
		}

		case ZOOM_FACTOR_INCREASE:
			CurrentWebView()->IncreaseZoomFactor(fZoomTextOnly);
			break;
		case ZOOM_FACTOR_DECREASE:
			CurrentWebView()->DecreaseZoomFactor(fZoomTextOnly);
			break;
		case ZOOM_FACTOR_RESET:
			CurrentWebView()->ResetZoomFactor();
			break;
		case ZOOM_TEXT_ONLY:
			fZoomTextOnly = !fZoomTextOnly;
			fZoomTextOnlyMenuItem->SetMarked(fZoomTextOnly);
			// TODO: Would be nice to have an instant update if the page is
			// already zoomed.
			break;

		case TOGGLE_FULLSCREEN:
			ToggleFullscreen();
			break;

		case TOGGLE_AUTO_HIDE_INTERFACE_IN_FULLSCREEN:
			_SetAutoHideInterfaceInFullscreen(
				!fAutoHideInterfaceInFullscreenMode);
			break;

		case CHECK_AUTO_HIDE_INTERFACE:
			_CheckAutoHideInterface();
			break;

		case SHOW_PAGE_SOURCE:
			CurrentWebView()->WebPage()->SendPageSource();
			break;
		case B_PAGE_SOURCE_RESULT:
			_HandlePageSourceResult(message);
			break;

		case EDIT_FIND_NEXT:
			CurrentWebView()->FindString(fFindTextControl->Text(), true,
				fFindCaseSensitiveCheckBox->Value());
			break;
		case FIND_TEXT_CHANGED:
		{
			bool findTextAvailable = strlen(fFindTextControl->Text()) > 0;
			fFindPreviousMenuItem->SetEnabled(findTextAvailable);
			fFindNextMenuItem->SetEnabled(findTextAvailable);
			break;
		}
		case EDIT_FIND_PREVIOUS:
			CurrentWebView()->FindString(fFindTextControl->Text(), false,
				fFindCaseSensitiveCheckBox->Value());
			break;
		case EDIT_SHOW_FIND_GROUP:
			if (!fFindGroup->IsVisible())
				fFindGroup->SetVisible(true);
			fFindTextControl->MakeFocus(true);
			break;
		case EDIT_HIDE_FIND_GROUP:
			if (fFindGroup->IsVisible()) {
				fFindGroup->SetVisible(false);
				if (CurrentWebView() != NULL)
					CurrentWebView()->MakeFocus(true);
			}
			break;

		case B_CUT:
		case B_COPY:
		case B_PASTE:
		{
			BTextView* textView = dynamic_cast<BTextView*>(CurrentFocus());
			if (textView != NULL)
				textView->MessageReceived(message);
			else if (CurrentWebView() != NULL)
				CurrentWebView()->MessageReceived(message);
			break;
		}

		case B_EDITING_CAPABILITIES_RESULT:
		{
			BWebView* webView;
			if (message->FindPointer("view",
					reinterpret_cast<void**>(&webView)) != B_OK
				|| webView != CurrentWebView()) {
				break;
			}
			bool canCut;
			bool canCopy;
			bool canPaste;
			if (message->FindBool("can cut", &canCut) != B_OK)
				canCut = false;
			if (message->FindBool("can copy", &canCopy) != B_OK)
				canCopy = false;
			if (message->FindBool("can paste", &canPaste) != B_OK)
				canPaste = false;
			fCutMenuItem->SetEnabled(canCut);
			fCopyMenuItem->SetEnabled(canCopy);
			fPasteMenuItem->SetEnabled(canPaste);
			break;
		}

		case SHOW_DOWNLOAD_WINDOW:
		case SHOW_SETTINGS_WINDOW:
		case SHOW_CONSOLE_WINDOW:
		case SHOW_COOKIE_WINDOW:
			message->AddUInt32("workspaces", Workspaces());
			be_app->PostMessage(message);
			break;

		case CLOSE_TAB:
			if (fTabManager->CountTabs() > 1) {
				int32 index;
				if (message->FindInt32("tab index", &index) != B_OK)
					index = fTabManager->SelectedTabIndex();
				_ShutdownTab(index);
				_UpdateTabGroupVisibility();
			} else
				PostMessage(B_QUIT_REQUESTED);
			break;

		case SELECT_TAB:
		{
			int32 index;
			if (message->FindInt32("tab index", &index) == B_OK
				&& fTabManager->SelectedTabIndex() != index
				&& fTabManager->CountTabs() > index) {
				fTabManager->SelectTab(index);
			}

			break;
		}

		case CYCLE_TABS:
		{
			int32 index = fTabManager->SelectedTabIndex() + 1;
			if (index >= fTabManager->CountTabs())
				index = 0;
			fTabManager->SelectTab(index);
			break;
		}

		case TAB_CHANGED:
		{
			// This message may be received also when the last tab closed,
			// i.e. with index == -1.
			int32 index;
			if (message->FindInt32("tab index", &index) != B_OK)
				index = -1;
			_TabChanged(index);
			break;
		}

		case SETTINGS_VALUE_CHANGED:
		{
			BString name;
			if (message->FindString("name", &name) != B_OK)
				break;
			bool flag;
			BString string;
			uint32 value;
			if (name == kSettingsKeyShowTabsIfSinglePageOpen
				&& message->FindBool("value", &flag) == B_OK) {
				if (fShowTabsIfSinglePageOpen != flag) {
					fShowTabsIfSinglePageOpen = flag;
					_UpdateTabGroupVisibility();
				}
			} else if (name == kSettingsKeyAutoHidePointer
				&& message->FindBool("value", &flag) == B_OK) {
				fAutoHidePointer = flag;
				if (CurrentWebView())
					CurrentWebView()->SetAutoHidePointer(fAutoHidePointer);
			} else if (name == kSettingsKeyStartPageURL
				&& message->FindString("value", &string) == B_OK) {
				fStartPageURL = string;
			} else if (name == kSettingsKeySearchPageURL
				&& message->FindString("value", &string) == B_OK) {
				fSearchPageURL = string;
			} else if (name == kSettingsKeyNewWindowPolicy
				&& message->FindUInt32("value", &value) == B_OK) {
				fNewWindowPolicy = value;
			} else if (name == kSettingsKeyNewTabPolicy
				&& message->FindUInt32("value", &value) == B_OK) {
				fNewTabPolicy = value;
			} else if (name == kSettingsKeyAutoHideInterfaceInFullscreenMode
				&& message->FindBool("value", &flag) == B_OK) {
				_SetAutoHideInterfaceInFullscreen(flag);
			} else if (name == kSettingsKeyShowHomeButton
				&& message->FindBool("value", &flag) == B_OK) {
				if (flag)
					fHomeButton->Show();
				else
					fHomeButton->Hide();
			} else if (name == kSettingsShowBookmarkBar
				&& message->FindBool("value", &flag) == B_OK) {
				_ShowBookmarkBar(flag);
			}
			break;
		}
		case ADD_CONSOLE_MESSAGE:
			be_app->PostMessage(message);
			BWebWindow::MessageReceived(message);
			break;

		case B_COPY_TARGET:
		{
			const char* filetype = message->GetString("be:filetypes");
			if (filetype != NULL && strcmp(filetype, "application/x-vnd.Be-bookmark") == 0) {
				// Tracker replied after the user dragged and dropped something
				// that can be bookmarked (e.g. the page icon) to a Tracker window.
				_CreateBookmark(message);
				break;
			} else {
				BWebWindow::MessageReceived(message);
				break;
			}
		}

		default:
			BWebWindow::MessageReceived(message);
			break;
	}
}


status_t
BrowserWindow::Archive(BMessage* archive, bool deep) const
{
	status_t status = archive->AddRect("window frame", Frame());
	if (status == B_OK)
		status = archive->AddUInt32("window workspaces", Workspaces());

	for (int i = 0; i < fTabManager->CountTabs(); i++) {
		BWebView* view = dynamic_cast<BWebView*>(fTabManager->ViewForTab(i));
		if (view == NULL) {
			continue;
		}

		if (status == B_OK)
			status = archive->AddString("tab", view->MainFrameURL());
	}

	return status;
}


bool
BrowserWindow::QuitRequested()
{
	// TODO: Check for modified form data and ask user for confirmation, etc.

	BMessage message(WINDOW_CLOSED);
	Archive(&message);

	// Iterate over all tabs to delete all BWebViews.
	// Do this here, so WebKit tear down happens earlier.
	SetCurrentWebView(NULL);
	while (fTabManager->CountTabs() > 0)
		_ShutdownTab(0);

	message.AddRect("window frame", WindowFrame());
	be_app->PostMessage(&message);
	return true;
}


void
BrowserWindow::MenusBeginning()
{
	_UpdateHistoryMenu();
	_UpdateClipboardItems();
	fMenusRunning = true;
}


void
BrowserWindow::MenusEnded()
{
	fMenusRunning = false;
}


void
BrowserWindow::ScreenChanged(BRect screenSize, color_space format)
{
	if (fIsFullscreen)
		_ResizeToScreen();
}


void
BrowserWindow::WorkspacesChanged(uint32 oldWorkspaces, uint32 newWorkspaces)
{
	if (fIsFullscreen)
		_ResizeToScreen();
}


static bool
viewIsChild(const BView* parent, const BView* view)
{
	if (parent == view)
		return true;

	int32 count = parent->CountChildren();
	for (int32 i = 0; i < count; i++) {
		BView* child = parent->ChildAt(i);
		if (viewIsChild(child, view))
			return true;
	}
	return false;
}


void
BrowserWindow::SetCurrentWebView(BWebView* webView)
{
	if (webView == CurrentWebView())
		return;

	if (CurrentWebView() != NULL) {
		// Remember the currently focused view before switching tabs,
		// so that we can revert the focus when switching back to this tab
		// later.
		PageUserData* userData = static_cast<PageUserData*>(
			CurrentWebView()->GetUserData());
		if (userData == NULL) {
			userData = new PageUserData(CurrentFocus());
			CurrentWebView()->SetUserData(userData);
		}
		userData->SetFocusedView(CurrentFocus());
		userData->SetURLInputContents(fURLInputGroup->Text());
		int32 selectionStart;
		int32 selectionEnd;
		fURLInputGroup->TextView()->GetSelection(&selectionStart,
			&selectionEnd);
		userData->SetURLInputSelection(selectionStart, selectionEnd);
	}

	BWebWindow::SetCurrentWebView(webView);

	if (webView != NULL) {
		webView->SetAutoHidePointer(fAutoHidePointer);

		_UpdateTitle(webView->MainFrameTitle());

		// Restore the previous focus or focus the web view.
		PageUserData* userData = static_cast<PageUserData*>(
			webView->GetUserData());
		BView* focusedView = NULL;
		if (userData != NULL)
			focusedView = userData->FocusedView();

		if (focusedView != NULL
			&& viewIsChild(GetLayout()->View(), focusedView)) {
			focusedView->MakeFocus(true);
		} else
			webView->MakeFocus(true);

		bool state = fURLInputGroup->IsURLInputLocked();
		fURLInputGroup->LockURLInput(false);
			// Unlock it so the following code can update the URL

		if (userData != NULL) {
			fURLInputGroup->SetPageIcon(userData->PageIcon());
			if (userData->URLInputContents().Length())
				fURLInputGroup->SetText(userData->URLInputContents());
			else
				fURLInputGroup->SetText(webView->MainFrameURL());
			if (userData->URLInputSelectionStart() >= 0) {
				fURLInputGroup->TextView()->Select(
					userData->URLInputSelectionStart(),
					userData->URLInputSelectionEnd());
			}
		} else {
			fURLInputGroup->SetPageIcon(NULL);
			fURLInputGroup->SetText(webView->MainFrameURL());
		}

		fURLInputGroup->LockURLInput(state);
			// Restore the state

		// Trigger update of the interface to the new page, by requesting
		// to resend all notifications.
		webView->WebPage()->ResendNotifications();
	} else
		_UpdateTitle("");
}


bool
BrowserWindow::IsBlankTab() const
{
	if (CurrentWebView() == NULL)
		return false;
	BString requestedURL = CurrentWebView()->MainFrameRequestedURL();
	return requestedURL.Length() == 0
		|| requestedURL == _NewTabURL(fTabManager->CountTabs() == 1);
}


void
BrowserWindow::CreateNewTab(const BString& _url, bool select,
	BWebView* webView)
{
	bool applyNewPagePolicy = webView == NULL;
	// Executed in app thread (new BWebPage needs to be created in app thread).
	if (webView == NULL)
		webView = new BWebView("web view", fContext);

	bool isNewWindow = fTabManager->CountTabs() == 0;

	fTabManager->AddTab(webView, B_TRANSLATE("New tab"));

	BString url(_url);
	if (applyNewPagePolicy && url.Length() == 0)
		url = _NewTabURL(isNewWindow);

	if (url.Length() > 0)
		webView->LoadURL(url.String());

	if (select) {
		fTabManager->SelectTab(fTabManager->CountTabs() - 1);
		SetCurrentWebView(webView);
		webView->WebPage()->ResendNotifications();
		fURLInputGroup->SetPageIcon(NULL);
		fURLInputGroup->SetText(url.String());
		fURLInputGroup->MakeFocus(true);
	}

	_ShowInterface(true);
	_UpdateTabGroupVisibility();
}


BRect
BrowserWindow::WindowFrame() const
{
	if (fIsFullscreen)
		return fNonFullscreenWindowFrame;
	else
		return Frame();
}


void
BrowserWindow::ToggleFullscreen()
{
	if (fIsFullscreen) {
		MoveTo(fNonFullscreenWindowFrame.LeftTop());
		ResizeTo(fNonFullscreenWindowFrame.Width(),
			fNonFullscreenWindowFrame.Height());

		SetFlags(Flags() & ~(B_NOT_RESIZABLE | B_NOT_MOVABLE));
		SetLook(B_DOCUMENT_WINDOW_LOOK);

		_ShowInterface(true);
	} else {
		fNonFullscreenWindowFrame = Frame();
		_ResizeToScreen();

		SetFlags(Flags() | (B_NOT_RESIZABLE | B_NOT_MOVABLE));
		SetLook(B_TITLED_WINDOW_LOOK);
	}
	fIsFullscreen = !fIsFullscreen;
	fFullscreenItem->SetMarked(fIsFullscreen);
	fToggleFullscreenButton->SetVisible(fIsFullscreen);
}


// #pragma mark - Notification API


void
BrowserWindow::NavigationRequested(const BString& url, BWebView* view)
{
}


void
BrowserWindow::NewWindowRequested(const BString& url, bool primaryAction)
{
	// Always open new windows in the application thread, since
	// creating a BWebView will try to grab the application lock.
	// But our own WebPage may already try to lock us from within
	// the application thread -> dead-lock. Thus we can't wait for
	// a reply here.
	BMessage message(NEW_TAB);
	message.AddPointer("window", this);
	message.AddString("url", url);
	message.AddBool("select", primaryAction);
	be_app->PostMessage(&message);
}


void
BrowserWindow::NewPageCreated(BWebView* view, BRect windowFrame,
	bool modalDialog, bool resizable, bool activate)
{
	if (windowFrame.IsValid()) {
		BrowserWindow* window = new BrowserWindow(windowFrame, fAppSettings,
			BString(), fContext, INTERFACE_ELEMENT_STATUS,
			view);
		window->Show();
	} else
		CreateNewTab(BString(), activate, view);
}


void
BrowserWindow::CloseWindowRequested(BWebView* view)
{
	int32 index = fTabManager->TabForView(view);
	if (index < 0) {
		// Tab is already gone.
		return;
	}
	BMessage message(CLOSE_TAB);
	message.AddInt32("tab index", index);
	PostMessage(&message, this);
}


void
BrowserWindow::LoadNegotiating(const BString& url, BWebView* view)
{
	if (view != CurrentWebView()) {
		// Update the userData contents instead so the user sees
		// the correct URL when they switch back to that tab.
		PageUserData* userData = static_cast<PageUserData*>(
			view->GetUserData());
		if (userData != NULL && userData->URLInputContents().Length() == 0) {
			userData->SetURLInputContents(url);
		}
	}

	fURLInputGroup->SetText(url.String());

	BString status(B_TRANSLATE("Requesting %url"));
	status.ReplaceFirst("%url", url);
	view->WebPage()->SetStatusMessage(status);
}


void
BrowserWindow::LoadCommitted(const BString& url, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	// This hook is invoked when the load is committed.
	fURLInputGroup->SetText(url.String());

	BString status(B_TRANSLATE("Loading %url"));
	status.ReplaceFirst("%url", url);
	view->WebPage()->SetStatusMessage(status);
}


void
BrowserWindow::LoadProgress(float progress, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	if (progress < 100 && fLoadingProgressBar->IsHidden())
		_ShowProgressBar(true);
	else if (progress == 100 && !fLoadingProgressBar->IsHidden())
		_ShowProgressBar(false);
	fLoadingProgressBar->SetTo(progress);
}


void
BrowserWindow::LoadFailed(const BString& url, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	BString status(B_TRANSLATE_COMMENT("%url failed", "Loading URL failed. "
		"Don't translate variable %url."));
	status.ReplaceFirst("%url", url);
	view->WebPage()->SetStatusMessage(status);
	if (!fLoadingProgressBar->IsHidden())
		fLoadingProgressBar->Hide();
}


void
BrowserWindow::LoadFinished(const BString& url, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	fURLInputGroup->SetText(url.String());

	BString status(B_TRANSLATE_COMMENT("%url finished", "Loading URL "
		"finished. Don't translate variable %url."));
	status.ReplaceFirst("%url", url);
	view->WebPage()->SetStatusMessage(status);
	if (!fLoadingProgressBar->IsHidden())
		fLoadingProgressBar->Hide();

	NavigationCapabilitiesChanged(fBackButton->IsEnabled(),
		fForwardButton->IsEnabled(), false, view);

	int32 tabIndex = fTabManager->TabForView(view);
	if (tabIndex > 0 && strcmp(B_TRANSLATE("New tab"),
		fTabManager->TabLabel(tabIndex)) == 0)
			fTabManager->SetTabLabel(tabIndex, url);
}


void
BrowserWindow::MainDocumentError(const BString& failingURL,
	const BString& localizedDescription, BWebView* view)
{
	// Make sure we show the page that contains the view.
	if (!_ShowPage(view))
		return;

	// Try delegating the URL to an external app instead.
	int32 at = failingURL.FindFirst(":");
	if (at > 0) {
		BString proto;
		failingURL.CopyInto(proto, 0, at);

		bool handled = false;

		for (unsigned int i = 0; i < sizeof(kHandledProtocols) / sizeof(char*);
				i++) {
			handled = (proto == kHandledProtocols[i]);
			if (handled)
				break;
		}

		if (!handled) {
			_SmartURLHandler(failingURL);
			return;
		}
	}

	BWebWindow::MainDocumentError(failingURL, localizedDescription, view);

	// TODO: Remove the failing URL from the BrowsingHistory!
}


void
BrowserWindow::TitleChanged(const BString& title, BWebView* view)
{
	int32 tabIndex = fTabManager->TabForView(view);
	if (tabIndex < 0)
		return;

	fTabManager->SetTabLabel(tabIndex, title);

	if (view != CurrentWebView())
		return;

	_UpdateTitle(title);
}


void
BrowserWindow::IconReceived(const BBitmap* icon, BWebView* view)
{
	// The view may already be gone, since this notification arrives
	// asynchronously.
	if (!fTabManager->HasView(view))
		return;

	_SetPageIcon(view, icon);
}


void
BrowserWindow::ResizeRequested(float width, float height, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	// Ignore request when there is more than one BWebView embedded.
	if (fTabManager->CountTabs() > 1)
		return;

	// Make sure the new frame is not larger than the screen frame minus
	// window decorator border.
	BScreen screen(this);
	BRect screenFrame = screen.Frame();
	BRect decoratorFrame = DecoratorFrame();
	BRect frame = Frame();

	screenFrame.left += decoratorFrame.left - frame.left;
	screenFrame.right += decoratorFrame.right - frame.right;
	screenFrame.top += decoratorFrame.top - frame.top;
	screenFrame.bottom += decoratorFrame.bottom - frame.bottom;

	width = min_c(width, screen.Frame().Width());
	height = min_c(height, screen.Frame().Height());

	frame.right = frame.left + width;
	frame.bottom = frame.top + height;

	// frame is now not larger than screenFrame, but may still be partly outside
	if (!screenFrame.Contains(frame)) {
		if (frame.left < screenFrame.left)
			frame.OffsetBy(screenFrame.left - frame.left, 0);
		else if (frame.right > screenFrame.right)
			frame.OffsetBy(screenFrame.right - frame.right, 0);
		if (frame.top < screenFrame.top)
			frame.OffsetBy(screenFrame.top - frame.top, 0);
		else if (frame.bottom > screenFrame.bottom)
			frame.OffsetBy(screenFrame.bottom - frame.bottom, 0);
	}

	MoveTo(frame.left, frame.top);
	ResizeTo(width, height);
}


void
BrowserWindow::SetToolBarsVisible(bool flag, BWebView* view)
{
	// TODO
	// TODO: Ignore request when there is more than one BWebView embedded!
}


void
BrowserWindow::SetStatusBarVisible(bool flag, BWebView* view)
{
	// TODO
	// TODO: Ignore request when there is more than one BWebView embedded!
}


void
BrowserWindow::SetMenuBarVisible(bool flag, BWebView* view)
{
	// TODO
	// TODO: Ignore request when there is more than one BWebView embedded!
}


void
BrowserWindow::SetResizable(bool flag, BWebView* view)
{
	// TODO: Ignore request when there is more than one BWebView embedded!

	if (flag)
		SetFlags(Flags() & ~B_NOT_RESIZABLE);
	else
		SetFlags(Flags() | B_NOT_RESIZABLE);
}


void
BrowserWindow::StatusChanged(const BString& statusText, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	if (fStatusText)
		fStatusText->SetText(statusText.String());
}


void
BrowserWindow::NavigationCapabilitiesChanged(bool canGoBackward,
	bool canGoForward, bool canStop, BWebView* view)
{
	if (view != CurrentWebView())
		return;

	fBackButton->SetEnabled(canGoBackward);
	fForwardButton->SetEnabled(canGoForward);
	fStopButton->SetEnabled(canStop);

	fBackMenuItem->SetEnabled(canGoBackward);
	fForwardMenuItem->SetEnabled(canGoForward);
}


void
BrowserWindow::UpdateGlobalHistory(const BString& url)
{
	BrowsingHistory::DefaultInstance()->AddItem(BrowsingHistoryItem(url));

	fURLInputGroup->SetText(CurrentWebView()->MainFrameURL());
}


bool
BrowserWindow::AuthenticationChallenge(BString message, BString& inOutUser,
	BString& inOutPassword, bool& inOutRememberCredentials,
	uint32 failureCount, BWebView* view)
{
	CredentialsStorage* persistentStorage
		= CredentialsStorage::PersistentInstance();
	CredentialsStorage* sessionStorage
		= CredentialsStorage::SessionInstance();

	// TODO: Using the message as key here is not so smart.
	HashString key(message);

	if (failureCount == 0) {
		if (persistentStorage->Contains(key)) {
			Credentials credentials = persistentStorage->GetCredentials(key);
			inOutUser = credentials.Username();
			inOutPassword = credentials.Password();
			return true;
		} else if (sessionStorage->Contains(key)) {
			Credentials credentials = sessionStorage->GetCredentials(key);
			inOutUser = credentials.Username();
			inOutPassword = credentials.Password();
			return true;
		}
	}
	// Switch to the page for which this authentication is required.
	if (!_ShowPage(view))
		return false;

	AuthenticationPanel* panel = new AuthenticationPanel(Frame());
		// Panel auto-destructs.
	bool success = panel->getAuthentication(message, inOutUser, inOutPassword,
		inOutRememberCredentials, failureCount > 0, inOutUser, inOutPassword,
		&inOutRememberCredentials);
	if (success) {
		Credentials credentials(inOutUser, inOutPassword);
		if (inOutRememberCredentials)
			persistentStorage->PutCredentials(key, credentials);
		else
			sessionStorage->PutCredentials(key, credentials);
	}
	return success;
}


// #pragma mark - private


void
BrowserWindow::_UpdateTitle(const BString& title)
{
	BString windowTitle;

	if (title.Length() > 0)
		windowTitle = title;
	else {
		BWebView* webView = CurrentWebView();
		if (webView != NULL) {
			BString url = webView->MainFrameURL();
			int32 leafPos = url.FindLast('/');
			url.Remove(0, leafPos + 1);
			windowTitle = url;
		}
	}

	if (windowTitle.Length() > 0)
		windowTitle << " - ";
	windowTitle << kApplicationName;
	SetTitle(windowTitle.String());
}


void
BrowserWindow::_UpdateTabGroupVisibility()
{
	if (Lock()) {
		if (fInterfaceVisible)
			fTabGroup->SetVisible(_TabGroupShouldBeVisible());
		fTabManager->SetCloseButtonsAvailable(fTabManager->CountTabs() > 1);
		Unlock();
	}
}


bool
BrowserWindow::_TabGroupShouldBeVisible() const
{
	return (fShowTabsIfSinglePageOpen || fTabManager->CountTabs() > 1)
		&& (fVisibleInterfaceElements & INTERFACE_ELEMENT_TABS) != 0;
}


void
BrowserWindow::_ShutdownTab(int32 index)
{
	BView* view = fTabManager->RemoveTab(index);
	BWebView* webView = dynamic_cast<BWebView*>(view);
	if (webView == CurrentWebView())
		SetCurrentWebView(NULL);
	if (webView != NULL)
		webView->Shutdown();
	else
		delete view;
}


void
BrowserWindow::_TabChanged(int32 index)
{
	SetCurrentWebView(dynamic_cast<BWebView*>(fTabManager->ViewForTab(index)));
}


status_t
BrowserWindow::_BookmarkPath(BPath& path) const
{
	status_t ret = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (ret != B_OK)
		return ret;

	ret = path.Append(kApplicationName);
	if (ret != B_OK)
		return ret;

	ret = path.Append("Bookmarks");
	if (ret != B_OK)
		return ret;

	return create_directory(path.Path(), 0777);
}

/*! If fileName is an empty BString, a valid file name will be derived from title.
	miniIcon and largeIcon may be NULL.
*/
void
BrowserWindow::_CreateBookmark(const BPath& path, BString fileName, const BString& title,
	const BString& url, const BBitmap* miniIcon, const BBitmap* largeIcon)
{
	// Determine the file name if one was not provided
	bool presetFileName = true;
	if (fileName.IsEmpty() == true) {
		presetFileName = false;
		fileName = title;
		if (fileName.Length() == 0) {
			fileName = url;
			int32 leafPos = fileName.FindLast('/');
			if (leafPos >= 0)
				fileName.Remove(0, leafPos + 1);
		}
		fileName.ReplaceAll('/', '-');
		fileName.Truncate(B_FILE_NAME_LENGTH - 1);
	}

	BPath entryPath(path);
	status_t status = entryPath.Append(fileName);
	BEntry entry;
	if (status == B_OK)
		status = entry.SetTo(entryPath.Path(), true);

	// There are several reasons why an entry matching the path argument could already exist.
	if (status == B_OK && entry.Exists() == true) {
		off_t size;
		entry.GetSize(&size);
		char attrName[B_ATTR_NAME_LENGTH];
		BNode node(&entry);
		status_t attrStatus = node.GetNextAttrName(attrName);
		if (strcmp(attrName, "_trk/pinfo_le") == 0)
			attrStatus = node.GetNextAttrName(attrName);

		if (presetFileName == true && size == 0 && attrStatus == B_ENTRY_NOT_FOUND) {
			// Tracker's drag-and-drop routine created an empty entry for us to fill in.
			// Go ahead and write to the existing entry.
		} else {
			BDirectory directory(path.Path());
			if (_CheckBookmarkExists(directory, fileName, url) == true) {
				// The existing entry is a bookmark with the same URL.  No further action needed.
				return;
			} else {
				// Find a unique name for the bookmark.
				int32 tries = 1;
				while (entry.Exists()) {
					fileName << " " << tries++;
					entryPath = path;
					status = entryPath.Append(fileName);
					if (status == B_OK)
						status = entry.SetTo(entryPath.Path(), true);
					if (status != B_OK)
						break;
				}
			}
		}
	}

	BFile bookmarkFile;
	if (status == B_OK) {
		status = bookmarkFile.SetTo(&entry,
			B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	}

	// Write bookmark meta data
	if (status == B_OK)
		status = bookmarkFile.WriteAttrString("META:url", &url);
	if (status == B_OK) {
		bookmarkFile.WriteAttrString("META:title", &title);
	}

	BNodeInfo nodeInfo(&bookmarkFile);
	if (status == B_OK) {
		status = nodeInfo.SetType("application/x-vnd.Be-bookmark");
		// Replace the standard Be-bookmark file icons with the argument icons,
		// if any were provided.
		if (status == B_OK) {
			status_t ret = B_OK;
			if (miniIcon != NULL) {
				ret = nodeInfo.SetIcon(miniIcon, B_MINI_ICON);
				if (ret != B_OK) {
					fprintf(stderr, "Failed to store mini icon for bookmark: "
						"%s\n", strerror(ret));
				}
			}
			if (largeIcon != NULL && ret == B_OK)
				ret = nodeInfo.SetIcon(largeIcon, B_LARGE_ICON);
			else if (largeIcon == NULL && miniIcon != NULL && ret == B_OK) {
				// If largeIcon is not available but miniIcon is, use a magnified miniIcon instead.
				BBitmap substituteLargeIcon(BRect(0, 0, 31, 31), B_BITMAP_NO_SERVER_LINK,
					B_CMAP8);
				const uint8* src = (const uint8*)miniIcon->Bits();
				uint32 srcBPR = miniIcon->BytesPerRow();
				uint8* dst = (uint8*)substituteLargeIcon.Bits();
				uint32 dstBPR = substituteLargeIcon.BytesPerRow();
				for (uint32 y = 0; y < 16; y++) {
					const uint8* s = src;
					uint8* d = dst;
					for (uint32 x = 0; x < 16; x++) {
						*d++ = *s;
						*d++ = *s++;
					}
					dst += dstBPR;
					s = src;
					for (uint32 x = 0; x < 16; x++) {
						*d++ = *s;
						*d++ = *s++;
					}
					dst += dstBPR;
					src += srcBPR;
				}
				ret = nodeInfo.SetIcon(&substituteLargeIcon, B_LARGE_ICON);
			} else
				ret = B_OK;
			if (ret != B_OK) {
				fprintf(stderr, "Failed to store large icon for bookmark: "
					"%s\n", strerror(ret));
			}
		}
	}

	if (status != B_OK) {
		BString message(B_TRANSLATE_COMMENT("There was an error creating the "
			"bookmark file.\n\nError: %error", "Don't translate variable "
			"%error"));
		message.ReplaceFirst("%error", strerror(status));
		BAlert* alert = new BAlert(B_TRANSLATE("Bookmark error"),
			message.String(), B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
		alert->Go();
		return;
	}
}


void
BrowserWindow::_CreateBookmark(BMessage* message)
{
		entry_ref ref;
		BMessage originatorData;
		const char* url;
		const char* title;
		bool validData = (message->FindRef("directory", &ref) == B_OK
			&& message->FindMessage("be:originator-data", &originatorData) == B_OK
			&& originatorData.FindString("url", &url) == B_OK
			&& originatorData.FindString("title", &title) == B_OK);

		// Optional data
		const char* fileName;
		if (message->FindString("name", &fileName) != B_OK) {
			// This string is only present if the message originated from Tracker (drag and drop).
			fileName = "";
		}
		const BBitmap* miniIcon = NULL;
		const BBitmap* largeIcon = NULL;
		originatorData.FindData("miniIcon", B_COLOR_8_BIT_TYPE,
			reinterpret_cast<const void**>(&miniIcon), NULL);
		originatorData.FindData("largeIcon", B_COLOR_8_BIT_TYPE,
			reinterpret_cast<const void**>(&miniIcon), NULL);

		if (validData == true) {
			_CreateBookmark(BPath(&ref), BString(fileName), BString(title), BString(url),
				miniIcon, largeIcon);
		} else {
			BString message(B_TRANSLATE("There was an error setting up "
				"the bookmark."));
			BAlert* alert = new BAlert(B_TRANSLATE("Bookmark error"),
				message.String(), B_TRANSLATE("OK"), NULL, NULL,
				B_WIDTH_AS_USUAL, B_STOP_ALERT);
			alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
			alert->Go();
		}
		return;
}


void
BrowserWindow::_CreateBookmark()
{
	BString fileName;
		// A name will be derived from the title later.
	BString title(CurrentWebView()->MainFrameTitle());
	BString url(CurrentWebView()->MainFrameURL());
	BPath path;
	status_t status = _BookmarkPath(path);

	BBitmap* miniIcon = NULL;
	BBitmap* largeIcon = NULL;
	PageUserData* userData = static_cast<PageUserData*>(CurrentWebView()->GetUserData());
	if (userData != NULL && userData->PageIcon() != NULL) {
		miniIcon = new BBitmap(BRect(0, 0, 15, 15), B_BITMAP_NO_SERVER_LINK, B_CMAP8);
		miniIcon->ImportBits(userData->PageIcon());
		// TODO:  retrieve the large icon too, once PageUserData can provide it.
	}

	if (status == B_OK)
		_CreateBookmark(path, fileName, title, url, miniIcon, largeIcon);
	else {
		BString message(B_TRANSLATE_COMMENT("There was an error retrieving "
			"the bookmark folder.\n\nError: %error", "Don't translate the "
			"variable %error"));
		message.ReplaceFirst("%error", strerror(status));
		BAlert* alert = new BAlert(B_TRANSLATE("Bookmark error"),
			message.String(), B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
		alert->Go();
	}
	return;
}


void
BrowserWindow::_ShowBookmarks()
{
	BPath path;
	entry_ref ref;
	status_t status = _BookmarkPath(path);
	if (status == B_OK)
		status = get_ref_for_path(path.Path(), &ref);
	if (status == B_OK)
		status = be_roster->Launch(&ref);

	if (status != B_OK && status != B_ALREADY_RUNNING) {
		BString message(B_TRANSLATE_COMMENT("There was an error trying to "
			"show the Bookmarks folder.\n\nError: %error",
			"Don't translate variable %error"));
		message.ReplaceFirst("%error", strerror(status));
		BAlert* alert = new BAlert(B_TRANSLATE("Bookmark error"),
			message.String(), B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
		alert->Go();
		return;
	}
}


bool BrowserWindow::_CheckBookmarkExists(BDirectory& directory,
	const BString& bookmarkName, const BString& url) const
{
	BEntry entry;
	while (directory.GetNextEntry(&entry) == B_OK) {
		char entryName[B_FILE_NAME_LENGTH];
		if (entry.GetName(entryName) != B_OK || bookmarkName != entryName)
			continue;
		BString storedURL;
		BFile file(&entry, B_READ_ONLY);
		if (_ReadURLAttr(file, storedURL)) {
			// Just bail if the bookmark already exists
			if (storedURL == url)
				return true;
		}
	}
	return false;
}


bool
BrowserWindow::_ReadURLAttr(BFile& bookmarkFile, BString& url) const
{
	return bookmarkFile.InitCheck() == B_OK
		&& bookmarkFile.ReadAttrString("META:url", &url) == B_OK;
}


void
BrowserWindow::_AddBookmarkURLsRecursively(BDirectory& directory,
	BMessage* message, uint32& addedCount) const
{
	BEntry entry;
	while (directory.GetNextEntry(&entry) == B_OK) {
		if (entry.IsDirectory()) {
			BDirectory subBirectory(&entry);
			// At least preserve the entry file handle when recursing into
			// sub-folders... eventually we will run out, though, with very
			// deep hierarchy.
			entry.Unset();
			_AddBookmarkURLsRecursively(subBirectory, message, addedCount);
		} else {
			BString storedURL;
			BFile file(&entry, B_READ_ONLY);
			if (_ReadURLAttr(file, storedURL)) {
				message->AddString("url", storedURL.String());
				addedCount++;
			}
		}
	}
}


void
BrowserWindow::_SetPageIcon(BWebView* view, const BBitmap* icon)
{
	PageUserData* userData = static_cast<PageUserData*>(view->GetUserData());
	if (userData == NULL) {
		userData = new(std::nothrow) PageUserData(NULL);
		if (userData == NULL)
			return;
		view->SetUserData(userData);
	}
	// The PageUserData makes a copy of the icon, which we pass on to
	// the TabManager for display in the respective tab.
	userData->SetPageIcon(icon);
	fTabManager->SetTabIcon(view, userData->PageIcon());
	if (view == CurrentWebView())
		fURLInputGroup->SetPageIcon(icon);
}


static void
addItemToMenuOrSubmenu(BMenu* menu, BMenuItem* newItem)
{
	BString baseURLLabel = baseURL(BString(newItem->Label()));
	for (int32 i = menu->CountItems() - 1; i >= 0; i--) {
		BMenuItem* item = menu->ItemAt(i);
		BString label = item->Label();
		if (label.FindFirst(baseURLLabel) >= 0) {
			if (item->Submenu()) {
				// Submenu was already added in previous iteration.
				item->Submenu()->AddItem(newItem);
				return;
			} else {
				menu->RemoveItem(item);
				BMenu* subMenu = new BMenu(baseURLLabel.String());
				subMenu->AddItem(item);
				subMenu->AddItem(newItem);
				// Add common submenu for this base URL, clickable.
				BMessage* message = new BMessage(GOTO_URL);
				message->AddString("url", baseURLLabel.String());
				menu->AddItem(new BMenuItem(subMenu, message), i);
				return;
			}
		}
	}
	menu->AddItem(newItem);
}


static void
addOrDeleteMenu(BMenu* menu, BMenu* toMenu)
{
	if (menu->CountItems() > 0)
		toMenu->AddItem(menu);
	else
		delete menu;
}


void
BrowserWindow::_UpdateHistoryMenu()
{
	BMenuItem* menuItem;
	while ((menuItem = fHistoryMenu->RemoveItem(fHistoryMenuFixedItemCount)))
		delete menuItem;

	BrowsingHistory* history = BrowsingHistory::DefaultInstance();
	if (!history->Lock())
		return;

	int32 count = history->CountItems();
	BMenuItem* clearHistoryItem = new BMenuItem(B_TRANSLATE("Clear history"),
		new BMessage(CLEAR_HISTORY));
	clearHistoryItem->SetEnabled(count > 0);
	fHistoryMenu->AddItem(clearHistoryItem);
	if (count == 0) {
		history->Unlock();
		return;
	}
	fHistoryMenu->AddSeparatorItem();

	BDateTime todayStart = BDateTime::CurrentDateTime(B_LOCAL_TIME);
	todayStart.SetTime(BTime(0, 0, 0));

	BDateTime oneDayAgoStart = todayStart;
	oneDayAgoStart.Date().AddDays(-1);

	BDateTime twoDaysAgoStart = oneDayAgoStart;
	twoDaysAgoStart.Date().AddDays(-1);

	BDateTime threeDaysAgoStart = twoDaysAgoStart;
	threeDaysAgoStart.Date().AddDays(-1);

	BDateTime fourDaysAgoStart = threeDaysAgoStart;
	fourDaysAgoStart.Date().AddDays(-1);

	BDateTime fiveDaysAgoStart = fourDaysAgoStart;
	fiveDaysAgoStart.Date().AddDays(-1);

	BMenu* todayMenu = new BMenu(B_TRANSLATE("Today"));
	BMenu* yesterdayMenu = new BMenu(B_TRANSLATE("Yesterday"));
	BMenu* twoDaysAgoMenu = new BMenu(
		twoDaysAgoStart.Date().LongDayName().String());
	BMenu* threeDaysAgoMenu = new BMenu(
		threeDaysAgoStart.Date().LongDayName().String());
	BMenu* fourDaysAgoMenu = new BMenu(
		fourDaysAgoStart.Date().LongDayName().String());
	BMenu* fiveDaysAgoMenu = new BMenu(
		fiveDaysAgoStart.Date().LongDayName().String());
	BMenu* earlierMenu = new BMenu(B_TRANSLATE("Earlier"));

	for (int32 i = 0; i < count; i++) {
		BrowsingHistoryItem historyItem = history->HistoryItemAt(i);
		BMessage* message = new BMessage(GOTO_URL);
		message->AddString("url", historyItem.URL().String());

		BString truncatedUrl(historyItem.URL());
		be_plain_font->TruncateString(&truncatedUrl, B_TRUNCATE_END, 480);
		menuItem = new BMenuItem(truncatedUrl, message);

		if (historyItem.DateTime() < fiveDaysAgoStart)
			addItemToMenuOrSubmenu(earlierMenu, menuItem);
		else if (historyItem.DateTime() < fourDaysAgoStart)
			addItemToMenuOrSubmenu(fiveDaysAgoMenu, menuItem);
		else if (historyItem.DateTime() < threeDaysAgoStart)
			addItemToMenuOrSubmenu(fourDaysAgoMenu, menuItem);
		else if (historyItem.DateTime() < twoDaysAgoStart)
			addItemToMenuOrSubmenu(threeDaysAgoMenu, menuItem);
		else if (historyItem.DateTime() < oneDayAgoStart)
			addItemToMenuOrSubmenu(twoDaysAgoMenu, menuItem);
		else if (historyItem.DateTime() < todayStart)
			addItemToMenuOrSubmenu(yesterdayMenu, menuItem);
		else
			addItemToMenuOrSubmenu(todayMenu, menuItem);
	}
	history->Unlock();

	addOrDeleteMenu(todayMenu, fHistoryMenu);
	addOrDeleteMenu(yesterdayMenu, fHistoryMenu);
	addOrDeleteMenu(twoDaysAgoMenu, fHistoryMenu);
	addOrDeleteMenu(threeDaysAgoMenu, fHistoryMenu);
	addOrDeleteMenu(fourDaysAgoMenu, fHistoryMenu);
	addOrDeleteMenu(fiveDaysAgoMenu, fHistoryMenu);
	addOrDeleteMenu(earlierMenu, fHistoryMenu);
}


void
BrowserWindow::_UpdateClipboardItems()
{
	BTextView* focusTextView = dynamic_cast<BTextView*>(CurrentFocus());
	if (focusTextView != NULL) {
		int32 selectionStart;
		int32 selectionEnd;
		focusTextView->GetSelection(&selectionStart, &selectionEnd);
		bool hasSelection = selectionStart < selectionEnd;
		bool canPaste = false;
		// A BTextView has the focus.
		if (be_clipboard->Lock()) {
			BMessage* data = be_clipboard->Data();
			if (data != NULL)
				canPaste = data->HasData("text/plain", B_MIME_TYPE);
			be_clipboard->Unlock();
		}
		fCutMenuItem->SetEnabled(hasSelection);
		fCopyMenuItem->SetEnabled(hasSelection);
		fPasteMenuItem->SetEnabled(canPaste);
	} else if (CurrentWebView() != NULL) {
		// Trigger update of the clipboard items, even if the
		// BWebView doesn't have focus, we'll dispatch these message
		// there anyway. This works so fast that the user can never see
		// the wrong enabled state when the menu opens until the result
		// message arrives. The initial state needs to be enabled, since
		// standard shortcut handling is always wrapped inside MenusBeginning()
		// and MenusEnded(), and since we update items asynchronously, we need
		// to have them enabled to begin with.
		fCutMenuItem->SetEnabled(true);
		fCopyMenuItem->SetEnabled(true);
		fPasteMenuItem->SetEnabled(true);

		CurrentWebView()->WebPage()->SendEditingCapabilities();
	}
}


bool
BrowserWindow::_ShowPage(BWebView* view)
{
	if (view != CurrentWebView()) {
		int32 tabIndex = fTabManager->TabForView(view);
		if (tabIndex < 0) {
			// Page seems to be gone already?
			return false;
		}
		fTabManager->SelectTab(tabIndex);
		_TabChanged(tabIndex);
		UpdateIfNeeded();
	}
	return true;
}


void
BrowserWindow::_ResizeToScreen()
{
	BScreen screen(this);
	MoveTo(0, 0);
	ResizeTo(screen.Frame().Width(), screen.Frame().Height());
}


void
BrowserWindow::_SetAutoHideInterfaceInFullscreen(bool doIt)
{
	if (fAutoHideInterfaceInFullscreenMode == doIt)
		return;

	fAutoHideInterfaceInFullscreenMode = doIt;
	if (fAppSettings->GetValue(kSettingsKeyAutoHideInterfaceInFullscreenMode,
			doIt) != doIt) {
		fAppSettings->SetValue(kSettingsKeyAutoHideInterfaceInFullscreenMode,
			doIt);
	}

	if (fAutoHideInterfaceInFullscreenMode) {
		BMessage message(CHECK_AUTO_HIDE_INTERFACE);
		fPulseRunner = new BMessageRunner(BMessenger(this), &message, 300000);
	} else {
		delete fPulseRunner;
		fPulseRunner = NULL;
		_ShowInterface(true);
	}
}


void
BrowserWindow::_CheckAutoHideInterface()
{
	if (!fIsFullscreen || !fAutoHideInterfaceInFullscreenMode
		|| (CurrentWebView() != NULL && !CurrentWebView()->IsFocus())) {
		return;
	}

	if (fLastMousePos.y == 0)
		_ShowInterface(true);
	else if (fNavigationGroup->IsVisible()
		&& fLastMousePos.y > fNavigationGroup->Frame().bottom
		&& system_time() - fLastMouseMovedTime > 1000000) {
		// NOTE: Do not re-use navigationGroupBottom in the above
		// check, since we only want to hide the interface when it is visible.
		_ShowInterface(false);
	}
}


void
BrowserWindow::_ShowInterface(bool show)
{
	if (fInterfaceVisible == show)
		return;

	fInterfaceVisible = show;

	if (show) {
#if !INTEGRATE_MENU_INTO_TAB_BAR
		fMenuGroup->SetVisible(
			(fVisibleInterfaceElements & INTERFACE_ELEMENT_MENU) != 0);
#endif
		fTabGroup->SetVisible(_TabGroupShouldBeVisible());
		fNavigationGroup->SetVisible(
			(fVisibleInterfaceElements & INTERFACE_ELEMENT_NAVIGATION) != 0);
		fStatusGroup->SetVisible(
			(fVisibleInterfaceElements & INTERFACE_ELEMENT_STATUS) != 0);
	} else {
		fMenuGroup->SetVisible(false);
		fTabGroup->SetVisible(false);
		fNavigationGroup->SetVisible(false);
		fStatusGroup->SetVisible(false);
	}
	// TODO: Setting the group visible seems to unhide the status bar.
	// Fix in Haiku?
	while (!fLoadingProgressBar->IsHidden())
		fLoadingProgressBar->Hide();
}


void
BrowserWindow::_ShowProgressBar(bool show)
{
	if (show) {
		if (!fStatusGroup->IsVisible() && (fVisibleInterfaceElements
			& INTERFACE_ELEMENT_STATUS) != 0)
				fStatusGroup->SetVisible(true);
		fLoadingProgressBar->Show();
	} else {
		if (!fInterfaceVisible)
			fStatusGroup->SetVisible(false);
		// TODO: This is also used in _ShowInterface. Without it the status bar
		// doesn't always hide again. It may be an Interface Kit bug.
		while (!fLoadingProgressBar->IsHidden())
			fLoadingProgressBar->Hide();
	}
}


void
BrowserWindow::_InvokeButtonVisibly(BButton* button)
{
	button->SetValue(B_CONTROL_ON);
	UpdateIfNeeded();
	button->Invoke();
	snooze(1000);
	button->SetValue(B_CONTROL_OFF);
}


BString
BrowserWindow::_NewTabURL(bool isNewWindow) const
{
	BString url;
	uint32 policy = isNewWindow ? fNewWindowPolicy : fNewTabPolicy;
	// Implement new page policy
	switch (policy) {
		case OpenStartPage:
			url = fStartPageURL;
			break;
		case OpenSearchPage:
			url.SetTo(fSearchPageURL);
			url.ReplaceAll("%s", "");
			break;
		case CloneCurrentPage:
			if (CurrentWebView() != NULL)
				url = CurrentWebView()->MainFrameURL();
			break;
		case OpenBlankPage:
		default:
			break;
	}
	return url;
}


BString
BrowserWindow::_EncodeURIComponent(const BString& search)
{
	// We have to take care of some of the escaping before we hand over the
	// search string to WebKit, if we want queries like "4+3" to not be
	// searched as "4 3".
	const BString escCharList = " $&`:<>[]{}\"+#%@/;=?\\^|~\',";
	BString result = search;
	char hexcode[4];

	for (int32 i = 0; i < result.Length(); i++) {
		if (escCharList.FindFirst(result[i]) != B_ERROR) {
			sprintf(hexcode, "%02X", (unsigned int)result[i]);
			result.SetByteAt(i, '%');
			result.Insert(hexcode, i + 1);
			i += 2;
		}
	}

	return result;
}


void
BrowserWindow::_VisitURL(const BString& url)
{
	// fURLInputGroup->TextView()->SetText(url);
	CurrentWebView()->LoadURL(url.String());
}


void
BrowserWindow::_VisitSearchEngine(const BString& search)
{
	BString searchQuery = search;

	BString searchPrefix;
	search.CopyCharsInto(searchPrefix, 0, 2);

	// Default search URL
	BString engine(fSearchPageURL);

	// Check if the string starts with one of the search engine shortcuts
	for (int i = 0; kSearchEngines[i].url != NULL; i++) {
		if (kSearchEngines[i].shortcut == searchPrefix) {
			engine = kSearchEngines[i].url;
			searchQuery.Remove(0, 2);
			break;
		}
	}

	engine.ReplaceAll("%s", _EncodeURIComponent(searchQuery));
	_VisitURL(engine);
}


inline bool
BrowserWindow::_IsValidDomainChar(char ch)
{
	// TODO: Currenlty, only a whitespace character breaks a domain name. It
	// might be a good idea (or a bad one) to make character filtering based on
	// the IDNA 2008 standard.

	return ch != ' ';
}


/*! \brief "smart" parser for user-entered URLs

	We try to be flexible in what we accept as a valid URL. The protocol may
	be missing, or something we can't handle (in that case we run the matching
	app). If all attempts to make sense of the input fail, we make a search
	engine query for it.
 */
void
BrowserWindow::_SmartURLHandler(const BString& url)
{
	// First test if the URL has a protocol field
	int32 at = url.FindFirst(":");

	if (at != B_ERROR) {
		// There is a protocol, let's see if we can handle it
		BString proto;
		url.CopyInto(proto, 0, at);

		bool handled = false;

		// First try the built-in supported ones
		for (unsigned int i = 0; i < sizeof(kHandledProtocols) / sizeof(char*);
				i++) {
			handled = (proto == kHandledProtocols[i]);
			if (handled)
				break;
		}

		if (handled) {
			// This is the easy case, a complete and well-formed URL, we can
			// navigate to it without further efforts.
			_VisitURL(url);
			return;
		} else {
			// There is what looks like a protocol, but one we don't know.
			// Ask the BRoster if there is a matching filetype and app which
			// can handle it.
			BString temp;
			temp = "application/x-vnd.Be.URL.";
			temp += proto;

			const char* argv[] = { url.String(), NULL };

			if (be_roster->Launch(temp.String(), 1, argv) == B_OK)
				return;
		}
	}

	// There is no protocol or only an unsupported one. So let's try harder to
	// guess what the request is.

	// "localhost" is a special case, it is a valid domain name but has no dots.
	// Handle it separately.
	if (url == "localhost")
		_VisitURL("http://localhost/");
	else {
		// Also handle URLs starting with "localhost" followed by a path.
		const char* localhostPrefix = "localhost/";

		if (url.Compare(localhostPrefix, strlen(localhostPrefix)) == 0)
			_VisitURL(url);
		else {
			// In all other cases we try to detect a valid domain name. There
			// must be at least one dot and no spaces until the first / in the
			// URL.
			bool isURL = false;

			for (int32 i = 0; i < url.CountChars(); i++) {
				if (url[i] == '.')
					isURL = true;
				else if (url[i] == '/')
					break;
				else if (!_IsValidDomainChar(url[i])) {
					isURL = false;

					break;
				}
			}

			if (isURL) {
				// This is apparently an URL missing the protocol part. In that
				// case we default to http.
				BString prefixed = "http://";
				prefixed << url;
				_VisitURL(prefixed);
				return;
			} else {
				// We couldn't find anything that looks like an URL. Let's
				// assume what we have is a search request and go to the search
				// engine.
				_VisitSearchEngine(url);
			}
		}
	}
}


void
BrowserWindow::_HandlePageSourceResult(const BMessage* message)
{
	// TODO: This should be done in an extra thread perhaps. Doing it in
	// the application thread is not much better, since it actually draws
	// the pages...

	BPath pathToPageSource;

	BString url;
	status_t ret = message->FindString("url", &url);
	if (ret == B_OK && url.FindFirst("file://") == 0) {
		// Local file
		url.Remove(0, strlen("file://"));
		pathToPageSource.SetTo(url.String());
	} else {
		// Something else, store it.
		// TODO: What if it isn't HTML, but for example SVG?
		BString source;
		ret = message->FindString("source", &source);

		if (ret == B_OK)
			ret = find_directory(B_SYSTEM_TEMP_DIRECTORY, &pathToPageSource);

		BString tmpFileName("PageSource_");
		tmpFileName << system_time() << ".html";
		if (ret == B_OK)
			ret = pathToPageSource.Append(tmpFileName.String());

		BFile pageSourceFile(pathToPageSource.Path(),
			B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
		if (ret == B_OK)
			ret = pageSourceFile.InitCheck();

		if (ret == B_OK) {
			ssize_t written = pageSourceFile.Write(source.String(),
				source.Length());
			if (written != source.Length())
				ret = (status_t)written;
		}

		if (ret == B_OK) {
			const char* type = "text/html";
			size_t size = strlen(type);
			pageSourceFile.WriteAttr("BEOS:TYPE", B_STRING_TYPE, 0, type, size);
				// If it fails we don't care.
		}
	}

	entry_ref ref;
	if (ret == B_OK)
		ret = get_ref_for_path(pathToPageSource.Path(), &ref);

	if (ret == B_OK) {
		BMessage refsMessage(B_REFS_RECEIVED);
		ret = refsMessage.AddRef("refs", &ref);
		if (ret == B_OK) {
			ret = be_roster->Launch("text/x-source-code", &refsMessage);
			if (ret == B_ALREADY_RUNNING)
				ret = B_OK;
		}
	}

	if (ret != B_OK) {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "Failed to show the "
			"page source: %s\n", strerror(ret));
		BAlert* alert = new BAlert(B_TRANSLATE("Page source error"), buffer,
			B_TRANSLATE("OK"));
		alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
		alert->Go(NULL);
	}
}


void
BrowserWindow::_ShowBookmarkBar(bool show)
{
	// It is not allowed to show the bookmark bar when it is empty
	if (show && (fBookmarkBar == NULL || fBookmarkBar->CountItems() <= 1))
	{
		fBookmarkBarMenuItem->SetMarked(false);
		return;
	}

	fBookmarkBarMenuItem->SetMarked(show);

	if (fBookmarkBar == NULL || fBookmarkBar->IsHidden() != show)
		return;

	fAppSettings->SetValue(kSettingsShowBookmarkBar, show);

	if (show)
		fBookmarkBar->Show();
	else
		fBookmarkBar->Hide();
}
