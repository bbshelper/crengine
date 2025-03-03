/*******************************************************

 CoolReader Engine

 lvdocview.cpp:  XML DOM tree rendering tools

 (c) Vadim Lopatin, 2000-2009
 This source code is distributed under the terms of
 GNU General Public License
 See LICENSE file for details

 *******************************************************/

#include "../include/crsetup.h"
#include "../include/fb2def.h"
#include "../include/lvdocview.h"
#include "../include/rtfimp.h"

#include "../include/lvstyles.h"
#include "../include/lvrend.h"
#include "../include/lvstsheet.h"
#include "../include/textlang.h"

#include "../include/wolutil.h"
#include "../include/crtxtenc.h"
#include "../include/crtrace.h"
#include "../include/epubfmt.h"
#include "../include/chmfmt.h"
#include "../include/wordfmt.h"
#include "../include/pdbfmt.h"
#include "../include/fb3fmt.h"
#include "../include/docxfmt.h"
#include "../include/odtfmt.h"

/// to show page bounds rectangles
//#define SHOW_PAGE_RECT

/// uncomment to save copy of loaded document to file
//#define SAVE_COPY_OF_LOADED_DOCUMENT

// TESTING GRAYSCALE MODE
#if 0
#undef COLOR_BACKBUFFER
#define COLOR_BACKBUFFER 0
#undef GRAY_BACKBUFFER_BITS
#define GRAY_BACKBUFFER_BITS 4
#endif


#if 0
#define REQUEST_RENDER(txt) {CRLog::trace("request render from "  txt); requestRender();}
#define CHECK_RENDER(txt) {CRLog::trace("LVDocView::checkRender() - from " txt); checkRender();}
#else
#define REQUEST_RENDER(txt) requestRender();
#define CHECK_RENDER(txt) checkRender();
#endif

/// to avoid showing title/author if coverpage image present
#define NO_TEXT_IN_COVERPAGE

const char
		* def_stylesheet =
				"image { text-align: center; text-indent: 0px } \n"
					"empty-line { height: 1em; } \n"
					"sub { vertical-align: sub; font-size: 70% }\n"
					"sup { vertical-align: super; font-size: 70% }\n"
					"body > image, section > image { text-align: center; margin-before: 1em; margin-after: 1em }\n"
					"p > image { display: inline }\n"
					"a { vertical-align: super; font-size: 80% }\n"
					"p { margin-top:0em; margin-bottom: 0em }\n"
					"text-author { font-weight: bold; font-style: italic; margin-left: 5%}\n"
					"empty-line { height: 1em }\n"
					"epigraph { margin-left: 30%; margin-right: 4%; text-align: left; text-indent: 1px; font-style: italic; margin-top: 15px; margin-bottom: 25px; font-family: Times New Roman, serif }\n"
					"strong, b { font-weight: bold }\n"
					"emphasis, i { font-style: italic }\n"
					"title { text-align: center; text-indent: 0px; font-size: 130%; font-weight: bold; margin-top: 10px; margin-bottom: 10px; font-family: Times New Roman, serif }\n"
					"subtitle { text-align: center; text-indent: 0px; font-size: 150%; margin-top: 10px; margin-bottom: 10px }\n"
					"title { page-break-before: always; page-break-inside: avoid; page-break-after: avoid; }\n"
					"body { text-align: justify; text-indent: 2em }\n"
					"cite { margin-left: 30%; margin-right: 4%; text-align: justyfy; text-indent: 0px;  margin-top: 20px; margin-bottom: 20px; font-family: Times New Roman, serif }\n"
					"td, th { text-indent: 0px; font-size: 80%; margin-left: 2px; margin-right: 2px; margin-top: 2px; margin-bottom: 2px; text-align: left; padding: 2px }\n"
					"th { font-weight: bold }\n"
					"table > caption { padding: 2px; text-indent: 0px; font-size: 80%; font-weight: bold; text-align: left; background-color: #AAAAAA }\n"
					"table { border-spacing:2px;}\n"
					"body[name=\"notes\"] { font-size: 70%; }\n"
					"body[name=\"notes\"]  section[id] { text-align: left; }\n"
					"body[name=\"notes\"]  section[id] title { display: block; text-align: left; font-size: 110%; font-weight: bold; page-break-before: auto; page-break-inside: auto; page-break-after: auto; }\n"
					"body[name=\"notes\"]  section[id] title p { text-align: left; display: inline }\n"
					"body[name=\"notes\"]  section[id] empty-line { display: inline }\n"
					"code, pre { display: block; white-space: pre; font-family: \"Courier New\", monospace }\n";

static const char * DEFAULT_FONT_NAME = "Arial, DejaVu Sans"; //Times New Roman";
static const char * DEFAULT_STATUS_FONT_NAME =
		"Arial Narrow, Arial, DejaVu Sans"; //Times New Roman";
static css_font_family_t DEFAULT_FONT_FAMILY = css_ff_sans_serif;
//    css_ff_serif,
//    css_ff_sans_serif,
//    css_ff_cursive,
//    css_ff_fantasy,
//    css_ff_monospace

#ifdef LBOOK
#define INFO_FONT_SIZE      22
#else
#define INFO_FONT_SIZE      22
#endif

#ifndef MIN_STATUS_FONT_SIZE
#define MIN_STATUS_FONT_SIZE 8
#endif

#ifndef MAX_STATUS_FONT_SIZE
#define MAX_STATUS_FONT_SIZE 255
#endif

// Fallback defines, see crsetup.h
#ifndef SCREEN_SIZE_MIN
#define SCREEN_SIZE_MIN 80
#endif

#ifndef SCREEN_SIZE_MAX
#define SCREEN_SIZE_MAX 32767
#endif

#if defined(__SYMBIAN32__)
#include <e32std.h>
#define DEFAULT_PAGE_MARGIN 2
#else
#ifdef LBOOK
#define DEFAULT_PAGE_MARGIN      8
#else
#define DEFAULT_PAGE_MARGIN      12
#endif
#endif

/// minimum EM width of page (prevents show two pages for windows that not enougn wide)
#define MIN_EM_PER_PAGE     20

static int def_font_sizes[] = { 18, 20, 22, 24, 29, 33, 39, 44 };

LVDocView::LVDocView(int bitsPerPixel, bool noDefaultDocument) :
	m_bitsPerPixel(bitsPerPixel), m_dx(400), m_dy(200), _pos(0), _page(0),
			_posIsSet(false), m_battery_state(CR_BATTERY_STATE_NO_BATTERY)
#if (LBOOK==1)
			, m_requested_font_size(32)
#elif defined(__SYMBIAN32__)
			, m_requested_font_size(30)
#else
			, m_requested_font_size(24)
#endif
			, m_status_font_size(INFO_FONT_SIZE),
			m_def_interline_space(100),
#if USE_LIMITED_FONT_SIZES_SET
			m_font_sizes(def_font_sizes, sizeof(def_font_sizes) / sizeof(int)),
			m_font_sizes_cyclic(false),
#else
			m_min_font_size(6),
			m_max_font_size(72),
#endif
			m_is_rendered(false),
			m_view_mode(1 ? DVM_PAGES : DVM_SCROLL) // choose 0/1
			/*
			 , m_drawbuf(100, 100
			 #if COLOR_BACKBUFFER==0
			 , GRAY_BACKBUFFER_BITS
			 #endif
			 )
			 */
			, m_stream(NULL), m_doc(NULL), m_stylesheet(def_stylesheet),
            m_backgroundTiled(true),
            m_stylesheetUseMacros(true),
            m_stylesheetNeedsUpdate(true),
            m_highlightBookmarks(1),
			m_pageMargins(DEFAULT_PAGE_MARGIN,
					DEFAULT_PAGE_MARGIN / 2 /*+ INFO_FONT_SIZE + 4 */,
					DEFAULT_PAGE_MARGIN, DEFAULT_PAGE_MARGIN / 2),
			m_pagesVisible(2), m_pagesVisible_onlyIfSane(true), m_twoVisiblePagesAsOnePageNumber(false),
			m_pageHeaderInfo(PGHDR_PAGE_NUMBER
#ifndef LBOOK
					| PGHDR_CLOCK
#endif
					| PGHDR_BATTERY | PGHDR_PAGE_COUNT | PGHDR_AUTHOR
					| PGHDR_TITLE), m_showCover(true)
#if CR_INTERNAL_PAGE_ORIENTATION==1
			, m_rotateAngle(CR_ROTATE_ANGLE_0)
#endif
			, m_section_bounds_externally_updated(false)
			, m_section_bounds_valid(false), m_doc_format(doc_format_none),
			m_callback(NULL), m_swapDone(false), m_drawBufferBits(
					GRAY_BACKBUFFER_BITS) {
#if (COLOR_BACKBUFFER==1)
	m_backgroundColor = 0xFFFFFF;
	m_textColor = 0x000000;
#else
#if (GRAY_INVERSE==1)
	m_backgroundColor = 0;
	m_textColor = 3;
#else
	m_backgroundColor = 3;
	m_textColor = 0;
#endif
#endif
	m_statusColor = 0xFF000000;
	m_defaultFontFace = lString8(DEFAULT_FONT_NAME);
	m_statusFontFace = lString8(DEFAULT_STATUS_FONT_NAME);
	m_props = LVCreatePropsContainer();
	m_doc_props = LVCreatePropsContainer();
	propsUpdateDefaults( m_props);

	//m_drawbuf.Clear(m_backgroundColor);

    if (!noDefaultDocument)
        // NOLINTNEXTLINE: Call to virtual function during construction
        createDefaultDocument(cs32("No document"), lString32(
                    U"Welcome to CoolReader! Please select file to open"));

    m_font_size = scaleFontSizeForDPI(m_requested_font_size);
    m_font = fontMan->GetFont(m_font_size, 400, false, DEFAULT_FONT_FAMILY,
			m_defaultFontFace);
	m_infoFont = fontMan->GetFont(m_status_font_size, 700, false,
			DEFAULT_FONT_FAMILY, m_statusFontFace);
#ifdef ANDROID
	//m_batteryFont = fontMan->GetFont( 20, 700, false, DEFAULT_FONT_FAMILY, m_statusFontFace );
#endif

}

LVDocView::~LVDocView() {
	Clear();
}

CRPageSkinRef LVDocView::getPageSkin() {
	return _pageSkin;
}

void LVDocView::setPageSkin(CRPageSkinRef skin) {
	_pageSkin = skin;
}

/// get text format options
txt_format_t LVDocView::getTextFormatOptions() {
    return m_doc && m_doc->getDocFlag(DOC_FLAG_PREFORMATTED_TEXT) ? txt_format_pre
			: txt_format_auto;
}

/// set text format options
void LVDocView::setTextFormatOptions(txt_format_t fmt) {
	txt_format_t m_text_format = getTextFormatOptions();
	CRLog::trace("setTextFormatOptions( %d ), current state = %d", (int) fmt,
			(int) m_text_format);
	if (m_text_format == fmt)
		return; // no change
	m_props->setBool(PROP_TXT_OPTION_PREFORMATTED, (fmt == txt_format_pre));
	if (m_doc) // not when noDefaultDocument=true
		m_doc->setDocFlag(DOC_FLAG_PREFORMATTED_TEXT, (fmt == txt_format_pre));
	if (getDocFormat() == doc_format_txt) {
		requestReload();
		CRLog::trace(
				"setTextFormatOptions() -- new value set, reload requested");
	} else {
		CRLog::trace(
				"setTextFormatOptions() -- doc format is %d, reload is necessary for %d only",
				(int) getDocFormat(), (int) doc_format_txt);
	}
}

/// invalidate document data, request reload
void LVDocView::requestReload() {
	if (getDocFormat() != doc_format_txt)
		return; // supported for text files only
	if (m_callback) {
        if (m_callback->OnRequestReload()) {
            CRLog::info("LVDocView::requestReload() : reload request will be processed by external code");
            return;
        }
        m_callback->OnLoadFileStart(m_doc_props->getStringDef(
				DOC_PROP_FILE_NAME, ""));
	}
	if (m_stream.isNull() && isDocumentOpened()) {
		savePosition();
		CRFileHist * hist = getHistory();
		if (!hist || hist->getRecords().length() <= 0)
			return;
        //lString32 fn = hist->getRecords()[0]->getFilePathName();
        lString32 fn = m_filename;
		bool res = LoadDocument(fn.c_str());
		if (res) {
			//swapToCache();
			restorePosition();
		} else {
            createDefaultDocument(lString32::empty_str, lString32(
					"Error while opening document ") + fn);
		}
		checkRender();
		return;
	}
	ParseDocument();
	// TODO: save position
	checkRender();
}

/// returns true if document is opened
bool LVDocView::isDocumentOpened() {
	return m_doc && m_doc->getRootNode() && !m_doc_props->getStringDef(
			DOC_PROP_FILE_NAME, "").empty();
}

/// rotate rectangle by current angle, winToDoc==false for doc->window translation, true==ccw
lvRect LVDocView::rotateRect(lvRect & rc, bool winToDoc) {
#if CR_INTERNAL_PAGE_ORIENTATION==1
	lvRect rc2;
	cr_rotate_angle_t angle = m_rotateAngle;
	if ( winToDoc )
	angle = (cr_rotate_angle_t)((4 - (int)angle) & 3);
	switch ( angle ) {
		case CR_ROTATE_ANGLE_0:
		rc2 = rc;
		break;
		case CR_ROTATE_ANGLE_90:
		/*
		 . . . . . .      . . . . . . . .
		 . . . . . .      . . . . . 1 . .
		 . 1 . . . .      . . . . . . . .
		 . . . . . .  ==> . . . . . . . .
		 . . . . . .      . 2 . . . . . .
		 . . . . . .      . . . . . . . .
		 . . . . 2 .
		 . . . . . .

		 */
		rc2.left = m_dy - rc.bottom - 1;
		rc2.right = m_dy - rc.top - 1;
		rc2.top = rc.left;
		rc2.bottom = rc.right;
		break;
		case CR_ROTATE_ANGLE_180:
		rc2.left = m_dx - rc.left - 1;
		rc2.right = m_dx - rc.right - 1;
		rc2.top = m_dy - rc.top - 1;
		rc2.bottom = m_dy - rc.bottom - 1;
		break;
		case CR_ROTATE_ANGLE_270:
		/*
		 . . . . . .      . . . . . . . .
		 . . . . . .      . 1 . . . . . .
		 . . . . 2 .      . . . . . . . .
		 . . . . . .  <== . . . . . . . .
		 . . . . . .      . . . . . 2 . .
		 . . . . . .      . . . . . . . .
		 . 1 . . . .
		 . . . . . .

		 */
		rc2.left = rc.top;
		rc2.right = rc.bottom;
		rc2.top = m_dx - rc.right - 1;
		rc2.bottom = m_dx - rc.left - 1;
		break;
	}
	return rc2;
#else
	return rc;
#endif
}

/// rotate point by current angle, winToDoc==false for doc->window translation, true==ccw
lvPoint LVDocView::rotatePoint(lvPoint & pt, bool winToDoc) {
#if CR_INTERNAL_PAGE_ORIENTATION==1
	lvPoint pt2;
	cr_rotate_angle_t angle = m_rotateAngle;
	if ( winToDoc )
	angle = (cr_rotate_angle_t)((4 - (int)angle) & 3);
	switch ( angle ) {
		case CR_ROTATE_ANGLE_0:
		pt2 = pt;
		break;
		case CR_ROTATE_ANGLE_90:
		/*
		 . . . . . .      . . . . . . . .
		 . . . . . .      . . . . . 1 . .
		 . 1 . . . .      . . . . . . . .
		 . . . . . .  ==> . . . . . . . .
		 . . . . . .      . 2 . . . . . .
		 . . . . . .      . . . . . . . .
		 . . . . 2 .
		 . . . . . .

		 */
		pt2.y = pt.x;
		pt2.x = m_dx - pt.y - 1;
		break;
		case CR_ROTATE_ANGLE_180:
		pt2.y = m_dy - pt.y - 1;
		pt2.x = m_dx - pt.x - 1;
		break;
		case CR_ROTATE_ANGLE_270:
		/*
		 . . . . . .      . . . . . . . .
		 . . . . . .      . 1 . . . . . .
		 . . . . 2 .      . . . . . . . .
		 . . . . . .  <== . . . . . . . .
		 . . . . . .      . . . . . 2 . .
		 . . . . . .      . . . . . . . .
		 . 1 . . . .
		 . . . . . .

		 */
		pt2.y = m_dy - pt.x - 1;
		pt2.x = pt.y;
		break;
	}
	return pt2;
#else
	return pt;
#endif
}

/// sets page margins
void LVDocView::setPageMargins(const lvRect & rc) {
	if (m_pageMargins.left + m_pageMargins.right != rc.left + rc.right
            || m_pageMargins.top + m_pageMargins.bottom != rc.top + rc.bottom) {

        m_pageMargins = rc;
        updateLayout();
        REQUEST_RENDER("setPageMargins")
    } else {
		clearImageCache();
        m_pageMargins = rc;
    }
}

void LVDocView::setPageHeaderInfo(int hdrFlags) {
	if (m_pageHeaderInfo == hdrFlags)
		return;
	LVLock lock(getMutex());
	int oldH = getPageHeaderHeight();
	m_pageHeaderInfo = hdrFlags;
	int h = getPageHeaderHeight();
	if (h != oldH) {
        REQUEST_RENDER("setPageHeaderInfo")
	} else {
		clearImageCache();
	}
}

lString32 mergeCssMacros(CRPropRef props) {
    lString8 res = lString8::empty_str;
    for (int i=0; i<props->getCount(); i++) {
    	lString8 n(props->getName(i));
    	if (n.endsWith(".day") || n.endsWith(".night"))
    		continue;
        lString32 v = props->getValue(i);
        if (!v.empty()) {
            if (v.lastChar() != ';')
                v.append(1, ';');
            if (v.lastChar() != ' ')
                v.append(1, ' ');
            res.append(UnicodeToUtf8(v));
        }
    }
    //CRLog::trace("merged: %s", res.c_str());
    return Utf8ToUnicode(res);
}

lString8 substituteCssMacros(lString8 src, CRPropRef props) {
    lString8 res = lString8(src.length());
    const char * s = src.c_str();
    for (; *s; s++) {
        if (*s == '$') {
            const char * s2 = s + 1;
            bool err = false;
            for (; *s2 && *s2 != ';' && *s2 != '}' &&  *s2 != ' ' &&  *s2 != '\r' &&  *s2 != '\n' &&  *s2 != '\t'; s2++) {
                char ch = *s2;
                if (ch != '.' && ch != '-' && (ch < 'a' || ch > 'z')) {
                    err = true;
                }
            }
            if (!err) {
                // substitute variable
                lString8 prop(s + 1, (lvsize_t)(s2 - s - 1));
                lString32 v;
                // $styles.stylename.all will merge all properties like styles.stylename.*
                if (prop.endsWith(".all")) {
                    // merge whole branch
                    v = mergeCssMacros(props->getSubProps(prop.substr(0, prop.length() - 3).c_str()));
                    //CRLog::trace("merged %s = %s", prop.c_str(), LCSTR(v));
                } else {
                    // single property
                    props->getString(prop.c_str(), v);
                    if (!v.empty()) {
                        if (v.lastChar() != ';')
                            v.append(1, ';');
                        if (v.lastChar() != ' ')
                            v.append(1, ' ');
                    }
                }
                if (!v.empty()) {
                    res.append(UnicodeToUtf8(v));
                } else {
                    //CRLog::trace("CSS macro not found: %s", prop.c_str());
                }
            }
            s = s2;
        } else {
            res.append(1, *s);
        }
    }
    return res;
}

/// set document stylesheet text
void LVDocView::setStyleSheet(lString8 css_text, bool use_macros) {
	LVLock lock(getMutex());
    REQUEST_RENDER("setStyleSheet")

    m_stylesheet = css_text;
    m_stylesheetUseMacros = use_macros;
    m_stylesheetNeedsUpdate = true;
}

void LVDocView::updateDocStyleSheet() {
    // Don't skip this when document is not yet rendered (or is being re-rendered)
    if (m_is_rendered && !m_stylesheetNeedsUpdate)
        return;
    CRPropRef p = m_props->getSubProps("styles.");
    if ( m_stylesheetUseMacros )
        m_doc->setStyleSheet(substituteCssMacros(m_stylesheet, p).c_str(), true);
    else
        m_doc->setStyleSheet(m_stylesheet.c_str(), true);
    m_stylesheetNeedsUpdate = false;
}

void LVDocView::Clear() {
	{
		LVLock lock(getMutex());
		if (m_doc)
			delete m_doc;
		m_doc = NULL;
		m_doc_props->clear();
		if (!m_stream.isNull())
			m_stream.Clear();
		if (!m_container.isNull())
			m_container.Clear();
		if (!m_arc.isNull())
			m_arc.Clear();
		_posBookmark = ldomXPointer();
		m_is_rendered = false;
		m_swapDone = false;
		_pos = 0;
		_page = 0;
		_posIsSet = false;
		m_cursorPos.clear();
		m_filename.clear();
		m_section_bounds_valid = false;
	}
	clearImageCache();
	_navigationHistory.clear();
	// Also drop font instances from previous document (see
	// lvtinydom.cpp ldomDocument::render() for the reason)
	fontMan->gc();
	fontMan->gc();
}

/// invalidate image cache, request redraw
void LVDocView::clearImageCache() {
#if CR_ENABLE_PAGE_IMAGE_CACHE==1
	m_imageCache.clear();
#endif
	if (m_callback != NULL)
		m_callback->OnImageCacheClear();
}

/// invalidate formatted data, request render
void LVDocView::requestRender() {
	LVLock lock(getMutex());
	if (!m_doc) // nothing to render when noDefaultDocument=true
		return;
	m_is_rendered = false;
	clearImageCache();
	m_doc->clearRendBlockCache();
}

/// render document, if not rendered
void LVDocView::checkRender() {
	if (!m_is_rendered) {
		LVLock lock(getMutex());
		CRLog::trace("LVDocView::checkRender() : render is required");
		Render();
		clearImageCache();
		m_is_rendered = true;
		_posIsSet = false;
		//CRLog::trace("LVDocView::checkRender() compeleted");
	}
}

/// ensure current position is set to current bookmark value
void LVDocView::checkPos() {
    CHECK_RENDER("checkPos()");
	if (_posIsSet)
		return;
	_posIsSet = true;
	LVLock lock(getMutex());
	if (_posBookmark.isNull()) {
		if (isPageMode()) {
			goToPage(0);
		} else {
			SetPos(0, false);
		}
	} else {
		if (isPageMode()) {
			// (We can work with external page numbers)
			int p = getBookmarkPage(_posBookmark);
			goToPage(p, false, false);
		} else {
			//CRLog::trace("checkPos() _posBookmark node=%08X offset=%d", (unsigned)_posBookmark.getNode(), (int)_posBookmark.getOffset());
			lvPoint pt = _posBookmark.toPoint();
			SetPos(pt.y, false);
		}
	}
}

#if CR_ENABLE_PAGE_IMAGE_CACHE==1
/// returns true if current page image is ready
bool LVDocView::IsDrawed()
{
	return isPageImageReady( 0 );
}

/// returns true if page image is available (0=current, -1=prev, 1=next)
bool LVDocView::isPageImageReady( int delta )
{
	if ( !m_is_rendered || !_posIsSet )
	return false;
	LVDocImageRef ref;
	if ( isPageMode() ) {
		int p = _page;
		if ( delta<0 )
		p--;
		else if ( delta>0 )
		p++;
		//CRLog::trace("getPageImage: checking cache for page [%d] (delta=%d)", offset, delta);
		ref = m_imageCache.get( -1, p );
	} else {
		int offset = _pos;
		if ( delta<0 )
		offset = getPrevPageOffset();
		else if ( delta>0 )
		offset = getNextPageOffset();
		//CRLog::trace("getPageImage: checking cache for page [%d] (delta=%d)", offset, delta);
		ref = m_imageCache.get( offset, -1 );
	}
	return ( !ref.isNull() );
}

/// get page image
LVDocImageRef LVDocView::getPageImage( int delta )
{
	checkPos();
	// find existing object in cache
	LVDocImageRef ref;
	int p = -1;
	int offset = -1;
	if ( isPageMode() ) {
		p = _page;
		if ( delta<0 )
		p--;
		else if ( delta>0 )
		p++;
		if ( p<0 || p>=m_pages.length() )
		return ref;
		ref = m_imageCache.get( -1, p );
		if ( !ref.isNull() ) {
			//CRLog::trace("getPageImage: + page [%d] found in cache", offset);
			return ref;
		}
	} else {
		offset = _pos;
		if ( delta<0 )
		offset = getPrevPageOffset();
		else if ( delta>0 )
		offset = getNextPageOffset();
		//CRLog::trace("getPageImage: checking cache for page [%d] (delta=%d)", offset, delta);
		ref = m_imageCache.get( offset, -1 );
		if ( !ref.isNull() ) {
			//CRLog::trace("getPageImage: + page [%d] found in cache", offset);
			return ref;
		}
	}
	while ( ref.isNull() ) {
		//CRLog::trace("getPageImage: - page [%d] not found, force rendering", offset);
		cachePageImage( delta );
		ref = m_imageCache.get( offset, p );
	}
	//CRLog::trace("getPageImage: page [%d] is ready", offset);
	return ref;
}

class LVDrawThread : public LVThread {
	LVDocView * _view;
	int _offset;
	int _page;
	LVRef<LVDrawBuf> _drawbuf;
public:
	LVDrawThread( LVDocView * view, int offset, int page, LVRef<LVDrawBuf> drawbuf )
	: _view(view), _offset(offset), _page(page), _drawbuf(drawbuf)
	{
		start();
	}
	virtual void run()
	{
		//CRLog::trace("LVDrawThread::run() offset==%d", _offset);
		_view->Draw( *_drawbuf, _offset, _page, true );
		//_drawbuf->Rotate( _view->GetRotateAngle() );
	}
};
#endif

/// draw current page to specified buffer
void LVDocView::Draw(LVDrawBuf & drawbuf, bool autoResize) {
	checkPos();
	int offset = -1;
	int p = -1;
	if (isPageMode()) {
		p = _page;
		if (p < 0 || p >= m_pages.length())
			return;
	} else {
		offset = _pos;
	}
	//CRLog::trace("Draw() : calling Draw(buf(%d x %d), %d, %d, false)",
	//		drawbuf.GetWidth(), drawbuf.GetHeight(), offset, p);
	Draw(drawbuf, offset, p, false, autoResize);
}

#if CR_ENABLE_PAGE_IMAGE_CACHE==1
/// cache page image (render in background if necessary)
void LVDocView::cachePageImage( int delta )
{
	int offset = -1;
	int p = -1;
	if ( isPageMode() ) {
		p = _page;
		if ( delta<0 )
		p--;
		else if ( delta>0 )
		p++;
		if ( p<0 || p>=m_pages.length() )
		return;
	} else {
		offset = _pos;
		if ( delta<0 )
		offset = getPrevPageOffset();
		else if ( delta>0 )
		offset = getNextPageOffset();
	}
	//CRLog::trace("cachePageImage: request to cache page [%d] (delta=%d)", offset, delta);
	if ( m_imageCache.has(offset, p) ) {
		//CRLog::trace("cachePageImage: Page [%d] is found in cache", offset);
		return;
	}
	//CRLog::trace("cachePageImage: starting new render task for page [%d]", offset);
	LVDrawBuf * buf = NULL;
	if ( m_bitsPerPixel==-1 ) {
#if (COLOR_BACKBUFFER==1)
        buf = new LVColorDrawBuf( m_dx, m_dy, DEF_COLOR_BUFFER_BPP );
#else
		buf = new LVGrayDrawBuf( m_dx, m_dy, m_drawBufferBits );
#endif
	} else {
        if ( m_bitsPerPixel==32 || m_bitsPerPixel==16 ) {
            buf = new LVColorDrawBuf( m_dx, m_dy, m_bitsPerPixel );
		} else {
			buf = new LVGrayDrawBuf( m_dx, m_dy, m_bitsPerPixel );
		}
	}
	LVRef<LVDrawBuf> drawbuf( buf );
	LVRef<LVThread> thread( new LVDrawThread( this, offset, p, drawbuf ) );
	m_imageCache.set( offset, p, drawbuf, thread );
	//CRLog::trace("cachePageImage: caching page [%d] is finished", offset);
}
#endif

bool LVDocView::exportWolFile(const char * fname, bool flgGray, int levels) {
	LVStreamRef stream = LVOpenFileStream(fname, LVOM_WRITE);
	if (!stream)
		return false;
	return exportWolFile(stream.get(), flgGray, levels);
}

bool LVDocView::exportWolFile(const lChar32 * fname, bool flgGray, int levels) {
	LVStreamRef stream = LVOpenFileStream(fname, LVOM_WRITE);
	if (!stream)
		return false;
	return exportWolFile(stream.get(), flgGray, levels);
}

void dumpSection(ldomNode * elem) {
	lvRect rc;
	elem->getAbsRect(rc);
	//fprintf( log.f, "rect(%d, %d, %d, %d)  ", rc.left, rc.top, rc.right, rc.bottom );
}

LVTocItem * LVDocView::getToc() {
	if (!m_doc)
		return NULL;
        // When just loaded from cache, TocItems are missing their _position
        // properties (a XPointer object), but all other properties (_path,
        // _page, _percent) are valid and enough to display TOC.
        // Avoid calling updatePageNumbers() in that case (as it is expensive
        // and would delay book opening when loaded from cache - it will be
        // called when it is really needed: after next full rendering)
        if (!m_doc->isTocFromCacheValid() || !m_doc->getToc()->hasValidPageNumbers(getVisiblePageNumberCount())) {
            updatePageNumbers(m_doc->getToc());
            m_doc->setCacheFileStale(true); // have cache saved with the updated TOC
        }
	return m_doc->getToc();
}

static lString32 getSectionHeader(ldomNode * section) {
	lString32 header;
	if (!section || section->getChildCount() == 0)
		return header;
    ldomNode * child = section->getChildElementNode(0, U"title");
    if (!child)
		return header;
	header = child->getText(U' ', 1024);
	return header;
}

int getSectionPage(ldomNode * section, LVRendPageList & pages) {
	if (!section)
		return -1;
#if 1
	int y = ldomXPointer(section, 0).toPoint().y;
#else
	lvRect rc;
	section->getAbsRect(rc);
	int y = rc.top;
#endif
	int page = -1;
	if (y >= 0) {
		page = pages.FindNearestPage(y, -1);
		//dumpSection( section );
		//fprintf(log.f, "page %d: %d->%d..%d\n", page+1, y, pages[page].start, pages[page].start+pages[page].height );
	}
	return page;
}

/*
 static void addTocItems( ldomNode * basesection, LVTocItem * parent )
 {
 if ( !basesection || !parent )
 return;
 lString32 name = getSectionHeader( basesection );
 if ( name.empty() )
 return; // section without header
 ldomXPointer ptr( basesection, 0 );
 LVTocItem * item = parent->addChild( name, ptr );
 int cnt = basesection->getChildCount();
 for ( int i=0; i<cnt; i++ ) {
 ldomNode * section = basesection->getChildNode(i);
 if ( section->getNodeId() != el_section  )
 continue;
 addTocItems( section, item );
 }
 }

 void LVDocView::makeToc()
 {
 LVTocItem * toc = m_doc->getToc();
 if ( toc->getChildCount() ) {
 return;
 }
 CRLog::info("LVDocView::makeToc()");
 toc->clear();
 ldomNode * body = m_doc->getRootNode();
 if ( !body )
 return;
 body = body->findChildElement( LXML_NS_ANY, el_FictionBook, -1 );
 if ( body )
 body = body->findChildElement( LXML_NS_ANY, el_body, 0 );
 if ( !body )
 return;
 int cnt = body->getChildCount();
 for ( int i=0; i<cnt; i++ ) {
 ldomNode * section = body->getChildNode(i);
 if ( section->getNodeId()!=el_section )
 continue;
 addTocItems( section, toc );
 }
 }
 */

/// update page numbers for items
void LVDocView::updatePageNumbers(LVTocItem * item) {
	if (!item->getXPointer().isNull()) {
		lvPoint p = item->getXPointer().toPoint();
		int y = p.y;
		int h = GetFullHeight();
		int page = getBookmarkPage(item->_position);
		if (page >= 0 && page < getPageCount())
			item->_page = page;
		else
			item->_page = -1;
		if (y >= 0 && y < h && h > 0)
			item->_percent = (int) ((lInt64) y * 10000 / h); // % * 100
		else
			item->_percent = -1;
	} else {
		//CRLog::error("Page position is not found for path %s", LCSTR(item->getPath()) );
		// unknown position
                // Don't update _page of root toc item, as it carries the alternative TOC flag
                if (item->_level > 0)
                    item->_page = -1;
		// And have its _percent carries the number of visible page numbers
		// with which toc item pages have been computed
		item->_percent = - getVisiblePageNumberCount(); // (as a negative number to make sure it's not used as %)
	}
	for (int i = 0; i < item->getChildCount(); i++) {
		updatePageNumbers(item->getChild(i));
	}
}

LVPageMap * LVDocView::getPageMap() {
    if (!m_doc)
        return NULL;
    if ( !m_doc->getPageMap()->hasValidPageInfo( getVisiblePageNumberCount() ) ) {
        updatePageMapInfo(m_doc->getPageMap());
        m_doc->setCacheFileStale(true); // have cache saved with the updated pagemap
    }
    return m_doc->getPageMap();
}

/// update page info for LVPageMapItems
void LVDocView::updatePageMapInfo(LVPageMap * pagemap) {
    // Ensure page and doc_y never go backward
    int prev_page = 0;
    int prev_doc_y = 0;
    for (int i = 0; i < pagemap->getChildCount(); i++) {
        LVPageMapItem * item = pagemap->getChild(i);
        if (!item->getXPointer().isNull()) {
            int doc_y = item->getDocY(true); // refresh
            int page = -1;
            if (doc_y >= 0) {
                page = m_pages.FindNearestPage(doc_y, 0);
                if (page < 0 || page >= getPageCount(true))
                    page = -1;
                else
                    page = getExternalPageNumber(page);
            }
            item->_page = page;
            if ( item->_page < prev_page )
                item->_page = prev_page;
            else
                prev_page = item->_page;
            if ( item->_doc_y < prev_doc_y )
                item->_doc_y = prev_doc_y;
            else
                prev_doc_y = item->_doc_y;
        }
        else {
            item->_page = prev_page;
            item->_doc_y = prev_doc_y;
        }
    }
    pagemap->setPageValidForVisiblePageNumbers( getVisiblePageNumberCount() );
}


/// get a stream for reading to document internal file (path inside the ZIP for EPUBs,
/// path relative to document directory for non-container documents like HTML)
LVStreamRef LVDocView::getDocumentFileStream( lString32 filePath ) {
    if ( !filePath.empty() ) {
        LVContainerRef cont = m_doc->getContainer();
        if ( cont.isNull() ) // no real container
            cont = m_container; // consider document directory as the container
        LVStreamRef stream = cont->OpenStream(filePath.c_str(), LVOM_READ);
        // if failure, a NULL stream is returned
        return stream;
    }
    return LVStreamRef(); // not found: return NULL ref
}

/// returns cover page image stream, if any
LVStreamRef LVDocView::getCoverPageImageStream() {
    lString32 fileName;
    //m_doc_props
//    for ( int i=0; i<m_doc_props->getCount(); i++ ) {
//        CRLog::debug("%s = %s", m_doc_props->getName(i), LCSTR(m_doc_props->getValue(i)));
//    }
    m_doc_props->getString(DOC_PROP_COVER_FILE, fileName);
//    CRLog::debug("coverpage = %s", LCSTR(fileName));
    if ( !fileName.empty() ) {
//        CRLog::debug("trying to open %s", LCSTR(fileName));
    	LVContainerRef cont = m_doc->getContainer();
    	if ( cont.isNull() )
    		cont = m_container;
        LVStreamRef stream = cont->OpenStream(fileName.c_str(), LVOM_READ);
        if ( stream.isNull() ) {
            CRLog::error("Cannot open coverpage image from %s", LCSTR(fileName));
            for ( int i=0; i<cont->GetObjectCount(); i++ ) {
                CRLog::info("item %d : %s", i+1, LCSTR(cont->GetObjectInfo(i)->GetName()));
            }
        } else {
//            CRLog::debug("coverpage file %s is opened ok", LCSTR(fileName));
        }
        return stream;
    }

    // FB2 coverpage
	//CRLog::trace("LVDocView::getCoverPageImage()");
	//m_doc->dumpStatistics();
	lUInt16 path[] = { el_FictionBook, el_description, el_title_info, el_coverpage, 0 };
	//lUInt16 path[] = { el_FictionBook, el_description, el_title_info, el_coverpage, el_image, 0 };
	ldomNode * rootNode = m_doc->getRootNode();
	ldomNode * cover_el = 0;
	if (rootNode) {
		cover_el = rootNode->findChildElement(path);
		if (!cover_el) { // might otherwise be found inside <src-title-info>
			lUInt16 path2[] = { el_FictionBook, el_description, el_src_title_info, el_coverpage, 0 };
			cover_el = rootNode->findChildElement(path2);
		}
	}
	if (cover_el) {
		ldomNode * cover_img_el = cover_el->findChildElement(LXML_NS_ANY,
				el_image, 0);
		if (cover_img_el) {
			LVStreamRef imgsrc = cover_img_el->getObjectImageStream();
			return imgsrc;
		}
	}
	return LVStreamRef(); // not found: return NULL ref
}

/// returns cover page image source, if any
LVImageSourceRef LVDocView::getCoverPageImage() {
	//    LVStreamRef stream = getCoverPageImageStream();
	//    if ( !stream.isNull() )
	//        CRLog::trace("Image stream size is %d", (int)stream->GetSize() );
	//CRLog::trace("LVDocView::getCoverPageImage()");
	//m_doc->dumpStatistics();
	lUInt16 path[] = { el_FictionBook, el_description, el_title_info, el_coverpage, 0 };
	//lUInt16 path[] = { el_FictionBook, el_description, el_title_info, el_coverpage, el_image, 0 };
	ldomNode * cover_el = 0;
	ldomNode * rootNode = m_doc->getRootNode();
	if (rootNode) {
		cover_el = rootNode->findChildElement(path);
		if (!cover_el) { // might otherwise be found inside <src-title-info>
			lUInt16 path2[] = { el_FictionBook, el_description, el_src_title_info, el_coverpage, 0 };
			cover_el = rootNode->findChildElement(path2);
		}
	}
	if (cover_el) {
		ldomNode * cover_img_el = cover_el->findChildElement(LXML_NS_ANY,
				el_image, 0);
		if (cover_img_el) {
			LVImageSourceRef imgsrc = cover_img_el->getObjectImageSource();
			return imgsrc;
		}
	}
	return LVImageSourceRef(); // not found: return NULL ref
}

/// draws coverpage to image buffer
void LVDocView::drawCoverTo(LVDrawBuf * drawBuf, lvRect & rc) {
    CRLog::trace("drawCoverTo");
	if (rc.width() < 130 || rc.height() < 130)
		return;
	int base_font_size = 16;
	int w = rc.width();
	if (w < 200)
		base_font_size = 16;
	else if (w < 300)
		base_font_size = 18;
	else if (w < 500)
		base_font_size = 20;
	else if (w < 700)
		base_font_size = 22;
	else
		base_font_size = 24;
	//CRLog::trace("drawCoverTo() - loading fonts...");
	LVFontRef author_fnt(fontMan->GetFont(base_font_size, 700, false,
            css_ff_serif, cs8("Times New Roman")));
	LVFontRef title_fnt(fontMan->GetFont(base_font_size + 4, 700, false,
            css_ff_serif, cs8("Times New Roman")));
	LVFontRef series_fnt(fontMan->GetFont(base_font_size - 3, 400, true,
            css_ff_serif, cs8("Times New Roman")));
	lString32 authors = getAuthors();
	lString32 title = getTitle();
	lString32 series = getSeries();
	if (title.empty())
        title = "no title";
	LFormattedText txform;
	if (!authors.empty())
		txform.AddSourceLine(authors.c_str(), authors.length(), LTEXT_COLOR_CURRENT,
				LTEXT_COLOR_CURRENT, author_fnt.get(), NULL, LTEXT_ALIGN_CENTER,
				author_fnt->getHeight() * 18 / 16);
	txform.AddSourceLine(title.c_str(), title.length(), LTEXT_COLOR_CURRENT, LTEXT_COLOR_CURRENT,
			title_fnt.get(), NULL, LTEXT_ALIGN_CENTER,
			title_fnt->getHeight() * 18 / 16);
	if (!series.empty())
		txform.AddSourceLine(series.c_str(), series.length(), LTEXT_COLOR_CURRENT,
				LTEXT_COLOR_CURRENT, series_fnt.get(), NULL, LTEXT_ALIGN_CENTER,
				series_fnt->getHeight() * 18 / 16);
	int title_w = rc.width() - rc.width() / 4;
	int h = txform.Format((lUInt16)title_w, (lUInt16)rc.height());

	lvRect imgrc = rc;

	//CRLog::trace("drawCoverTo() - getting cover image");
	LVImageSourceRef imgsrc = getCoverPageImage();
	LVImageSourceRef defcover = getDefaultCover();
	if (!imgsrc.isNull() && imgrc.height() > 30) {
#ifdef NO_TEXT_IN_COVERPAGE
		h = 0;
#endif
		if (h)
			imgrc.bottom -= h + 16;
		//fprintf( stderr, "Writing coverpage image...\n" );
		int src_dx = imgsrc->GetWidth();
		int src_dy = imgsrc->GetHeight();
		int scale_x = imgrc.width() * 0x10000 / src_dx;
		int scale_y = imgrc.height() * 0x10000 / src_dy;
		if (scale_x < scale_y)
			scale_y = scale_x;
		else
			scale_x = scale_y;
		int dst_dx = (src_dx * scale_x) >> 16;
		int dst_dy = (src_dy * scale_y) >> 16;
        if (dst_dx > rc.width() * 6 / 8)
			dst_dx = imgrc.width();
        if (dst_dy > rc.height() * 6 / 8)
			dst_dy = imgrc.height();
		//CRLog::trace("drawCoverTo() - drawing image");
        LVColorDrawBuf buf2(src_dx, src_dy, 32);
        buf2.Draw(imgsrc, 0, 0, src_dx, src_dy, true);
        drawBuf->DrawRescaled(&buf2, imgrc.left + (imgrc.width() - dst_dx) / 2,
                imgrc.top + (imgrc.height() - dst_dy) / 2, dst_dx, dst_dy, 0);
	} else if (!defcover.isNull()) {
		if (h)
			imgrc.bottom -= h + 16;
		// draw default cover with title at center
		imgrc = rc;
		int src_dx = defcover->GetWidth();
		int src_dy = defcover->GetHeight();
		int scale_x = imgrc.width() * 0x10000 / src_dx;
		int scale_y = imgrc.height() * 0x10000 / src_dy;
		if (scale_x < scale_y)
			scale_y = scale_x;
		else
			scale_x = scale_y;
		int dst_dx = (src_dx * scale_x) >> 16;
		int dst_dy = (src_dy * scale_y) >> 16;
		if (dst_dx > rc.width() - 10)
			dst_dx = imgrc.width();
		if (dst_dy > rc.height() - 10)
			dst_dy = imgrc.height();
		//CRLog::trace("drawCoverTo() - drawing image");
		drawBuf->Draw(defcover, imgrc.left + (imgrc.width() - dst_dx) / 2,
				imgrc.top + (imgrc.height() - dst_dy) / 2, dst_dx, dst_dy);
		//CRLog::trace("drawCoverTo() - drawing text");
		txform.Draw(drawBuf, (rc.right + rc.left - title_w) / 2, (rc.bottom
				+ rc.top - h) / 2, NULL);
		//CRLog::trace("drawCoverTo() - done");
		return;
	} else {
		imgrc.bottom = imgrc.top;
	}
	rc.top = imgrc.bottom;
	//CRLog::trace("drawCoverTo() - drawing text");
	if (h)
		txform.Draw(drawBuf, (rc.right + rc.left - title_w) / 2, (rc.bottom
				+ rc.top - h) / 2, NULL);
	//CRLog::trace("drawCoverTo() - done");
}

/// export to WOL format
bool LVDocView::exportWolFile(LVStream * stream, bool flgGray, int levels) {
	checkRender();
	int save_m_dx = m_dx;
	int save_m_dy = m_dy;
	int old_flags = m_pageHeaderInfo;
	int save_pos = _pos;
	int save_page = _pos;
	bool showCover = getShowCover();
	m_pageHeaderInfo &= ~(PGHDR_CLOCK | PGHDR_BATTERY);
	int dx = 600; // - m_pageMargins.left - m_pageMargins.right;
	int dy = 800; // - m_pageMargins.top - m_pageMargins.bottom;
	Resize(dx, dy);

	LVRendPageList &pages = m_pages;

	//Render(dx, dy, &pages);

	const lChar8 * * table = GetCharsetUnicode2ByteTable(U"windows-1251");

	//ldomXPointer bm = getBookmark();
	{
		WOLWriter wol(stream);
		lString8 authors = UnicodeTo8Bit(getAuthors(), table);
		lString8 name = UnicodeTo8Bit(getTitle(), table);
        wol.addTitle(name, cs8("-"), authors, cs8("-"), //adapter
                cs8("-"), //translator
                cs8("-"), //publisher
                cs8("-"), //2006-11-01
                cs8("-"), //This is introduction.
                cs8("") //ISBN
		);

		LVGrayDrawBuf cover(600, 800);
		lvRect coverRc(0, 0, 600, 800);
		cover.Clear(m_backgroundColor);
		drawCoverTo(&cover, coverRc);
		wol.addCoverImage(cover);

		int lastPercent = 0;
		for (int i = showCover ? 1 : 0; i < pages.length(); i
				+= getVisiblePageCount()) {
			int percent = i * 100 / pages.length();
			percent -= percent % 5;
			if (percent != lastPercent) {
				lastPercent = percent;
				if (m_callback != NULL)
					m_callback->OnExportProgress(percent);
			}
			LVGrayDrawBuf drawbuf(600, 800, flgGray ? 2 : 1); //flgGray ? 2 : 1);
			//drawbuf.SetBackgroundColor(0xFFFFFF);
			//drawbuf.SetTextColor(0x000000);
			drawbuf.Clear(m_backgroundColor);
			drawPageTo(&drawbuf, *pages[i], NULL, pages.length(), 0);
			_pos = pages[i]->start;
			_page = i;
			Draw(drawbuf, -1, _page, true);
			if (!flgGray) {
				drawbuf.ConvertToBitmap(false);
				drawbuf.Invert();
			} else {
				//drawbuf.Invert();
			}
			wol.addImage(drawbuf);
		}

		// add TOC
		ldomNode * body = m_doc->nodeFromXPath(lString32(
                "/FictionBook/body[1]"));
		lUInt16 section_id = m_doc->getElementNameIndex(U"section");

		if (body) {
			int l1n = 0;
			for (int l1 = 0; l1 < 1000; l1++) {
				ldomNode * l1section = body->findChildElement(LXML_NS_ANY,
						section_id, l1);
				if (!l1section)
					break;
				lString8 title = UnicodeTo8Bit(getSectionHeader(l1section),
						table);
				int page = getSectionPage(l1section, pages);
				if (!showCover)
					page++;
				if (!title.empty() && page >= 0) {
					wol.addTocItem(++l1n, 0, 0, page, title);
					int l2n = 0;
					if (levels < 2)
						continue;
					for (int l2 = 0; l2 < 1000; l2++) {
						ldomNode * l2section = l1section->findChildElement(
								LXML_NS_ANY, section_id, l2);
						if (!l2section)
							break;
						lString8 title = UnicodeTo8Bit(getSectionHeader(
								l2section), table);
						int page = getSectionPage(l2section, pages);
						if (!title.empty() && page >= 0) {
							wol.addTocItem(l1n, ++l2n, 0, page, title);
							int l3n = 0;
							if (levels < 3)
								continue;
							for (int l3 = 0; l3 < 1000; l3++) {
								ldomNode * l3section =
										l2section->findChildElement(
												LXML_NS_ANY, section_id, l3);
								if (!l3section)
									break;
								lString8 title = UnicodeTo8Bit(
										getSectionHeader(l3section), table);
								int page = getSectionPage(l3section, pages);
								if (!title.empty() && page >= 0) {
									wol.addTocItem(l1n, l2n, ++l3n, page, title);
								}
							}
						}
					}
				}
			}
		}
	}
	m_pageHeaderInfo = old_flags;
	_pos = save_pos;
	_page = save_page;
	bool rotated =
#if CR_INTERNAL_PAGE_ORIENTATION==1
			(GetRotateAngle()&1);
#else
			false;
#endif
	int ndx = rotated ? save_m_dy : save_m_dx;
	int ndy = rotated ? save_m_dx : save_m_dy;
	Resize(ndx, ndy);
	clearImageCache();

	return true;
}

int LVDocView::GetFullHeight() {
	LVLock lock(getMutex());
    CHECK_RENDER("getFullHeight()");
	RenderRectAccessor rd(m_doc->getRootNode());
	return (rd.getHeight() + rd.getY());
}

#define HEADER_MARGIN 4
/// calculate page header height
int LVDocView::getPageHeaderHeight() {
	if (!getPageHeaderInfo())
		return 0;
	if (!getInfoFont())
		return 0;
        int h = getInfoFont()->getHeight();
        int bh = m_batteryIcons.length()>0 ? m_batteryIcons[0]->GetHeight() * 11/10 + HEADER_MARGIN / 2 : 0;
        if ( bh>h )
            h = bh;
        return h + HEADER_MARGIN;
}

/// calculate page header rectangle
void LVDocView::getPageHeaderRectangle(int pageIndex, lvRect & headerRc, bool mergeTwoHeaders) {
	lvRect pageRc;
	getPageRectangle(pageIndex, pageRc, mergeTwoHeaders);
	headerRc = pageRc;
	if (pageIndex == 0 && m_showCover) {
		headerRc.bottom = 0;
	} else {
		int h = getPageHeaderHeight();
		headerRc.bottom = headerRc.top + h;
		headerRc.top += HEADER_MARGIN;
		headerRc.left += HEADER_MARGIN;
		headerRc.right -= HEADER_MARGIN;
	}
}

/// returns current time representation string
lString32 LVDocView::getTimeString() {
	time_t t = (time_t) time(0);
	tm * bt = localtime(&t);
	char str[12];
	if ( m_props->getBoolDef(PROP_SHOW_TIME_12HOURS, false) ) {
		strftime(str, sizeof(str), "%I:%M %p", bt);
	}
	else {
		strftime(str, sizeof(str), "%H:%M", bt);
	}
	return Utf8ToUnicode(lString8(str));
}

/// draw battery state to buffer
void LVDocView::drawBatteryState(LVDrawBuf * drawbuf, const lvRect & batteryRc,
		bool /*isVertical*/) {
	if (m_battery_state == CR_BATTERY_STATE_NO_BATTERY)
		return;
	LVDrawStateSaver saver(*drawbuf);
	int textColor = drawbuf->GetBackgroundColor();
	int bgColor = drawbuf->GetTextColor();
	drawbuf->SetTextColor(bgColor);
	drawbuf->SetBackgroundColor(textColor);
	LVRefVec<LVImageSource> icons;
	bool drawPercent = m_props->getBoolDef(PROP_SHOW_BATTERY_PERCENT, true) || m_batteryIcons.size()<=2;
	if ( m_batteryIcons.size()>1 ) {
		icons.add(m_batteryIcons[0]);
		if ( drawPercent ) {
            m_batteryFont = fontMan->GetFont(m_batteryIcons[0]->GetHeight()-1, 900, false,
                    DEFAULT_FONT_FAMILY, m_statusFontFace);
            icons.add(m_batteryIcons[m_batteryIcons.length()-1]);
		} else {
			for ( int i=1; i<m_batteryIcons.length()-1; i++ )
				icons.add(m_batteryIcons[i]);
		}
	} else {
		if ( m_batteryIcons.size()==1 )
			icons.add(m_batteryIcons[0]);
	}
	LVDrawBatteryIcon(drawbuf, batteryRc, m_battery_state, m_battery_state
			== CR_BATTERY_STATE_CHARGING, icons, drawPercent ? m_batteryFont.get() : NULL);
#if 0
	if ( m_batteryIcons.length()>1 ) {
		int iconIndex = ((m_batteryIcons.length() - 1 ) * m_battery_state + (100/m_batteryIcons.length()/2) )/ 100;
		if ( iconIndex<0 )
		iconIndex = 0;
		if ( iconIndex>m_batteryIcons.length()-1 )
		iconIndex = m_batteryIcons.length()-1;
		CRLog::trace("battery icon index = %d", iconIndex);
		LVImageSourceRef icon = m_batteryIcons[iconIndex];
		drawbuf->Draw( icon, (batteryRc.left + batteryRc.right - icon->GetWidth() ) / 2,
				(batteryRc.top + batteryRc.bottom - icon->GetHeight())/2,
				icon->GetWidth(),
				icon->GetHeight() );
	} else {
		lvRect rc = batteryRc;
		if ( m_battery_state<0 )
		return;
		lUInt32 cl = 0xA0A0A0;
		lUInt32 cl2 = 0xD0D0D0;
		if ( drawbuf->GetBitsPerPixel()<=2 ) {
			cl = 1;
			cl2 = 2;
		}
#if 1

		if ( isVertical ) {
			int h = rc.height();
			h = ( (h - 4) / 4 * 4 ) + 3;
			int dh = (rc.height() - h) / 2;
			rc.bottom -= dh;
			rc.top = rc.bottom - h;
			int w = rc.width();
			int h0 = 4; //h / 6;
			int w0 = w / 3;
			// contour
			drawbuf->FillRect( rc.left, rc.top+h0, rc.left+1, rc.bottom, cl );
			drawbuf->FillRect( rc.right-1, rc.top+h0, rc.right, rc.bottom, cl );

			drawbuf->FillRect( rc.left, rc.top+h0, rc.left+w0, rc.top+h0+1, cl );
			drawbuf->FillRect( rc.right-w0, rc.top+h0, rc.right, rc.top+h0+1, cl );

			drawbuf->FillRect( rc.left+w0-1, rc.top, rc.left+w0, rc.top+h0, cl );
			drawbuf->FillRect( rc.right-w0, rc.top, rc.right-w0+1, rc.top+h0, cl );

			drawbuf->FillRect( rc.left+w0, rc.top, rc.right-w0, rc.top+1, cl );
			drawbuf->FillRect( rc.left, rc.bottom-1, rc.right, rc.bottom, cl );
			// fill
			int miny = rc.bottom - 2 - (h - 4) * m_battery_state / 100;
			for ( int i=0; i<h-4; i++ ) {
				if ( (i&3) != 3 ) {
					int y = rc.bottom - 2 - i;
					int w = 2;
					if ( y < rc.top + h0 + 2 )
					w = w0 + 1;
					lUInt32 c = cl2;
					if ( y >= miny )
					c = cl;
					drawbuf->FillRect( rc.left+w, y-1, rc.right-w, y, c );
				}
			}
		} else {
			// horizontal
			int h = rc.width();
			h = ( (h - 4) / 4 * 4 ) + 3;
			int dh = (rc.height() - h) / 2;
			rc.right -= dh;
			rc.left = rc.right - h;
			h = rc.height();
			dh = h - (rc.width() * 4/8 + 1);
			if ( dh>0 ) {
				rc.bottom -= dh/2;
				rc.top += (dh/2);
				h = rc.height();
			}
			int w = rc.width();
			int h0 = h / 3; //h / 6;
			int w0 = 4;
			// contour
			drawbuf->FillRect( rc.left+w0, rc.top, rc.right, rc.top+1, cl );
			drawbuf->FillRect( rc.left+w0, rc.bottom-1, rc.right, rc.bottom, cl );

			drawbuf->FillRect( rc.left+w0, rc.top, rc.left+w0+1, rc.top+h0, cl );
			drawbuf->FillRect( rc.left+w0, rc.bottom-h0, rc.left+w0+1, rc.bottom, cl );

			drawbuf->FillRect( rc.left, rc.top+h0-1, rc.left+w0, rc.top+h0, cl );
			drawbuf->FillRect( rc.left, rc.bottom-h0, rc.left+w0, rc.bottom-h0+1, cl );

			drawbuf->FillRect( rc.left, rc.top+h0, rc.left+1, rc.bottom-h0, cl );
			drawbuf->FillRect( rc.right-1, rc.top, rc.right, rc.bottom, cl );
			// fill
			int minx = rc.right - 2 - (w - 4) * m_battery_state / 100;
			for ( int i=0; i<w-4; i++ ) {
				if ( (i&3) != 3 ) {
					int x = rc.right - 2 - i;
					int h = 2;
					if ( x < rc.left + w0 + 2 )
					h = h0 + 1;
					lUInt32 c = cl2;
					if ( x >= minx )
					c = cl;
					drawbuf->FillRect( x-1, rc.top+h, x, rc.bottom-h, c );
				}
			}
		}
#else
		//lUInt32 cl = getTextColor();
		int h = rc.height() / 6;
		if ( h<5 )
		h = 5;
		int n = rc.height() / h;
		int dy = rc.height() % h / 2;
		if ( n<1 )
		n = 1;
		int k = m_battery_state * n / 100;
		for ( int i=0; i<n; i++ ) {
			lvRect rrc = rc;
			rrc.bottom -= h * i + dy;
			rrc.top = rrc.bottom - h + 1;
			int dx = (i<n-1) ? 0 : rc.width()/5;
			rrc.left += dx;
			rrc.right -= dx;
			if ( i<k ) {
				// full
				drawbuf->FillRect( rrc.left, rrc.top, rrc.right, rrc.bottom, cl );
			} else {
				// empty
				drawbuf->FillRect( rrc.left, rrc.top, rrc.right, rrc.top+1, cl );
				drawbuf->FillRect( rrc.left, rrc.bottom-1, rrc.right, rrc.bottom, cl );
				drawbuf->FillRect( rrc.left, rrc.top, rrc.left+1, rrc.bottom, cl );
				drawbuf->FillRect( rrc.right-1, rrc.top, rrc.right, rrc.bottom, cl );
			}
		}
#endif
	}
#endif
}

/// returns section bounds, in 1/100 of percent
LVArray<int> & LVDocView::getSectionBounds( bool for_external_update ) {
	if (for_external_update || m_section_bounds_externally_updated) {
		// Progress bar markes will be externally updated: we don't care
		// about m_section_bounds_valid and we never trash it here.
		// It's the frontend responsability to notice it needs some
		// update and to update it.
		m_section_bounds_externally_updated = true;
		return m_section_bounds;
	}
	if (m_section_bounds_valid)
		return m_section_bounds;
	m_section_bounds.clear();
	m_section_bounds.add(0);
    // Get sections from FB2 books
    ldomNode * body = m_doc->nodeFromXPath(cs32("/FictionBook/body[1]"));
	lUInt16 section_id = m_doc->getElementNameIndex(U"section");
    if (body == NULL) {
        // Get sections from EPUB books
        body = m_doc->nodeFromXPath(cs32("/body[1]"));
        section_id = m_doc->getElementNameIndex(U"DocFragment");
    }
	int fh = GetFullHeight();
    int pc = getVisiblePageCount();
	if (body && fh > 0) {
		int cnt = body->getChildCount();
		for (int i = 0; i < cnt; i++) {

            ldomNode * l1section = body->getChildElementNode(i, section_id);
            if (!l1section)
				continue;

            lvRect rc;
            l1section->getAbsRect(rc);
            if (getViewMode() == DVM_SCROLL) {
                int p = (int) (((lInt64) rc.top * 10000) / fh);
                m_section_bounds.add(p);
            } else {
                int fh = m_pages.length();
                if ( (pc==2 && (fh&1)) )
                    fh++;
                int p = m_pages.FindNearestPage(rc.top, 0);
                if (fh > 1)
                    m_section_bounds.add((int) (((lInt64) p * 10000) / fh));
            }

		}
	}
	m_section_bounds.add(10000);
	m_section_bounds_valid = true;
	return m_section_bounds;
}

int LVDocView::getPosEndPagePercent() {
    LVLock lock(getMutex());
    checkPos();
    if (getViewMode() == DVM_SCROLL) {
        int fh = GetFullHeight();
        int p = GetPos() + m_pageRects[0].height() - m_pageMargins.top - m_pageMargins.bottom - 10;
        if (fh > 0)
            return (int) (((lInt64) p * 10000) / fh);
        else
            return 0;
    } else {
        int pc = m_pages.length();
        if (pc > 0) {
            int p = getCurPage(true) + 1;// + 1;
            if (getVisiblePageCount() > 1)
                p++;
            if (p > pc - 1)
                p = pc - 1;
            if (p < 0)
                p = 0;
            p = m_pages[p]->start - 10;
            int fh = GetFullHeight();
            if (fh > 0)
                return (int) (((lInt64) p * 10000) / fh);
            else
                return 0;
        } else
            return 0;
    }
}

int LVDocView::getPosPercent() {
	LVLock lock(getMutex());
	checkPos();
	if (getViewMode() == DVM_SCROLL) {
		int fh = GetFullHeight();
		int p = GetPos();
		if (fh > 0)
			return (int) (((lInt64) p * 10000) / fh);
		else
			return 0;
	} else {
        int fh = m_pages.length();
        if ( (getVisiblePageCount()==2 && (fh&1)) )
            fh++;
        int p = getCurPage(true);// + 1;
//        if ( getVisiblePageCount()>1 )
//            p++;
		if (fh > 0)
			return (int) (((lInt64) p * 10000) / fh);
		else
			return 0;
	}
}

void LVDocView::getPageRectangle(int pageIndex, lvRect & pageRect, bool mergeTwoPages) {
    if ( getVisiblePageCount() < 2 ) {
        pageRect = m_pageRects[0];
    }
    else {
        if ( mergeTwoPages ) {
            pageRect = m_pageRects[0];
            pageRect.right = m_pageRects[1].right;
        }
        else if ( (pageIndex & 1) == 0 ) { // Left page
            pageRect = m_pageRects[0];
        }
        else {
            pageRect = m_pageRects[1]; // Right page
        }
    }
}

/// sets battery state
bool LVDocView::setBatteryState(int newState) {
	if (m_battery_state == newState)
		return false;
	CRLog::info("New battery state: %d", newState);
	m_battery_state = newState;
	clearImageCache();
	return true;
}

/// set list of battery icons to display battery state
void LVDocView::setBatteryIcons(const LVRefVec<LVImageSource> & icons) {
	m_batteryIcons = icons;
}

lString32 fitTextWidthWithEllipsis(lString32 text, LVFontRef font, int maxwidth) {
	int w = font->getTextWidth(text.c_str(), text.length());
	if (w <= maxwidth)
		return text;
	int len;
	for (len = text.length() - 1; len > 1; len--) {
        lString32 s = text.substr(0, len) + "...";
		w = font->getTextWidth(s.c_str(), s.length());
		if (w <= maxwidth)
			return s;
	}
    return lString32::empty_str;
}

/// substitute page header with custom text (e.g. to be used while loading)
void LVDocView::setPageHeaderOverride(lString32 s) {
	m_pageHeaderOverride = s;
	clearImageCache();
}

/// draw page header to buffer
void LVDocView::drawPageHeader(LVDrawBuf * drawbuf, const lvRect & headerRc,
		int pageIndex, int phi, int pageCount) {
	lvRect oldcr;
	drawbuf->GetClipRect(&oldcr);
	lvRect hrc = headerRc;
    hrc.bottom += 2;
	drawbuf->SetClipRect(&hrc);
	bool drawGauge = true;
	lvRect info = headerRc;
//    if ( m_statusColor!=0xFF000000 ) {
//        CRLog::trace("Status color = %06x, textColor=%06x", m_statusColor, getTextColor());
//    } else {
//        CRLog::trace("Status color = TRANSPARENT, textColor=%06x", getTextColor());
//    }
	lUInt32 cl1 = m_statusColor!=0xFF000000 ? m_statusColor : getTextColor();
    //lUInt32 cl2 = getBackgroundColor();
    //lUInt32 cl3 = 0xD0D0D0;
    //lUInt32 cl4 = 0xC0C0C0;
	drawbuf->SetTextColor(cl1);
	//lUInt32 pal[4];
	int percent = getPosPercent();
	bool leftPage = (!m_twoVisiblePagesAsOnePageNumber && getVisiblePageCount() == 2 && !(pageIndex & 1));
	if (leftPage || !drawGauge)
		percent = 10000;
        int percent_pos = /*info.left + */percent * info.width() / 10000;
	//    int gh = 3; //drawGauge ? 3 : 1;
	LVArray<int> & sbounds = getSectionBounds();
	int gpos = info.bottom;
//	if (drawbuf->GetBitsPerPixel() <= 2) {
//		// gray
//		cl3 = 1;
//		cl4 = cl1;
//		//pal[0] = cl1;
//	}
	if ( leftPage )
		drawbuf->FillRect(info.left, gpos - 2, info.right, gpos - 2 + 1, cl1);
        //drawbuf->FillRect(info.left+percent_pos, gpos-gh, info.right, gpos-gh+1, cl1 ); //cl3
        //      drawbuf->FillRect(info.left + percent_pos, gpos - 2, info.right, gpos - 2
        //                      + 1, cl1); // cl3

	int sbound_index = 0;
	bool enableMarks = !leftPage && (phi & PGHDR_CHAPTER_MARKS) && sbounds.length()<info.width()/5;
	int w = GetWidth();
	int h = GetHeight();
	if (w > h)
		w = h;
	int markh = 3;
	int markw = 1;
	if (w > 700) {
		markh = 7;
        markw = 2;
	}
	for ( int x = info.left; x<info.right; x++ ) {
		int cl = -1;
		int sz = 1;
		int szx = 1;
		int boundCategory = 0;
		while ( enableMarks && sbound_index<sbounds.length() ) {
			int sx = info.left + sbounds[sbound_index] * (info.width() - 1) / 10000;
			if ( sx<x ) {
				sbound_index++;
				continue;
			}
			if ( sx==x ) {
				boundCategory = 1;
			}
			break;
		}
		if ( leftPage ) {
			cl = cl1;
			sz = 1;
		} else {
            if ( x < info.left + percent_pos ) {
                sz = 3;
				if ( boundCategory==0 )
					cl = cl1;
                else
                    sz = 0;
            } else {
				if ( boundCategory!=0 )
					sz = markh;
				cl = cl1;
				szx = markw;
			}
		}
        if ( cl!=-1 && sz>0 )
            drawbuf->FillRect(x, gpos - 2 - sz/2, x + szx, gpos - 2 + sz/2 + 1, cl);
	}

	lString32 text;
	//int iy = info.top; // + (info.height() - m_infoFont->getHeight()) * 2 / 3;
        int iy = info.top + /*m_infoFont->getHeight() +*/ (info.height() - m_infoFont->getHeight()) / 2 - HEADER_MARGIN/2;

	if (!m_pageHeaderOverride.empty()) {
		text = m_pageHeaderOverride;
	} else {

//		if (!leftPage) {
//			drawbuf->FillRect(info.left, gpos - 3, info.left + percent_pos,
//					gpos - 3 + 1, cl1);
//			drawbuf->FillRect(info.left, gpos - 1, info.left + percent_pos,
//					gpos - 1 + 1, cl1);
//		}

		// disable section marks for left page, and for too many marks
//		if (!leftPage && (phi & PGHDR_CHAPTER_MARKS) && sbounds.length()<info.width()/5 ) {
//			for (int i = 0; i < sbounds.length(); i++) {
//				int x = info.left + sbounds[i] * (info.width() - 1) / 10000;
//				lUInt32 c = x < info.left + percent_pos ? cl2 : cl1;
//				drawbuf->FillRect(x, gpos - 4, x + 1, gpos - 0 + 2, c);
//			}
//		}

		if (getVisiblePageCount() == 1 || !(pageIndex & 1)) {
			int dwIcons = 0;
			int icony = iy + m_infoFont->getHeight() / 2;
			for (int ni = 0; ni < m_headerIcons.length(); ni++) {
				LVImageSourceRef icon = m_headerIcons[ni];
				int h = icon->GetHeight();
				int w = icon->GetWidth();
				drawbuf->Draw(icon, info.left + dwIcons, icony - h / 2, w, h);
				dwIcons += w + 4;
			}
			info.left += dwIcons;
		}

		bool batteryPercentNormalFont = false; // PROP_SHOW_BATTERY_PERCENT
		if ((phi & PGHDR_BATTERY) && m_battery_state >= CR_BATTERY_STATE_CHARGING) {
			batteryPercentNormalFont = m_props->getBoolDef(PROP_SHOW_BATTERY_PERCENT, true) || m_batteryIcons.size()<=2;
			if ( !batteryPercentNormalFont ) {
				lvRect brc = info;
				brc.right -= 2;
				//brc.top += 1;
				//brc.bottom -= 2;
				int h = brc.height();
				int batteryIconWidth = 32;
				if (m_batteryIcons.length() > 0)
					batteryIconWidth = m_batteryIcons[0]->GetWidth();
				bool isVertical = (h > 30);
				//if ( isVertical )
				//    brc.left = brc.right - brc.height()/2;
				//else
				brc.left = brc.right - batteryIconWidth - 2;
				brc.bottom -= 5;
				drawBatteryState(drawbuf, brc, isVertical);
				info.right = brc.left - info.height() / 2;
			}
		}
		lString32 pageinfo;
		if (pageCount > 0) {
			if (phi & PGHDR_PAGE_NUMBER)
                pageinfo += fmt::decimal( getExternalPageNumber(pageIndex) + 1 );
            if (phi & PGHDR_PAGE_COUNT) {
                if ( !pageinfo.empty() )
                    pageinfo += " / ";
                if ( m_twoVisiblePagesAsOnePageNumber && getVisiblePageCount() == 2)
                    pageCount = (pageCount + 1) / 2;
                pageinfo += fmt::decimal(pageCount);
            }
            if (phi & PGHDR_PERCENT) {
                if ( !pageinfo.empty() )
                    pageinfo += "  ";
                //pageinfo += lString32::itoa(percent/100)+U"%"; //+U"."+lString32::itoa(percent/10%10)+U"%";
                pageinfo += fmt::decimal(percent/100);
                pageinfo += ",";
                int pp = percent%100;
                if ( pp<10 )
                    pageinfo << "0";
                pageinfo << fmt::decimal(pp) << "%";
            }
            if ( batteryPercentNormalFont ) {
                if (m_battery_state >= 0)
                    pageinfo << "  [" << fmt::decimal(m_battery_state) << "%]";
                else if (m_battery_state == CR_BATTERY_STATE_CHARGING)
                    pageinfo << "  [ + ]";
            }
        }

		int piw = 0;
		if (!pageinfo.empty()) {
			piw = m_infoFont->getTextWidth(pageinfo.c_str(), pageinfo.length());
			m_infoFont->DrawTextString(drawbuf, info.right - piw, iy,
					pageinfo.c_str(), pageinfo.length(), U' ', NULL, false);
			info.right -= piw + info.height() / 2;
		}
		if (phi & PGHDR_CLOCK) {
			lString32 clock = getTimeString();
			m_last_clock = clock;
			int w = m_infoFont->getTextWidth(clock.c_str(), clock.length()) + 2;
			m_infoFont->DrawTextString(drawbuf, info.right - w, iy,
					clock.c_str(), clock.length(), U' ', NULL, false);
			info.right -= w + info.height() / 2;
		}
		int authorsw = 0;
		lString32 authors;
		if (phi & PGHDR_AUTHOR)
			authors = getAuthors();
		int titlew = 0;
		lString32 title;
		if (phi & PGHDR_TITLE) {
			title = getTitle();
			if (title.empty() && authors.empty())
				title = m_doc_props->getStringDef(DOC_PROP_FILE_NAME);
			if (!title.empty())
				titlew
						= m_infoFont->getTextWidth(title.c_str(),
								title.length());
		}
		if (phi & PGHDR_AUTHOR && !authors.empty()) {
			if (!title.empty())
				authors += U'.';
			authorsw = m_infoFont->getTextWidth(authors.c_str(),
					authors.length());
		}
		int w = info.width() - 10;
		if (authorsw + titlew + 10 > w) {
			// Not enough room for both: alternate them
			if (getExternalPageNumber(pageIndex) & 1)
				text = title;
			else {
				text = authors;
				if (!text.empty() && text[text.length() - 1] == '.')
					text = text.substr(0, text.length() - 1);
			}
		} else {
            text = authors + "  " + title;
		}
	}
	lvRect newcr = headerRc;
	newcr.right = info.right - 10;
	drawbuf->SetClipRect(&newcr);
	text = fitTextWidthWithEllipsis(text, m_infoFont, newcr.width());
	if (!text.empty()) {
		m_infoFont->DrawTextString(drawbuf, info.left, iy, text.c_str(),
				text.length(), U' ', NULL, false);
	}
	drawbuf->SetClipRect(&oldcr);
	//--------------
	drawbuf->SetTextColor(getTextColor());
}

void LVDocView::drawPageTo(LVDrawBuf * drawbuf, LVRendPageInfo & page,
		lvRect * pageRect, int pageCount, int basePage, bool hasTwoVisiblePages, bool isRightPage, bool isLastPage) {
	int start = page.start;
	int height = page.height;
	int headerHeight = getPageHeaderHeight();
	//CRLog::trace("drawPageTo(%d,%d)", start, height);

	// pageRect is actually the full draw buffer, except in 2-page mode where
	// it is half the width plus/minus some possibly tweaked middle margin
	lvRect fullRect(0, 0, drawbuf->GetWidth(), drawbuf->GetHeight());
	if (!pageRect)
		pageRect = &fullRect;
	// drawbuf->setHidePartialGlyphs(getViewMode() == DVM_PAGES); // (is always true)
	// setHidePartialGlyphs() was added to allow drawing parts of glyphs outside the
	// clip (which can happen when small interline space), but its implementation is
	// a bit weird (at top: if less of 1/2 original height remain, don't draw the glyph;
	// at bottom: removethe clip and allow drawing the full glyph), and it feels this
	// is not the right way to solve this issue. So we don't use it.
	drawbuf->setHidePartialGlyphs(false);
	// We'll solve this issue another way: by using an alternative larger clip when drawing
	// line boxes fully contained into the page: draw_extra_info.content_overflow_clip below.

	// Clip, for normal content drawing: we use the computed pageRect, and the page.height
	// made from page splitting (that we have to enforce to not get painted some content
	// that is set to be on prev/next page).
	lvRect clip;
	clip.top = pageRect->top + m_pageMargins.top + headerHeight;
	clip.bottom = pageRect->top + m_pageMargins.top + height + headerHeight;
	// clip.left = pageRect->left + m_pageMargins.left;
	// clip.right = pageRect->left + pageRect->width() - m_pageMargins.right;
	// We don't really need to enforce left and right clipping of page margins:
	// this allows glyphs that need to (like 'J' at start of line or 'f' at
	// end of line with some fonts) to not be cut by this clipping.
	clip.left = pageRect->left;
	clip.right = pageRect->left + pageRect->width();

	// Extra info that DrawDocument() can fetch from drawbuf when some
	// alternative clipping is preferred
	draw_extra_info_t draw_extra_info = { 0 };
	drawbuf->SetDrawExtraInfo(&draw_extra_info);
	draw_extra_info.is_page_mode = true;
	draw_extra_info.is_left_page = false;
	draw_extra_info.is_right_page = false;
	draw_extra_info.draw_body_background = true;
	draw_extra_info.body_background_clip.top = headerHeight;
	draw_extra_info.body_background_clip.bottom = fullRect.bottom;
	draw_extra_info.body_background_clip.left = fullRect.left;
	draw_extra_info.body_background_clip.right = fullRect.right;
	// The above regular "clip" will be used to decide which lines of a paragraph
	// (possibly spanning multiple pages) will have to be drawn into this page.
	// For such lines fully contained into the regular clip, we will extend (in lvtextfm.cpp)
	// the clip so their tall glyphs, possibly overflowing the line box will, not get truncated.
	// We allow such glyphs to go into the top margin, but not into the header, and
	// allow them to go into the bottom margin, but not into any bottom footnotes area.
	// (If header, we could add 3px to not overflow into the progress bar and chapter marks,
	// but this would add some empty stripe that is more noticable that occasional overlaps.)
	draw_extra_info.content_overflow_clip.top = headerHeight;
	draw_extra_info.content_overflow_clip.bottom = fullRect.bottom; // will be reduced if footnotes
	draw_extra_info.content_overflow_clip.left = fullRect.left;
	draw_extra_info.content_overflow_clip.right = fullRect.right;

	if (hasTwoVisiblePages) {
		// Don't trust pageRects and their tweaked middle margin
		int middle_x = fullRect.left + fullRect.width() / 2;
		if (isRightPage) {
			clip.left = middle_x;
			clip.right = fullRect.right;
			draw_extra_info.is_right_page = true;
		}
		else {
			clip.left = fullRect.left;
			clip.right = middle_x;
			draw_extra_info.is_left_page = true;
		}
		draw_extra_info.body_background_clip.left = clip.left;
		draw_extra_info.body_background_clip.right = clip.right;
		if ( !isRightPage && isLastPage ) {
			// Left page is last page: have any background
			// drawn also on the right blank area
			draw_extra_info.body_background_clip.right = fullRect.right;
		}
		draw_extra_info.content_overflow_clip.left = clip.left;
		draw_extra_info.content_overflow_clip.right = clip.right;
	}

	if (page.flags & RN_PAGE_TYPE_COVER)
		clip.top = pageRect->top + m_pageMargins.top;
	if ( (m_pageHeaderInfo || !m_pageHeaderOverride.empty()) && getViewMode() == DVM_PAGES ) {
		// Decide what to draw in header
		// (In 2-pages mode, we are called for each of the 2 pages)
		bool drawHeader = true;
		bool hasTwoPages = getVisiblePageCount() == 2;
		bool useTwoHeaders = hasTwoPages && !m_twoVisiblePagesAsOnePageNumber;
		bool mergeTwoHeaders = hasTwoPages && !useTwoHeaders;
		bool isRightPage = page.index & 1;
		bool isCoverPage = !(page.flags & RN_PAGE_TYPE_NORMAL); // FB2 cover page
		// Handle a few edge cases
		if ( isCoverPage ) {
			// Never draw header on a FB2 cover page
			drawHeader = false;
		}
		else if ( mergeTwoHeaders ) {
			if ( isRightPage ) {
				if ( page.index == 1 && !(m_pages[0]->flags & RN_PAGE_TYPE_NORMAL) ) {
					// 2nd page, but left page is a cover without header
					// Draw it as if unmerged
					useTwoHeaders = true;
					mergeTwoHeaders = false;
				}
				else {
					// Right page when merged: skip drawing as left pages
					// has drawn it full width
					drawHeader = false;
				}
			}
		}
		int phi = m_pageHeaderInfo;
		if ( useTwoHeaders ) {
			if ( isRightPage ) { // Right page shows everything but author
				phi &= ~PGHDR_AUTHOR;
			}
			else { // Left page shows only author
				phi &= ~PGHDR_TITLE;
				phi &= ~PGHDR_PERCENT;
				phi &= ~PGHDR_PAGE_NUMBER;
				phi &= ~PGHDR_PAGE_COUNT;
				phi &= ~PGHDR_BATTERY;
				phi &= ~PGHDR_CLOCK;
			}
		}
		if ( drawHeader ) {
			lvRect info;
			getPageHeaderRectangle(page.index, info, mergeTwoHeaders);
			drawPageHeader(drawbuf, info, page.index - 1 + basePage, phi, pageCount - 1 + basePage);
			//clip.top = info.bottom;
		}
	}
	drawbuf->SetClipRect(&clip);
	if (m_doc) {
		if (page.flags & RN_PAGE_TYPE_COVER) {
			lvRect rc = *pageRect;
			drawbuf->SetClipRect(&rc);
			//if ( m_pageMargins.bottom > m_pageMargins.top )
			//    rc.bottom -= m_pageMargins.bottom - m_pageMargins.top;
			/*
			 rc.left += m_pageMargins.left / 2;
			 rc.top += m_pageMargins.bottom / 2;
			 rc.right -= m_pageMargins.right / 2;
			 rc.bottom -= m_pageMargins.bottom / 2;
			 */
			//CRLog::trace("Entering drawCoverTo()");
			drawCoverTo(drawbuf, rc);
		} else {
			// If we have footnotes that we'll put at bottom of page, make sure we
			// clip above them (tall inline-block content, even if split across pages,
			// could have their content painted over the footnotes)
			bool has_footnotes = page.footnotes.length() > 0;
			#define FOOTNOTE_MARGIN_REM 1 // as in lvpagesplitter.cpp
			int footnote_margin = FOOTNOTE_MARGIN_REM * m_font_size;
			int footnotes_height = 0;
			for (int fn = 0; fn < page.footnotes.length(); fn++) {
				footnotes_height += page.footnotes[fn].height;
			}
			if ( has_footnotes) {
				// We'll be drawing a separator at 2/3 in the footnote margin.
				// Allow over-painting to 1/2 in this footnote margin (so any
				// drop-cap tail and such may not get clipped out.
				draw_extra_info.content_overflow_clip.bottom = fullRect.bottom - m_pageMargins.bottom - footnotes_height - footnote_margin/2;
			}

			// draw main page text
			if ( m_markRanges.length() )
				CRLog::trace("Entering DrawDocument() : %d ranges", m_markRanges.length());
			//CRLog::trace("Entering DrawDocument()");
			if (page.height)
				DrawDocument(*drawbuf, m_doc->getRootNode(),
						pageRect->left + m_pageMargins.left, // x0
						clip.top,                            // y0
						pageRect->width() - m_pageMargins.left - m_pageMargins.right, // dx
						height,  // dy
						0,       // doc_x
						-start,  // doc_y
						m_dy,    // page_height
						&m_markRanges, &m_bmkRanges);
			//CRLog::trace("Done DrawDocument() for main text");

			// Draw footnotes at the bottom of page (put any remaining blank space above them)
			int fny = clip.top + (page.height ? page.height + footnote_margin
					: footnote_margin);
			if ( has_footnotes) {
				int h_avail = m_dy - getPageHeaderHeight()
						   - m_pageMargins.top - m_pageMargins.bottom
						   - height - footnote_margin;
				fny += h_avail - footnotes_height; // put empty space before first footnote
				draw_extra_info.draw_body_background = false;
				// We'll be drawing a separator at 2/3 in the footnote margin.
				// Allow over-painting up to it in this footnote margin (so any glyph head
				// overflowing the line box get less chances to be truncated.
				draw_extra_info.content_overflow_clip.top = fny - footnote_margin * 1/3 + 1;
				draw_extra_info.content_overflow_clip.bottom = fullRect.bottom;
			}
			int fy = fny;
			for (int fn = 0; fn < page.footnotes.length(); fn++) {
				int fstart = page.footnotes[fn].start;
				int fheight = page.footnotes[fn].height;
				clip.top = fy;
				clip.bottom = fy + fheight;
				// We keep the original clip.left/right unchanged
				drawbuf->SetClipRect(&clip);
				DrawDocument(*drawbuf, m_doc->getRootNode(),
						pageRect->left + m_pageMargins.left, // x0
						fy,                                  // y0
						pageRect->width() - m_pageMargins.left - m_pageMargins.right, // dx
						fheight,  // dy
						0,        // doc_x
						-fstart,  // doc_y
						m_dy,     // page_height
						&m_markRanges);
				fy += fheight;
			}
			if ( has_footnotes) {
				// Draw a small horizontal line as a separator inside
				// the margin between text and footnotes
				fny -= footnote_margin * 1/3;
				drawbuf->SetClipRect(NULL);
				lUInt32 cl = drawbuf->GetTextColor();
				cl = (cl & 0xFFFFFF) | (0x55000000);
				// The line separator was using the full page width:
				//   int x1 = pageRect->right - m_pageMargins.right;
				// but 1/7 of page width looks like what we can see in some books
				int sep_width = (pageRect->right - pageRect->left) / 7;
				int x0, x1;
				if ( page.flags & RN_PAGE_FOOTNOTES_MOSTLY_RTL ) { // draw separator on the right
					x1 = pageRect->right - m_pageMargins.right;
					x0 = x1 - sep_width;
				}
				else {
					x0 = pageRect->left + m_pageMargins.left;
					x1 = x0 + sep_width;
				}
				drawbuf->FillRect(x0, fny, x1, fny+1, cl);
			}
		}
	}
	drawbuf->SetClipRect(NULL);
#ifdef SHOW_PAGE_RECT
	drawbuf->FillRect(pageRect->left, pageRect->top, pageRect->left+1, pageRect->bottom, 0xAAAAAA);
	drawbuf->FillRect(pageRect->left, pageRect->top, pageRect->right, pageRect->top+1, 0xAAAAAA);
	drawbuf->FillRect(pageRect->right-1, pageRect->top, pageRect->right, pageRect->bottom, 0xAAAAAA);
	drawbuf->FillRect(pageRect->left, pageRect->bottom-1, pageRect->right, pageRect->bottom, 0xAAAAAA);
	drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->top+m_pageMargins.top+headerHeight, pageRect->left+1+m_pageMargins.left, pageRect->bottom-m_pageMargins.bottom, 0x555555);
	drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->top+m_pageMargins.top+headerHeight, pageRect->right-m_pageMargins.right, pageRect->top+1+m_pageMargins.top+headerHeight, 0x555555);
	drawbuf->FillRect(pageRect->right-1-m_pageMargins.right, pageRect->top+m_pageMargins.top+headerHeight, pageRect->right-m_pageMargins.right, pageRect->bottom-m_pageMargins.bottom, 0x555555);
	drawbuf->FillRect(pageRect->left+m_pageMargins.left, pageRect->bottom-1-m_pageMargins.bottom, pageRect->right-m_pageMargins.right, pageRect->bottom-m_pageMargins.bottom, 0x555555);
#endif

#if 0
	lString32 pagenum = lString32::itoa( page.index+1 );
	m_font->DrawTextString(drawbuf, 5, 0 , pagenum.c_str(), pagenum.length(), '?', NULL, false); //drawbuf->GetHeight()-m_font->getHeight()
#endif
}

/// returns page count
int LVDocView::getPageCount(bool internal) {
    if (internal || !m_twoVisiblePagesAsOnePageNumber || getVisiblePageCount() != 2 ) {
        return m_pages.length();
    }
    else {
        return (m_pages.length() + 1) / 2;
    }
}

//============================================================================
// Navigation code
//============================================================================

/// get position of view inside document
void LVDocView::GetPos(lvRect & rc) {
    checkPos();
	rc.left = 0;
	rc.right = GetWidth();
	if (isPageMode() && _page >= 0 && _page < m_pages.length()) {
		rc.top = m_pages[_page]->start;
        if (getVisiblePageCount() == 2) {
            if (_page < m_pages.length() - 1)
                rc.bottom = m_pages[_page + 1]->start + m_pages[_page + 1]->height;
            else
                rc.bottom = rc.top + m_pages[_page]->height;
        } else
            rc.bottom = rc.top + m_pages[_page]->height;
	} else {
		rc.top = _pos;
		rc.bottom = _pos + GetHeight();
	}
}

int LVDocView::getPageFlow(int pageIndex)
{
	if (pageIndex >= 0 && pageIndex < m_pages.length())
		return m_pages[pageIndex]->flow;
	return -1;
}

bool LVDocView::hasNonLinearFlows()
{
	return m_pages.hasNonLinearFlows();
}

int LVDocView::getPageHeight(int pageIndex)
{
	if (isPageMode() && pageIndex >= 0 && pageIndex < m_pages.length())
		return m_pages[pageIndex]->height;
	return 0;
}

int LVDocView::getPageStartY(int pageIndex)
{
	if (isPageMode() && pageIndex >= 0 && pageIndex < m_pages.length())
		return m_pages[pageIndex]->start;
	return -1;
}

/// get vertical position of view inside document
int LVDocView::GetPos() {
    checkPos();
    if (isPageMode() && _page >= 0 && _page < m_pages.length())
		return m_pages[_page]->start;
	return _pos;
}

int LVDocView::SetPos(int pos, bool savePos, bool allowScrollAfterEnd) {
	LVLock lock(getMutex());
	_posIsSet = true;
    CHECK_RENDER("setPos()")
	//if ( m_posIsSet && m_pos==pos )
	//    return;
	if (isScrollMode()) {
		if (pos > GetFullHeight() - m_dy && !allowScrollAfterEnd)
			pos = GetFullHeight() - m_dy;
		if (pos < 0)
			pos = 0;
		_pos = pos;
		int page = m_pages.FindNearestPage(pos, 0);
		if (page >= 0 && page < m_pages.length())
			_page = page;
		else
			_page = -1;
	} else {
		int pc = getVisiblePageCount();
		int page = m_pages.FindNearestPage(pos, 0);
		if (pc == 2)
			page &= ~1;
		if (page < m_pages.length()) {
			_pos = m_pages[page]->start;
			_page = page;
		} else {
			_pos = 0;
			_page = 0;
		}
	}
	if (savePos)
		_posBookmark = getBookmark();
	_posIsSet = true;
	updateScroll();
	return 1;
	//Draw();
}

int LVDocView::getCurPage(bool internal) {
	LVLock lock(getMutex());
	checkPos();
	int page = _page;
	if (!isPageMode() || _page < 0)
		page = m_pages.FindNearestPage(_pos, 0);
	return internal ? page : getExternalPageNumber( page );
}

bool LVDocView::goToPage(int page, bool internal, bool updatePosBookmark, bool regulateTwoPages) {
	LVLock lock(getMutex());
    CHECK_RENDER("goToPage()")
	if (!m_pages.length())
		return false;
	bool res = true;
	if (!internal)
		page = getInternalPageNumber(page);
	if (isScrollMode()) {
		if (page >= 0 && page < m_pages.length()) {
			_pos = m_pages[page]->start;
			_page = page;
		} else {
			res = false;
			_pos = 0;
			_page = 0;
		}
	} else {
		int pc = getVisiblePageCount();
		if (page >= m_pages.length()) {
			page = m_pages.length() - 1;
			res = false;
		}
		if (page < 0) {
			page = 0;
			res = false;
		}
		if (pc == 2 && regulateTwoPages)
			page &= ~1; // first page will always be odd (page are counted from 0)
		if (page >= 0 && page < m_pages.length()) {
			_pos = m_pages[page]->start;
			_page = page;
		} else {
			_pos = 0;
			_page = 0;
			res = false;
		}
	}
    if (updatePosBookmark) {
        _posBookmark = getBookmark();
    }
	_posIsSet = true;
	updateScroll();
    //if (res)
        //updateBookMarksRanges();
	return res;
}

/// returns true if time changed since clock has been last drawed
bool LVDocView::isTimeChanged() {
	if ( m_pageHeaderInfo & PGHDR_CLOCK ) {
		bool res = (m_last_clock != getTimeString());
		if (res)
			clearImageCache();
		return res;
	}
	return false;
}

/// check whether resize or creation of buffer is necessary, ensure buffer is ok
static bool checkBufferSize( LVRef<LVColorDrawBuf> & buf, int dx, int dy ) {
    if ( buf.isNull() || buf->GetWidth()!=dx || buf->GetHeight()!=dy ) {
        buf.Clear();
        buf = LVRef<LVColorDrawBuf>( new LVColorDrawBuf(dx, dy, 16) );
        return false; // need redraw
    } else
        return true;
}

/// clears page background
void LVDocView::drawPageBackground( LVDrawBuf & drawbuf, int offsetX, int offsetY, int alpha)
{
    //CRLog::trace("drawPageBackground() called");
    drawbuf.SetBackgroundColor(m_backgroundColor);
    if ( !m_backgroundImage.isNull() ) {
        // texture
        int dx = drawbuf.GetWidth();
        int dy = drawbuf.GetHeight();
        if ( m_backgroundTiled ) {
            //CRLog::trace("drawPageBackground() using texture %d x %d", m_backgroundImage->GetWidth(), m_backgroundImage->GetHeight());
            if ( !checkBufferSize( m_backgroundImageScaled, m_backgroundImage->GetWidth(), m_backgroundImage->GetHeight() ) ) {
                // unpack

                m_backgroundImageScaled->Draw(LVCreateAlphaTransformImageSource(m_backgroundImage, alpha), 0, 0, m_backgroundImage->GetWidth(), m_backgroundImage->GetHeight(), false);
            }
            LVImageSourceRef src = LVCreateDrawBufImageSource(m_backgroundImageScaled.get(), false);
            LVImageSourceRef tile = LVCreateTileTransform( src, dx, dy, offsetX, offsetY );
            //CRLog::trace("created tile image, drawing");
            drawbuf.Draw(LVCreateAlphaTransformImageSource(tile, alpha), 0, 0, dx, dy);
            //CRLog::trace("draw completed");
        } else {
            if ( getViewMode()==DVM_SCROLL ) {
                // scroll
                if ( !checkBufferSize( m_backgroundImageScaled, dx, m_backgroundImage->GetHeight() ) ) {
                    // unpack
                    LVImageSourceRef resized = LVCreateStretchFilledTransform(m_backgroundImage, dx, m_backgroundImage->GetHeight(),
                                                                              IMG_TRANSFORM_STRETCH,
                                                                              IMG_TRANSFORM_TILE,
                                                                              0, 0);
                    m_backgroundImageScaled->Draw(LVCreateAlphaTransformImageSource(resized, alpha), 0, 0, dx, m_backgroundImage->GetHeight(), false);
                }
                LVImageSourceRef src = LVCreateDrawBufImageSource(m_backgroundImageScaled.get(), false);
                LVImageSourceRef resized = LVCreateStretchFilledTransform(src, dx, dy,
                                                                          IMG_TRANSFORM_TILE,
                                                                          IMG_TRANSFORM_TILE,
                                                                          offsetX, offsetY);
                drawbuf.Draw(LVCreateAlphaTransformImageSource(resized, alpha), 0, 0, dx, dy);
            } else if ( getVisiblePageCount() != 2 ) {
                // single page
                if ( !checkBufferSize( m_backgroundImageScaled, dx, dy ) ) {
                    // unpack
                    LVImageSourceRef resized = LVCreateStretchFilledTransform(m_backgroundImage, dx, dy,
                                                                              IMG_TRANSFORM_STRETCH,
                                                                              IMG_TRANSFORM_STRETCH,
                                                                              offsetX, offsetY);
                    m_backgroundImageScaled->Draw(LVCreateAlphaTransformImageSource(resized, alpha), 0, 0, dx, dy, false);
                }
                LVImageSourceRef src = LVCreateDrawBufImageSource(m_backgroundImageScaled.get(), false);
                drawbuf.Draw(LVCreateAlphaTransformImageSource(src, alpha), 0, 0, dx, dy);
            } else {
                // two pages
                int halfdx = (dx + 1) / 2;
                if ( !checkBufferSize( m_backgroundImageScaled, halfdx, dy ) ) {
                    // unpack
                    LVImageSourceRef resized = LVCreateStretchFilledTransform(m_backgroundImage, halfdx, dy,
                                                                              IMG_TRANSFORM_STRETCH,
                                                                              IMG_TRANSFORM_STRETCH,
                                                                              offsetX, offsetY);
                    m_backgroundImageScaled->Draw(LVCreateAlphaTransformImageSource(resized, alpha), 0, 0, halfdx, dy, false);
                }
                LVImageSourceRef src = LVCreateDrawBufImageSource(m_backgroundImageScaled.get(), false);
                drawbuf.Draw(LVCreateAlphaTransformImageSource(src, alpha), 0, 0, halfdx, dy);
                drawbuf.Draw(LVCreateAlphaTransformImageSource(src, alpha), dx/2, 0, dx - halfdx, dy);
            }
        }
    } else {
        // solid color
        lUInt32 cl = m_backgroundColor;
        if (alpha > 0) {
            cl = (cl & 0xFFFFFF) | (alpha << 24);
            drawbuf.FillRect(0, 0, drawbuf.GetWidth(), drawbuf.GetHeight(), cl);
        } else
            drawbuf.Clear(cl);
    }
    // No need for a thin line separator: the middle margin was sized
    // to be like the other margins: a separator would cut it in half
    // and make it look like two smaller margins.
    /*
    if (drawbuf.GetBitsPerPixel() == 32 && getVisiblePageCount() == 2) {
        int x = drawbuf.GetWidth() / 2;
        lUInt32 cl = m_backgroundColor;
        cl = ((cl & 0xFCFCFC) + 0x404040) >> 1;
        drawbuf.FillRect(x, 0, x + 1, drawbuf.GetHeight(), cl);
    }
    */
}

/// draw to specified buffer
void LVDocView::Draw(LVDrawBuf & drawbuf, int position, int page, bool rotate, bool autoresize) {
	LVLock lock(getMutex());
	//CRLog::trace("Draw() : calling checkPos()");
	checkPos();
	//CRLog::trace("Draw() : calling drawbuf.resize(%d, %d)", m_dx, m_dy);
	if (autoresize)
		drawbuf.Resize(m_dx, m_dy);
	drawbuf.SetBackgroundColor(m_backgroundColor);
	drawbuf.SetTextColor(m_textColor);
	//CRLog::trace("Draw() : calling clear()", m_dx, m_dy);

	if (!m_is_rendered)
		return;
	if (!m_doc)
		return;
	if (m_font.isNull())
		return;
	if (isScrollMode()) {
		drawbuf.SetClipRect(NULL);
        drawbuf.setHidePartialGlyphs(false);
        drawPageBackground(drawbuf, 0, position);
        int cover_height = 0;
		if (m_pages.length() > 0 && (m_pages[0]->flags & RN_PAGE_TYPE_COVER))
			cover_height = m_pages[0]->height;
		if (position < cover_height) {
			lvRect rc;
			drawbuf.GetClipRect(&rc);
			rc.top -= position;
			rc.bottom -= position;
			rc.top += m_pageMargins.top;
			rc.bottom -= m_pageMargins.bottom;
			rc.left += m_pageMargins.left;
			rc.right -= m_pageMargins.right;
			drawCoverTo(&drawbuf, rc);
		}
		DrawDocument(drawbuf, m_doc->getRootNode(),
				m_pageMargins.left,  // x0
				0,                   // y0
				drawbuf.GetWidth() - m_pageMargins.left - m_pageMargins.right, // dx
				drawbuf.GetHeight(), // dy
				0,                   // doc_x
				-position,           // doc_y
				drawbuf.GetHeight(), // page_height
				&m_markRanges, &m_bmkRanges);
	} else {
		int pc = getVisiblePageCount();
		//CRLog::trace("searching for page with offset=%d", position);
		if (page == -1)
			page = m_pages.FindNearestPage(position, 0);
		//CRLog::trace("found page #%d", page);

        //drawPageBackground(drawbuf, (page * 1356) & 0xFFF, 0x1000 - (page * 1356) & 0xFFF);
        drawPageBackground(drawbuf, 0, 0);

        if (page >= 0 && page < m_pages.length())
			drawPageTo(&drawbuf, *m_pages[page], &m_pageRects[0],
					m_pages.length(), 1, pc==2, false, page==m_pages.length()-1);
		if (pc == 2 && page >= 0 && page + 1 < m_pages.length())
			drawPageTo(&drawbuf, *m_pages[page + 1], &m_pageRects[1],
					m_pages.length(), 1, true, true, page+1==m_pages.length()-1);
	}
#if CR_INTERNAL_PAGE_ORIENTATION==1
	if ( rotate ) {
		//CRLog::trace("Rotate drawing buffer. Src size=(%d, %d), angle=%d, buf(%d, %d)", m_dx, m_dy, m_rotateAngle, drawbuf.GetWidth(), drawbuf.GetHeight() );
		drawbuf.Rotate( m_rotateAngle );
		//CRLog::trace("Rotate done. buf(%d, %d)", drawbuf.GetWidth(), drawbuf.GetHeight() );
	}
#endif
}

//void LVDocView::Draw()
//{
//Draw( m_drawbuf, m_pos, true );
//}

/// converts point from window to document coordinates, returns true if success
bool LVDocView::windowToDocPoint(lvPoint & pt, bool pullInPageArea) {
    CHECK_RENDER("windowToDocPoint()")
#if CR_INTERNAL_PAGE_ORIENTATION==1
	pt = rotatePoint( pt, true );
#endif
	if (getViewMode() == DVM_SCROLL) {
		// SCROLL mode
		if ( pullInPageArea ) {
			if ( pt.x < m_pageMargins.left )
				pt.x = m_pageMargins.left;
			if ( pt.x >= m_dx - m_pageMargins.right )
				pt.x = m_dx - m_pageMargins.right - 1;
		}
		pt.y += _pos;
		pt.x -= m_pageMargins.left;
		return true;
	} else {
		// PAGES mode
		int page = getCurPage(true);
		lvRect * rc = NULL;
		lvRect page1(m_pageRects[0]);
		int headerHeight = getPageHeaderHeight();
		page1.left += m_pageMargins.left;
		page1.top += m_pageMargins.top + headerHeight;
		page1.right -= m_pageMargins.right;
		page1.bottom -= m_pageMargins.bottom;
		if ( pullInPageArea ) {
			if ( getVisiblePageCount() < 2 || pt.x <= m_dx / 2) {
				if ( pt.x < page1.left )
					pt.x = page1.left;
				if ( pt.x >= page1.right )
					pt.x = page1.right - 1;
				if ( pt.y < page1.top )
					pt.y = page1.top;
				if ( pt.y >= page1.bottom )
					pt.y = page1.bottom - 1;
			}
		}
		lvRect page2;
		if (page1.isPointInside(pt)) {
			rc = &page1;
		} else if (getVisiblePageCount() == 2) {
			page2 = m_pageRects[1];
			page2.left += m_pageMargins.left;
			page2.top += m_pageMargins.top + headerHeight;
			page2.right -= m_pageMargins.right;
			page2.bottom -= m_pageMargins.bottom;
			if ( pullInPageArea ) {
				if ( pt.x < page2.left )
					pt.x = page2.left;
				if ( pt.x >= page2.right )
					pt.x = page2.right - 1;
				if ( pt.y < page2.top )
					pt.y = page2.top;
				if ( pt.y >= page2.bottom )
					pt.y = page2.bottom - 1;
			}
			if (page2.isPointInside(pt)) {
				rc = &page2;
				page++;
			}
		}
		if (rc && page >= 0 && page < m_pages.length()) {
			int page_y = m_pages[page]->start;
			pt.x -= rc->left;
			pt.y -= rc->top;
			if ( pullInPageArea ) {
				if ( pt.y >= m_pages[page]->height )
					pt.y = m_pages[page]->height - 1;
			}
			if (pt.y < m_pages[page]->height) {
				//CRLog::debug(" point page offset( %d, %d )", pt.x, pt.y );
				pt.y += page_y;
				return true;
			}
		}
	}
	return false;
}

/// converts point from document to window coordinates, returns true if success
bool LVDocView::docToWindowPoint(lvPoint & pt, bool isRectBottom, bool fitToPage) {
	LVLock lock(getMutex());
    CHECK_RENDER("docToWindowPoint()")
	// TODO: implement coordinate conversion here
	if (getViewMode() == DVM_SCROLL) {
		// SCROLL mode
		pt.y -= _pos;
		pt.x += m_pageMargins.left;
		return true;
	} else {
            // PAGES mode
            int page = getCurPage(true);
            if (page >= 0 && page < m_pages.length() && pt.y >= m_pages[page]->start) {
                int index = -1;
                // The y at start+height is normally part of the next
                // page. But we are often called twice with a topLeft
                // and a bottomRight of a rectangle bounding words or
                // a line, with the bottomRight being a rect height,
                // so pointing to the y just outside the box.
                // So, when we're specified that this point is a rect bottom,
                // we return this page even if it's actually on next page.
                int page_bottom = m_pages[page]->start + m_pages[page]->height;
                if ( pt.y < page_bottom || (isRectBottom && pt.y == page_bottom) ) {
                    index = 0;
                }
                else if (getVisiblePageCount() == 2 && page + 1 < m_pages.length()) {
                    int next_page_bottom = m_pages[page + 1]->start + m_pages[page + 1]->height;
                    if ( pt.y < next_page_bottom || (isRectBottom && pt.y == next_page_bottom) ) {
                        index = 1;
                    }
                }
                if (index >= 0) {
                    /*
                    int x = pt.x + m_pageRects[index].left + m_pageMargins.left;
                    // We shouldn't get x ouside page width as we never crop on the
                    // width: if we do (if bug somewhere else), force it to be at the
                    // far right (this helps not discarding some highlights rects)
                    if (x >= m_pageRects[index].right - m_pageMargins.right) {
                        x = m_pageRects[index].right - m_pageMargins.right - 1;
                    }
                    if (x < m_pageRects[index].right - m_pageMargins.right) {
                        pt.x = x;
                        pt.y = pt.y + getPageHeaderHeight() + m_pageMargins.top - m_pages[page + index]->start;
                        return true;
                    }
                    */
                    // We don't crop on the left, so it feels like we don't need to
                    // ensure anything and crop on the right, and this allows text
                    // selection to grab bits of overflowed glyph
                    pt.x = pt.x + m_pageRects[index].left + m_pageMargins.left;
                    pt.y = pt.y + getPageHeaderHeight() + m_pageMargins.top - m_pages[page + index]->start;
                    return true;
                }
            }
            if (fitToPage) {
                // If we didn't find pt inside pages, adjust it to top or bottom page y
                if (page >= 0 && page < m_pages.length() && pt.y < m_pages[page]->start) {
                    // Before 1st page top: adjust to page top
                    pt.x = pt.x + m_pageRects[0].left + m_pageMargins.left;
                    pt.y = getPageHeaderHeight() + m_pageMargins.top;
                }
                else if (getVisiblePageCount() == 2 && page + 1 < m_pages.length()
                        && pt.y >= m_pages[page + 1]->start + m_pages[page + 1]->height) {
                    // After 2nd page bottom: adjust to 2nd page bottom
                    pt.x = pt.x + m_pageRects[1].left + m_pageMargins.left;
                    pt.y = getPageHeaderHeight() + m_pageMargins.top + m_pages[page+1]->height;
                }
                else {
                    // After single page bottom: adjust to page bottom
                    pt.x = pt.x + m_pageRects[0].left + m_pageMargins.left;
                    pt.y = getPageHeaderHeight() + m_pageMargins.top + m_pages[page]->height;
                }
                return true;
            }
            return false;
	}
#if CR_INTERNAL_PAGE_ORIENTATION==1
	pt = rotatePoint( pt, false );
#endif
	return false;
}

/// returns xpointer for specified window point
ldomXPointer LVDocView::getNodeByPoint(lvPoint pt, bool strictBounds, bool forTextSelection) {
	LVLock lock(getMutex());
    CHECK_RENDER("getNodeByPoint()")
	if (m_doc && windowToDocPoint(pt, forTextSelection)) {
		ldomXPointer ptr = m_doc->createXPointer(pt, PT_DIR_EXACT, strictBounds);
		//CRLog::debug("  ptr (%d, %d) node=%08X offset=%d", pt.x, pt.y, (lUInt32)ptr.getNode(), ptr.getOffset() );
		if ( forTextSelection ) {
			// For text selection, we don't want to return null if pt is not exactly on text:
			// - pt might be in the outer margins, and would then not be in the page area,
			//   so we called windowToDocPoint(pt, pullInPageArea=true) to get pt pulled
			//   into the page area.
			// - pt might be in some paragraph left or right margin, and it's possible PT_DIR_EXACT
			//   returns some text from a paragraph a few pages above that doesn't have that margin...
			// - pt might be in some inter paragraphs collapsed top/bottom margin, that would resolve
			//   to none of them.
			// Consider the ptr obtained with PT_DIR_EXACT valid only if non-null and if its rect contains pt.y
			bool valid = false;
			lvRect rc;
			if ( !ptr.isNull() && ptr.getRect(rc) ) {
				if ( pt.y >= rc.top && pt.y < rc.bottom )
					valid = true;
			}
			if ( !valid ) {
				// Find the nearest valid one using scan: SCAN_FORWARD if on the left side of
				// the page, SCAN_BACKWARD if on the right side
				int direction = PT_DIR_SCAN_FORWARD;
				if ( getVisiblePageCount() < 2 ) {
					if ( pt.x > m_dx / 2 )
						direction = PT_DIR_SCAN_BACKWARD;
				}
				else {
					if ( pt.x > m_dx * 3/4 )
						direction = PT_DIR_SCAN_BACKWARD;
					else if ( pt.x > m_dx * 1/4 && pt.x <= m_dx / 2 )
						direction = PT_DIR_SCAN_BACKWARD;
				}
				// Switch the scan direction if the page is mostly RTL
				// (all this might still not feel perfect on RTL pages...)
				int page = getCurPage(true);
				if ( getVisiblePageCount() == 2 && pt.x > m_dx / 2 ) {
					page += 1;
				}
				if ( page >= 0 && page < m_pages.length() && m_pages[page]->flags & RN_PAGE_MOSTLY_RTL ) {
					direction = -direction;
				}
				ptr = m_doc->createXPointer(pt, direction, strictBounds);
			}
		}
		return ptr;
	}
	return ldomXPointer();
}

/// returns image source for specified window point, if point is inside image
LVImageSourceRef LVDocView::getImageByPoint(lvPoint pt) {
    LVImageSourceRef res = LVImageSourceRef();
    ldomXPointer ptr = getNodeByPoint(pt);
    if (ptr.isNull())
        return res;
    //CRLog::debug("node: %s", LCSTR(ptr.toString()));
    ldomNode* node = ptr.getNode();
    if (node)
        res = node->getObjectImageSource();
    if (!res.isNull())
        CRLog::debug("getImageByPoint(%d, %d) : found image %d x %d", pt.x, pt.y, res->GetWidth(), res->GetHeight());
    return res;
}

bool LVDocView::drawImage(LVDrawBuf * buf, LVImageSourceRef img, int x, int y, int dx, int dy)
{
    if (img.isNull() || !buf)
        return false;
    // clear background
    drawPageBackground(*buf, 0, 0);
    // draw image
    buf->Draw(img, x, y, dx, dy, true);
    return true;
}

void LVDocView::updateLayout() {
	lvRect rc(0, 0, m_dx, m_dy);
	m_pageRects[0] = rc;
	m_pageRects[1] = rc;
	if (getVisiblePageCount() == 2) {
		int middle = (rc.left + rc.right) >> 1;
		// m_pageRects[0].right = middle - m_pageMargins.right / 2;
		// m_pageRects[1].left = middle + m_pageMargins.left / 2;
		// ^ seems wrong, as we later add the margins to these m_pageRects
		// so this would add to much uneeded middle margin
                // We will ensure a max middle margin the size of a single
                // left or right margin, whichever is the greatest.
		// We still want to ensure a minimal middle margin in case
		// the requested pageMargins are really small. Say 0.8em.
		int min_middle_margin = m_font_size * 80 / 100;
		int max_middle_margin = m_pageMargins.left > m_pageMargins.right ? m_pageMargins.left : m_pageMargins.right;
		int additional_middle_margin = 0;
		int middle_margin = m_pageMargins.right + m_pageMargins.left;
		if (middle_margin < min_middle_margin) {
			additional_middle_margin = min_middle_margin - middle_margin;
		}
		else if (middle_margin > max_middle_margin) {
			if (max_middle_margin > min_middle_margin)
				additional_middle_margin = max_middle_margin - middle_margin; // negative
			else
				additional_middle_margin = min_middle_margin - middle_margin; // negative
		}
		// Note: with negative values, we allow these 2 m_pageRects to
		// overlap. But it seems there is no issue doing that.
		m_pageRects[0].right = middle - additional_middle_margin / 2;
		m_pageRects[1].left = middle + additional_middle_margin / 2;
	}
}

/// set list of icons to display at left side of header
void LVDocView::setHeaderIcons(LVRefVec<LVImageSource> icons) {
	m_headerIcons = icons;
}

/// get page document range, -1 for current page
LVRef<ldomXRange> LVDocView::getPageDocumentRange(int pageIndex) {
    LVLock lock(getMutex());
    CHECK_RENDER("getPageDocRange()")
    // On some pages (eg: that ends with some padding between an
    // image on this page, and some text on next page), there may
    // be some area which is rendered "final" without any content,
    // thus holding no node. We could then get a null 'start' or
    // 'end' below by looking only at start_y or end_y.
    // So, in all cases, loop while increasing or decreasing y
    // to get more chances of finding a valid XPointer.
    LVRef < ldomXRange > res(NULL);
    int start_y;
    int end_y;
    if (isScrollMode()) {
        // SCROLL mode
        start_y = _pos;
        end_y = _pos + m_dy;
        int fh = GetFullHeight();
        if (end_y >= fh)
            end_y = fh - 1;
    }
    else {
        // PAGES mode
        if (pageIndex < 0 || pageIndex >= m_pages.length())
            pageIndex = getCurPage(true);
        if (pageIndex >= 0 && pageIndex < m_pages.length()) {
            LVRendPageInfo * page = m_pages[pageIndex];
            if (page->flags & RN_PAGE_TYPE_COVER)
                return res;
            start_y = page->start;
            end_y = page->start + page->height;
        }
        else {
            return res;
        }
    }
    if (!res.isNull())
        return res;
    int height = end_y - start_y;
    if (height < 0)
        return res;
    // Note: this may return a range that may not include some parts
    // of the document that are actually displayed on that page, like
    // floats that were in some HTML fragment of the previous page,
    // but whose positioning has been shifted/delayed and ended up
    // on this page (so, getCurrentPageLinks() may miss links from
    // such floats).
    // With floats, a page may actually show more than one range,
    // not sure how to deal with that (return a range including all
    // subranges, so possibly including stuff not shown on the page?)
    // We also want to get the xpointer to the first or last node
    // on a line in "logical order" instead of "visual order", which
    // is needed with bidi text to not miss some text on the first
    // or last line of the page.
    ldomXPointer start;
    ldomXPointer end;
    int start_h;
    for (start_h=0; start_h < height; start_h++) {
        start = m_doc->createXPointer(lvPoint(0, start_y + start_h), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST);
        // printf("  start (%d=%d): %s\n", start_h, start_y + start_h, UnicodeToLocal(start.toString()).c_str());
        if (!start.isNull()) {
            // Check what we got is really in current page
            lvPoint pt = start.toPoint();
            // printf("start pt.y %d start_y %d end_y %d\n", pt.y, start_y, end_y);
            if (pt.y >= start_y && pt.y <= end_y)
                break; // we found our start of page xpointer
        }
    }
    int end_h;
    for (end_h=height; end_h >= start_h; end_h--) {
        end = m_doc->createXPointer(lvPoint(GetWidth(), start_y + end_h), PT_DIR_SCAN_BACKWARD_LOGICAL_LAST);
            // (x=GetWidth() might be redundant with PT_DIR_SCAN_BACKWARD_LOGICAL_LAST, but it might help skiping floats)
        // printf("  end (%d=%d): %s\n", end_h, start_y + end_h, UnicodeToLocal(end.toString()).c_str());
        if (!end.isNull()) {
            // Check what we got is really in current page
            lvPoint pt = end.toPoint();
            // printf("end pt.y %d start_y %d end_y %d\n", pt.y, start_y, end_y);
            if (pt.y >= start_y && pt.y <= end_y)
                break; // we found our end of page xpointer
        }
    }
    if (start.isNull() || end.isNull())
        return res;
    res = LVRef<ldomXRange> (new ldomXRange(start, end));
    return res;
}

/// returns number of non-space characters on current page
int LVDocView::getCurrentPageCharCount()
{
    lString32 text = getPageText(true);
    int count = 0;
    for (int i=0; i<text.length(); i++) {
        lChar32 ch = text[i];
        if (ch>='0')
            count++;
    }
    return count;
}

/// returns number of images on current page
int LVDocView::getCurrentPageImageCount()
{
    CHECK_RENDER("getCurPageImgCount()")
    LVRef<ldomXRange> range = getPageDocumentRange(-1);
    class ImageCounter : public ldomNodeCallback {
        int count;
    public:
        int get() { return count; }
        ImageCounter() : count(0) { }
        /// called for each found text fragment in range
        virtual void onText(ldomXRange *) { }
        /// called for each found node in range
        virtual bool onElement(ldomXPointerEx * ptr) {
            lString32 nodeName = ptr->getNode()->getNodeName();
            if (nodeName == "img" || nodeName == "image")
                count++;
			return true;
        }

    };
    ImageCounter cnt;
    if (!range.isNull())
        range->forEach(&cnt);
    return cnt.get();
}

/// get page text, -1 for current page
lString32 LVDocView::getPageText(bool, int pageIndex) {
	LVLock lock(getMutex());
    CHECK_RENDER("getPageText()")
	lString32 txt;
	LVRef < ldomXRange > range = getPageDocumentRange(pageIndex);
	if (!range.isNull())
		txt = range->getRangeText();
	return txt;
}

void LVDocView::setRenderProps(int dx, int dy) {
	if (!m_doc || m_doc->getRootNode() == NULL)
		return;
	updateLayout();
	m_showCover = !getCoverPageImage().isNull();

	lString8 fontName = lString8(DEFAULT_FONT_NAME);
	m_font_size = scaleFontSizeForDPI(m_requested_font_size);
	m_font = fontMan->GetFont(m_font_size, LVRendGetBaseFontWeight(),
			false, DEFAULT_FONT_FAMILY, m_defaultFontFace);
	//m_font = LVCreateFontTransform( m_font, LVFONT_TRANSFORM_EMBOLDEN );
	m_infoFont = fontMan->GetFont(m_status_font_size, 400, false,
			DEFAULT_FONT_FAMILY, m_statusFontFace);
	if (!m_font || !m_infoFont)
		return;

	if (dx == 0)
		dx = m_pageRects[0].width() - m_pageMargins.left - m_pageMargins.right;
	if (dy == 0)
		dy = m_pageRects[0].height() - m_pageMargins.top - m_pageMargins.bottom
				- getPageHeaderHeight();

    // updateDocStyleSheet(); // moved after next line as possible CSS media queries like "(orientation:portrait)"
                              // need these props to be available and accurate in m_doc
    m_doc->setRenderProps(dx, dy, m_showCover, m_showCover ? dy
            + m_pageMargins.bottom * 4 : 0, m_font, m_def_interline_space, m_props);
    updateDocStyleSheet();

    text_highlight_options_t h;
    h.bookmarkHighlightMode = m_props->getIntDef(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, highlight_mode_underline);
    h.selectionColor = (m_props->getColorDef(PROP_HIGHLIGHT_SELECTION_COLOR, 0xC0C0C0) & 0xFFFFFF);
    h.commentColor = (m_props->getColorDef(PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT, 0xA08000) & 0xFFFFFF);
    h.correctionColor = (m_props->getColorDef(PROP_HIGHLIGHT_BOOKMARK_COLOR_CORRECTION, 0xA00000) & 0xFFFFFF);
    m_doc->setHightlightOptions(h);
}

void LVDocView::Render(int dx, int dy, LVRendPageList * pages) {
	LVLock lock(getMutex());
	{
		if (!m_doc || m_doc->getRootNode() == NULL)
			return;

		if (dx == 0)
			dx = m_pageRects[0].width() - m_pageMargins.left
					- m_pageMargins.right;
		if (dy == 0)
			dy = m_pageRects[0].height() - m_pageMargins.top
					- m_pageMargins.bottom - getPageHeaderHeight();

		setRenderProps(dx, dy);

		if (pages == NULL)
			pages = &m_pages;

		if (!m_font || !m_infoFont)
			return;

        CRLog::debug("Render(width=%d, height=%d, fontSize=%d, currentFontSize=%d, 0 char width=%d)", dx, dy,
                     m_font_size, m_font->getSize(), m_font->getCharWidth('0'));
		//CRLog::trace("calling render() for document %08X font=%08X", (unsigned int)m_doc, (unsigned int)m_font.get() );
		bool did_rerender = m_doc->render(pages, isDocumentOpened() ? m_callback : NULL, dx, dy,
					m_showCover, m_showCover ? dy + m_pageMargins.bottom * 4 : 0,
					m_font, m_def_interline_space, m_props,
					m_pageMargins.left, m_pageMargins.right);

#if 0
                // For debugging lvpagesplitter.cpp (small books)
                for (int i=0; i<m_pages.length(); i++) {
                    printf("%4d:   %7d .. %-7d [%d]\n", i, m_pages[i]->start, m_pages[i]->start+m_pages[i]->height, m_pages[i]->height);
                }
#endif

#if 0
                // For debugging lvpagesplitter.cpp (larger books)
		FILE * f = fopen("pagelist.log", "wt" STDIO_CLOEXEC);
		if (f) {
			for (int i=0; i<m_pages.length(); i++)
			{
				fprintf(f, "%4d:   %7d .. %-7d [%d]\n", i, m_pages[i].start, m_pages[i].start+m_pages[i].height, m_pages[i].height);
			}
			fclose(f);
		}
#endif
		if ( did_rerender ) {
			m_section_bounds_valid = false;
			fontMan->gc();
		}
		m_is_rendered = true;
		//CRLog::debug("Making TOC...");
		//makeToc();
		CRLog::debug("Updating selections...");
		updateSelections();
		CRLog::debug("Render is finished");

		if (!m_swapDone) {
			int fs = m_doc_props->getIntDef(DOC_PROP_FILE_SIZE, 0);
			int mfs = m_props->getIntDef(PROP_MIN_FILE_SIZE_TO_CACHE,
					DOCUMENT_CACHING_SIZE_THRESHOLD);
			CRLog::info(
					"Check whether to swap: file size = %d, min size to cache = %d",
					fs, mfs);
			if (fs >= mfs) {
                CRTimerUtil timeout(100); // 0.1 seconds
                swapToCache(timeout);
                m_swapDone = true;
            }
		}

        updateBookMarksRanges();
	}
}

/// Return a hash accounting for the rendering and the pages layout
/// A changed hash let frontends know their cached values of some document
/// properties (full height, TOC pages...) may have changed and that they
/// need to fetch them again
lUInt32 LVDocView::getDocumentRenderingHash() {
    if (m_doc) {
        // Also account for the number of pages, as toggling m_twoVisiblePagesAsOnePageNumber
        // does not change the document rendering hash, but it does change page numbers
        // Also account for the document height, just to be sure
        return ((( (lUInt32)m_doc->getDocumentRenderingHash()) * 31
                 + (lUInt32)m_doc->getFullHeight()) * 31
                 + (lUInt32)getPageCount());
    }
    return 0;
}

/// sets selection for whole element, clears previous selection
void LVDocView::selectElement(ldomNode * elem) {
	ldomXRangeList & sel = getDocument()->getSelections();
	sel.clear();
	sel.add(new ldomXRange(elem));
	updateSelections();
}

/// sets selection for list of words, clears previous selection
void LVDocView::selectWords(const LVArray<ldomWord> & words) {
	ldomXRangeList & sel = getDocument()->getSelections();
	sel.clear();
	sel.addWords(words);
	updateSelections();
}

/// sets selections for ranges, clears previous selections
void LVDocView::selectRanges(ldomXRangeList & ranges) {
    ldomXRangeList & sel = getDocument()->getSelections();
    if (sel.empty() && ranges.empty())
        return;
    sel.clear();
    for (int i=0; i<ranges.length(); i++) {
        ldomXRange * item = ranges[i];
        sel.add(new ldomXRange(*item));
    }
    updateSelections();
}

/// sets selection for range, clears previous selection
void LVDocView::selectRange(const ldomXRange & range) {
    // LVE:DEBUG
//    ldomXRange range2(range);
//    CRLog::trace("selectRange( %s, %s )", LCSTR(range2.getStart().toString()), LCSTR(range2.getEnd().toString()) );
	ldomXRangeList & sel = getDocument()->getSelections();
	if (sel.length() == 1) {
		if (range == *sel[0])
			return; // the same range is set
	}
	sel.clear();
	sel.add(new ldomXRange(range));
	updateSelections();
}

/// clears selection
void LVDocView::clearSelection() {
	ldomXRangeList & sel = getDocument()->getSelections();
	sel.clear();
	updateSelections();
}

/// selects first link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectFirstPageLink() {
	ldomXRangeList list;
	getCurrentPageLinks(list);
	if (!list.length())
		return NULL;
	//
	selectRange(*list[0]);
	//
	ldomXRangeList & sel = getDocument()->getSelections();
	updateSelections();
	return sel[0];
}

/// selects link on page, if any (delta==0 - current, 1-next, -1-previous). returns selected link range, null if no links.
ldomXRange * LVDocView::selectPageLink(int delta, bool wrapAround) {
	ldomXRangeList & sel = getDocument()->getSelections();
	ldomXRangeList list;
	getCurrentPageLinks(list);
	if (!list.length())
		return NULL;
	int currentLinkIndex = -1;
	if (sel.length() > 0) {
		ldomNode * currSel = sel[0]->getStart().getNode();
		for (int i = 0; i < list.length(); i++) {
			if (list[i]->getStart().getNode() == currSel) {
				currentLinkIndex = i;
				break;
			}
		}
	}
	bool error = false;
	if (delta == 1) {
		// next
		currentLinkIndex++;
		if (currentLinkIndex >= list.length()) {
			if (wrapAround)
				currentLinkIndex = 0;
			else
				error = true;
		}

	} else if (delta == -1) {
		// previous
		if (currentLinkIndex == -1)
			currentLinkIndex = list.length() - 1;
		else
			currentLinkIndex--;
		if (currentLinkIndex < 0) {
			if (wrapAround)
				currentLinkIndex = list.length() - 1;
			else
				error = true;
		}
	} else {
		// current
		if (currentLinkIndex < 0 || currentLinkIndex >= list.length())
			error = true;
	}
	if (error) {
		clearSelection();
		return NULL;
	}
	//
	selectRange(*list[currentLinkIndex]);
	//
	updateSelections();
	return sel[0];
}

/// selects next link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectNextPageLink(bool wrapAround) {
	return selectPageLink(+1, wrapAround);
}

/// selects previous link on page, if any. returns selected link range, null if no links.
ldomXRange * LVDocView::selectPrevPageLink(bool wrapAround) {
	return selectPageLink(-1, wrapAround);
}

/// returns selected link on page, if any. null if no links.
ldomXRange * LVDocView::getCurrentPageSelectedLink() {
	return selectPageLink(0, false);
}

/// get document rectangle for specified cursor position, returns false if not visible
bool LVDocView::getCursorDocRect(ldomXPointer ptr, lvRect & rc) {
	rc.clear();
	if (ptr.isNull())
		return false;
	if (!ptr.getRect(rc)) {
		rc.clear();
		return false;
	}
	return true;
}

/// get screen rectangle for specified cursor position, returns false if not visible
bool LVDocView::getCursorRect(ldomXPointer ptr, lvRect & rc,
		bool scrollToCursor) {
	if (!getCursorDocRect(ptr, rc))
		return false;
	for (;;) {

		lvPoint topLeft = rc.topLeft();
		lvPoint bottomRight = rc.bottomRight();
		if (docToWindowPoint(topLeft) && docToWindowPoint(bottomRight, true)) {
			rc.setTopLeft(topLeft);
			rc.setBottomRight(bottomRight);
			return true;
		}
		// try to scroll and convert doc->window again
		if (!scrollToCursor)
			break;
		// scroll
		goToBookmark(ptr);
		scrollToCursor = false;
	};
	rc.clear();
	return false;
}

/// follow link, returns true if navigation was successful
bool LVDocView::goLink(lString32 link, bool savePos) {
	CRLog::debug("goLink(%s)", LCSTR(link));
	ldomNode * element = NULL;
	if (link.empty()) {
		ldomXRange * node = LVDocView::getCurrentPageSelectedLink();
		if (node) {
			link = node->getHRef();
			ldomNode * p = node->getStart().getNode();
			if (p->isText())
				p = p->getParentNode();
			element = p;
		}
		if (link.empty())
			return false;
	}
	if (link[0] != '#' || link.length() <= 1) {
		lString32 filename = link;
		lString32 id;
        int p = filename.pos("#");
		if (p >= 0) {
			// split filename and anchor
			// part1.html#chapter3 =>   part1.html & chapter3
			id = filename.substr(p + 1);
			filename = filename.substr(0, p);
		}
        if (filename.pos(":") >= 0) {
			// URL with protocol like http://
			if (m_callback) {
				m_callback->OnExternalLink(link, element);
				return true;
			}
		} else {
			// otherwise assume link to another file
			CRLog::debug("Link to another file: %s   anchor=%s", UnicodeToUtf8(
					filename).c_str(), UnicodeToUtf8(id).c_str());

			lString32 baseDir = m_doc_props->getStringDef(DOC_PROP_FILE_PATH,
					".");
			LVAppendPathDelimiter(baseDir);
			lString32 fn = m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "");
			CRLog::debug("Current path: %s   filename:%s", UnicodeToUtf8(
					baseDir).c_str(), UnicodeToUtf8(fn).c_str());
			baseDir = LVExtractPath(baseDir + fn);
			//lString32 newPathName = LVMakeRelativeFilename( baseDir, filename );
			lString32 newPathName = LVCombinePaths(baseDir, filename);
			lString32 dir = LVExtractPath(newPathName);
			lString32 filename = LVExtractFilename(newPathName);
			LVContainerRef container = m_container;
			lString32 arcname =
					m_doc_props->getStringDef(DOC_PROP_ARC_NAME, "");
			if (arcname.empty()) {
				container = LVOpenDirectory(dir.c_str());
				if (container.isNull())
					return false;
			} else {
				filename = newPathName;
				dir.clear();
			}
			CRLog::debug("Base dir: %s newPathName=%s",
					UnicodeToUtf8(baseDir).c_str(),
					UnicodeToUtf8(newPathName).c_str());

			LVStreamRef stream = container->OpenStream(filename.c_str(),
					LVOM_READ);
			if (stream.isNull()) {
				CRLog::error("Go to link: cannot find file %s", UnicodeToUtf8(
						filename).c_str());
				return false;
			}
			CRLog::info("Go to link: file %s is found",
					UnicodeToUtf8(filename).c_str());
			// return point
			if (savePos)
				savePosToNavigationHistory();

			// close old document
			savePosition();
			clearSelection();
			_posBookmark = ldomXPointer();
			m_is_rendered = false;
			m_swapDone = false;
			_pos = 0;
			_page = 0;
			m_section_bounds_valid = false;
			m_doc_props->setString(DOC_PROP_FILE_PATH, dir);
			m_doc_props->setString(DOC_PROP_FILE_NAME, filename);
			m_doc_props->setString(DOC_PROP_CODE_BASE, LVExtractPath(filename));
			m_doc_props->setString(DOC_PROP_FILE_SIZE, lString32::itoa(
					(int) stream->GetSize()));
            m_doc_props->setHex(DOC_PROP_FILE_CRC32, stream->getcrc32());
			// TODO: load document from stream properly
			if (!LoadDocument(stream)) {
                createDefaultDocument(cs32("Load error"), lString32(
                        "Cannot open file ") + filename);
				return false;
			}
			//m_filename = newPathName;
			m_stream = stream;
			m_container = container;

			//restorePosition();

			// TODO: setup properties
			// go to anchor
			if (!id.empty())
                goLink(cs32("#") + id);
			clearImageCache();
			requestRender();
			return true;
		}
		return false; // only internal links supported (started with #)
	}
	link = link.substr(1, link.length() - 1);
	lUInt32 id = m_doc->getAttrValueIndex(link.c_str());
	ldomNode * dest = m_doc->getNodeById(id);
	if (!dest)
		return false;
	savePosToNavigationHistory();
	ldomXPointer newPos(dest, 0);
	goToBookmark(newPos);
        updateBookMarksRanges();
	return true;
}

/// follow selected link, returns true if navigation was successful
bool LVDocView::goSelectedLink() {
	ldomXRange * link = getCurrentPageSelectedLink();
	if (!link)
		return false;
	lString32 href = link->getHRef();
	if (href.empty())
		return false;
	return goLink(href);
}

#define NAVIGATION_FILENAME_SEPARATOR U":"
bool splitNavigationPos(lString32 pos, lString32 & fname, lString32 & path) {
	int p = pos.pos(lString32(NAVIGATION_FILENAME_SEPARATOR));
	if (p <= 0) {
        fname = lString32::empty_str;
		path = pos;
		return false;
	}
	fname = pos.substr(0, p);
	path = pos.substr(p + 1);
	return true;
}

/// packs current file path and name
lString32 LVDocView::getNavigationPath() {
	lString32 fname = m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "");
	lString32 fpath = m_doc_props->getStringDef(DOC_PROP_FILE_PATH, "");
	LVAppendPathDelimiter(fpath);
	lString32 s = fpath + fname;
	if (!m_arc.isNull())
        s = cs32("/") + s;
	return s;
}

/// saves position to navigation history, to be able return back
bool LVDocView::savePosToNavigationHistory(lString32 path) {
    if (!path.empty()) {
        lString32 s = getNavigationPath() + NAVIGATION_FILENAME_SEPARATOR
                + path;
        CRLog::debug("savePosToNavigationHistory(%s)",
                UnicodeToUtf8(s).c_str());
        return _navigationHistory.save(s);
    }
    return false;
}

bool LVDocView::savePosToNavigationHistory() {
	ldomXPointer bookmark = getBookmark();
	if (!bookmark.isNull()) {
		lString32 path = bookmark.toString();
        return savePosToNavigationHistory(path);
	}
	return false;
}

/// navigate to history path URL
bool LVDocView::navigateTo(lString32 historyPath) {
	CRLog::debug("navigateTo(%s)", LCSTR(historyPath));
	lString32 fname, path;
	if (splitNavigationPos(historyPath, fname, path)) {
		lString32 curr_fname = getNavigationPath();
		if (curr_fname != fname) {
			CRLog::debug(
					"navigateTo() : file name doesn't match: current=%s %s, new=%s %s",
					LCSTR(curr_fname), LCSTR(fname));
			if (!goLink(fname, false))
				return false;
		}
	}
	if (path.empty())
		return false;
	ldomXPointer bookmark = m_doc->createXPointer(path);
	if (bookmark.isNull())
		return false;
	goToBookmark(bookmark);
    updateBookMarksRanges();
	return true;
}

/// go back. returns true if navigation was successful
bool LVDocView::canGoBack() {
    return _navigationHistory.backCount() > 0;
}

/// go forward. returns true if navigation was successful
bool LVDocView::canGoForward() {
    return _navigationHistory.forwardCount() > 0;
}

/// go back. returns true if navigation was successful
bool LVDocView::goBack() {
	if (_navigationHistory.forwardCount() == 0 && savePosToNavigationHistory())
		_navigationHistory.back();
	lString32 s = _navigationHistory.back();
	if (s.empty())
		return false;
	return navigateTo(s);
}

/// go forward. returns true if navigation was successful
bool LVDocView::goForward() {
	lString32 s = _navigationHistory.forward();
	if (s.empty())
		return false;
	return navigateTo(s);
}

/// update selection ranges
void LVDocView::updateSelections() {
    CHECK_RENDER("updateSelections()")
	clearImageCache();
	LVLock lock(getMutex());
	ldomXRangeList ranges(m_doc->getSelections(), true);
    CRLog::trace("updateSelections() : selection count = %d", m_doc->getSelections().length());
	ranges.getRanges(m_markRanges);
	if (m_markRanges.length() > 0) {
//		crtrace trace;
//		trace << "LVDocView::updateSelections() - " << "selections: "
//				<< m_doc->getSelections().length() << ", ranges: "
//				<< ranges.length() << ", marked ranges: "
//				<< m_markRanges.length() << " ";
//		for (int i = 0; i < m_markRanges.length(); i++) {
//			ldomMarkedRange * item = m_markRanges[i];
//			trace << "(" << item->start.x << "," << item->start.y << "--"
//					<< item->end.x << "," << item->end.y << " #" << item->flags
//					<< ") ";
//		}
	}
}

void LVDocView::updateBookMarksRanges()
{
    checkRender();
    LVLock lock(getMutex());
    clearImageCache();

    ldomXRangeList ranges;
    CRFileHistRecord * rec = m_highlightBookmarks ? getCurrentFileHistRecord() : NULL;
    if (rec) {
        LVPtrVector<CRBookmark> &bookmarks = rec->getBookmarks();
        for (int i = 0; i < bookmarks.length(); i++) {
            CRBookmark * bmk = bookmarks[i];
            int t = bmk->getType();
            if (t != bmkt_lastpos) {
                ldomXPointer p = m_doc->createXPointer(bmk->getStartPos());
                if (p.isNull())
                    continue;
                lvPoint pt = p.toPoint();
                if (pt.y < 0)
                    continue;
                ldomXPointer ep = (t == bmkt_pos) ? p : m_doc->createXPointer(bmk->getEndPos());
                if (ep.isNull())
                    continue;
                lvPoint ept = ep.toPoint();
                if (ept.y < 0)
                    continue;
                ldomXRange *n_range = new ldomXRange(p, ep);
                if (!n_range->isNull()) {
                    int flags = 1;
                    if (t == bmkt_pos)
                        flags = 2;
                    if (t == bmkt_comment)
                        flags = 4;
                    if (t == bmkt_correction)
                        flags = 8;
                    n_range->setFlags(flags);
                    ranges.add(n_range);
                } else
                    delete n_range;
            }
        }
    }
    ranges.getRanges(m_bmkRanges);
#if 0

    m_bookmarksPercents.clear();
    if (m_highlightBookmarks) {
        CRFileHistRecord * rec = getCurrentFileHistRecord();
        if (rec) {
            LVPtrVector < CRBookmark > &bookmarks = rec->getBookmarks();

            m_bookmarksPercents.reserve(m_pages.length());
            for (int i = 0; i < bookmarks.length(); i++) {
                CRBookmark * bmk = bookmarks[i];
                if (bmk->getType() != bmkt_comment && bmk->getType() != bmkt_correction)
                    continue;
                lString32 pos = bmk->getStartPos();
                ldomXPointer p = m_doc->createXPointer(pos);
                if (p.isNull())
                    continue;
                lvPoint pt = p.toPoint();
                if (pt.y < 0)
                    continue;
                ldomXPointer ep = m_doc->createXPointer(bmk->getEndPos());
                if (ep.isNull())
                    continue;
                lvPoint ept = ep.toPoint();
                if (ept.y < 0)
                    continue;
                insertBookmarkPercentInfo(m_pages.FindNearestPage(pt.y, 0),
                                          ept.y, bmk->getPercent());
            }
        }
    }

    ldomXRangeList ranges;
    CRFileHistRecord * rec = m_bookmarksPercents.length() ? getCurrentFileHistRecord() : NULL;
    if (!rec) {
        m_bmkRanges.clear();
        return;
    }
    int page_index = getCurPage(true);
    if (page_index >= 0 && page_index < m_bookmarksPercents.length()) {
        LVPtrVector < CRBookmark > &bookmarks = rec->getBookmarks();
        LVRef < ldomXRange > page = getPageDocumentRange();
        LVBookMarkPercentInfo *bmi = m_bookmarksPercents[page_index];
        for (int i = 0; bmi != NULL && i < bmi->length(); i++) {
            for (int j = 0; j < bookmarks.length(); j++) {
                CRBookmark * bmk = bookmarks[j];
                if ((bmk->getType() != bmkt_comment && bmk->getType() != bmkt_correction) ||
                    bmk->getPercent() != bmi->get(i))
                    continue;
                lString32 epos = bmk->getEndPos();
                ldomXPointer ep = m_doc->createXPointer(epos);
                if (!ep.isNull()) {
                    lString32 spos = bmk->getStartPos();
                    ldomXPointer sp = m_doc->createXPointer(spos);
                    if (!sp.isNull()) {
                        ldomXRange bmk_range(sp, ep);

                        ldomXRange *n_range = new ldomXRange(*page, bmk_range);
                        if (!n_range->isNull())
                            ranges.add(n_range);
                        else
                            delete n_range;
                    }
                }
            }
        }
    }
    ranges.getRanges(m_bmkRanges);
#endif
}

/// set view mode (pages/scroll)
void LVDocView::setViewMode(LVDocViewMode view_mode, int visiblePageCount) {
    //CRLog::trace("setViewMode(%d, %d) currMode=%d currPages=%d", (int)view_mode, visiblePageCount, m_view_mode, m_pagesVisible);
	if (m_view_mode == view_mode && (visiblePageCount == m_pagesVisible
			|| visiblePageCount < 1))
		return;
	clearImageCache();
	LVLock lock(getMutex());
	m_view_mode = view_mode;
	m_props->setInt(PROP_PAGE_VIEW_MODE, m_view_mode == DVM_PAGES ? 1 : 0);
    if (visiblePageCount == 1 || visiblePageCount == 2) {
		m_pagesVisible = visiblePageCount;
        m_props->setInt(PROP_LANDSCAPE_PAGES, m_pagesVisible);
    }
    updateLayout();
    REQUEST_RENDER("setViewMode")
    _posIsSet = false;

//	goToBookmark( _posBookmark);
//        updateBookMarksRanges();
}

/// get view mode (pages/scroll)
LVDocViewMode LVDocView::getViewMode() {
	return m_view_mode;
}

/// toggle pages/scroll view mode
void LVDocView::toggleViewMode() {
	if (m_view_mode == DVM_SCROLL)
		setViewMode( DVM_PAGES);
	else
		setViewMode( DVM_SCROLL);

}

int LVDocView::getVisiblePageCount() {
    if ( m_pagesVisible == 1 ) // No check needed when set to 1
        return 1;
    if ( m_view_mode == DVM_SCROLL ) // Can't do 2-pages in scroll mode
        return 1;
    if ( m_pagesVisible_onlyIfSane ) {
        // Not if this would make only a few chars per page (20 / 2)
        if ( m_dx < m_font_size * MIN_EM_PER_PAGE )
            return 1;
        // Not if aspect ratio w/h < 6/5
        if ( m_dx * 5 < m_dy * 6 )
            return 1;
    }
    // Allow 2-pages if it has been set
    return m_pagesVisible;
}

/// set window visible page count (1 or 2)
void LVDocView::setVisiblePageCount(int n, bool onlyIfSane) {
    //CRLog::trace("setVisiblePageCount(%d) currPages=%d", n, m_pagesVisible);
    clearImageCache();
	LVLock lock(getMutex());
    m_pagesVisible_onlyIfSane = onlyIfSane;
    int newCount = (n == 2) ? 2 : 1;
    if (m_pagesVisible == newCount)
        return;
    m_pagesVisible = newCount;
	updateLayout();
    REQUEST_RENDER("setVisiblePageCount")
    _posIsSet = false;
}

#if USE_LIMITED_FONT_SIZES_SET
static int findBestFit(LVArray<int> & v, int n, bool rollCyclic = false) {
	int bestsz = -1;
	int bestfit = -1;
	if (rollCyclic) {
		if (n < v[0])
			return v[v.length() - 1];
		if (n > v[v.length() - 1])
			return v[0];
	}
	for (int i = 0; i < v.length(); i++) {
		int delta = v[i] - n;
		if (delta < 0)
			delta = -delta;
		if (bestfit == -1 || bestfit > delta) {
			bestfit = delta;
			bestsz = v[i];
		}
	}
	if (bestsz < 0)
		bestsz = n;
	return bestsz;
}
#endif

void LVDocView::setDefaultInterlineSpace(int percent) {
    LVLock lock(getMutex());
    REQUEST_RENDER("setDefaultInterlineSpace")
    m_def_interline_space = percent; // not used
    if (m_doc) {
        if (percent == 100) // (avoid any rounding issue)
            m_doc->setInterlineScaleFactor(INTERLINE_SCALE_FACTOR_NO_SCALE);
        else
            m_doc->setInterlineScaleFactor(INTERLINE_SCALE_FACTOR_NO_SCALE * percent / 100);
    }
    _posIsSet = false;
//	goToBookmark( _posBookmark);
//        updateBookMarksRanges();
}

/// sets new status bar font size
void LVDocView::setStatusFontSize(int newSize) {
	LVLock lock(getMutex());
	int oldSize = m_status_font_size;
	m_status_font_size = newSize;
	if (oldSize != newSize) {
		propsGetCurrent()->setInt(PROP_STATUS_FONT_SIZE, m_status_font_size);
		m_infoFont = fontMan->GetFont(m_status_font_size, 400, false,
			DEFAULT_FONT_FAMILY, m_statusFontFace);
        REQUEST_RENDER("setStatusFontSize")
	}
	//goToBookmark(_posBookmark);
}

int LVDocView::scaleFontSizeForDPI(int fontSize) {
    if (gRenderScaleFontWithDPI) {
        fontSize = scaleForRenderDPI(fontSize);
#if USE_LIMITED_FONT_SIZES_SET
        fontSize = findBestFit(m_font_sizes, fontSize);
#else
        if (fontSize < m_min_font_size)
            fontSize = m_min_font_size;
        else if (fontSize > m_max_font_size)
            fontSize = m_max_font_size;
#endif
    }
    return fontSize;
}

void LVDocView::setFontSize(int newSize) {
    LVLock lock(getMutex());

    // We don't scale m_requested_font_size itself, so font size and gRenderDPI
    // can be changed independantly.
    int oldSize = m_requested_font_size;
    if (oldSize != newSize) {
#if USE_LIMITED_FONT_SIZES_SET
        m_requested_font_size = findBestFit(m_font_sizes, newSize);
#else
        if (newSize < m_min_font_size)
            m_requested_font_size = m_min_font_size;
        else if (newSize > m_max_font_size)
            m_requested_font_size = m_max_font_size;
        else
            m_requested_font_size = newSize;
#endif
        propsGetCurrent()->setInt(PROP_FONT_SIZE, m_requested_font_size);
        m_font_size = scaleFontSizeForDPI(m_requested_font_size);
        CRLog::debug("New requested font size: %d (asked: %d)", m_requested_font_size, newSize);
        updateLayout(); // when 2 visible pages, the middle margin width depends on this font size
        REQUEST_RENDER("setFontSize")
    }
    //goToBookmark(_posBookmark);
}

void LVDocView::setDefaultFontFace(const lString8 & newFace) {
	m_defaultFontFace = newFace;
    REQUEST_RENDER("setDefaulFontFace")
}

void LVDocView::setStatusFontFace(const lString8 & newFace) {
	m_statusFontFace = newFace;
	m_infoFont = fontMan->GetFont(m_status_font_size, 400, false,
			DEFAULT_FONT_FAMILY, m_statusFontFace);
    REQUEST_RENDER("setStatusFontFace")
}

#if USE_LIMITED_FONT_SIZES_SET
/// sets posible base font sizes (for ZoomFont feature)
void LVDocView::setFontSizes(LVArray<int> & sizes, bool cyclic) {
	m_font_sizes = sizes;
	m_font_sizes_cyclic = cyclic;
}
#endif

#if !USE_LIMITED_FONT_SIZES_SET
void LVDocView::setMinFontSize( int size ) {
	m_min_font_size = size;
}

void LVDocView::setMaxFontSize( int size ) {
	m_max_font_size = size;
}
#endif

void LVDocView::ZoomFont(int delta) {
	if (m_font.isNull())
		return;
#if 1
#if USE_LIMITED_FONT_SIZES_SET
	int sz = m_requested_font_size;
	for (int i = 0; i < 15; i++) {
		sz += delta;
		int nsz = findBestFit(m_font_sizes, sz, m_font_sizes_cyclic);
		if (nsz != m_requested_font_size) {
			setFontSize(nsz);
			return;
		}
		if (sz < 12)
			break;
	}
#else
	setFontSize(m_requested_font_size + delta);
#endif
#else
	LVFontRef nfnt;
	int sz = m_font->getHeight();
	for (int i=0; i<15; i++)
	{
		sz += delta;
		nfnt = fontMan->GetFont( sz, 400, false, DEFAULT_FONT_FAMILY, lString8(DEFAULT_FONT_NAME) );
		if ( !nfnt.isNull() && nfnt->getHeight() != m_font->getHeight() )
		{
			// found!
			//ldomXPointer bm = getBookmark();
			m_requested_font_size = nfnt->getHeight();
			Render();
			goToBookmark(_posBookmark);
			return;
		}
		if (sz<12)
		break;
	}
#endif
}

/// sets current bookmark
void LVDocView::setBookmark(ldomXPointer bm) {
	_posBookmark = bm;
}

/// get view height
int LVDocView::GetHeight() {
#if CR_INTERNAL_PAGE_ORIENTATION==1
	return (m_rotateAngle & 1) ? m_dx : m_dy;
#else
	return m_dy;
#endif
}

/// get view width
int LVDocView::GetWidth() {
#if CR_INTERNAL_PAGE_ORIENTATION==1
	return (m_rotateAngle & 1) ? m_dy : m_dx;
#else
	return m_dx;
#endif
}

#if CR_INTERNAL_PAGE_ORIENTATION==1
/// sets rotate angle
void LVDocView::SetRotateAngle( cr_rotate_angle_t angle )
{
	if ( m_rotateAngle==angle )
	return;
	m_props->setInt( PROP_ROTATE_ANGLE, ((int)angle) & 3 );
	clearImageCache();
	LVLock lock(getMutex());
	if ( (m_rotateAngle & 1) == (angle & 1) ) {
		m_rotateAngle = angle;
		return;
	}
	m_rotateAngle = angle;
	int ndx = (angle&1) ? m_dx : m_dy;
	int ndy = (angle&1) ? m_dy : m_dx;
	Resize( ndx, ndy );
}
#endif

void LVDocView::Resize(int dx, int dy) {
	//LVCHECKPOINT("Resize");
	CRLog::trace("LVDocView:Resize(%dx%d)", dx, dy);
	if (dx < SCREEN_SIZE_MIN)
		dx = SCREEN_SIZE_MIN;
	else if (dx > SCREEN_SIZE_MAX)
		dx = SCREEN_SIZE_MAX;
	if (dy < SCREEN_SIZE_MIN)
		dy = SCREEN_SIZE_MIN;
	else if (dy > SCREEN_SIZE_MAX)
		dy = SCREEN_SIZE_MAX;
#if CR_INTERNAL_PAGE_ORIENTATION==1
	if ( m_rotateAngle==CR_ROTATE_ANGLE_90 || m_rotateAngle==CR_ROTATE_ANGLE_270 ) {
		CRLog::trace("Screen is rotated, swapping dimensions");
		int tmp = dx;
		dx = dy;
		dy = tmp;
	}
#endif

	if (dx == m_dx && dy == m_dy) {
		CRLog::trace("Size is not changed: %dx%d", dx, dy);
		return;
	}

	clearImageCache();
	//m_drawbuf.Resize(dx, dy);
	if (m_doc) {
		m_doc->setScreenSize(m_dx, m_dy); // only used for CSS @media queries
		//ldomXPointer bm = getBookmark();
		if (dx != m_dx || dy != m_dy || m_view_mode != DVM_SCROLL
				|| !m_is_rendered) {
			m_dx = dx;
			m_dy = dy;
			CRLog::trace("LVDocView:Resize() :  new size: %dx%d", dx, dy);
			updateLayout();
            REQUEST_RENDER("resize")
		}
        _posIsSet = false;
//		goToBookmark( _posBookmark);
//                updateBookMarksRanges();
	}
	m_dx = dx;
	m_dy = dy;
}

#define XS_IMPLEMENT_SCHEME 1
#include "../include/fb2def.h"

#if 0
void SaveBase64Objects( ldomNode * node )
{
	if ( !node->isElement() || node->getNodeId()!=el_binary )
	return;
	lString32 name = node->getAttributeValue(attr_id);
	if ( name.empty() )
	return;
	fprintf( stderr, "opening base64 stream...\n" );
	LVStreamRef in = node->createBase64Stream();
	if ( in.isNull() )
	return;
	fprintf( stderr, "base64 stream opened: %d bytes\n", (int)in->GetSize() );
	fprintf( stderr, "opening out stream...\n" );
	LVStreamRef outstream = LVOpenFileStream( name.c_str(), LVOM_WRITE );
	if (outstream.isNull())
	return;
	//outstream->Write( "test", 4, NULL );
	fprintf( stderr, "streams opened, copying...\n" );
	/*
	 lUInt8 dbuf[128000];
	 lvsize_t bytesRead = 0;
	 if ( in->Read( dbuf, 128000, &bytesRead )==LVERR_OK )
	 {
	 fprintf(stderr, "Read %d bytes, writing...\n", (int) bytesRead );
	 //outstream->Write( "test2", 5, NULL );
	 //outstream->Write( "test3", 5, NULL );
	 outstream->Write( dbuf, 100, NULL );
	 outstream->Write( dbuf, bytesRead, NULL );
	 //outstream->Write( "test4", 5, NULL );
	 }
	 */
	LVPumpStream( outstream, in );
	fprintf(stderr, "...\n");
}
#endif

/// returns pointer to bookmark/last position containter of currently opened file
CRFileHistRecord * LVDocView::getCurrentFileHistRecord() {
	if (m_filename.empty())
		return NULL;
	//CRLog::trace("LVDocView::getCurrentFileHistRecord()");
	//CRLog::trace("get title, authors, series");
	lString32 title = getTitle();
	lString32 authors = getAuthors();
	lString32 series = getSeries();
	//CRLog::trace("get bookmark");
	ldomXPointer bmk = getBookmark();
    lString32 fn = m_filename;
#ifdef ORIGINAL_FILENAME_PATCH
    if ( !m_originalFilename.empty() )
        fn = m_originalFilename;
#endif
    //CRLog::debug("m_hist.savePosition(%s, %d)", LCSTR(fn), m_filesize);
    CRFileHistRecord * res = m_hist.savePosition(fn, m_filesize, title,
			authors, series, bmk);
	//CRLog::trace("savePosition() returned");
	return res;
}

/// save last file position
void LVDocView::savePosition() {
	getCurrentFileHistRecord();
}

/// restore last file position
void LVDocView::restorePosition() {
	//CRLog::trace("LVDocView::restorePosition()");
	if (m_filename.empty())
		return;
	LVLock lock(getMutex());
	//checkRender();
    lString32 fn = m_filename;
#ifdef ORIGINAL_FILENAME_PATCH
    if ( !m_originalFilename.empty() )
        fn = m_originalFilename;
#endif
//    CRLog::debug("m_hist.restorePosition(%s, %d)", LCSTR(fn),
//			m_filesize);
    ldomXPointer pos = m_hist.restorePosition(m_doc, fn, m_filesize);
	if (!pos.isNull()) {
		//goToBookmark( pos );
		CRLog::info("LVDocView::restorePosition() - last position is found");
		_posBookmark = pos; //getBookmark();
                updateBookMarksRanges();
		_posIsSet = false;
	} else {
		CRLog::info(
				"LVDocView::restorePosition() - last position not found for file %s, size %d",
				UnicodeToUtf8(m_filename).c_str(), (int) m_filesize);
	}
}

static void FileToArcProps(CRPropRef props) {
	lString32 s = props->getStringDef(DOC_PROP_FILE_NAME);
	if (!s.empty())
		props->setString(DOC_PROP_ARC_NAME, s);
	s = props->getStringDef(DOC_PROP_FILE_PATH);
	if (!s.empty())
		props->setString(DOC_PROP_ARC_PATH, s);
	s = props->getStringDef(DOC_PROP_FILE_SIZE);
	if (!s.empty())
		props->setString(DOC_PROP_ARC_SIZE, s);
    props->setString(DOC_PROP_FILE_NAME, lString32::empty_str);
    props->setString(DOC_PROP_FILE_PATH, lString32::empty_str);
    props->setString(DOC_PROP_FILE_SIZE, lString32::empty_str);
	props->setHex(DOC_PROP_FILE_CRC32, 0);
}

/// load document from file
bool LVDocView::LoadDocument(const lChar32 * fname, bool metadataOnly) {
	if (!fname || !fname[0])
		return false;

	Clear();

    CRLog::debug("LoadDocument(%s) textMode=%s", LCSTR(lString32(fname)), getTextFormatOptions()==txt_format_pre ? "pre" : "autoformat");

	// split file path and name
	lString32 filename32(fname);

	lString32 arcPathName;
	lString32 arcItemPathName;
	bool isArchiveFile = LVSplitArcName(filename32, arcPathName,
			arcItemPathName);
	if (isArchiveFile) {
		// load from archive, using @/ separated arhive/file pathname
		CRLog::info("Loading document %s from archive %s", LCSTR(
				arcItemPathName), LCSTR(arcPathName));
		LVStreamRef stream = LVOpenFileStream(arcPathName.c_str(), LVOM_READ);
		int arcsize = 0;
		if (stream.isNull()) {
			CRLog::error("Cannot open archive file %s", LCSTR(arcPathName));
			return false;
		}
		arcsize = (int) stream->GetSize();
		m_container = LVOpenArchieve(stream);
		if (m_container.isNull()) {
			CRLog::error("Cannot read archive contents from %s", LCSTR(
					arcPathName));
			return false;
		}
		stream = m_container->OpenStream(arcItemPathName.c_str(), LVOM_READ);
		if (stream.isNull()) {
			CRLog::error("Cannot open archive file item stream %s", LCSTR(
					filename32));
			return false;
		}

		lString32 fn = LVExtractFilename(arcPathName);
		lString32 dir = LVExtractPath(arcPathName);

		m_doc_props->setString(DOC_PROP_ARC_NAME, fn);
		m_doc_props->setString(DOC_PROP_ARC_PATH, dir);
		m_doc_props->setString(DOC_PROP_ARC_SIZE, lString32::itoa(arcsize));
		m_doc_props->setString(DOC_PROP_FILE_SIZE, lString32::itoa(
				(int) stream->GetSize()));
		m_doc_props->setString(DOC_PROP_FILE_NAME, arcItemPathName);
        m_doc_props->setHex(DOC_PROP_FILE_CRC32, stream->getcrc32());
		// loading document
		if (LoadDocument(stream, metadataOnly)) {
			m_filename = lString32(fname);
			m_stream.Clear();
			return true;
		}
		m_stream.Clear();
		return false;
	}

	lString32 fn = LVExtractFilename(filename32);
	lString32 dir = LVExtractPath(filename32);

	CRLog::info("Loading document %s : fn=%s, dir=%s", LCSTR(filename32),
			LCSTR(fn), LCSTR(dir));
#if 0
	int i;
	int last_slash = -1;
	lChar32 slash_char = 0;
	for ( i=0; fname[i]; i++ ) {
		if ( fname[i]=='\\' || fname[i]=='/' ) {
			last_slash = i;
			slash_char = fname[i];
		}
	}
	lString32 dir;
	if ( last_slash==-1 )
        dir = ".";
	else if ( last_slash == 0 )
        dir << slash_char;
	else
        dir = lString32( fname, last_slash );
	lString32 fn( fname + last_slash + 1 );
#endif

	m_doc_props->setString(DOC_PROP_FILE_PATH, dir);
	m_container = LVOpenDirectory(dir.c_str());
	if (m_container.isNull())
		return false;
	LVStreamRef stream = m_container->OpenStream(fn.c_str(), LVOM_READ);
	if (!stream)
		return false;
    m_doc_props->setString(DOC_PROP_FILE_NAME, fn);
	m_doc_props->setString(DOC_PROP_FILE_SIZE, lString32::itoa(
			(int) stream->GetSize()));
    m_doc_props->setHex(DOC_PROP_FILE_CRC32, stream->getcrc32());

	if (LoadDocument(stream, metadataOnly)) {
		m_filename = lString32(fname);
		m_stream.Clear();

#define DUMP_OPENED_DOCUMENT_SENTENCES 0 // debug XPointer navigation
#if DUMP_OPENED_DOCUMENT_SENTENCES==1
        LVStreamRef out = LVOpenFileStream("/tmp/sentences.txt", LVOM_WRITE);
        if ( !out.isNull() ) {
            checkRender();
            {
                ldomXPointerEx ptr( m_doc->getRootNode(), m_doc->getRootNode()->getChildCount());
                *out << "FORWARD ORDER:\n\n";
                //ptr.nextVisibleText();
                ptr.prevVisibleWordEnd();
                if ( ptr.thisSentenceStart() ) {
                    while ( 1 ) {
                        ldomXPointerEx ptr2(ptr);
                        ptr2.thisSentenceEnd();
                        ldomXRange range(ptr, ptr2);
                        lString32 str = range.getRangeText();
                        *out << ">sentence: " << UnicodeToUtf8(str) << "\n";
                        if ( !ptr.nextSentenceStart() )
                            break;
                    }
                }
            }
            {
                ldomXPointerEx ptr( m_doc->getRootNode(), 1);
                *out << "\n\nBACKWARD ORDER:\n\n";
                while ( ptr.lastChild() )
                    ;// do nothing
                if ( ptr.thisSentenceStart() ) {
                    while ( 1 ) {
                        ldomXPointerEx ptr2(ptr);
                        ptr2.thisSentenceEnd();
                        ldomXRange range(ptr, ptr2);
                        lString32 str = range.getRangeText();
                        *out << "<sentence: " << UnicodeToUtf8(str) << "\n";
                        if ( !ptr.prevSentenceStart() )
                            break;
                    }
                }
            }
        }
#endif

		return true;
	}
	m_stream.Clear();
	return false;
}

void LVDocView::close() {
    if ( m_doc )
        m_doc->updateMap(m_callback); // show save cache file progress
    createDefaultDocument(lString32::empty_str, lString32::empty_str);
}


/// create empty document with specified message (to show errors)
void LVDocView::createHtmlDocument(lString32 code) {
    Clear();
    m_showCover = false;
    createEmptyDocument();

    //ldomDocumentWriter writer(m_doc);
    ldomDocumentWriterFilter writerFilter(m_doc, false,
            HTML_AUTOCLOSE_TABLE);

    _pos = 0;
    _page = 0;


    lString8 s = UnicodeToUtf8(lString32(U"\xFEFF<html><body>") + code + "</body>");
    setDocFormat( doc_format_html);
    LVStreamRef stream = LVCreateMemoryStream();
    stream->Write(s.c_str(), s.length(), NULL);
    stream->SetPos(0);
    LVHTMLParser parser(stream, &writerFilter);
    if (!parser.CheckFormat()) {
        // error - cannot parse
    } else {
        parser.Parse();
    }
    REQUEST_RENDER("resize")
}

void LVDocView::createDefaultDocument(lString32 title, lString32 message) {
    Clear();
    m_showCover = false;
    createEmptyDocument();

    ldomDocumentWriter writer(m_doc);
    lString32Collection lines;
    lines.split(message, lString32("\n"));

    _pos = 0;
    _page = 0;

    // make fb2 document structure
    writer.OnTagOpen(NULL, U"?xml");
    writer.OnAttribute(NULL, U"version", U"1.0");
    writer.OnAttribute(NULL, U"encoding", U"utf-8");
    writer.OnEncoding(U"utf-8", NULL);
    writer.OnTagBody();
    writer.OnTagClose(NULL, U"?xml");
    writer.OnTagOpenNoAttr(NULL, U"FictionBook");
    // DESCRIPTION
    writer.OnTagOpenNoAttr(NULL, U"description");
    writer.OnTagOpenNoAttr(NULL, U"title-info");
    writer.OnTagOpenNoAttr(NULL, U"book-title");
    writer.OnTagOpenNoAttr(NULL, U"lang");
    writer.OnText(title.c_str(), title.length(), 0);
    writer.OnTagClose(NULL, U"book-title");
    writer.OnTagOpenNoAttr(NULL, U"title-info");
    writer.OnTagClose(NULL, U"description");
    // BODY
    writer.OnTagOpenNoAttr(NULL, U"body");
    //m_callback->OnTagOpen( NULL, U"section" );
    // process text
    if (title.length()) {
        writer.OnTagOpenNoAttr(NULL, U"title");
        writer.OnTagOpenNoAttr(NULL, U"p");
        writer.OnText(title.c_str(), title.length(), 0);
        writer.OnTagClose(NULL, U"p");
        writer.OnTagClose(NULL, U"title");
    }
    lString32Collection messageLines;
    messageLines.split(message, lString32("\n"));
    for (int i = 0; i < messageLines.length(); i++) {
        writer.OnTagOpenNoAttr(NULL, U"p");
        writer.OnText(messageLines[i].c_str(), messageLines[i].length(), 0);
        writer.OnTagClose(NULL, U"p");
    }
    //m_callback->OnTagClose( NULL, U"section" );
    writer.OnTagClose(NULL, U"body");
    writer.OnTagClose(NULL, U"FictionBook");

    // set stylesheet
    updateDocStyleSheet();
    //m_doc->getStyleSheet()->clear();
    //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());

    m_doc_props->clear();
    m_doc->setProps(m_doc_props);

    m_doc_props->setString(DOC_PROP_TITLE, title);

    REQUEST_RENDER("resize")
}

/// load document from stream
bool LVDocView::LoadDocument(LVStreamRef stream, bool metadataOnly) {


	m_swapDone = false;

	setRenderProps(0, 0); // to allow apply styles and rend method while loading

	if (m_callback) {
		m_callback->OnLoadFileStart(m_doc_props->getStringDef(
				DOC_PROP_FILE_NAME, ""));
	}
	LVLock lock(getMutex());

//    int pdbFormat = 0;
//    LVStreamRef pdbStream = LVOpenPDBStream( stream, pdbFormat );
//    if ( !pdbStream.isNull() ) {
//        CRLog::info("PDB format detected, stream size=%d", (int)pdbStream->GetSize() );
//        LVStreamRef out = LVOpenFileStream("/tmp/pdb.txt", LVOM_WRITE);
//        if ( !out.isNull() )
//            LVPumpStream(out.get(), pdbStream.get()); // DEBUG
//        stream = pdbStream;
//        //return false;
//    }

	{
		clearImageCache();
		m_filesize = stream->GetSize();
		m_stream = stream;

#if (USE_ZLIB==1)

        doc_format_t pdbFormat = doc_format_none;
        if ( DetectPDBFormat(m_stream, pdbFormat) ) {
            // PDB
            CRLog::info("PDB format detected");
            createEmptyDocument();
            m_doc->setProps( m_doc_props );
            setRenderProps( 0, 0 ); // to allow apply styles and rend method while loading
            setDocFormat( pdbFormat );
            if ( m_callback )
                m_callback->OnLoadFileFormatDetected(pdbFormat);
            updateDocStyleSheet();
            doc_format_t contentFormat = doc_format_none;
            bool res = ImportPDBDocument( m_stream, m_doc, m_callback, this, contentFormat );
            if ( !res ) {
                setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading PDB format"), cs32("Cannot open document") );
                if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading PDB document") );
                }
                return false;
            } else {
                setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
                if ( m_callback ) {
                    m_callback->OnLoadFileEnd( );
                    //m_doc->compact();
                    m_doc->dumpStatistics();
                }
                return true;
            }
        }


		if ( DetectEpubFormat( m_stream ) ) {
			// EPUB
			CRLog::info("EPUB format detected");
			createEmptyDocument();
            m_doc->setProps( m_doc_props );
			setRenderProps( 0, 0 ); // to allow apply styles and rend method while loading
			setDocFormat( doc_format_epub );
			if ( m_callback )
                m_callback->OnLoadFileFormatDetected(doc_format_epub);
            updateDocStyleSheet();
            bool res = ImportEpubDocument( m_stream, m_doc, m_callback, this, metadataOnly );
			if ( !res ) {
				setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading EPUB format"), cs32("Cannot open document") );
				if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading EPUB document") );
				}
				return false;
			} else {
				m_container = m_doc->getContainer();
				m_doc_props = m_doc->getProps();
				setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
				if ( m_callback ) {
					m_callback->OnLoadFileEnd( );
					//m_doc->compact();
					m_doc->dumpStatistics();
				}
				m_arc = m_doc->getContainer();

#ifdef SAVE_COPY_OF_LOADED_DOCUMENT //def _DEBUG
        LVStreamRef ostream = LVOpenFileStream( "test_save_source.xml", LVOM_WRITE );
        m_doc->saveToStream( ostream, "utf-16" );
#endif

				return true;
			}
		}

        if( DetectFb3Format(m_stream) ) {
            CRLog::info("FB3 format detected");
            createEmptyDocument();
            m_doc->setProps( m_doc_props );
            setRenderProps( 0, 0 );
            setDocFormat( doc_format_fb3 );
            if ( m_callback )
                m_callback->OnLoadFileFormatDetected(doc_format_fb3);
            updateDocStyleSheet();
            bool res = ImportFb3Document( m_stream, m_doc, m_callback, this );
            if ( !res ) {
                setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading FB3 format"), cs32("Cannot open document") );
                if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading FB3 document") );
                }
                return false;
            } else {
                m_container = m_doc->getContainer();
                m_doc_props = m_doc->getProps();
                setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
                if ( m_callback ) {
                    m_callback->OnLoadFileEnd( );
                    //m_doc->compact();
                    m_doc->dumpStatistics();
                }
                m_arc = m_doc->getContainer();
                return true;
            }
        }

        if( DetectDocXFormat(m_stream) ) {
            CRLog::info("DOCX format detected");
            createEmptyDocument();
            m_doc->setProps( m_doc_props );
            setRenderProps( 0, 0 );
            setDocFormat( doc_format_docx );
            if ( m_callback )
                m_callback->OnLoadFileFormatDetected(doc_format_docx);
            updateDocStyleSheet();
            bool res = ImportDocXDocument( m_stream, m_doc, m_callback, this );
            if ( !res ) {
                setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading DOCX format"), cs32("Cannot open document") );
                if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading DOCX document") );
                }
                return false;
            } else {
                m_container = m_doc->getContainer();
                m_doc_props = m_doc->getProps();
                setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
                if ( m_callback ) {
                    m_callback->OnLoadFileEnd( );
                    //m_doc->compact();
                    m_doc->dumpStatistics();
                }
                m_arc = m_doc->getContainer();
                return true;
            }
        }

        if( DetectOpenDocumentFormat(m_stream) ) {
            CRLog::info("ODT format detected");
            createEmptyDocument();
            m_doc->setProps( m_doc_props );
            setRenderProps( 0, 0 );
            setDocFormat( doc_format_odt );
            if ( m_callback )
                m_callback->OnLoadFileFormatDetected(doc_format_odt);
            updateDocStyleSheet();
            bool res = ImportOpenDocument(m_stream, m_doc, m_callback, this );
            if ( !res ) {
                setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading DOCX format"), cs32("Cannot open document") );
                if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading DOCX document") );
                }
                return false;
            } else {
                m_container = m_doc->getContainer();
                m_doc_props = m_doc->getProps();
                setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
                if ( m_callback ) {
                    m_callback->OnLoadFileEnd( );
                    //m_doc->compact();
                    m_doc->dumpStatistics();
                }
                m_arc = m_doc->getContainer();
                return true;
            }
        }

#if CHM_SUPPORT_ENABLED==1
        if ( DetectCHMFormat( m_stream ) ) {
			// CHM
			CRLog::info("CHM format detected");
			createEmptyDocument();
			m_doc->setProps( m_doc_props );
			setRenderProps( 0, 0 ); // to allow apply styles and rend method while loading
			setDocFormat( doc_format_chm );
			if ( m_callback )
			m_callback->OnLoadFileFormatDetected(doc_format_chm);
            updateDocStyleSheet();
            bool res = ImportCHMDocument( m_stream, m_doc, m_callback, this );
			if ( !res ) {
				setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading CHM format"), cs32("Cannot open document") );
				if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading CHM document") );
				}
				return false;
			} else {
				setRenderProps( 0, 0 );
				requestRender();
				if ( m_callback ) {
					m_callback->OnLoadFileEnd( );
					//m_doc->compact();
					m_doc->dumpStatistics();
				}
				m_arc = m_doc->getContainer();
				return true;
			}
		}
#endif

#if ENABLE_ANTIWORD==1
        if ( DetectWordFormat( m_stream ) ) {
            // DOC
            CRLog::info("Word format detected");
            createEmptyDocument();
            m_doc->setProps( m_doc_props );
            setRenderProps( 0, 0 ); // to allow apply styles and rend method while loading
            setDocFormat( doc_format_doc );
            if ( m_callback )
                m_callback->OnLoadFileFormatDetected(doc_format_doc);
            updateDocStyleSheet();
            bool res = ImportWordDocument( m_stream, m_doc, m_callback, this );
            if ( !res ) {
                setDocFormat( doc_format_none );
                createDefaultDocument( cs32("ERROR: Error reading DOC format"), cs32("Cannot open document") );
                if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("Error reading DOC document") );
                }
                return false;
            } else {
                setRenderProps( 0, 0 );
                REQUEST_RENDER("loadDocument")
                if ( m_callback ) {
                    m_callback->OnLoadFileEnd( );
                    //m_doc->compact();
                    m_doc->dumpStatistics();
                }
                m_arc = m_doc->getContainer();
                return true;
            }
        }
#endif

        m_arc = LVOpenArchieve( m_stream );
		if (!m_arc.isNull())
		{
			m_container = m_arc;
			// archieve
			FileToArcProps( m_doc_props );
			m_container = m_arc;
			m_doc_props->setInt( DOC_PROP_ARC_FILE_COUNT, m_arc->GetObjectCount() );
			bool found = false;
			int htmCount = 0;
			int fb2Count = 0;
			int rtfCount = 0;
			int txtCount = 0;
			int fbdCount = 0;
            int pmlCount = 0;
			lString32 defHtml;
			lString32 firstGood;
			for (int i=0; i<m_arc->GetObjectCount(); i++)
			{
				const LVContainerItemInfo * item = m_arc->GetObjectInfo(i);
				if (item)
				{
					if ( !item->IsContainer() )
					{
						lString32 name( item->GetName() );
						CRLog::debug("arc item[%d] : %s", i, LCSTR(name) );
						lString32 s = name;
						s.lowercase();
						bool nameIsOk = true;
                        if ( s.endsWith(".htm") || s.endsWith(".html") ) {
							lString32 nm = LVExtractFilenameWithoutExtension( s );
                            if ( nm == "index" || nm == "default" )
							defHtml = name;
							htmCount++;
                        } else if ( s.endsWith(".fb2") ) {
							fb2Count++;
                        } else if ( s.endsWith(".rtf") ) {
							rtfCount++;
                        } else if ( s.endsWith(".txt") ) {
							txtCount++;
                        } else if ( s.endsWith(".pml") ) {
                            pmlCount++;
                        } else if ( s.endsWith(".fbd") ) {
							fbdCount++;
						} else {
							nameIsOk = false;
						}
						if ( nameIsOk ) {
							if ( firstGood.empty() )
                                firstGood = name;
						}
						if ( name.length() >= 5 )
						{
							name.lowercase();
							const lChar32 * pext = name.c_str() + name.length() - 4;
                            if (!lStr_cmp(pext, ".fb2"))
                                nameIsOk = true;
                            else if (!lStr_cmp(pext, ".txt"))
                                nameIsOk = true;
                            else if (!lStr_cmp(pext, ".rtf"))
                                nameIsOk = true;
						}
						if ( !nameIsOk )
						continue;
					}
				}
			}
			lString32 fn = !defHtml.empty() ? defHtml : firstGood;
			if ( !fn.empty() ) {
				m_stream = m_arc->OpenStream( fn.c_str(), LVOM_READ );
				if ( !m_stream.isNull() ) {
					CRLog::debug("Opened archive stream %s", LCSTR(fn) );
					m_doc_props->setString(DOC_PROP_FILE_NAME, fn);
					m_doc_props->setString(DOC_PROP_CODE_BASE, LVExtractPath(fn) );
					m_doc_props->setString(DOC_PROP_FILE_SIZE, lString32::itoa((int)m_stream->GetSize()));
                    m_doc_props->setHex(DOC_PROP_FILE_CRC32, m_stream->getcrc32());
					found = true;
				}
			}
			// opened archieve stream
			if ( !found )
			{
				Clear();
				if ( m_callback ) {
                    m_callback->OnLoadFileError( cs32("File with supported extension not found in archive.") );
				}
				return false;
			}

		}
		else

#endif //USE_ZLIB
		{
#if 1
			//m_stream = LVCreateBufferedStream( m_stream, FILE_STREAM_BUFFER_SIZE );
#else
			LVStreamRef stream = LVCreateBufferedStream( m_stream, FILE_STREAM_BUFFER_SIZE );
			lvsize_t sz = stream->GetSize();
			const lvsize_t bufsz = 0x1000;
			lUInt8 buf[bufsz];
			lUInt8 * fullbuf = new lUInt8 [sz];
			stream->SetPos(0);
			stream->Read(fullbuf, sz, NULL);
			lvpos_t maxpos = sz - bufsz;
			for (int i=0; i<1000; i++)
			{
				lvpos_t pos = (lvpos_t)((((lUInt64)i) * 1873456178) % maxpos);
				stream->SetPos( pos );
				lvsize_t readsz = 0;
				stream->Read( buf, bufsz, &readsz );
				if (readsz != bufsz)
				{
					//
					fprintf(stderr, "data read error!\n");
				}
				for (int j=0; j<bufsz; j++)
				{
					if (fullbuf[pos+j] != buf[j])
					{
						fprintf(stderr, "invalid data!\n");
					}
				}
			}
			delete[] fullbuf;
#endif
			LVStreamRef tcrDecoder = LVCreateTCRDecoderStream(m_stream);
			if (!tcrDecoder.isNull())
				m_stream = tcrDecoder;
		}

        // TEST FB2 Coverpage parser
    #if 0
        LVStreamRef cover = GetFB2Coverpage(m_stream);
        if (!cover.isNull()) {
            CRLog::info("cover page found: %d bytes", (int)cover->GetSize());
            LVImageSourceRef img = LVCreateStreamImageSource(cover);
            if (!img.isNull()) {
                CRLog::info("image size %d x %d", img->GetWidth(), img->GetHeight());
                LVColorDrawBuf buf(200, 200);
                CRLog::info("trying to draw");
                buf.Draw(img, 0, 0, 200, 200, true);
            }
        }
    #endif

		return ParseDocument();

	}
}

const lChar32 * getDocFormatName(doc_format_t fmt) {
	switch (fmt) {
	case doc_format_fb2:
		return U"FictionBook (FB2)";
	case doc_format_fb3:
		return U"FictionBook (FB3)";
	case doc_format_txt:
		return U"Plain text (TXT)";
	case doc_format_rtf:
		return U"Rich text (RTF)";
	case doc_format_epub:
		return U"EPUB";
	case doc_format_chm:
		return U"CHM";
	case doc_format_html:
		return U"HTML";
	case doc_format_txt_bookmark:
		return U"CR3 TXT Bookmark";
	case doc_format_doc:
		return U"DOC";
	case doc_format_docx:
		return U"DOCX";
	case doc_format_odt:
		return U"OpenDocument (ODT)";
	case doc_format_svg:
		return U"SVG";
	default:
		return U"Unknown format";
	}
}

/// sets current document format
void LVDocView::setDocFormat(doc_format_t fmt) {
	m_doc_format = fmt;
	lString32 desc(getDocFormatName(fmt));
	m_doc_props->setString(DOC_PROP_FILE_FORMAT, desc);
	m_doc_props->setInt(DOC_PROP_FILE_FORMAT_ID, (int) fmt);
}

/// create document and set flags
void LVDocView::createEmptyDocument() {
	_posIsSet = false;
	m_swapDone = false;
	_posBookmark = ldomXPointer();
	//lUInt32 saveFlags = 0;

	//m_doc ? m_doc->getDocFlags() : DOC_FLAG_DEFAULTS;
	m_is_rendered = false;
	if (m_doc)
		delete m_doc;
	m_doc = new ldomDocument();
	m_cursorPos.clear();
	m_markRanges.clear();
        m_bmkRanges.clear();
	_posBookmark.clear();
	m_section_bounds.clear();
	m_section_bounds_valid = false;
	_posIsSet = false;
	m_swapDone = false;

	m_doc->setProps(m_doc_props);
	m_doc->setDocFlags(0);
	m_doc->setDocFlag(DOC_FLAG_PREFORMATTED_TEXT, m_props->getBoolDef(
			PROP_TXT_OPTION_PREFORMATTED, false));
	m_doc->setDocFlag(DOC_FLAG_ENABLE_FOOTNOTES, m_props->getBoolDef(
			PROP_FOOTNOTES, true));
	m_doc->setDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES, m_props->getBoolDef(
			PROP_EMBEDDED_STYLES, true));
    m_doc->setDocFlag(DOC_FLAG_ENABLE_DOC_FONTS, m_props->getBoolDef(
            PROP_EMBEDDED_FONTS, true));
    m_doc->setDocFlag(DOC_FLAG_NONLINEAR_PAGEBREAK, m_props->getBoolDef(
            PROP_NONLINEAR_PAGEBREAK, false));
    m_doc->setSpaceWidthScalePercent(m_props->getIntDef(PROP_FORMAT_SPACE_WIDTH_SCALE_PERCENT, DEF_SPACE_WIDTH_SCALE_PERCENT));
    m_doc->setMinSpaceCondensingPercent(m_props->getIntDef(PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT, DEF_MIN_SPACE_CONDENSING_PERCENT));
    m_doc->setUnusedSpaceThresholdPercent(m_props->getIntDef(PROP_FORMAT_UNUSED_SPACE_THRESHOLD_PERCENT, DEF_UNUSED_SPACE_THRESHOLD_PERCENT));
    m_doc->setMaxAddedLetterSpacingPercent(m_props->getIntDef(PROP_FORMAT_MAX_ADDED_LETTER_SPACING_PERCENT, DEF_MAX_ADDED_LETTER_SPACING_PERCENT));
    m_doc->setCJKWidthScalePercent(m_props->getIntDef(PROP_FORMAT_CJK_WIDTH_SCALE_PERCENT, DEF_CJK_WIDTH_SCALE_PERCENT));
    m_doc->setHangingPunctiationEnabled(m_props->getBoolDef(PROP_FLOATING_PUNCTUATION, false));
    m_doc->setRenderBlockRenderingFlags(m_props->getIntDef(PROP_RENDER_BLOCK_RENDERING_FLAGS, DEF_RENDER_BLOCK_RENDERING_FLAGS));
    m_doc->setDOMVersionRequested(m_props->getIntDef(PROP_REQUESTED_DOM_VERSION, gDOMVersionCurrent));
    if (m_def_interline_space == 100) // (avoid any rounding issue)
        m_doc->setInterlineScaleFactor(INTERLINE_SCALE_FACTOR_NO_SCALE);
    else
        m_doc->setInterlineScaleFactor(INTERLINE_SCALE_FACTOR_NO_SCALE * m_def_interline_space / 100);
    m_doc->setScreenSize(m_dx, m_dy); // only used for CSS @media queries
    m_doc->setFontFamilyFonts(UnicodeToUtf8(m_props->getStringDef(PROP_FONT_FAMILY_FACES, "")));

    m_doc->setContainer(m_container);
    // This sets the element names default style (display, whitespace)
    // as defined in fb2def.h (createEmptyDocument() is called for all
    // document formats, so FB2 and HTML starts with the fb2def.h styles.
    // They are then updated with the <format>.css files, but they get back
    // these original styles when one selects "Clear all external styles".
    m_doc->setNodeTypes(fb2_elem_table);
    m_doc->setAttributeTypes(fb2_attr_table);
    m_doc->setNameSpaceTypes(fb2_ns_table);
    // Note:
    // All book formats parsing start with this, and get these fb2def.h
    // elements defined and included in the document ids maps. This allows
    // all our specific XHTML DOM element handling in ldomDocumentWriter
    // using "==el_body", "==el_head" to match on books.
    // But secondary XML files (.opf, .ncx...) get parsed with
    // LVParseXMLStream(), which will not inject these known ids maps
    // and will start a DOM with empty maps: met elements and attributes
    // will get IDs starting from 512 (even if named <body> or <head>),
    // and won't trigger these specific elements handling where we
    // use these < 512 IDs (el_body, el_head...), which is good!
}

/// format of document from cache is known
void LVDocView::OnCacheFileFormatDetected( doc_format_t fmt )
{
    // update document format id
    m_doc_format = fmt;
    // notify about format detection, to allow setting format-specific CSS
    if (m_callback) {
        m_callback->OnLoadFileFormatDetected(getDocFormat());
    }
    // set stylesheet
    updateDocStyleSheet();
}

void LVDocView::insertBookmarkPercentInfo(int start_page, int end_y, int percent)
{
    CR_UNUSED3(start_page, end_y, percent);
#if 0
    for (int j = start_page; j < m_pages.length(); j++) {
        if (m_pages[j]->start > end_y)
            break;
        LVBookMarkPercentInfo *bmi = m_bookmarksPercents[j];
        if (bmi == NULL) {
            bmi = new LVBookMarkPercentInfo(1, percent);
            m_bookmarksPercents.set(j, bmi);
        } else
            bmi->add(percent);
    }
#endif
}

bool LVDocView::ParseDocument() {

	createEmptyDocument();
	setRenderProps(0, 0); // to allow apply styles and rend method while loading

	if (m_stream->GetSize() > DOCUMENT_CACHING_MIN_SIZE) {
		// try loading from cache

		//lString32 fn( m_stream->GetName() );
		lString32 fn =
				m_doc_props->getStringDef(DOC_PROP_FILE_NAME, "untitled");
		fn = LVExtractFilename(fn);
		lUInt32 crc = 0;
        m_stream->getcrc32(crc);
		CRLog::debug("Check whether document %s crc %08x exists in cache",
				UnicodeToUtf8(fn).c_str(), crc);

		// set stylesheet
        updateDocStyleSheet();
		//m_doc->getStyleSheet()->clear();
		//m_doc->getStyleSheet()->parse(m_stylesheet.c_str());

        if (m_doc->openFromCache(this, m_callback)) {
			CRLog::info("Document is found in cache, will reuse");


			//            lString32 docstyle = m_doc->createXPointer(U"/FictionBook/stylesheet").getText();
			//            if ( !docstyle.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {
			//                //m_doc->getStyleSheet()->parse(UnicodeToUtf8(docstyle).c_str());
			//                m_doc->setStyleSheet( UnicodeToUtf8(docstyle).c_str(), false );
			//            }

			m_showCover = !getCoverPageImage().isNull();

            if ( m_callback )
                m_callback->OnLoadFileEnd( );

			return true;
		}
		CRLog::info("Cannot get document from cache, parsing...");
	}

	{
        ldomDocumentWriter writer(m_doc);
        ldomDocumentWriterFilter writerFilter(m_doc, false,
                HTML_AUTOCLOSE_TABLE);
        // Note: creating these 2 writers here, and using only one,
        // will still have both their destructors called when
        // leaving this scope. Each destructor call will have
        // ldomDocumentWriter::~ldomDocumentWriter() called, and
        // both will do the same work on m_doc. So, beware there
        // that this causes no issue.
        // We might want to refactor this section to avoid any issue.

        LVFileFormatParser * parser = NULL;
        if (m_stream->GetSize() >= 5) {
            // Only check the following formats when the document is
            // at least 5 bytes: if not, go directly with plain text.

            /// FB2 format
            setDocFormat( doc_format_fb2);
            parser = new LVXMLParser(m_stream, &writer, false, true);
            if (!parser->CheckFormat()) {
                delete parser;
                parser = NULL;
            }

            /// SVG format
            if (parser == NULL) {
                setDocFormat( doc_format_svg);
                parser = new LVXMLParser(m_stream, &writer, false, false, true);
                if (!parser->CheckFormat()) {
                    delete parser;
                    parser = NULL;
                }
            }

            /// RTF format
            if (parser == NULL) {
                setDocFormat( doc_format_rtf);
                parser = new LVRtfParser(m_stream, &writer);
                if (!parser->CheckFormat()) {
                    delete parser;
                    parser = NULL;
                }
            }

            /// HTML format
            if (parser == NULL) {
                setDocFormat( doc_format_html);
                parser = new LVHTMLParser(m_stream, &writerFilter);
                if (!parser->CheckFormat()) {
                    delete parser;
                    parser = NULL;
                }
            }

            ///cool reader bookmark in txt format
            if (parser == NULL) {
                //m_text_format = txt_format_pre; // DEBUG!!!
                setDocFormat( doc_format_txt_bookmark);
                parser = new LVTextBookmarkParser(m_stream, &writer);
                if (!parser->CheckFormat()) {
                    delete parser;
                    parser = NULL;
                }
            }
        }

        /// plain text format
        if (parser == NULL) {
            //m_text_format = txt_format_pre; // DEBUG!!!
            setDocFormat( doc_format_txt);
            parser = new LVTextParser(m_stream, &writer,
                            getTextFormatOptions() == txt_format_pre);
            if (!parser->CheckFormat()) {
                delete parser;
                parser = NULL;
            }
        }

        /// plain text format (robust, never fail)
        if (parser == NULL) {
            setDocFormat( doc_format_txt);
            parser = new LVTextRobustParser(m_stream, &writer,
                             getTextFormatOptions() == txt_format_pre);
            if (!parser->CheckFormat()) {
                // Never reached
                delete parser;
                parser = NULL;
            }
        }

        // unknown format (never reached)
        if (!parser) {
            setDocFormat( doc_format_none);
            createDefaultDocument(cs32("ERROR: Unknown document format"),
                    cs32("Cannot open document"));
            if (m_callback) {
                m_callback->OnLoadFileError(
                        cs32("Unknown document format"));
            }
            return false;
        }

		if (m_callback) {
			m_callback->OnLoadFileFormatDetected(getDocFormat());
		}
        updateDocStyleSheet();
		setRenderProps(0, 0);

		// set stylesheet
		//m_doc->getStyleSheet()->clear();
		//m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
		//m_doc->setStyleSheet( m_stylesheet.c_str(), true );

		// parse
		parser->setProgressCallback(m_callback);
		if (!parser->Parse()) {
			delete parser;
			if (m_callback) {
                m_callback->OnLoadFileError(cs32("Bad document format"));
			}
            createDefaultDocument(cs32("ERROR: Bad document format"),
                    cs32("Cannot open document"));
			return false;
		}
		delete parser;
		_pos = 0;
		_page = 0;

		//m_doc->compact();
		m_doc->dumpStatistics();

		if (m_doc_format == doc_format_html) {
			static lUInt16 path[] = { el_html, el_head, el_title, 0 };
			ldomNode * el = NULL;
			ldomNode * rootNode = m_doc->getRootNode();
			if (rootNode)
				el = rootNode->findChildElement(path);
			if (el != NULL) {
                lString32 s = el->getText(U' ', 1024);
				if (!s.empty()) {
					m_doc_props->setString(DOC_PROP_TITLE, s);
				}
			}
			// HTML documents may have gotten a TOC built from ldomElementWriter internal
			// support for FictionBook (FB2) structure (nested <section><title>), which
			// would have gather not much info from HTML. So build a new TOC from classic
			// HTML headings.
			m_doc->buildTocFromHeadings();
		}

		if (m_doc_format == doc_format_svg) {
			// A SVG file is its own cover
			m_doc_props->setString(DOC_PROP_COVER_FILE, m_doc_props->getStringDef(DOC_PROP_FILE_NAME, ""));
			// Use any <svg><title> as the document title
			// initNodeRendMethod() will have wrapped the <svg> inside a <autoBoxing>
			static lUInt16 path[] = { el_autoBoxing, el_svg, el_title, 0 };
			ldomNode * el = NULL;
			ldomNode * rootNode = m_doc->getRootNode();
			if (rootNode)
				el = rootNode->findChildElement(path);
			if (el != NULL) {
				lString32 s = el->getText(U' ', 1024);
				if (!s.empty()) {
					m_doc_props->setString(DOC_PROP_TITLE, s);
				}
			}
		}

		//        lString32 docstyle = m_doc->createXPointer(U"/FictionBook/stylesheet").getText();
		//        if ( !docstyle.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {
		//            //m_doc->getStyleSheet()->parse(UnicodeToUtf8(docstyle).c_str());
		//            m_doc->setStyleSheet( UnicodeToUtf8(docstyle).c_str(), false );
		//        }

#ifdef SAVE_COPY_OF_LOADED_DOCUMENT //def _DEBUG
		LVStreamRef ostream = LVOpenFileStream( "test_save_source.xml", LVOM_WRITE );
		m_doc->saveToStream( ostream, "utf-16" );
#endif
#if 0
		{
			LVStreamRef ostream = LVOpenFileStream( "test_save.fb2", LVOM_WRITE );
			m_doc->saveToStream( ostream, "utf-16" );
			m_doc->getRootNode()->recurseElements( SaveBase64Objects );
		}
#endif

		//m_doc->getProps()->clear();
		if (m_doc_props->getStringDef(DOC_PROP_TITLE, "").empty()) {
			m_doc_props->setString(DOC_PROP_AUTHORS, extractDocAuthors(m_doc));
			m_doc_props->setString(DOC_PROP_TITLE, extractDocTitle(m_doc));
			m_doc_props->setString(DOC_PROP_LANGUAGE, extractDocLanguage(m_doc));
			m_doc_props->setString(DOC_PROP_KEYWORDS, extractDocKeywords(m_doc));
			m_doc_props->setString(DOC_PROP_DESCRIPTION, extractDocDescription(m_doc));
            /* EPUB returns seriesNumber as a string, which can hold numbers with decimal: do the same
            int seriesNumber = -1;
            lString32 seriesName = extractDocSeries(m_doc, &seriesNumber);
            m_doc_props->setString(DOC_PROP_SERIES_NAME, seriesName);
            m_doc_props->setString(DOC_PROP_SERIES_NUMBER, seriesNumber>0 ? lString32::itoa(seriesNumber) :lString32::empty_str);
            */
            lString32 seriesNumber;
            lString32 seriesName = extractDocSeriesAndNumber(m_doc, seriesNumber);
            m_doc_props->setString(DOC_PROP_SERIES_NAME, seriesName);
            m_doc_props->setString(DOC_PROP_SERIES_NUMBER, seriesNumber);
        }
	}
	//m_doc->persist();

	m_showCover = !getCoverPageImage().isNull();

    REQUEST_RENDER("loadDocument")
	if (m_callback) {
		m_callback->OnLoadFileEnd();
	}

#if 0 // test serialization
	SerialBuf buf( 1024 );
	m_doc->serializeMaps(buf);
	if ( !buf.error() ) {
		int sz = buf.pos();
		SerialBuf buf2( buf.buf(), buf.pos() );
		ldomDocument * newdoc = new ldomDocument();
		if ( newdoc->deserializeMaps( buf2 ) ) {
			delete newdoc;
		}
	}
#endif
#if 0// test swap to disk
    lString32 cacheFile = cs32("/tmp/cr3swap.bin");
	bool res = m_doc->swapToCacheFile( cacheFile );
	if ( !res ) {
		CRLog::error( "Failed to swap to disk" );
		return false;
	}
#endif
#if 0 // test restore from swap
	delete m_doc;
	m_doc = new ldomDocument();
	res = m_doc->openFromCacheFile( cacheFile );
	m_doc->setDocFlags( saveFlags );
	m_doc->setContainer( m_container );
	if ( !res ) {
		CRLog::error( "Failed loading of swap from disk" );
		return false;
	}
	m_doc->getStyleSheet()->clear();
	m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
#endif
#if 0
	{
		LVStreamRef ostream = LVOpenFileStream(U"out.xml", LVOM_WRITE );
		if ( !ostream.isNull() )
		m_doc->saveToStream( ostream, "utf-8" );
	}
#endif

	return true;
}

/// save unsaved data to cache file (if one is created), with timeout option
ContinuousOperationResult LVDocView::updateCache(CRTimerUtil & maxTime)
{
    return m_doc->updateMap(maxTime);
}

/// save unsaved data to cache file (if one is created), w/o timeout
ContinuousOperationResult LVDocView::updateCache()
{
    CRTimerUtil infinite;
    return swapToCache(infinite);
}

/// save document to cache file, with timeout option
ContinuousOperationResult LVDocView::swapToCache(CRTimerUtil & maxTime)
{
    int fs = m_doc_props->getIntDef(DOC_PROP_FILE_SIZE, 0);
    CRLog::trace("LVDocView::swapToCache(fs = %d)", fs);
    // minimum file size to swap, even if forced
    // TODO
    int mfs = 30000; //m_props->getIntDef(PROP_FORCED_MIN_FILE_SIZE_TO_CACHE, 30000); // 30K
    if (fs < mfs) {
        //CRLog::trace("LVDocView::swapToCache : file is too small for caching");
        return CR_DONE;
    }
    return m_doc->swapToCache( maxTime );
}

void LVDocView::swapToCache() {
    CRTimerUtil infinite;
    swapToCache(infinite);
    m_swapDone = true;
}

bool LVDocView::LoadDocument(const char * fname, bool metadataOnly) {
	if (!fname || !fname[0])
		return false;
	return LoadDocument(LocalToUnicode(lString8(fname)).c_str(), metadataOnly);
}

/// returns XPointer to middle paragraph of current page
ldomXPointer LVDocView::getCurrentPageMiddleParagraph() {
	LVLock lock(getMutex());
	checkPos();
	ldomXPointer ptr;
	if (!m_doc)
		return ptr;

	if (getViewMode() == DVM_SCROLL) {
		// SCROLL mode
		int starty = _pos;
		int endy = _pos + m_dy;
		int fh = GetFullHeight();
		if (endy >= fh)
			endy = fh - 1;
		ptr = m_doc->createXPointer(lvPoint(0, (starty + endy) / 2));
	} else {
		// PAGES mode
		int pageIndex = getCurPage(true);
		if (pageIndex < 0 || pageIndex >= m_pages.length())
			pageIndex = getCurPage(true);
		if (pageIndex >= 0 && pageIndex < m_pages.length()) {
			LVRendPageInfo *page = m_pages[pageIndex];
			if (page->flags & RN_PAGE_TYPE_NORMAL)
				ptr = m_doc->createXPointer(lvPoint(0, page->start + page->height / 2));
		}
	}
	if (ptr.isNull())
		return ptr;
	ldomXPointerEx p(ptr);
	if (!p.isVisibleFinal())
		if (!p.ensureFinal())
			if (!p.prevVisibleFinal())
				if (!p.nextVisibleFinal())
					return ptr;
	return ldomXPointer(p);
}

/// returns bookmark
ldomXPointer LVDocView::getBookmark( bool precise ) {
	LVLock lock(getMutex());
	checkPos();
	ldomXPointer ptr;
	if (m_doc) {
		if (isPageMode()) {
			if (_page >= 0 && _page < m_pages.length()) {
				// In some edge cases, getting the xpointer for y=m_pages[_page]->start
				// could resolve back to the previous page. We need to check for that
				// and increase y until we find a coherent one.
				// (In the following, we always want to find the first logical word/char.)
				LVRendPageInfo * page = m_pages[_page];
				bool found = false;
				ldomXPointer fallback_ptr;
				if (precise) {
					for (int y = page->start; y < page->start + page->height; y++) {
						ptr = m_doc->createXPointer(lvPoint(0, y), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST);
						lvPoint pt = ptr.toPoint();
						if (pt.y >= page->start) {
							if (!fallback_ptr)
								fallback_ptr = ptr;
							if (pt.y < page->start + page->height) {
								// valid, resolves back to same page
								found = true;
								break;
							}
						}
					}
				} else {
					ptr = m_doc->createXPointer(lvPoint(0, page->start));
					found = true;
				}
				if (!found) {
					// None looking forward resolved to that same page, we
					// might find a better one looking backward (eg: when an
					// element contains some floats that overflows its height).
					ptr = m_doc->createXPointer(lvPoint(0, page->start), PT_DIR_SCAN_BACKWARD_LOGICAL_FIRST);
					lvPoint pt = ptr.toPoint();
					if (pt.y >= page->start && pt.y < page->start + page->height ) {
						found = true;
					}
				}
				if (!found) {
					if (!fallback_ptr.isNull()) {
						ptr = fallback_ptr;
					}
					else {
						// fallback to the one for page->start, even if not good
						ptr = m_doc->createXPointer(lvPoint(0, page->start), PT_DIR_SCAN_BACKWARD_LOGICAL_FIRST);
					}
				}
			}
		} else {
			// In scroll mode, the y position may not resolve to any xpointer
			// (because of margins, empty elements...)
			// When inside an image (top of page being the middle of an image),
			// we get the top of the image, and when restoring this position,
			// we'll have the top of the image at the top of the page, so
			// scrolling a bit up.
			// Let's do the same in that case: get the previous text node
			// position
			if (precise) {
				for (int y = _pos; y >= 0; y--) {
					ptr = m_doc->createXPointer(lvPoint(0, y), PT_DIR_SCAN_BACKWARD_LOGICAL_FIRST);
					if (!ptr.isNull())
						break;
				}
			} else {
				ptr = m_doc->createXPointer(lvPoint(0, _pos));
			}
		}
	}
	return ptr;
	/*
	 lUInt64 pos = m_pos;
	 if (m_view_mode==DVM_PAGES)
	 m_pos += m_dy/3;
	 lUInt64 h = GetFullHeight();
	 if (h<1)
	 h = 1;
	 return pos*1000000/h;
	 */
}

/// returns bookmark for specified page
ldomXPointer LVDocView::getPageBookmark(int page) {
	LVLock lock(getMutex());
    CHECK_RENDER("getPageBookmark()")
	if (page < 0 || page >= m_pages.length())
		return ldomXPointer();
	ldomXPointer ptr = m_doc->createXPointer(lvPoint(0, m_pages[page]->start));
	return ptr;
}

/// get bookmark position text
bool LVDocView::getBookmarkPosText(ldomXPointer bm, lString32 & titleText,
		lString32 & posText) {
	LVLock lock(getMutex());
	checkRender();
    titleText = posText = lString32::empty_str;
	if (bm.isNull())
		return false;
	ldomNode * el = bm.getNode();
	CRLog::trace("getBookmarkPosText() : getting position text");
	if (el->isText()) {
        lString32 txt = bm.getNode()->getText();
		int startPos = bm.getOffset();
		int len = txt.length() - startPos;
		if (len > 0)
			txt = txt.substr(startPos, len);
		if (startPos > 0)
            posText = "...";
        posText += txt;
		el = el->getParentNode();
	} else {
        posText = el->getText(U' ', 1024);
	}
    bool inTitle = false;
	do {
		while (el && el->getNodeId() != el_section && el->getNodeId()
				!= el_body) {
			if (el->getNodeId() == el_title || el->getNodeId() == el_subtitle)
				inTitle = true;
			el = el->getParentNode();
		}
		if (el) {
			if (inTitle) {
				posText.clear();
				if (el->getChildCount() > 1) {
					ldomNode * node = el->getChildNode(1);
                    posText = node->getText(' ', 8192);
				}
				inTitle = false;
			}
			if (el->getNodeId() == el_body && !titleText.empty())
				break;
			lString32 txt = getSectionHeader(el);
			lChar32 lastch = !txt.empty() ? txt[txt.length() - 1] : 0;
			if (!titleText.empty()) {
				if (lastch != '.' && lastch != '?' && lastch != '!')
                    txt += ".";
                txt += " ";
			}
			titleText = txt + titleText;
			el = el->getParentNode();
		}
		if (titleText.length() > 50)
			break;
	} while (el);
    limitStringSize(titleText, 70);
	limitStringSize(posText, 120);
	return true;
}

/// moves position to bookmark
void LVDocView::goToBookmark(ldomXPointer bm) {
	LVLock lock(getMutex());
    CHECK_RENDER("goToBookmark()")
	_posIsSet = false;
	_posBookmark = bm;
}

/// get page number by bookmark
int LVDocView::getBookmarkPage(ldomXPointer bm, bool internal) {
	LVLock lock(getMutex());
    CHECK_RENDER("getBookmarkPage()")
	if (bm.isNull()) {
		return 0;
	} else {
		lvPoint pt = bm.toPoint();
		if (pt.y < 0)
			return 0;
		int page = m_pages.FindNearestPage(pt.y, 0);
		return internal ? page : getExternalPageNumber( page );
	}
}

void LVDocView::updateScroll() {
	checkPos();
	if (m_view_mode == DVM_SCROLL) {
		int npos = _pos;
		int fh = GetFullHeight();
		int shift = 0;
		int npage = m_dy;
		while (fh > 16384) {
			fh >>= 1;
			npos >>= 1;
			npage >>= 1;
			shift++;
		}
		if (npage < 1)
			npage = 1;
		m_scrollinfo.pos = npos;
		m_scrollinfo.maxpos = fh - npage;
		m_scrollinfo.pagesize = npage;
		m_scrollinfo.scale = shift;
		char str[32];
		sprintf(str, "%d%%", (int) (fh > 0 ? (100 * npos / fh) : 0));
		m_scrollinfo.posText = lString32(str);
	} else {
		int page = getCurPage(true);
		int vpc = getVisiblePageCount();
		m_scrollinfo.pos = page / vpc;
		m_scrollinfo.maxpos = (m_pages.length() + vpc - 1) / vpc - 1;
		m_scrollinfo.pagesize = 1;
		m_scrollinfo.scale = 0;
		char str[32] = { 0 };
		if (m_pages.length() > 1) {
			if (page <= 0) {
				sprintf(str, "cover");
			} else
				sprintf(str, "%d / %d", page, m_pages.length() - 1);
		}
		m_scrollinfo.posText = lString32(str);
	}
}

/// move to position specified by scrollbar
bool LVDocView::goToScrollPos(int pos) {
	if (m_view_mode == DVM_SCROLL) {
		SetPos(scrollPosToDocPos(pos));
		return true;
	} else {
		int vpc = this->getVisiblePageCount();
		int curPage = getCurPage(true);
		pos = pos * vpc;
		if (pos >= getPageCount(true))
			pos = getPageCount(true) - 1;
		if (pos < 0)
			pos = 0;
		if (curPage == pos)
			return false;
		goToPage(pos, true);
		return true;
	}
}

/// converts scrollbar pos to doc pos
int LVDocView::scrollPosToDocPos(int scrollpos) {
	if (m_view_mode == DVM_SCROLL) {
		int n = scrollpos << m_scrollinfo.scale;
		if (n < 0)
			n = 0;
		int fh = GetFullHeight();
		if (n > fh)
			n = fh;
		return n;
	} else {
		int vpc = getVisiblePageCount();
		int n = scrollpos * vpc;
		if (!m_pages.length())
			return 0;
		if (n >= m_pages.length())
			n = m_pages.length() - 1;
		if (n < 0)
			n = 0;
		return m_pages[n]->start;
	}
}

/// get list of links
void LVDocView::getCurrentPageLinks(ldomXRangeList & list) {
	list.clear();
	/// get page document range, -1 for current page
	LVRef < ldomXRange > page = getPageDocumentRange();
	if (!page.isNull()) {
		// search for links
		class LinkKeeper: public ldomNodeCallback {
			ldomXRangeList &_list;
			bool check_first_text_node_parents_done;
		public:
			LinkKeeper(ldomXRangeList & list) : _list(list) {
				check_first_text_node_parents_done = false;
			}
			/// called for each found text fragment in range
			virtual void onText(ldomXRange * r) {
				if (check_first_text_node_parents_done)
					return;
				// For the first text node, look at its parents
				// as one might be a <A>
				ldomNode * node = r->getStart().getNode();
				while ( node && !node->isElement() )
					node = node->getParentNode();
				while ( node && node->getNodeId() != el_a )
					node = node->getParentNode();
				if ( node ) { // <a> parent of first text node found
					ldomXPointerEx xp = ldomXPointerEx(node, 0);
					onElement( &xp );
				}
				check_first_text_node_parents_done = true;
			}
			/// called for each found node in range
			virtual bool onElement(ldomXPointerEx * ptr) {
				//
				ldomNode * elem = ptr->getNode();
				if (elem->getNodeId() == el_a) {
					for (int i = 0; i < _list.length(); i++) {
						if (_list[i]->getStart().getNode() == elem)
							return true; // don't add, duplicate found!
					}
					// We ignore <a/> and <a></a> with no content, as we
					// can't make a ldomXRange out of them
					// We also want all the <a> content, so we use a ldomXRange()
					// extended to include its deepest text child
					if (elem->hasChildren())
						_list.add(new ldomXRange(elem, true));
				}
				return true;
			}
		};
		LinkKeeper callback(list);
		page->forEach(&callback);
		if (m_view_mode == DVM_PAGES && getVisiblePageCount() > 1) {
			// process second page
			int pageNumber = getCurPage(true);
			page = getPageDocumentRange(pageNumber + 1);
			if (!page.isNull())
				page->forEach(&callback);
		}
	}
}

/// returns document offset for next page
int LVDocView::getNextPageOffset() {
	LVLock lock(getMutex());
	checkPos();
	if (isScrollMode()) {
		return GetPos() + m_dy;
	} else {
		int p = getCurPage(true) + getVisiblePageCount();
		if (p < m_pages.length())
			return m_pages[p]->start;
		if (!p || m_pages.length() == 0)
			return 0;
		return m_pages[m_pages.length() - 1]->start;
	}
}

/// returns document offset for previous page
int LVDocView::getPrevPageOffset() {
	LVLock lock(getMutex());
	checkPos();
	if (m_view_mode == DVM_SCROLL) {
		return GetPos() - m_dy;
	} else {
		int p = getCurPage(true);
		p -= getVisiblePageCount();
		if (p < 0)
			p = 0;
		if (p >= m_pages.length())
			return 0;
		return m_pages[p]->start;
	}
}

static void addItem(LVPtrVector<LVTocItem, false> & items, LVTocItem * item) {
	if (item->getLevel() > 0)
		items.add(item);
	for (int i = 0; i < item->getChildCount(); i++) {
		addItem(items, item->getChild(i));
	}
}

/// returns pointer to TOC root node
bool LVDocView::getFlatToc(LVPtrVector<LVTocItem, false> & items) {
	items.clear();
	addItem(items, getToc());
	return items.length() > 0;
}

/// -1 moveto previous page, 1 to next page
bool LVDocView::moveByPage(int delta) {
	if (m_view_mode == DVM_SCROLL) {
		int p = GetPos();
		SetPos(p + m_dy * delta);
		return GetPos() != p;
	} else {
		int cp = getCurPage();
		int p = cp + delta * getVisiblePageNumberCount();
		goToPage(p);
		return getCurPage() != cp;
	}
}

/// -1 moveto previous chapter, 0 to current chaoter first pae, 1 to next chapter
bool LVDocView::moveByChapter(int delta) {
	/// returns pointer to TOC root node
	LVPtrVector < LVTocItem, false > items;
	if (!getFlatToc(items))
		return false;
	int cp = getCurPage();
	int prevPage = -1;
	int nextPage = -1;
    int vcp = getVisiblePageNumberCount();
    if (vcp < 1 || vcp > 2)
        vcp = 1;
	for (int i = 0; i < items.length(); i++) {
		LVTocItem * item = items[i];
		int p = item->getPage();
		if (p < cp && (prevPage == -1 || prevPage < p))
			prevPage = p;
        if (p >= cp + vcp && (nextPage == -1 || nextPage > p))
			nextPage = p;
	}
	if (prevPage < 0)
		prevPage = 0;
	if (nextPage < 0)
		nextPage = getPageCount() - 1;
	int page = delta < 0 ? prevPage : nextPage;
	if (getCurPage() != page) {
		savePosToNavigationHistory();
        goToPage(page);
	}
	return true;
}

/// saves new bookmark
CRBookmark * LVDocView::saveRangeBookmark(ldomXRange & range, bmk_type type,
		lString32 comment) {
	if (range.isNull())
		return NULL;
	if (range.getStart().isNull())
		return NULL;
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return NULL;
	CRBookmark * bmk = new CRBookmark();
	bmk->setType(type);
	bmk->setStartPos(range.getStart().toString());
	if (!range.getEnd().isNull())
		bmk->setEndPos(range.getEnd().toString());
	int p = range.getStart().toPoint().y;
	int fh = m_doc->getFullHeight();
	int percent = fh > 0 ? (int) (p * (lInt64) 10000 / fh) : 0;
	if (percent < 0)
		percent = 0;
	if (percent > 10000)
		percent = 10000;
	bmk->setPercent(percent);
	lString32 postext = range.getRangeText();
	bmk->setPosText(postext);
	bmk->setCommentText(comment);
	bmk->setTitleText(CRBookmark::getChapterName(range.getStart()));
	rec->getBookmarks().add(bmk);
        updateBookMarksRanges();
#if 0
        if (m_highlightBookmarks && !range.getEnd().isNull())
            insertBookmarkPercentInfo(m_pages.FindNearestPage(p, 0),
                                  range.getEnd().toPoint().y, percent);
#endif
	return bmk;
}

/// sets new list of bookmarks, removes old values
void LVDocView::setBookmarkList(LVPtrVector<CRBookmark> & bookmarks)
{
    CRFileHistRecord * rec = getCurrentFileHistRecord();
    if (!rec)
        return;
    LVPtrVector<CRBookmark>  & v = rec->getBookmarks();
    v.clear();
    for (int i=0; i<bookmarks.length(); i++) {
        v.add(new CRBookmark(*bookmarks[i]));
    }
    updateBookMarksRanges();
}

/// removes bookmark from list, and deletes it, false if not found
bool LVDocView::removeBookmark(CRBookmark * bm) {
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return false;
	bm = rec->getBookmarks().remove(bm);
	if (bm) {
        updateBookMarksRanges();
        delete bm;
		return true;
#if 0
            if (m_highlightBookmarks && bm->getType() == bmkt_comment || bm->getType() == bmkt_correction) {
                int by = m_doc->createXPointer(bm->getStartPos()).toPoint().y;
                int page_index = m_pages.FindNearestPage(by, 0);
                bool updateRanges = false;

                if (page_index > 0 && page_index < m_bookmarksPercents.length()) {
                    LVBookMarkPercentInfo *bmi = m_bookmarksPercents[page_index];
                    int percent = bm->getPercent();

                    for (int i = 0; bmi != NULL && i < bmi->length(); i++) {
                        if ((updateRanges = bmi->get(i) == percent))
                            bmi->remove(i);
                    }
                }
                if (updateRanges)
                    updateBookMarksRanges();
            }
            delete bm;
            return true;
#endif
	} else {
		return false;
	}
}

#define MAX_EXPORT_BOOKMARKS_SIZE 200000
/// export bookmarks to text file
bool LVDocView::exportBookmarks(lString32 filename) {
	if (m_filename.empty())
		return true; // no document opened
	lChar32 lastChar = filename.lastChar();
	lString32 dir;
	CRLog::trace("exportBookmarks(%s)", UnicodeToUtf8(filename).c_str());
	if (lastChar == '/' || lastChar == '\\') {
		dir = filename;
		CRLog::debug("Creating directory, if not exist %s",
				UnicodeToUtf8(dir).c_str());
		LVCreateDirectory(dir);
		filename.clear();
	}
	if (filename.empty()) {
		CRPropRef props = getDocProps();
		lString32 arcname = props->getStringDef(DOC_PROP_ARC_NAME);
		lString32 arcpath = props->getStringDef(DOC_PROP_ARC_PATH);
		int arcFileCount = props->getIntDef(DOC_PROP_ARC_FILE_COUNT, 0);
		if (!arcpath.empty())
			LVAppendPathDelimiter(arcpath);
		lString32 fname = props->getStringDef(DOC_PROP_FILE_NAME);
		lString32 fpath = props->getStringDef(DOC_PROP_FILE_PATH);
		if (!fpath.empty())
			LVAppendPathDelimiter(fpath);
		if (!arcname.empty()) {
			if (dir.empty())
				dir = arcpath;
			if (arcFileCount > 1)
                filename = arcname + "." + fname + ".bmk.txt";
			else
                filename = arcname + ".bmk.txt";
		} else {
			if (dir.empty())
				dir = fpath;
            filename = fname + ".bmk.txt";
		}
		LVAppendPathDelimiter(dir);
		filename = dir + filename;
	}
	CRLog::debug("Exported bookmark filename: %s",
			UnicodeToUtf8(filename).c_str());
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return false;
	// check old content
	lString8 oldContent;
	{
		LVStreamRef is = LVOpenFileStream(filename.c_str(), LVOM_READ);
		if (!is.isNull()) {
			int sz = (int) is->GetSize();
			if (sz > 0 && sz < MAX_EXPORT_BOOKMARKS_SIZE) {
				oldContent.append(sz, ' ');
				lvsize_t bytesRead = 0;
				if (is->Read(oldContent.modify(), sz, &bytesRead) != LVERR_OK
						|| (int) bytesRead != sz)
					oldContent.clear();
			}
		}
	}
	lString8 newContent;
	LVPtrVector < CRBookmark > &bookmarks = rec->getBookmarks();
	for (int i = 0; i < bookmarks.length(); i++) {
		CRBookmark * bmk = bookmarks[i];
		if (bmk->getType() != bmkt_comment && bmk->getType() != bmkt_correction)
			continue;
		if (newContent.empty()) {
			newContent.append(1, (lChar8)0xef);
			newContent.append(1, (lChar8)0xbb);
			newContent.append(1, (lChar8)0xbf);
			newContent << "# Cool Reader 3 - exported bookmarks\r\n";
			newContent << "# file name: " << UnicodeToUtf8(rec->getFileName())
					<< "\r\n";
			if (!rec->getFilePathName().empty())
				newContent << "# file path: " << UnicodeToUtf8(
						rec->getFilePath()) << "\r\n";
			newContent << "# book title: " << UnicodeToUtf8(rec->getTitle())
					<< "\r\n";
			newContent << "# author: " << UnicodeToUtf8(rec->getAuthor())
					<< "\r\n";
			if (!rec->getSeries().empty())
				newContent << "# series: " << UnicodeToUtf8(rec->getSeries())
						<< "\r\n";
			newContent << "\r\n";
		}
		char pos[16];
		int percent = bmk->getPercent();
		lString32 title = bmk->getTitleText();
		sprintf(pos, "%d.%02d%%", percent / 100, percent % 100);
		newContent << "## " << pos << " - "
				<< (bmk->getType() == bmkt_comment ? "comment" : "correction")
				<< "\r\n";
		if (!title.empty())
			newContent << "## " << UnicodeToUtf8(title) << "\r\n";
		if (!bmk->getPosText().empty())
			newContent << "<< " << UnicodeToUtf8(bmk->getPosText()) << "\r\n";
		if (!bmk->getCommentText().empty())
			newContent << ">> " << UnicodeToUtf8(bmk->getCommentText())
					<< "\r\n";
		newContent << "\r\n";
	}

	if (newContent == oldContent)
            return true;

        if (newContent.length() > 0) {
            LVStreamRef os = LVOpenFileStream(filename.c_str(), LVOM_WRITE);
            if (os.isNull())
                    return false;
            lvsize_t bytesWritten = 0;

            if (os->Write(newContent.c_str(), newContent.length(), &bytesWritten) != LVERR_OK ||
                bytesWritten != (lUInt32)newContent.length())
                return false;
        } else {
            LVDeleteFile(filename);
            return false;
        }
	return true;
}

/// saves current page bookmark under numbered shortcut
CRBookmark * LVDocView::saveCurrentPageShortcutBookmark(int number) {
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return NULL;
	ldomXPointer p = getBookmark();
	if (p.isNull())
		return NULL;
	if (number == 0)
		number = rec->getFirstFreeShortcutBookmark();
	if (number == -1) {
		CRLog::error("Cannot add bookmark: no space left in bookmarks storage.");
		return NULL;
	}
	CRBookmark * bm = rec->setShortcutBookmark(number, p);
	lString32 titleText;
	lString32 posText;
	if (bm && getBookmarkPosText(p, titleText, posText)) {
		bm->setTitleText(titleText);
		bm->setPosText(posText);
		return bm;
	}
	return NULL;
}

/// saves current page bookmark under numbered shortcut
CRBookmark * LVDocView::saveCurrentPageBookmark(lString32 comment) {
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return NULL;
	ldomXPointer p = getBookmark();
	if (p.isNull())
		return NULL;
	CRBookmark * bm = new CRBookmark(p);
	lString32 titleText;
	lString32 posText;
	bm->setType(bmkt_pos);
	if (getBookmarkPosText(p, titleText, posText)) {
		bm->setTitleText(titleText);
		bm->setPosText(posText);
	}
	bm->setStartPos(p.toString());
	int pos = p.toPoint().y;
	int fh = m_doc->getFullHeight();
	int percent = fh > 0 ? (int) (pos * (lInt64) 10000 / fh) : 0;
	if (percent < 0)
		percent = 0;
	if (percent > 10000)
		percent = 10000;
	bm->setPercent(percent);
	bm->setCommentText(comment);
	rec->getBookmarks().add(bm);
        updateBookMarksRanges();
	return bm;
}

/// restores page using bookmark by numbered shortcut
bool LVDocView::goToPageShortcutBookmark(int number) {
	CRFileHistRecord * rec = getCurrentFileHistRecord();
	if (!rec)
		return false;
	CRBookmark * bmk = rec->getShortcutBookmark(number);
	if (!bmk)
		return false;
	lString32 pos = bmk->getStartPos();
	ldomXPointer p = m_doc->createXPointer(pos);
	if (p.isNull())
		return false;
	if (getCurPage(true) != getBookmarkPage(p, true))
		savePosToNavigationHistory();
	goToBookmark(p);
        updateBookMarksRanges();
	return true;
}

inline int myabs(int n) { return n < 0 ? -n : n; }

static int calcBookmarkMatch(lvPoint pt, lvRect & rc1, lvRect & rc2, int type) {
    if (pt.y < rc1.top || pt.y >= rc2.bottom)
        return -1;
    if (type == bmkt_pos) {
        return myabs(pt.x - 0);
    }
    if (rc1.top == rc2.top) {
        // single line
        if (pt.y >= rc1.top && pt.y < rc2.bottom && pt.x >= rc1.left && pt.x < rc2.right) {
            return myabs(pt.x - (rc1.left + rc2.right) / 2);
        }
        return -1;
    } else {
        // first line
        if (pt.y >= rc1.top && pt.y < rc1.bottom && pt.x >= rc1.left) {
            return myabs(pt.x - (rc1.left + rc1.right) / 2);
        }
        // last line
        if (pt.y >= rc2.top && pt.y < rc2.bottom && pt.x < rc2.right) {
            return myabs(pt.x - (rc2.left + rc2.right) / 2);
        }
        // middle line
        return myabs(pt.y - (rc1.top + rc2.bottom) / 2);
    }
}

/// find bookmark by window point, return NULL if point doesn't belong to any bookmark
CRBookmark * LVDocView::findBookmarkByPoint(lvPoint pt) {
    CRFileHistRecord * rec = getCurrentFileHistRecord();
    if (!rec)
        return NULL;
    if (!windowToDocPoint(pt))
        return NULL;
    LVPtrVector<CRBookmark>  & bookmarks = rec->getBookmarks();
    CRBookmark * best = NULL;
    int bestMatch = -1;
    for (int i=0; i<bookmarks.length(); i++) {
        CRBookmark * bmk = bookmarks[i];
        int t = bmk->getType();
        if (t == bmkt_lastpos)
            continue;
        ldomXPointer p = m_doc->createXPointer(bmk->getStartPos());
        if (p.isNull())
            continue;
        lvRect rc;
        if (!p.getRect(rc))
            continue;
        ldomXPointer ep = (t == bmkt_pos) ? p : m_doc->createXPointer(bmk->getEndPos());
        if (ep.isNull())
            continue;
        lvRect erc;
        if (!ep.getRect(erc))
            continue;
        int match = calcBookmarkMatch(pt, rc, erc, t);
        if (match < 0)
            continue;
        if (match < bestMatch || bestMatch == -1) {
            bestMatch = match;
            best = bmk;
        }
    }
    return best;
}

// execute command
int LVDocView::doCommand(LVDocCmd cmd, int param) {
	CRLog::trace("doCommand(%d, %d)", (int)cmd, param);
	switch (cmd) {
    case DCMD_SET_DOC_FONTS:
        CRLog::trace("DCMD_SET_DOC_FONTS(%d)", param);
        m_props->setBool(PROP_EMBEDDED_FONTS, (param&1)!=0);
        getDocument()->setDocFlag(DOC_FLAG_ENABLE_DOC_FONTS, param!=0);
        REQUEST_RENDER("doCommand-set embedded doc fonts")
        break;
    case DCMD_SET_INTERNAL_STYLES:
        CRLog::trace("DCMD_SET_INTERNAL_STYLES(%d)", param);
        m_props->setBool(PROP_EMBEDDED_STYLES, (param&1)!=0);
        getDocument()->setDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES, param!=0);
        REQUEST_RENDER("doCommand-set internal styles")
        break;
    case DCMD_REQUEST_RENDER:
        REQUEST_RENDER("doCommand-request render")
		break;
    case DCMD_SET_BASE_FONT_WEIGHT: {
        // replaces DCMD_TOGGLE_BOLD
        m_props->setInt(PROP_FONT_BASE_WEIGHT, param);
        LVRendSetBaseFontWeight(param);
        REQUEST_RENDER("doCommand-set font weight")
        break;
    }
	case DCMD_TOGGLE_PAGE_SCROLL_VIEW: {
		toggleViewMode();
	}
		break;
	case DCMD_GO_SCROLL_POS: {
		return goToScrollPos(param);
	}
		break;
	case DCMD_BEGIN: {
		if (getCurPage() > 0) {
			savePosToNavigationHistory();
			return SetPos(0);
		}
	}
		break;
	case DCMD_LINEUP: {
		if (m_view_mode == DVM_SCROLL) {
			return SetPos(GetPos() - param * (m_font_size * 3 / 2));
		} else {
			int p = getCurPage();
			return goToPage(p - getVisiblePageNumberCount());
			//goToPage( m_pages.FindNearestPage(m_pos, -1));
		}
	}
		break;
	case DCMD_PAGEUP: {
		if (param < 1)
			param = 1;
		return moveByPage(-param);
	}
		break;
	case DCMD_PAGEDOWN: {
		if (param < 1)
			param = 1;
		return moveByPage(param);
	}
		break;
	case DCMD_LINK_NEXT: {
		selectNextPageLink(true);
	}
		break;
	case DCMD_LINK_PREV: {
		selectPrevPageLink(true);
	}
		break;
	case DCMD_LINK_FIRST: {
		selectFirstPageLink();
	}
		break;
#if CR_INTERNAL_PAGE_ORIENTATION==1
		case DCMD_ROTATE_BY: // rotate view, param =  +1 - clockwise, -1 - counter-clockwise
		{
			int a = (int)m_rotateAngle;
			if ( param==0 )
			param = 1;
			a = (a + param) & 3;
			SetRotateAngle( (cr_rotate_angle_t)(a) );
		}
		break;
		case DCMD_ROTATE_SET: // rotate viewm param = 0..3 (0=normal, 1=90`, ...)
		{
			SetRotateAngle( (cr_rotate_angle_t)(param & 3 ) );
		}
		break;
#endif
	case DCMD_LINK_GO: {
		goSelectedLink();
	}
		break;
	case DCMD_LINK_BACK: {
		return goBack() ? 1 : 0;
	}
		break;
	case DCMD_LINK_FORWARD: {
		return goForward() ? 1 : 0;
	}
		break;
	case DCMD_LINEDOWN: {
		if (m_view_mode == DVM_SCROLL) {
			return SetPos(GetPos() + param * (m_font_size * 3 / 2));
		} else {
			int p = getCurPage();
			return goToPage(p + getVisiblePageNumberCount());
			//goToPage( m_pages.FindNearestPage(m_pos, +1));
		}
	}
		break;
	case DCMD_END: {
		if (getCurPage() < getPageCount() - getVisiblePageNumberCount()) {
			savePosToNavigationHistory();
			return SetPos(GetFullHeight());
		}
	}
		break;
	case DCMD_GO_POS: {
		if (m_view_mode == DVM_SCROLL) {
			return SetPos(param, true, true);
		} else {
			return goToPage(m_pages.FindNearestPage(param, 0), true);
		}
	}
		break;
	case DCMD_SCROLL_BY: {
		if (m_view_mode == DVM_SCROLL) {
			CRLog::trace("DCMD_SCROLL_BY %d", param);
			return SetPos(GetPos() + param);
		} else {
			CRLog::trace("DCMD_SCROLL_BY ignored: not in SCROLL mode");
		}
	}
		break;
	case DCMD_GO_PAGE: {
		if (getCurPage() != param) {
			savePosToNavigationHistory();
			return goToPage(param);
		}
	}
		break;
    case DCMD_GO_PAGE_DONT_SAVE_HISTORY: {
            if (getCurPage() != param) {
                return goToPage(param);
            }
        }
        break;
	case DCMD_ZOOM_IN: {
		ZoomFont(+1);
	}
		break;
	case DCMD_ZOOM_OUT: {
		ZoomFont(-1);
	}
		break;
	case DCMD_TOGGLE_TEXT_FORMAT: {
		if (getTextFormatOptions() == txt_format_auto)
			setTextFormatOptions( txt_format_pre);
		else
			setTextFormatOptions( txt_format_auto);
	}
		break;
    case DCMD_SET_TEXT_FORMAT: {
        CRLog::trace("DCMD_SET_TEXT_FORMAT(%d)", param);
        setTextFormatOptions(param ? txt_format_auto : txt_format_pre);
        REQUEST_RENDER("doCommand-set text format")
    }
        break;
    case DCMD_BOOKMARK_SAVE_N: {
		// save current page bookmark under spicified number
		saveCurrentPageShortcutBookmark(param);
	}
		break;
	case DCMD_BOOKMARK_GO_N: {
		// go to bookmark with specified number
		if (!goToPageShortcutBookmark(param)) {
			// if no bookmark exists with specified shortcut, save new bookmark instead
			saveCurrentPageShortcutBookmark(param);
		}
	}
		break;
	case DCMD_MOVE_BY_CHAPTER: {
		return moveByChapter(param);
	}
		break;
    case DCMD_SELECT_FIRST_SENTENCE:
    case DCMD_SELECT_NEXT_SENTENCE:
    case DCMD_SELECT_PREV_SENTENCE:
    case DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS: // move selection start by words
    case DCMD_SELECT_MOVE_RIGHT_BOUND_BY_WORDS: // move selection end by words
        return onSelectionCommand( cmd, param );

    /*
                ldomXPointerEx ptr( m_doc->getRootNode(), m_doc->getRootNode()->getChildCount());
                *out << "FORWARD ORDER:\n\n";
                //ptr.nextVisibleText();
                ptr.prevVisibleWordEnd();
                if ( ptr.thisSentenceStart() ) {
                    while ( 1 ) {
                        ldomXPointerEx ptr2(ptr);
                        ptr2.thisSentenceEnd();
                        ldomXRange range(ptr, ptr2);
                        lString32 str = range.getRangeText();
                        *out << ">sentence: " << UnicodeToUtf8(str) << "\n";
                        if ( !ptr.nextSentenceStart() )
                            break;
                    }
                }
    */
	default:
		// DO NOTHING
		break;
	}
	return 1;
}

int LVDocView::onSelectionCommand( int cmd, int param )
{
    CHECK_RENDER("onSelectionCommand()")
    LVRef<ldomXRange> pageRange = getPageDocumentRange();
    if (pageRange.isNull()) {
        clearSelection();
        return 0;
    }
    ldomXPointerEx pos( getBookmark() );
    ldomXRangeList & sel = getDocument()->getSelections();
    ldomXRange currSel;
    if ( sel.length()>0 )
        currSel = *sel[0];
    bool moved = false;
    bool makeSelStartVisible = true; // true: start, false: end
    if ( !currSel.isNull() && !pageRange->isInside(currSel.getStart()) && !pageRange->isInside(currSel.getEnd()) )
        currSel.clear();
    if ( currSel.isNull() || currSel.getStart().isNull() ) {
        // select first sentence on page
        if ( pos.isNull() ) {
            clearSelection();
            return 0;
        }
        if ( pos.thisSentenceStart() )
            currSel.setStart(pos);
        moved = true;
    }
    if ( currSel.getStart().isNull() ) {
        clearSelection();
        return 0;
    }
    if (cmd==DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS || cmd==DCMD_SELECT_MOVE_RIGHT_BOUND_BY_WORDS) {
        if (cmd==DCMD_SELECT_MOVE_RIGHT_BOUND_BY_WORDS)
            makeSelStartVisible = false;
        int dir = param>0 ? 1 : -1;
        int distance = param>0 ? param : -param;
        CRLog::debug("Changing selection by words: bound=%s dir=%d distance=%d", (cmd==DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS?"left":"right"), dir, distance);
        bool res;
        if (cmd==DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS) {
            // DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS
            for (int i=0; i<distance; i++) {
                if (dir>0) {
                    res = currSel.getStart().nextVisibleWordStart();
                    CRLog::debug("nextVisibleWordStart returned %s", res?"true":"false");
                } else {
                    res = currSel.getStart().prevVisibleWordStart();
                    CRLog::debug("prevVisibleWordStart returned %s", res?"true":"false");
                }
            }
            if (currSel.isNull()) {
                currSel.setEnd(currSel.getStart());
                currSel.getEnd().nextVisibleWordEnd();
            }
        } else {
            // DCMD_SELECT_MOVE_RIGHT_BOUND_BY_WORDS
            for (int i=0; i<distance; i++) {
                if (dir>0) {
                    res = currSel.getEnd().nextVisibleWordEnd();
                    CRLog::debug("nextVisibleWordEnd returned %s", res?"true":"false");
                } else {
                    res = currSel.getEnd().prevVisibleWordEnd();
                    CRLog::debug("prevVisibleWordEnd returned %s", res?"true":"false");
                }
            }
            if (currSel.isNull()) {
                currSel.setStart(currSel.getEnd());
                currSel.getStart().prevVisibleWordStart();
            }
        }
        // moved = true; // (never read)
    } else {
        // selection start doesn't match sentence bounds
        if ( !currSel.getStart().isSentenceStart() ) {
            currSel.getStart().thisSentenceStart();
            moved = true;
        }
        // update sentence end
        if ( !moved )
            switch ( cmd ) {
            case DCMD_SELECT_NEXT_SENTENCE:
                if ( !currSel.getStart().nextSentenceStart() )
                    return 0;
                break;
            case DCMD_SELECT_PREV_SENTENCE:
                if ( !currSel.getStart().prevSentenceStart() )
                    return 0;
                break;
            case DCMD_SELECT_FIRST_SENTENCE:
            default: // unknown action
                break;
        }
        currSel.setEnd(currSel.getStart());
        currSel.getEnd().thisSentenceEnd();
    }
    currSel.setFlags(1);
    selectRange(currSel);
    lvPoint startPoint = currSel.getStart().toPoint();
    lvPoint endPoint = currSel.getEnd().toPoint();
    int y0 = GetPos();
    int h = m_pageRects[0].height() - m_pageMargins.top
            - m_pageMargins.bottom - getPageHeaderHeight();
    //int y1 = y0 + h;
    if (makeSelStartVisible) {
        // make start of selection visible
        if (isScrollMode()) {
            if (startPoint.y < y0 + m_font_size * 2 || startPoint.y > y0 + h * 3/4)
                SetPos(startPoint.y - m_font_size * 2);
        } else {
            if (startPoint.y < y0 || startPoint.y >= y0 + h)
                SetPos(startPoint.y);
        }
        //goToBookmark(currSel.getStart());
    } else {
        // make end of selection visible
        if (isScrollMode()) {
            if (endPoint.y > y0 + h * 3/4 - m_font_size * 2)
                SetPos(endPoint.y - h * 3/4 + m_font_size * 2, false);
        } else {
            if (endPoint.y < y0 || endPoint.y >= y0 + h)
                SetPos(endPoint.y, false);
        }
    }
    CRLog::debug("Sel: %s", LCSTR(currSel.getRangeText()));
    return 1;
}

//static int cr_font_sizes[] = { 24, 29, 33, 39, 44 };
static int cr_interline_spaces[] = { 100, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150, 160, 180, 200 };

static const char * def_style_macros[] = {
    "styles.def.align", "text-align: justify",
    "styles.def.text-indent", "text-indent: 1.2em",
    "styles.def.margin-top", "margin-top: 0em",
    "styles.def.margin-bottom", "margin-bottom: 0em",
    "styles.def.margin-left", "margin-left: 0em",
    "styles.def.margin-right", "margin-right: 0em",
    "styles.title.align", "text-align: center",
    "styles.title.text-indent", "text-indent: 0em",
    "styles.title.margin-top", "margin-top: 0.3em",
    "styles.title.margin-bottom", "margin-bottom: 0.3em",
    "styles.title.margin-left", "margin-left: 0em",
    "styles.title.margin-right", "margin-right: 0em",
    "styles.title.font-size", "font-size: 110%",
    "styles.title.font-weight", "font-weight: bolder",
    "styles.subtitle.align", "text-align: center",
    "styles.subtitle.text-indent", "text-indent: 0em",
    "styles.subtitle.margin-top", "margin-top: 0.2em",
    "styles.subtitle.margin-bottom", "margin-bottom: 0.2em",
    "styles.subtitle.font-style", "font-style: italic",
    "styles.cite.align", "text-align: justify",
    "styles.cite.text-indent", "text-indent: 1.2em",
    "styles.cite.margin-top", "margin-top: 0.3em",
    "styles.cite.margin-bottom", "margin-bottom: 0.3em",
    "styles.cite.margin-left", "margin-left: 1em",
    "styles.cite.margin-right", "margin-right: 1em",
    "styles.cite.font-style", "font-style: italic",
    "styles.epigraph.align", "text-align: left",
    "styles.epigraph.text-indent", "text-indent: 1.2em",
    "styles.epigraph.margin-top", "margin-top: 0.3em",
    "styles.epigraph.margin-bottom", "margin-bottom: 0.3em",
    "styles.epigraph.margin-left", "margin-left: 15%",
    "styles.epigraph.margin-right", "margin-right: 1em",
    "styles.epigraph.font-style", "font-style: italic",
    "styles.pre.align", "text-align: left",
    "styles.pre.text-indent", "text-indent: 0em",
    "styles.pre.margin-top", "margin-top: 0em",
    "styles.pre.margin-bottom", "margin-bottom: 0em",
    "styles.pre.margin-left", "margin-left: 0em",
    "styles.pre.margin-right", "margin-right: 0em",
    "styles.pre.font-face", "font-family: \"Courier New\", \"Courier\", monospace",
    "styles.poem.align", "text-align: left",
    "styles.poem.text-indent", "text-indent: 0em",
    "styles.poem.margin-top", "margin-top: 0.3em",
    "styles.poem.margin-bottom", "margin-bottom: 0.3em",
    "styles.poem.margin-left", "margin-left: 15%",
    "styles.poem.margin-right", "margin-right: 1em",
    "styles.poem.font-style", "font-style: italic",
    "styles.text-author.font-style", "font-style: italic",
    "styles.text-author.font-weight", "font-weight: bolder",
    "styles.text-author.margin-left", "margin-left: 1em",
    "styles.text-author.font-style", "font-style: italic",
    "styles.text-author.font-weight", "font-weight: bolder",
    "styles.text-author.margin-left", "margin-left: 1em",
    "styles.link.text-decoration", "text-decoration: underline",
    "styles.footnote-link.font-size", "font-size: 70%",
    "styles.footnote-link.vertical-align", "vertical-align: super",
    "styles.footnote", "font-size: 70%",
    "styles.footnote-title", "font-size: 110%",
    "styles.annotation.font-size", "font-size: 90%",
    "styles.annotation.margin-left", "margin-left: 1em",
    "styles.annotation.margin-right", "margin-right: 1em",
    "styles.annotation.font-style", "font-style: italic",
    "styles.annotation.align", "text-align: justify",
    "styles.annotation.text-indent", "text-indent: 1.2em",
    NULL,
    NULL,
};

/// sets default property values if properties not found, checks ranges
void LVDocView::propsUpdateDefaults(CRPropRef props) {
	lString32Collection list;
	fontMan->getFaceList(list);
	static int def_aa_props[] = { 2, 1, 0 };

	props->setIntDef(PROP_MIN_FILE_SIZE_TO_CACHE,
            300000); // ~6M
	props->setIntDef(PROP_FORCED_MIN_FILE_SIZE_TO_CACHE,
			DOCUMENT_CACHING_MIN_SIZE); // 32K
	props->setIntDef(PROP_PROGRESS_SHOW_FIRST_PAGE, 1);

	props->limitValueList(PROP_FONT_ANTIALIASING, def_aa_props,
			sizeof(def_aa_props) / sizeof(int));
	props->setHexDef(PROP_FONT_COLOR, 0x000000);
	props->setHexDef(PROP_BACKGROUND_COLOR, 0xFFFFFF);
	props->setHexDef(PROP_STATUS_FONT_COLOR, 0xFF000000);
//	props->setIntDef(PROP_TXT_OPTION_PREFORMATTED, 0);
	props->setIntDef(PROP_AUTOSAVE_BOOKMARKS, 1);
	props->setIntDef(PROP_DISPLAY_FULL_UPDATE_INTERVAL, 1);
	props->setIntDef(PROP_DISPLAY_TURBO_UPDATE_MODE, 0);

	lString8 defFontFace;
	// static const char * goodFonts[] = { "DejaVu Sans", "FreeSans", "Liberation Sans", "Arial", "Verdana", NULL };
	// static const char * fallbackFonts = "Droid Sans Fallback|Noto Sans CJK SC|Noto Sans Arabic UI|Noto Sans Devanagari UI|FreeSans|FreeSerif|Noto Serif|Noto Sans|Arial Unicode MS";
	// KOReader: avoid loading many fonts on launch, as these will be re-set by frontend
	static const char * goodFonts[] = { "FreeSerif", NULL };
	static const char * fallbackFonts = "FreeSerif";
	for (int i = 0; goodFonts[i]; i++) {
		if (list.contains(lString32(goodFonts[i]))) {
			defFontFace = lString8(goodFonts[i]);
			break;
		}
	}
	if (defFontFace.empty())
		defFontFace = UnicodeToUtf8(list[0]);

	lString8 defStatusFontFace(DEFAULT_STATUS_FONT_NAME);
	props->setStringDef(PROP_FONT_FACE, defFontFace.c_str());
	props->setStringDef(PROP_STATUS_FONT_FACE, defStatusFontFace.c_str());
	if (list.length() > 0 && !list.contains(props->getStringDef(PROP_FONT_FACE,
			defFontFace.c_str())))
		props->setString(PROP_FONT_FACE, list[0]);
	props->setStringDef(PROP_FALLBACK_FONT_FACES, fallbackFonts);

#if USE_LIMITED_FONT_SIZES_SET
	props->setIntDef(PROP_FONT_SIZE,
			m_font_sizes[m_font_sizes.length() * 2 / 3]);
	props->limitValueList(PROP_FONT_SIZE, m_font_sizes.ptr(),
			m_font_sizes.length());
#else
	props->setIntDef(PROP_FONT_SIZE, m_min_font_size + (m_min_font_size + m_max_font_size)/7);
#endif
	props->limitValueList(PROP_INTERLINE_SPACE, cr_interline_spaces,
			sizeof(cr_interline_spaces) / sizeof(int));
#if CR_INTERNAL_PAGE_ORIENTATION==1
	static int def_rot_angle[] = {0, 1, 2, 3};
	props->limitValueList( PROP_ROTATE_ANGLE, def_rot_angle, 4 );
#endif
	static int bool_options_def_true[] = { 1, 0 };
	static int bool_options_def_false[] = { 0, 1 };
	static int int_option_weight[] = { 100, 200, 300, 400, 425, 450, 475, 500, 525, 550, 600, 650, 700, 800, 900, 950 };

	props->limitValueList(PROP_FONT_BASE_WEIGHT, int_option_weight, sizeof(int_option_weight) / sizeof(int), 3);
#ifndef ANDROID
	props->limitValueList(PROP_EMBEDDED_STYLES, bool_options_def_true, 2);
	props->limitValueList(PROP_EMBEDDED_FONTS, bool_options_def_true, 2);
#endif
	static int int_option_hinting[] = { 0, 1, 2 };
	props->limitValueList(PROP_FONT_HINTING, int_option_hinting, 3);
	static int int_option_kerning[] = { 0, 1, 2, 3 };
	props->limitValueList(PROP_FONT_KERNING, int_option_kerning, 4);
    static int int_options_1_2[] = { 2, 1 };
	props->limitValueList(PROP_LANDSCAPE_PAGES, int_options_1_2, 2);
	props->limitValueList(PROP_PAGES_TWO_VISIBLE_AS_ONE_PAGE_NUMBER, bool_options_def_false, 2);
	props->limitValueList(PROP_PAGE_VIEW_MODE, bool_options_def_true, 2);
	props->limitValueList(PROP_FOOTNOTES, bool_options_def_true, 2);
	props->limitValueList(PROP_SHOW_TIME, bool_options_def_false, 2);
	props->limitValueList(PROP_DISPLAY_INVERSE, bool_options_def_false, 2);
	props->limitValueList(PROP_BOOKMARK_ICONS, bool_options_def_false, 2);
	// props->limitValueList(PROP_FONT_KERNING_ENABLED, bool_options_def_false, 2);
    //props->limitValueList(PROP_FLOATING_PUNCTUATION, bool_options_def_true, 2);
    static int def_bookmark_highlight_modes[] = { 0, 1, 2 };
    props->setIntDef(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, highlight_mode_underline);
    props->limitValueList(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, def_bookmark_highlight_modes, sizeof(def_bookmark_highlight_modes)/sizeof(int));
    props->setColorDef(PROP_HIGHLIGHT_SELECTION_COLOR, 0xC0C0C0); // silver
    props->setColorDef(PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT, 0xA08020); // yellow
    props->setColorDef(PROP_HIGHLIGHT_BOOKMARK_COLOR_CORRECTION, 0xA04040); // red

    static int def_status_line[] = { 0, 1, 2 };
	props->limitValueList(PROP_STATUS_LINE, def_status_line, 3);
    static int def_margin[] = {8, 0, 1, 2, 3, 4, 5, 8, 10, 12, 14, 15, 16, 20, 25, 30, 40, 50, 60, 80, 100, 130, 150, 200, 300};
	props->limitValueList(PROP_PAGE_MARGIN_TOP, def_margin, sizeof(def_margin)/sizeof(int));
	props->limitValueList(PROP_PAGE_MARGIN_BOTTOM, def_margin, sizeof(def_margin)/sizeof(int));
	props->limitValueList(PROP_PAGE_MARGIN_LEFT, def_margin, sizeof(def_margin)/sizeof(int));
	props->limitValueList(PROP_PAGE_MARGIN_RIGHT, def_margin, sizeof(def_margin)/sizeof(int));
	static int def_updates[] = { 1, 0, 2, 3, 4, 5, 6, 7, 8, 10, 14 };
	props->limitValueList(PROP_DISPLAY_FULL_UPDATE_INTERVAL, def_updates, 11);
	int fs = props->getIntDef(PROP_STATUS_FONT_SIZE, INFO_FONT_SIZE);
    if (fs < MIN_STATUS_FONT_SIZE)
        fs = MIN_STATUS_FONT_SIZE;
    else if (fs > MAX_STATUS_FONT_SIZE)
        fs = MAX_STATUS_FONT_SIZE;
	props->setIntDef(PROP_STATUS_FONT_SIZE, fs);
	lString32 hyph = props->getStringDef(PROP_HYPHENATION_DICT,
			DEF_HYPHENATION_DICT);
	HyphDictionaryList * dictlist = HyphMan::getDictList();
	if (dictlist) {
		if (dictlist->find(hyph))
			props->setStringDef(PROP_HYPHENATION_DICT, hyph);
		else
			props->setStringDef(PROP_HYPHENATION_DICT, lString32(
					HYPH_DICT_ID_ALGORITHM));
	}
	props->setIntDef(PROP_STATUS_LINE, 0);
	props->setIntDef(PROP_SHOW_TITLE, 1);
	props->setIntDef(PROP_SHOW_TIME, 1);
	props->setIntDef(PROP_SHOW_TIME_12HOURS, 0);
	props->setIntDef(PROP_SHOW_BATTERY, 1);
    props->setIntDef(PROP_SHOW_BATTERY_PERCENT, 0);
    props->setIntDef(PROP_SHOW_PAGE_COUNT, 1);
    props->setIntDef(PROP_SHOW_PAGE_NUMBER, 1);
    props->setIntDef(PROP_SHOW_POS_PERCENT, 0);
    props->setIntDef(PROP_STATUS_CHAPTER_MARKS, 1);
    props->setIntDef(PROP_FLOATING_PUNCTUATION, 1);

#ifndef ANDROID
    props->setIntDef(PROP_EMBEDDED_STYLES, 1);
    props->setIntDef(PROP_EMBEDDED_FONTS, 1);
    props->setIntDef(PROP_TXT_OPTION_PREFORMATTED, 0);
    props->limitValueList(PROP_TXT_OPTION_PREFORMATTED, bool_options_def_false,
            2);
#endif

    props->setStringDef(PROP_FONT_GAMMA, "1.00");

    img_scaling_option_t defImgScaling;
    props->setIntDef(PROP_IMG_SCALING_ZOOMOUT_BLOCK_SCALE, defImgScaling.max_scale);
    props->setIntDef(PROP_IMG_SCALING_ZOOMOUT_INLINE_SCALE, 0); //auto
    props->setIntDef(PROP_IMG_SCALING_ZOOMIN_BLOCK_SCALE, defImgScaling.max_scale);
    props->setIntDef(PROP_IMG_SCALING_ZOOMIN_INLINE_SCALE, 0); // auto
    props->setIntDef(PROP_IMG_SCALING_ZOOMOUT_BLOCK_MODE, defImgScaling.mode);
    props->setIntDef(PROP_IMG_SCALING_ZOOMOUT_INLINE_MODE, defImgScaling.mode);
    props->setIntDef(PROP_IMG_SCALING_ZOOMIN_BLOCK_MODE, defImgScaling.mode);
    props->setIntDef(PROP_IMG_SCALING_ZOOMIN_INLINE_MODE, defImgScaling.mode);

    int p = props->getIntDef(PROP_FORMAT_SPACE_WIDTH_SCALE_PERCENT, DEF_SPACE_WIDTH_SCALE_PERCENT);
    if (p<10)
        p = 10;
    if (p>500)
        p = 500;
    props->setInt(PROP_FORMAT_SPACE_WIDTH_SCALE_PERCENT, p);

    p = props->getIntDef(PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT, DEF_MIN_SPACE_CONDENSING_PERCENT);
    if (p<25)
        p = 25;
    if (p>100)
        p = 100;
    props->setInt(PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT, p);

    p = props->getIntDef(PROP_FORMAT_UNUSED_SPACE_THRESHOLD_PERCENT, DEF_UNUSED_SPACE_THRESHOLD_PERCENT);
    if (p<0)
        p = 0;
    if (p>20)
        p = 20;
    props->setInt(PROP_FORMAT_UNUSED_SPACE_THRESHOLD_PERCENT, p);

    p = props->getIntDef(PROP_FORMAT_MAX_ADDED_LETTER_SPACING_PERCENT, DEF_MAX_ADDED_LETTER_SPACING_PERCENT);
    if (p<0)
        p = 0;
    if (p>20)
        p = 20;
    props->setInt(PROP_FORMAT_MAX_ADDED_LETTER_SPACING_PERCENT, p);

    p = props->getIntDef(PROP_FORMAT_CJK_WIDTH_SCALE_PERCENT, DEF_CJK_WIDTH_SCALE_PERCENT);
    if (p<100)
        p = 100;
    if (p>150)
        p = 150;
    props->setInt(PROP_FORMAT_CJK_WIDTH_SCALE_PERCENT, p);

    props->setIntDef(PROP_RENDER_DPI, DEF_RENDER_DPI); // 96 dpi
    props->setIntDef(PROP_RENDER_SCALE_FONT_WITH_DPI, DEF_RENDER_SCALE_FONT_WITH_DPI); // no scale
    props->setIntDef(PROP_RENDER_BLOCK_RENDERING_FLAGS, DEF_RENDER_BLOCK_RENDERING_FLAGS);

    props->setIntDef(PROP_FILE_PROPS_FONT_SIZE, 22);

    for (int i=0; def_style_macros[i*2]; i++)
        props->setStringDef(def_style_macros[i * 2], def_style_macros[i * 2 + 1]);
}

#define H_MARGIN 8
#define V_MARGIN 8
#define ALLOW_BOTTOM_STATUSBAR 0
void LVDocView::setStatusMode(int newMode, bool showClock, bool showTitle,
        bool showBattery, bool showChapterMarks, bool showPercent, bool showPageNumber, bool showPageCount) {
	CRLog::debug("LVDocView::setStatusMode(%d, %s %s %s %s)", newMode,
			showClock ? "clock" : "", showTitle ? "title" : "",
			showBattery ? "battery" : "", showChapterMarks ? "marks" : "");
#if ALLOW_BOTTOM_STATUSBAR==1
	lvRect margins( H_MARGIN, V_MARGIN, H_MARGIN, V_MARGIN/2 );
	lvRect oldMargins = _docview->getPageMargins( );
	if (newMode==1)
	margins.bottom = STANDARD_STATUSBAR_HEIGHT + V_MARGIN/4;
#endif
	if (newMode == 0)
        setPageHeaderInfo(
                  (showPageNumber ? PGHDR_PAGE_NUMBER : 0)
                | (showClock ? PGHDR_CLOCK : 0)
                | (showBattery ? PGHDR_BATTERY : 0)
                | (showPageCount ? PGHDR_PAGE_COUNT : 0)
				| (showTitle ? PGHDR_AUTHOR : 0)
				| (showTitle ? PGHDR_TITLE : 0)
				| (showChapterMarks ? PGHDR_CHAPTER_MARKS : 0)
                | (showPercent ? PGHDR_PERCENT : 0)
        //| PGHDR_CLOCK
		);
	else
		setPageHeaderInfo(0);
#if ALLOW_BOTTOM_STATUSBAR==1
	setPageMargins( margins );
#endif
}

bool LVDocView::propApply(lString8 name, lString32 value) {
    CRPropRef props = LVCreatePropsContainer();
    props->setString(name.c_str(), value);
    CRPropRef unknown = propsApply( props );
    return ( 0 == unknown->getCount() );
}

/// applies properties, returns list of not recognized properties
CRPropRef LVDocView::propsApply(CRPropRef props) {
    CRLog::trace("LVDocView::propsApply( %d items )", props->getCount());
    CRPropRef unknown = LVCreatePropsContainer();
    for (int i = 0; i < props->getCount(); i++) {
        lString8 name(props->getName(i));
        lString32 value = props->getValue(i);
        if (name == PROP_FONT_ANTIALIASING) {
            int antialiasingMode = props->getIntDef(PROP_FONT_ANTIALIASING, 2);
            fontMan->SetAntialiasMode(antialiasingMode);
            REQUEST_RENDER("propsApply - font antialiasing")
        } else if (name.startsWith(cs8("styles."))) {
            REQUEST_RENDER("propsApply - styles.*")
        } else if (name == PROP_FONT_GAMMA) {
            double gamma = 1.0;
            lString32 s = props->getStringDef(PROP_FONT_GAMMA, "1.0");
            // When parsing a gamma value string, the decimal point is always '.' char.
            // So if a different decimal point character is used in the current locale,
            // and if we use some library function to convert the string to floating point number, then it may fail.
            if (s.atod(gamma, '.')) {
                fontMan->SetGamma(gamma);
                clearImageCache();
            } else {
                CRLog::error("Invalid gamma value (%s)", LCSTR(s));
            }
        } else if (name == PROP_FONT_HINTING) {
            int mode = props->getIntDef(PROP_FONT_HINTING, (int)HINTING_MODE_AUTOHINT);
            if ((int)fontMan->GetHintingMode() != mode && mode>=0 && mode<=2) {
                //CRLog::debug("Setting hinting mode to %d", mode);
                fontMan->SetHintingMode((hinting_mode_t)mode);
                REQUEST_RENDER("propsApply - font hinting")
                    // requestRender() does m_doc->clearRendBlockCache(), which is needed
                    // on hinting mode change
            }
        } else if (name == PROP_HIGHLIGHT_SELECTION_COLOR || name == PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT || name == PROP_HIGHLIGHT_BOOKMARK_COLOR_COMMENT) {
            REQUEST_RENDER("propsApply - highlight")
        } else if (name == PROP_LANDSCAPE_PAGES) {
            int pages = props->getIntDef(PROP_LANDSCAPE_PAGES, 2);
            setVisiblePageCount(pages);
        } else if (name == PROP_PAGES_TWO_VISIBLE_AS_ONE_PAGE_NUMBER) {
            bool two_as_one = props->getBoolDef(PROP_PAGES_TWO_VISIBLE_AS_ONE_PAGE_NUMBER, false);
            setTwoVisiblePagesAsOnePageNumber(two_as_one);
        // } else if (name == PROP_FONT_KERNING_ENABLED) {
        //     bool kerning = props->getBoolDef(PROP_FONT_KERNING_ENABLED, false);
        //     fontMan->setKerning(kerning);
        //     REQUEST_RENDER("propsApply - kerning")
        } else if (name == PROP_FONT_KERNING) {
            int mode = props->getIntDef(PROP_FONT_KERNING, (int)KERNING_MODE_DISABLED);
            if ((int)fontMan->GetKerningMode() != mode && mode>=0 && mode<=3) {
                //CRLog::debug("Setting kerning mode to %d", mode);
                fontMan->SetKerningMode((kerning_mode_t)mode);
                REQUEST_RENDER("propsApply - font kerning")
            }
        } else if (name == PROP_FONT_BASE_WEIGHT) {
            // replaces PROP_FONT_WEIGHT_EMBOLDEN
            int v = props->getIntDef(PROP_FONT_BASE_WEIGHT, 400);
            if ( LVRendGetBaseFontWeight() != v ) {
                LVRendSetBaseFontWeight(v);
                REQUEST_RENDER("propsApply - font weight")
            }
        } else if (name == PROP_TXT_OPTION_PREFORMATTED) {
            bool preformatted = props->getBoolDef(PROP_TXT_OPTION_PREFORMATTED,
                                                  false);
            setTextFormatOptions(preformatted ? txt_format_pre
                                              : txt_format_auto);
        } else if (name == PROP_IMG_SCALING_ZOOMIN_INLINE_SCALE || name == PROP_IMG_SCALING_ZOOMIN_INLINE_MODE
                   || name == PROP_IMG_SCALING_ZOOMOUT_INLINE_SCALE || name == PROP_IMG_SCALING_ZOOMOUT_INLINE_MODE
                   || name == PROP_IMG_SCALING_ZOOMIN_BLOCK_SCALE || name == PROP_IMG_SCALING_ZOOMIN_BLOCK_MODE
                   || name == PROP_IMG_SCALING_ZOOMOUT_BLOCK_SCALE || name == PROP_IMG_SCALING_ZOOMOUT_BLOCK_MODE
                   ) {
            m_props->setString(name.c_str(), value);
            REQUEST_RENDER("propsApply -img scale")
        } else if (name == PROP_FONT_COLOR || name == PROP_BACKGROUND_COLOR
                   || name == PROP_DISPLAY_INVERSE || name==PROP_STATUS_FONT_COLOR) {
            // update current value in properties
            m_props->setString(name.c_str(), value);
            lUInt32 textColor = props->getColorDef(PROP_FONT_COLOR, m_props->getColorDef(PROP_FONT_COLOR, 0x000000));
            lUInt32 backColor = props->getColorDef(PROP_BACKGROUND_COLOR,
                                                   m_props->getIntDef(PROP_BACKGROUND_COLOR,
                                                                      0xFFFFFF));
            lUInt32 statusColor = props->getColorDef(PROP_STATUS_FONT_COLOR,
                                                     m_props->getColorDef(PROP_STATUS_FONT_COLOR,
                                                                          0xFF000000));
            bool inverse = props->getBoolDef(PROP_DISPLAY_INVERSE, m_props->getBoolDef(PROP_DISPLAY_INVERSE, false));
            if (inverse) {
                CRLog::trace("Setting inverse colors");
                //if (name == PROP_FONT_COLOR)
                setBackgroundColor(textColor);
                //if (name == PROP_BACKGROUND_COLOR)
                setTextColor(backColor);
                //if (name == PROP_BACKGROUND_COLOR)
                setStatusColor(backColor);
                REQUEST_RENDER("propsApply  color") // TODO: only colors to be changed
            } else {
                CRLog::trace("Setting normal colors");
                //if (name == PROP_BACKGROUND_COLOR)
                setBackgroundColor(backColor);
                //if (name == PROP_FONT_COLOR)
                setTextColor(textColor);
                //if (name == PROP_STATUS_FONT_COLOR)
                setStatusColor(statusColor);
                REQUEST_RENDER("propsApply  color") // TODO: only colors to be changed
            }
        } else if (name == PROP_PAGE_MARGIN_TOP || name
                   == PROP_PAGE_MARGIN_LEFT || name == PROP_PAGE_MARGIN_RIGHT
                   || name == PROP_PAGE_MARGIN_BOTTOM) {
            int margin = props->getIntDef(name.c_str(), 8);
            int maxmargin = (name == PROP_PAGE_MARGIN_LEFT || name == PROP_PAGE_MARGIN_RIGHT) ? m_dx / 3 : m_dy / 3;
            if (margin > maxmargin)
                margin = maxmargin;
            lvRect rc = getPageMargins();
            if (name == PROP_PAGE_MARGIN_TOP)
                rc.top = margin;
            else if (name == PROP_PAGE_MARGIN_BOTTOM)
                rc.bottom = margin;
            else if (name == PROP_PAGE_MARGIN_LEFT)
                rc.left = margin;
            else if (name == PROP_PAGE_MARGIN_RIGHT)
                rc.right = margin;
            setPageMargins(rc);
        } else if (name == PROP_FONT_FACE) {
            setDefaultFontFace(UnicodeToUtf8(value));
        } else if (name == PROP_FALLBACK_FONT_FACES) {
            lString8 oldFaces = fontMan->GetFallbackFontFaces();
            if ( UnicodeToUtf8(value)!=oldFaces )
                fontMan->SetFallbackFontFaces(UnicodeToUtf8(value));
            value = Utf8ToUnicode(fontMan->GetFallbackFontFaces());
            if ( UnicodeToUtf8(value) != oldFaces ) {
                REQUEST_RENDER("propsApply  fallback font faces")
            }
        } else if (name == PROP_FALLBACK_FONT_SIZES_ADJUSTED) {
            bool adjusted = props->getIntDef(PROP_FALLBACK_FONT_SIZES_ADJUSTED, false);
            if (fontMan->GetFallbackFontSizesAdjusted() != adjusted) {
                fontMan->SetFallbackFontSizesAdjusted(adjusted);
                REQUEST_RENDER("propsApply - adjusted fallback font sizes")
            }
        } else if (name == PROP_FONT_FAMILY_FACES) {
            if (m_doc) // not when noDefaultDocument=true
                getDocument()->setFontFamilyFonts(UnicodeToUtf8(value));
            REQUEST_RENDER("propsApply font family faces")
        } else if (name == PROP_STATUS_FONT_FACE) {
            setStatusFontFace(UnicodeToUtf8(value));
        } else if (name == PROP_STATUS_LINE || name == PROP_SHOW_TIME
                   || name	== PROP_SHOW_TITLE || name == PROP_SHOW_BATTERY
                   || name == PROP_STATUS_CHAPTER_MARKS || name == PROP_SHOW_POS_PERCENT
                   || name == PROP_SHOW_PAGE_COUNT || name == PROP_SHOW_PAGE_NUMBER) {
            m_props->setString(name.c_str(), value);
            setStatusMode(m_props->getIntDef(PROP_STATUS_LINE, 0),
                          m_props->getBoolDef(PROP_SHOW_TIME, false),
                          m_props->getBoolDef(PROP_SHOW_TITLE, true),
                          m_props->getBoolDef(PROP_SHOW_BATTERY, true),
                          m_props->getBoolDef(PROP_STATUS_CHAPTER_MARKS, true),
                          m_props->getBoolDef(PROP_SHOW_POS_PERCENT, false),
                          m_props->getBoolDef(PROP_SHOW_PAGE_NUMBER, true),
                          m_props->getBoolDef(PROP_SHOW_PAGE_COUNT, true)
                          );
            //} else if ( name==PROP_BOOKMARK_ICONS ) {
            //    enableBookmarkIcons( value==U"1" );
        } else if (name == PROP_FONT_SIZE) {
#if USE_LIMITED_FONT_SIZES_SET
            int fontSize = props->getIntDef(PROP_FONT_SIZE, m_font_sizes[0]);
#else
            int fontSize = props->getIntDef(PROP_FONT_SIZE, m_min_font_size);
#endif
            setFontSize(fontSize);//cr_font_sizes
            value = lString32::itoa(m_requested_font_size);
        } else if (name == PROP_STATUS_FONT_SIZE) {
            int fontSize = props->getIntDef(PROP_STATUS_FONT_SIZE,
                                            INFO_FONT_SIZE);
            if (fontSize < MIN_STATUS_FONT_SIZE)
                fontSize = MIN_STATUS_FONT_SIZE;
            else if (fontSize > MAX_STATUS_FONT_SIZE)
                fontSize = MAX_STATUS_FONT_SIZE;
            setStatusFontSize(fontSize);//cr_font_sizes
            value = lString32::itoa(fontSize);
        } else if (name == PROP_FONT_MONOSPACE_SCALE) {
            int scale = props->getIntDef(PROP_FONT_MONOSPACE_SCALE, 100);
            if (fontMan->GetMonospaceSizeScale() != scale) {
                fontMan->SetMonospaceSizeScale(scale);
                REQUEST_RENDER("propsApply - font monospace size scale")
            }
        } else if (name == PROP_HYPHENATION_DICT) {
            // hyphenation dictionary
            lString32 id = props->getStringDef(PROP_HYPHENATION_DICT,
                                               DEF_HYPHENATION_DICT);
            CRLog::debug("PROP_HYPHENATION_DICT = %s", LCSTR(id));
            HyphDictionaryList * list = HyphMan::getDictList();
            HyphDictionary * curr = HyphMan::getSelectedDictionary();
            if (list) {
                if (!curr || curr->getId() != id) {
                    //if (
                    CRLog::debug("Changing hyphenation to %s", LCSTR(id));
                    list->activate(id);
                    //)
                    REQUEST_RENDER("propsApply hyphenation dict")
                }
            }
        } else if (name == PROP_HYPHENATION_LEFT_HYPHEN_MIN) {
            int leftHyphenMin = props->getIntDef(PROP_HYPHENATION_LEFT_HYPHEN_MIN, HYPH_DEFAULT_HYPHEN_MIN);
            if (HyphMan::getLeftHyphenMin() != leftHyphenMin) {
                HyphMan::setLeftHyphenMin(leftHyphenMin);
                REQUEST_RENDER("propsApply hyphenation left_hyphen_min")
            }
        } else if (name == PROP_HYPHENATION_RIGHT_HYPHEN_MIN) {
            int rightHyphenMin = props->getIntDef(PROP_HYPHENATION_RIGHT_HYPHEN_MIN, HYPH_DEFAULT_HYPHEN_MIN);
            if (HyphMan::getRightHyphenMin() != rightHyphenMin) {
                HyphMan::setRightHyphenMin(rightHyphenMin);
                REQUEST_RENDER("propsApply hyphenation right_hyphen_min")
            }
        } else if (name == PROP_HYPHENATION_TRUST_SOFT_HYPHENS) {
            int trustSoftHyphens = props->getIntDef(PROP_HYPHENATION_TRUST_SOFT_HYPHENS, HYPH_DEFAULT_TRUST_SOFT_HYPHENS);
            if (HyphMan::getTrustSoftHyphens() != trustSoftHyphens) {
                HyphMan::setTrustSoftHyphens(trustSoftHyphens);
                REQUEST_RENDER("propsApply hyphenation trust_soft_hyphens")
            }
        } else if (name == PROP_TEXTLANG_MAIN_LANG) {
            lString32 lang = props->getStringDef(PROP_TEXTLANG_MAIN_LANG, TEXTLANG_DEFAULT_MAIN_LANG);
            if ( lang != TextLangMan::getMainLang() ) {
                TextLangMan::setMainLang( lang );
                REQUEST_RENDER("propsApply textlang main_lang")
            }
        } else if (name == PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED) {
            bool enabled = props->getIntDef(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, TEXTLANG_DEFAULT_EMBEDDED_LANGS_ENABLED);
            if ( enabled != TextLangMan::getEmbeddedLangsEnabled() ) {
                TextLangMan::setEmbeddedLangsEnabled( enabled );
                REQUEST_RENDER("propsApply textlang embedded_langs_enabled")
            }
        } else if (name == PROP_TEXTLANG_HYPHENATION_ENABLED) {
            bool enabled = props->getIntDef(PROP_TEXTLANG_HYPHENATION_ENABLED, TEXTLANG_DEFAULT_HYPHENATION_ENABLED);
            if ( enabled != TextLangMan::getHyphenationEnabled() ) {
                TextLangMan::setHyphenationEnabled( enabled );
                REQUEST_RENDER("propsApply textlang hyphenation_enabled")
            }
        } else if (name == PROP_TEXTLANG_HYPH_SOFT_HYPHENS_ONLY) {
            bool enabled = props->getIntDef(PROP_TEXTLANG_HYPH_SOFT_HYPHENS_ONLY, TEXTLANG_DEFAULT_HYPH_SOFT_HYPHENS_ONLY);
            if ( enabled != TextLangMan::getHyphenationSoftHyphensOnly() ) {
                TextLangMan::setHyphenationSoftHyphensOnly( enabled );
                REQUEST_RENDER("propsApply textlang hyphenation_soft_hyphens_only")
            }
        } else if (name == PROP_TEXTLANG_HYPH_USER_DICT) {
            lString32 dict = props->getStringDef(PROP_TEXTLANG_HYPH_USER_DICT, "");
            if ( UserHyphDict::init(dict) != USER_HYPH_DICT_ERROR_NOT_SORTED )
                REQUEST_RENDER("propsApply textlang user hyph dictionary");
        } else if (name == PROP_TEXTLANG_HYPH_FORCE_ALGORITHMIC) {
            bool enabled = props->getIntDef(PROP_TEXTLANG_HYPH_FORCE_ALGORITHMIC, TEXTLANG_DEFAULT_HYPH_FORCE_ALGORITHMIC);
            if ( enabled != TextLangMan::getHyphenationForceAlgorithmic() ) {
                TextLangMan::setHyphenationForceAlgorithmic( enabled );
                REQUEST_RENDER("propsApply textlang hyphenation_force_algorithmic")
            }
        } else if (name == PROP_INTERLINE_SPACE) {
            int interlineSpace = props->getIntDef(PROP_INTERLINE_SPACE,
                                                  cr_interline_spaces[0]);
            setDefaultInterlineSpace(interlineSpace);//cr_font_sizes
            value = lString32::itoa(m_def_interline_space);
#if CR_INTERNAL_PAGE_ORIENTATION==1
        } else if ( name==PROP_ROTATE_ANGLE ) {
            cr_rotate_angle_t angle = (cr_rotate_angle_t) (props->getIntDef( PROP_ROTATE_ANGLE, 0 )&3);
            SetRotateAngle( angle );
            value = lString32::itoa( m_rotateAngle );
#endif
        } else if (name == PROP_EMBEDDED_STYLES) {
            bool value = props->getBoolDef(PROP_EMBEDDED_STYLES, true);
            if (m_doc) // not when noDefaultDocument=true
                getDocument()->setDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES, value);
            REQUEST_RENDER("propsApply embedded styles")
        } else if (name == PROP_EMBEDDED_FONTS) {
            bool value = props->getBoolDef(PROP_EMBEDDED_FONTS, true);
            if (m_doc) // not when noDefaultDocument=true
                getDocument()->setDocFlag(DOC_FLAG_ENABLE_DOC_FONTS, value);
            REQUEST_RENDER("propsApply doc fonts")
        } else if (name == PROP_NONLINEAR_PAGEBREAK) {
            bool value = props->getBoolDef(PROP_NONLINEAR_PAGEBREAK, false);
            if (m_doc) // not when noDefaultDocument=true
                getDocument()->setDocFlag(DOC_FLAG_NONLINEAR_PAGEBREAK, value);
            REQUEST_RENDER("propsApply nonlinear")
        } else if (name == PROP_FOOTNOTES) {
            bool value = props->getBoolDef(PROP_FOOTNOTES, true);
            if (m_doc) // not when noDefaultDocument=true
                getDocument()->setDocFlag(DOC_FLAG_ENABLE_FOOTNOTES, value);
            REQUEST_RENDER("propsApply footnotes")
        } else if (name == PROP_FLOATING_PUNCTUATION) {
            bool value = props->getBoolDef(PROP_FLOATING_PUNCTUATION, true);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setHangingPunctiationEnabled(value)) {
                    REQUEST_RENDER("propsApply - hanging punctuation")
                    // requestRender() does m_doc->clearRendBlockCache(), which is needed
                    // on hanging punctuation change
                }
        } else if (name == PROP_REQUESTED_DOM_VERSION) {
            int value = props->getIntDef(PROP_REQUESTED_DOM_VERSION, gDOMVersionCurrent);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setDOMVersionRequested(value)) {
                    REQUEST_RENDER("propsApply requested dom version")
                }
        } else if (name == PROP_RENDER_BLOCK_RENDERING_FLAGS) {
            lUInt32 value = (lUInt32)props->getIntDef(PROP_RENDER_BLOCK_RENDERING_FLAGS, DEF_RENDER_BLOCK_RENDERING_FLAGS);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setRenderBlockRenderingFlags(value))
                    REQUEST_RENDER("propsApply render block rendering flags")
        } else if (name == PROP_RENDER_DPI) {
            int value = props->getIntDef(PROP_RENDER_DPI, DEF_RENDER_DPI);
            if ( gRenderDPI != value ) {
                gRenderDPI = value;
                REQUEST_RENDER("propsApply render dpi")
            }
        } else if (name == PROP_RENDER_SCALE_FONT_WITH_DPI) {
            int value = props->getIntDef(PROP_RENDER_SCALE_FONT_WITH_DPI, DEF_RENDER_SCALE_FONT_WITH_DPI);
            if ( gRenderScaleFontWithDPI != value ) {
                gRenderScaleFontWithDPI = value;
                REQUEST_RENDER("propsApply render scale font with dpi")
            }
        } else if (name == PROP_FORMAT_SPACE_WIDTH_SCALE_PERCENT) {
            int value = props->getIntDef(PROP_FORMAT_SPACE_WIDTH_SCALE_PERCENT, DEF_SPACE_WIDTH_SCALE_PERCENT);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setSpaceWidthScalePercent(value))
                    REQUEST_RENDER("propsApply space width scale percent")
        } else if (name == PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT) {
            int value = props->getIntDef(PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT, DEF_MIN_SPACE_CONDENSING_PERCENT);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setMinSpaceCondensingPercent(value))
                    REQUEST_RENDER("propsApply condensing percent")
        } else if (name == PROP_FORMAT_UNUSED_SPACE_THRESHOLD_PERCENT) {
            int value = props->getIntDef(PROP_FORMAT_UNUSED_SPACE_THRESHOLD_PERCENT, DEF_UNUSED_SPACE_THRESHOLD_PERCENT);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setUnusedSpaceThresholdPercent(value))
                    REQUEST_RENDER("propsApply unused space threshold percent")
        } else if (name == PROP_FORMAT_MAX_ADDED_LETTER_SPACING_PERCENT) {
            int value = props->getIntDef(PROP_FORMAT_MAX_ADDED_LETTER_SPACING_PERCENT, DEF_MAX_ADDED_LETTER_SPACING_PERCENT);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setMaxAddedLetterSpacingPercent(value))
                    REQUEST_RENDER("propsApply max added letter spacing percent")
        } else if (name == PROP_FORMAT_CJK_WIDTH_SCALE_PERCENT) {
            int value = props->getIntDef(PROP_FORMAT_CJK_WIDTH_SCALE_PERCENT, DEF_CJK_WIDTH_SCALE_PERCENT);
            if (m_doc) // not when noDefaultDocument=true
                if (getDocument()->setCJKWidthScalePercent(value))
                    REQUEST_RENDER("propsApply CJK width scale percent")
        } else if (name == PROP_HIGHLIGHT_COMMENT_BOOKMARKS) {
            int value = props->getIntDef(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, highlight_mode_underline);
            if (m_highlightBookmarks != value) {
                m_highlightBookmarks = value;
                updateBookMarksRanges();
            }
            REQUEST_RENDER("propsApply - PROP_HIGHLIGHT_COMMENT_BOOKMARKS")
        } else if (name == PROP_PAGE_VIEW_MODE) {
            LVDocViewMode m =
                    props->getIntDef(PROP_PAGE_VIEW_MODE, 1) ? DVM_PAGES
                                                             : DVM_SCROLL;
            setViewMode(m);
        } else if (name == PROP_CACHE_VALIDATION_ENABLED) {
            bool value = props->getBoolDef(PROP_CACHE_VALIDATION_ENABLED, true);
            enableCacheFileContentsValidation(value);
        } else {

            // unknown property, adding to list of unknown properties
            unknown->setString(name.c_str(), value);
        }
        // Update current value in properties
        // Even if not used above to set anything if no m_doc, this saves
        // the value in m_props so it might be used by createEmptyDocument()
        // when creating the coming up document (and further documents
        // if the value is not re-set).
        m_props->setString(name.c_str(), value);
    }
    return unknown;
}

/// returns current values of supported properties
CRPropRef LVDocView::propsGetCurrent() {
	return m_props;
}

void LVPageWordSelector::updateSelection()
{
    LVArray<ldomWord> list;
    if ( _words.getSelWord() )
        list.add(_words.getSelWord()->getWord() );
    if ( list.length() )
        _docview->selectWords(list);
    else
        _docview->clearSelection();
}

LVPageWordSelector::~LVPageWordSelector()
{
    _docview->clearSelection();
}

LVPageWordSelector::LVPageWordSelector( LVDocView * docview )
    : _docview(docview)
{
    LVRef<ldomXRange> range = _docview->getPageDocumentRange();
    if (!range.isNull()) {
		_words.addRangeWords(*range, true);
                if (_docview->getVisiblePageCount() > 1) { // _docview->isPageMode() &&
                        // process second page
                        int pageNumber = _docview->getCurPage(true);
                        range = _docview->getPageDocumentRange(pageNumber + 1);
                        if (!range.isNull())
                            _words.addRangeWords(*range, true);
                }
		_words.selectMiddleWord();
		updateSelection();
	}
}

void LVPageWordSelector::moveBy( MoveDirection dir, int distance )
{
    _words.selectNextWord(dir, distance);
    updateSelection();
}

void LVPageWordSelector::selectWord(int x, int y)
{
	ldomWordEx * word = _words.findNearestWord(x, y, DIR_ANY);
	_words.selectWord(word, DIR_ANY);
	updateSelection();
}

// append chars to search pattern
ldomWordEx * LVPageWordSelector::appendPattern( lString32 chars )
{
    ldomWordEx * res = _words.appendPattern(chars);
    if ( res )
        updateSelection();
    return res;
}

// remove last item from pattern
ldomWordEx * LVPageWordSelector::reducePattern()
{
    ldomWordEx * res = _words.reducePattern();
    if ( res )
        updateSelection();
    return res;
}

SimpleTitleFormatter::SimpleTitleFormatter(lString32 text, lString8 fontFace, bool bold, bool italic, lUInt32 color, int maxWidth, int maxHeight, int fntSize) : _text(text), _fontFace(fontFace), _bold(bold), _italic(italic), _color(color), _maxWidth(maxWidth), _maxHeight(maxHeight), _fntSize(fntSize) {
    if (_text.length() > 80)
        _text = _text.substr(0, 80) + "...";
    if (findBestSize())
        return;
    _text = _text.substr(0, 50) + "...";
    if (findBestSize())
        return;
    _text = _text.substr(0, 32) + "...";
    if (findBestSize())
        return;
    _text = _text.substr(0, 16) + "...";
    if (findBestSize())
        return;
    // Fallback to tiny font
    format(2);
}

bool SimpleTitleFormatter::measure() {
    _width = 0;
    _height = 0;
    for (int i=_lines.length() - 1; i >= 0; i--) {
        lString32 line = _lines[i].trim();
        int w = _font->getTextWidth(line.c_str(), line.length());
        if (w > _width)
            _width = w;
        _height += _lineHeight;
    }
    return _width < _maxWidth && _height < _maxHeight;
}
bool SimpleTitleFormatter::splitLines(const char * delimiter) {
    lString32 delim32(delimiter);
    int bestpos = -1;
    int bestdist = -1;
    int start = 0;
    bool skipDelimiter = *delimiter == '|';
    for (;;) {
        int p = _text.pos(delim32, start);
        if (p < 0)
            break;
        int dist = _text.length() / 2 - p;
        if (dist < 0)
            dist = -dist;
        if (bestdist == -1 || dist < bestdist) {
            bestdist = dist;
            bestpos = p;
        }
        start = p + 1;
    }
    if (bestpos < 0)
        return false;
    _lines.add(_text.substr(0, bestpos + (skipDelimiter ? 0 : delim32.length())).trim());
    _lines.add(_text.substr(bestpos + delim32.length()).trim());
    return measure();
}
bool SimpleTitleFormatter::format(int fontSize) {
    _font = fontMan->GetFont(fontSize, _bold ? 800 : 400, _italic, css_ff_sans_serif, _fontFace, 0, -1);
    _lineHeight = _font->getHeight() * 120 / 100;
    _lines.clear();
    _height = 0;
    int singleLineWidth = _font->getTextWidth(_text.c_str(), _text.length());
    if (singleLineWidth < _maxWidth) {
        _lines.add(_text);
        _width = singleLineWidth;
        _height = _lineHeight;
        return _width < _maxWidth && _height < _maxHeight;
    }
    if (splitLines("|"))
        return true;
    if (splitLines(","))
        return true;
    if (splitLines(";"))
        return true;
    if (splitLines(":"))
        return true;
    if (splitLines("-"))
        return true;
    if (splitLines(" "))
        return true;
    if (splitLines("_"))
        return true;
    if (splitLines("."))
        return true;
    _lines.clear();
    int p = _text.length() / 2;
    _lines.add(_text.substr(0, p));
    _lines.add(_text.substr(p, _text.length() - p));
    return false;
}
bool SimpleTitleFormatter::findBestSize() {
    if (_fntSize) {
        format(_fntSize);
        return true;
    }
    int maxSizeW = _maxWidth / 10;
    int maxSizeH = _maxHeight / 3;
    int maxSize = maxSizeW < maxSizeH ? maxSizeW : maxSizeH;
    if (maxSize > 50)
        maxSize = 50;
    int minSize = 11;
    for (int size = maxSize; size >= minSize; ) {
        if (format(size))
            return true;
        if (size > 30)
            size -= 3;
        else if (size > 20)
            size -= 2;
        else
            size--;
    }
    return false;
}
void SimpleTitleFormatter::draw(LVDrawBuf & buf, lString32 str, int x, int y, int align) {
    int w = _font->getTextWidth(str.c_str(), str.length());
    if (align == 0)
        x -= w / 2; // center
    else if (align == 1)
        x -= w; // right
    buf.SetTextColor(_color);
    _font->DrawTextString(&buf, x, y, str.c_str(), str.length(), '?');
}
void SimpleTitleFormatter::draw(LVDrawBuf & buf, lvRect rc, int halign, int valign) {
    int y0 = rc.top;
    if (valign == 0)
        y0 += (rc.height() - _lines.length() * _lineHeight) / 2;
    int x0 = halign < 0 ? rc.left : (halign > 0 ? rc.right : (rc.left + rc.right) / 2);
    for (int i=0; i<_lines.length(); i++) {
        draw(buf, _lines[i], x0, y0, halign);
        y0 += _lineHeight;
    }
}

struct cover_palette_t {
    lUInt32 frame;
    lUInt32 bg;
    lUInt32 hline;
    lUInt32 vline;
    lUInt32 title;
    lUInt32 authors;
    lUInt32 series;
    lUInt32 titleframe;
};

static cover_palette_t gray_palette[1] = {
    // frame,    bg,         hline,      vline,      title,      authors,    series      titleframe
    {0x00C0C0C0, 0x00FFFFFF, 0xC0FFC040, 0xC0F0D060, 0x00000000, 0x00000000, 0x00000000, 0x40FFFFFF},
};

static cover_palette_t normal_palette[8] = {
    // frame,    bg,         hline,      vline,      title,      authors,    series
    {0x00D0D0D0, 0x00E8E0E0, 0xC0CFC040, 0xC0F0E060, 0x00600000, 0x00000040, 0x00404040, 0x80FFFFFF},
    {0x00D0D0D0, 0x00E0E8E0, 0xC0FFC040, 0xD0F0D080, 0x00800000, 0x00000080, 0x00406040, 0x80FFFFFF},
    {0x00D0D0D0, 0x00E0E0E8, 0xC0CFC040, 0xC0F0D060, 0x00A00000, 0x00000060, 0x00404060, 0x80FFEFFF},
    {0x00D0D0D0, 0x00E0E8E8, 0xC0FFC040, 0xD0F0E060, 0x00800000, 0x00000080, 0x00406040, 0x80FFFFFF},
    {0x00D0D0D0, 0x00E8E8E0, 0xC0EFC040, 0xC0F0D060, 0x00400000, 0x00000050, 0x00404040, 0x80FFEFFF},
    {0x00D0D0D0, 0x00E8E0E8, 0xC0FFC040, 0xC0F0E040, 0x00000000, 0x00000080, 0x00606040, 0x80FFFFFF},
    {0x00D0D0D0, 0x00E8E8E8, 0xC0EFC040, 0xD0F0D060, 0x00600000, 0x000000A0, 0x00402040, 0x80FFEFFF},
    {0x00D0D0D0, 0x00E0F0E0, 0xC0FFC040, 0xC0F0D080, 0x00400000, 0x00000080, 0x00402020, 0x80FFFFFF},
};

static cover_palette_t series_palette[8] = {
    // frame,    bg,         hline,      vline,      title,      authors,    series
    {0x00C0D0C0, 0x20DFD0E0, 0xD0FFC040, 0xD0F0D080, 0x00800000, 0x00000080, 0x00406040, 0x80FFFFFF},
    {0x00C0C0D0, 0x00D0FFD0, 0xD0EFC040, 0xD0D0F060, 0x00400000, 0x00400040, 0x00404040, 0xA0FFFFFF},
    {0x00B0C0C0, 0x20D0D0FF, 0xE0FFC040, 0xD0F0D0E0, 0x00600000, 0x00000080, 0x00406040, 0x60FFEFFF},
    {0x00C0D0C0, 0x20D0FFFF, 0xC0EFC040, 0xD0FFD060, 0x00804000, 0x00000060, 0x00402020, 0x80FFFFFF},
    {0x00C0C0B0, 0x20FFFFD0, 0xC0EFC040, 0xD0FFD060, 0x00400040, 0x00000060, 0x00406040, 0xB0FFEFFF},
    {0x00D0C0C0, 0x20DFE0FF, 0xE0FFC040, 0xD0FFD0E0, 0x00804040, 0x00000080, 0x00204040, 0xA0FFFFFF},
    {0x00D0C0D0, 0x20E0E0F0, 0xC0FFC040, 0xD0FFE0E0, 0x00400000, 0x00400040, 0x00204040, 0xB0FFEFFF},
    {0x00C0D0C0, 0x20E8E8D8, 0xC0FFC040, 0xD0F0E0E0, 0x00400040, 0x00000080, 0x00402040, 0x80FFFFFF},
};

void LVDrawBookCover(LVDrawBuf & buf, LVImageSourceRef image, bool respectAspectRatio, lString8 fontFace, lString32 title, lString32 authors, lString32 seriesName, int seriesNumber) {
    CR_UNUSED(seriesNumber);
    bool isGray = buf.GetBitsPerPixel() <= 8;
    cover_palette_t * palette = NULL;
    if (isGray)
        palette = &gray_palette[0];
    else if (!seriesName.empty())
        palette = &series_palette[getHash(seriesName) & 7];
    else if (!authors.empty())
        palette = &normal_palette[getHash(authors) & 7];
    else
        palette = &normal_palette[getHash(title) & 7];
    int dx = buf.GetWidth();
    int dy = buf.GetHeight();
    if (!image.isNull() && image->GetWidth() > 0 && image->GetHeight() > 0) {
        int xoff = 0;
        int yoff = 0;
        if (respectAspectRatio) {
            // recalc dx, dy for respectAspectRatio
            int dst_aspect = 100*dx/dy;
            int src_aspect = 100*image->GetWidth()/image->GetHeight();
            if (dst_aspect > src_aspect) {
                int new_dx = src_aspect*dy/100;
                xoff = (dx - new_dx + 1)/2;
                dx = new_dx;
            } else if (dst_aspect < src_aspect) {
                int new_dy = 100*dx/src_aspect;
                yoff = (dy - new_dy + 1)/2;
                dy = new_dy;
            }
        }
        CRLog::trace("drawing image cover page %d x %d", dx, dy);
        buf.Draw(image, xoff, yoff, dx, dy);
		return;
	}


    CRLog::trace("drawing default cover page %d x %d", dx, dy);
	lvRect rc(0, 0, buf.GetWidth(), buf.GetHeight());
    buf.FillRect(rc, palette->frame);
	rc.shrink(rc.width() / 40);
    buf.FillRect(rc, palette->bg);

	lvRect rc2(rc);
	rc2.top = rc.height() * 8 / 10;
	rc2.bottom = rc2.top + rc.height() / 15;
    buf.FillRect(rc2, palette->hline);

	lvRect rc3(rc);
	rc3.left += rc.width() / 30;
	rc3.right = rc3.left + rc.width() / 30;
    buf.FillRect(rc3, palette->vline);


	LVFontRef fnt = fontMan->GetFont(16, 400, false, css_ff_sans_serif, fontFace, 0, -1); // = fontMan
	if (!fnt.isNull()) {

		rc.left += rc.width() / 10;
		rc.right -= rc.width() / 20;

        lUInt32 titleColor = palette->title;
        lUInt32 authorColor = palette->authors;
        lUInt32 seriesColor = palette->series;

		lvRect authorRc(rc);

		if (!authors.empty()) {
			authorRc.top += rc.height() * 1 / 20;
			authorRc.bottom = authorRc.top + rc.height() * 2 / 10;
			SimpleTitleFormatter authorFmt(authors, fontFace, false, false, authorColor, authorRc.width(), authorRc.height());
			authorFmt.draw(buf, authorRc, 0, 0);
		} else {
			authorRc.bottom = authorRc.top;
		}

		if (!title.empty()) {
			lvRect titleRc(rc);
			titleRc.top += rc.height() * 4 / 10;
			titleRc.bottom = titleRc.top + rc.height() * 7 / 10;

			lvRect rc3(titleRc);
			rc3.top -= rc.height() / 20;
			rc3.bottom = rc3.top + rc.height() / 40;
            buf.FillRect(rc3, palette->titleframe);

			SimpleTitleFormatter titleFmt(title, fontFace, true, false, titleColor, titleRc.width(), titleRc.height());
			titleFmt.draw(buf, titleRc, -1, -1);

			rc3.top += titleFmt.getHeight() + rc.height() / 20;
			rc3.bottom = rc3.top + rc.height() / 40;
            buf.FillRect(rc3, palette->titleframe);
		}

		if (!seriesName.empty()) {
			lvRect seriesRc(rc);
			seriesRc.top += rc.height() * 8 / 10;
			//seriesRc.bottom = rc.top + rc.height() * 9 / 10;
			SimpleTitleFormatter seriesFmt(seriesName, fontFace, false, true, seriesColor, seriesRc.width(), seriesRc.height());
			seriesFmt.draw(buf, seriesRc, 1, 0);
		}

    } else {
        CRLog::error("Cannot get font for coverpage");
    }
}
