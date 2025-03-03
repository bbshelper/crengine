/** \file lvxml.h
    \brief XML parser

   CoolReader Engine

   (c) Vadim Lopatin, 2000-2006

   This source code is distributed under the terms of
   GNU General Public License.

   See LICENSE file for details.

*/

#ifndef __LVXML_H_INCLUDED__
#define __LVXML_H_INCLUDED__

#include <time.h>
#include "lvtypes.h"
#include "lvstring.h"
#include "lvstream.h"
#include "crtxtenc.h"
#include "dtddef.h"

#define XML_CHAR_BUFFER_SIZE 4096
#define XML_FLAG_NO_SPACE_TEXT 1

//class LVXMLParser;
class LVFileFormatParser;

class tinyNode;
struct ldomNode;

/// XML parser callback interface
class LVXMLParserCallback
{
protected:
    LVFileFormatParser * _parser;
public:
    /// returns flags
    virtual lUInt32 getFlags() { return 0; }
    /// sets flags
    virtual void setFlags( lUInt32 ) { }
    /// called on document encoding definition
    virtual void OnEncoding( const lChar32 *, const lChar32 * ) { }
    /// called on parsing start
    virtual void OnStart(LVFileFormatParser * parser) { _parser = parser; }
    /// called on parsing end
    virtual void OnStop() = 0;
    /// called on opening tag <
    virtual ldomNode * OnTagOpen( const lChar32 * nsname, const lChar32 * tagname) = 0;
    /// called after > of opening tag (when entering tag body)
    virtual void OnTagBody() = 0;
    /// calls OnTagOpen & OnTagBody
    virtual void OnTagOpenNoAttr( const lChar32 * nsname, const lChar32 * tagname)
    {
        OnTagOpen( nsname, tagname);
        OnTagBody();
    }
    /// calls OnTagOpen & OnTagClose
    virtual void OnTagOpenAndClose( const lChar32 * nsname, const lChar32 * tagname)
    {
        OnTagOpen( nsname, tagname );
        OnTagBody();
        OnTagClose( nsname, tagname, true );
    }
    /// called on tag close
    virtual void OnTagClose( const lChar32 * nsname, const lChar32 * tagname, bool self_closing_tag=false ) = 0;
    /// called on element attribute
    virtual void OnAttribute( const lChar32 * nsname, const lChar32 * attrname, const lChar32 * attrvalue ) = 0;
    /// called on text
    virtual void OnText( const lChar32 * text, int len, lUInt32 flags ) = 0;
    /// add named BLOB data to document
    virtual bool OnBlob(lString32 name, const lUInt8 * data, int size) = 0;
    /// call to set document property
    virtual void OnDocProperty(const char * /*name*/, lString8 /*value*/) { }
    /// destructor
    virtual ~LVXMLParserCallback() {}
};

/// don't treat CR/LF and TAB characters as space nor remove duplicate spaces
#define TXTFLG_PRE        1
/// text is to be interpreted literally, as textual data, not as marked up or entity references
#define TXTFLG_CDATA      2

#define TXTFLG_TRIM                         4
#define TXTFLG_TRIM_ALLOW_START_SPACE       8
#define TXTFLG_TRIM_ALLOW_END_SPACE         16
#define TXTFLG_TRIM_REMOVE_EOL_HYPHENS      32
#define TXTFLG_RTF                          64
#define TXTFLG_PRE_PARA_SPLITTING           128
#define TXTFLG_ENCODING_MASK                0xFF00
#define TXTFLG_ENCODING_SHIFT               8
#define TXTFLG_CONVERT_8BIT_ENTITY_ENCODING 0x10000
#define TXTFLG_PROCESS_ATTRIBUTE            0x20000
#define TXTFLG_CASE_SENSITIVE_TAGS_ATTRS    0x40000

/// converts XML text: decode character entities, convert space chars
void PreProcessXmlString( lString32 & s, lUInt32 flags, const lChar32 * enc_table=NULL );
/// converts XML text in-place: decode character entities, convert space chars, returns new length of string
int PreProcessXmlString(lChar32 * str, int len, lUInt32 flags, const lChar32 * enc_table = NULL);

#define MAX_PERSISTENT_BUF_SIZE 16384

class LVDocViewCallback;

/// base class for all document format parsers
class LVFileFormatParser
{
public:
    /// returns pointer to loading progress callback object
    virtual LVDocViewCallback * getProgressCallback() { return NULL; }
    /// sets pointer to loading progress callback object
    virtual void setProgressCallback( LVDocViewCallback * /*callback*/ ) { }
    /// returns true if format is recognized by parser
    virtual bool CheckFormat() = 0;
    /// parses input stream
    virtual bool Parse() = 0;
    /// resets parsing, moves to beginning of stream
    virtual void Reset() = 0;
    /// stops parsing in the middle of file, to read header only
    virtual void Stop() = 0;
    /// sets charset by name
    virtual void SetCharset( const lChar32 * name ) = 0;
    /// sets 8-bit charset conversion table (128 items, for codes 128..255)
    virtual void SetCharsetTable( const lChar32 * table ) = 0;
    /// returns 8-bit charset conversion table (128 items, for codes 128..255)
    virtual lChar32 * GetCharsetTable( ) = 0;
    /// changes space mode
    virtual void SetSpaceMode( bool ) { }
    /// returns space mode
    virtual bool GetSpaceMode() { return false; }
    /// virtual destructor
    virtual ~LVFileFormatParser();
};

class LVFileParserBase : public LVFileFormatParser
{
protected:
    LVStreamRef m_stream;
    lUInt8 * m_buf;
    int      m_buf_size;
    lvsize_t m_stream_size;
    int      m_buf_len;
    int      m_buf_pos;
    lvpos_t  m_buf_fpos;
    bool     m_stopped; // true if Stop() is called
    LVDocViewCallback * m_progressCallback;
    time_t   m_lastProgressTime;
    int      m_progressLastPercent;
    int      m_progressUpdateCounter;
    int      m_firstPageTextCounter;
    /// fills buffer, to provide specified number of bytes for read
    bool FillBuffer( int bytesToRead );
    /// seek to specified stream position
    bool Seek( lvpos_t pos, int bytesToPrefetch=0 );
    /// override to return file reading position percent
    virtual int getProgressPercent();
public:
    /// call to send progress update to callback, if timeout expired
    void updateProgress();
    /// returns pointer to loading progress callback object
    virtual LVDocViewCallback * getProgressCallback();
    /// sets pointer to loading progress callback object
    virtual void setProgressCallback( LVDocViewCallback * callback );
    /// constructor
    LVFileParserBase( LVStreamRef stream );
    /// virtual destructor
    virtual ~LVFileParserBase();
    /// returns source stream
    LVStreamRef getStream() { return m_stream; }
    /// return stream file name
    lString32 getFileName();
    /// returns true if end of fle is reached, and there is no data left in buffer
    virtual bool Eof() { return m_buf_fpos + m_buf_pos >= m_stream_size; }
    /// resets parsing, moves to beginning of stream
    virtual void Reset();
    /// stops parsing in the middle of file, to read header only
    virtual void Stop();
};

class LVTextFileBase : public LVFileParserBase
{
protected:
    char_encoding_type m_enc_type;
    lString32 m_txt_buf;
    lString32 m_encoding_name;
    lString32 m_lang_name;
    lChar32 * m_conv_table; // charset conversion table for 8-bit encodings

    lChar32 m_read_buffer[XML_CHAR_BUFFER_SIZE];
    int m_read_buffer_len;
    int m_read_buffer_pos;
    bool m_eof;

    void checkEof();

    inline lChar32 ReadCharFromBuffer()
    {
        if ( m_read_buffer_pos >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return 0;
            }
        }
        return m_read_buffer[m_read_buffer_pos++];
    }
    inline lChar32 PeekCharFromBuffer()
    {
        if ( m_read_buffer_pos >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return 0;
            }
        }
        return m_read_buffer[m_read_buffer_pos];
    }
    inline lChar32 PeekCharFromBuffer( int offset )
    {
        if ( m_read_buffer_pos + offset >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return 0;
            }
            if ( m_read_buffer_pos + offset >= m_read_buffer_len )
                return 0;
        }
        return m_read_buffer[m_read_buffer_pos + offset];
    }
    // skip current char (was already peeked), peek next
    inline lChar32 PeekNextCharFromBuffer()
    {
        if ( m_read_buffer_pos + 1 >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return 0;
            }
        }
        return m_read_buffer[++m_read_buffer_pos];
    }
    // skip current char (was already peeked), peek next
    inline lChar32 PeekNextCharFromBuffer( int offset )
    {
        if ( m_read_buffer_pos+offset >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return 0;
            }
            if ( m_read_buffer_pos + offset >= m_read_buffer_len )
                return 0;
        }
        m_read_buffer_pos += offset + 1;
        if ( m_read_buffer_pos >= m_read_buffer_len )
            return 0;
        return m_read_buffer[m_read_buffer_pos];
    }
    void clearCharBuffer();
    /// returns number of available characters in buffer
    int fillCharBuffer();

    /// reads one character from buffer
    //lChar32 ReadChar();
    /// reads several characters from buffer
    int ReadChars( lChar32 * buf, int maxsize );
    /// reads one character from buffer in RTF format
    lChar32 ReadRtfChar( int enc_type, const lChar32 * conv_table );
    /// reads specified number of bytes, converts to characters and saves to buffer, returns number of chars read
    int ReadTextBytes( lvpos_t pos, int bytesToRead, lChar32 * buf, int buf_size, int flags );
#if 0
    /// reads specified number of characters and saves to buffer, returns number of chars read
    int ReadTextChars( lvpos_t pos, int charsToRead, lChar32 * buf, int buf_size, int flags );
#endif
public:
    /// returns true if end of fle is reached, and there is no data left in buffer
    virtual bool Eof() { return m_eof; /* m_buf_fpos + m_buf_pos >= m_stream_size;*/ }
    virtual void Reset();
    /// tries to autodetect text encoding
    bool AutodetectEncoding( bool utfOnly=false );
    /// reads next text line, tells file position and size of line, sets EOL flag
    lString32 ReadLine( int maxLineSize, lUInt32 & flags );
    //lString32 ReadLine( int maxLineSize, lvpos_t & fpos, lvsize_t & fsize, lUInt32 & flags );
    /// returns name of character encoding
    lString32 GetEncodingName() { return m_encoding_name; }
    /// returns name of language
    lString32 GetLangName() { return m_lang_name; }

    // overrides
    /// sets charset by name
    virtual void SetCharset( const lChar32 * name );
    /// sets 8-bit charset conversion table (128 items, for codes 128..255)
    virtual void SetCharsetTable( const lChar32 * table );
    /// returns 8-bit charset conversion table (128 items, for codes 128..255)
    virtual lChar32 * GetCharsetTable( ) { return m_conv_table; }

    /// constructor
    LVTextFileBase( LVStreamRef stream );
    /// destructor
    virtual ~LVTextFileBase();
};

/** \brief document text cache

    To read fragments of document text on demand.

*/
class LVXMLTextCache : public LVTextFileBase
{
private:
    struct cache_item
    {
        cache_item * next;
        lUInt32      pos;
        lUInt32      size;
        lUInt32      flags;
        lString32    text;
        cache_item( lString32 & txt )
            : next(NULL), pos(0), size(0), flags(0), text(txt)
        {
        }
    };

    cache_item * m_head;
    lUInt32    m_max_itemcount;
    lUInt32    m_max_charcount;

    void cleanOldItems( lUInt32 newItemChars );

    /// adds new item
    void addItem( lString32 & str );

public:
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
    /// parses input stream
    virtual bool Parse();
    /// constructor
    LVXMLTextCache( LVStreamRef stream, lUInt32 max_itemcount, lUInt32 max_charcount )
        : LVTextFileBase( stream ), m_head(NULL)
        , m_max_itemcount(max_itemcount)
        , m_max_charcount(max_charcount)
    {
    }
    /// destructor
    virtual ~LVXMLTextCache();
    /// reads text from cache or input stream
    lString32 getText( lUInt32 pos, lUInt32 size, lUInt32 flags );
};


class LVTextParser : public LVTextFileBase
{
protected:
    LVXMLParserCallback * m_callback;
    bool m_isPreFormatted;
public:
    /// constructor
    LVTextParser( LVStreamRef stream, LVXMLParserCallback * callback, bool isPreFormatted );
    /// descructor
    virtual ~LVTextParser();
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
    /// parses input stream
    virtual bool Parse();
};

class LVTextRobustParser : public LVTextParser
{
public:
    /// constructor
    LVTextRobustParser( LVStreamRef stream, LVXMLParserCallback * callback, bool isPreFormatted );
    /// destructor
    virtual ~LVTextRobustParser();
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
};

/// parser of CoolReader's text format bookmarks
class LVTextBookmarkParser : public LVTextParser
{
public:
    /// constructor
    LVTextBookmarkParser( LVStreamRef stream, LVXMLParserCallback * callback );
    /// descructor
    virtual ~LVTextBookmarkParser();
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
    /// parses input stream
    virtual bool Parse();
};

/// XML parser
class LVXMLParser : public LVTextFileBase
{
private:
    LVXMLParserCallback * m_callback;
    int  m_state;
    bool m_in_cdata;
    bool m_in_html_script_tag;
    bool m_trimspaces;
    bool SkipSpaces();
    bool SkipTillChar( lChar32 ch );
    bool ReadIdent( lString32 & ns, lString32 & str );
    bool ReadText();
protected:
    bool m_citags;
    bool m_allowHtml;
    bool m_fb2Only;
    bool m_svgOnly;
public:
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
    /// parses input stream
    virtual bool Parse();
    /// sets charset by name
    virtual void SetCharset( const lChar32 * name );
    /// resets parsing, moves to beginning of stream
    virtual void Reset();
    /// constructor
    LVXMLParser( LVStreamRef stream, LVXMLParserCallback * callback, bool allowHtml=true, bool fb2Only=false, bool svgOnly=false );
    /// changes space mode
    virtual void SetSpaceMode( bool flgTrimSpaces );
    /// returns space mode
    bool GetSpaceMode() { return m_trimspaces; }
    /// destructor
    virtual ~LVXMLParser();
};

extern const char * * HTML_AUTOCLOSE_TABLE[];

/// HTML parser
class LVHTMLParser : public LVXMLParser
{
private:
public:
    /// returns true if format is recognized by parser
    virtual bool CheckFormat();
    /// parses input stream
    virtual bool Parse();
    /// constructor
    LVHTMLParser( LVStreamRef stream, LVXMLParserCallback * callback );
    /// destructor
    virtual ~LVHTMLParser();
};

/// read stream contents to string
lString32 LVReadTextFile( LVStreamRef stream );
/// read file contents to string
lString32 LVReadTextFile( lString32 filename );

LVStreamRef GetFB2Coverpage(LVStreamRef stream);

#define BASE64_BUF_SIZE 128
class LVBase64Stream : public LVNamedStream
{
private:
    lString8    m_curr_text;
    int         m_text_pos;
    lvsize_t    m_size;
    lvpos_t     m_pos;
    int         m_iteration;
    lUInt32     m_value;
    lUInt8      m_bytes[BASE64_BUF_SIZE];
    int         m_bytes_count;
    int         m_bytes_pos;
    int readNextBytes();
    int bytesAvailable();
    bool rewind();
    bool skip( lvsize_t count );
public:
    virtual ~LVBase64Stream() { }
    LVBase64Stream(lString8 data);
    virtual lverror_t Seek(lvoffset_t offset, lvseek_origin_t origin, lvpos_t* newPos);
    virtual lverror_t Read(void* buf, lvsize_t size, lvsize_t* pBytesRead);
    virtual bool Eof() {
        return m_pos >= m_size;
    }
    virtual lvsize_t  GetSize() {
        return m_size;
    }
    virtual lvpos_t GetPos() {
        return m_pos;
    }
    virtual lverror_t GetPos( lvpos_t * pos ) {
        if (pos)
            *pos = m_pos;
        return LVERR_OK;
    }
    virtual lverror_t Write(const void*, lvsize_t, lvsize_t*) {
        return LVERR_NOTIMPL;
    }
    virtual lverror_t SetSize(lvsize_t) {
        return LVERR_NOTIMPL;
    }
};

#endif // __LVXML_H_INCLUDED__
