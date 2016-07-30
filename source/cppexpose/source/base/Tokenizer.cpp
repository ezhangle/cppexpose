
#include <cppexpose/base/Tokenizer.h>

#include <stdlib.h>
#include <algorithm>
#include <string>
#include <fstream>

#include <cppexpose/base/string_helpers.h>


static const int           MIN_INT  = int( ~(unsigned(-1)/2) );
static const int           MAX_INT  = int(  (unsigned(-1)/2) );
static const unsigned int  MAX_UINT = unsigned(-1);


namespace cppexpose
{


struct CompareStringLength
{
    bool operator() (const std::string & first, const std::string & second) {
        return first.size() > second.size();
    }
};


Tokenizer::Tokenizer()
: m_options(0)
, m_whitespace(" \t\r\n")
, m_quotationMarks("\"")
, m_singleCharacters("{}[],:")
, m_begin(nullptr)
, m_end(nullptr)
, m_current(nullptr)
, m_line(1)
, m_column(1)
, m_lastCR(false)
{
}

Tokenizer::~Tokenizer()
{
}

unsigned int Tokenizer::options() const
{
    return m_options;
}

void Tokenizer::setOptions(unsigned int options)
{
    m_options = options;
}

bool Tokenizer::hasOption(Tokenizer::Option option) const
{
    return ((m_options & option) != 0);
}

std::string Tokenizer::whitespace() const
{
    return m_whitespace;
}

void Tokenizer::setWhitespace(const std::string & whitespace)
{
    m_whitespace = whitespace;
}

std::string Tokenizer::quotationMarks() const
{
    return m_quotationMarks;
}

void Tokenizer::setQuotationMarks(const std::string & quotationMarks)
{
    m_quotationMarks = quotationMarks;
}

std::string Tokenizer::singleCharacters() const
{
    return m_singleCharacters;
}

void Tokenizer::setSingleCharacters(const std::string & singleCharacters)
{
    m_singleCharacters = singleCharacters;
}

const std::vector<std::string> & Tokenizer::standalones() const
{
    return m_standalones;
}

void Tokenizer::setStandalones(const std::vector<std::string> & standalones)
{
    m_standalones = standalones;

    // Sort by string length (longest first)
    CompareStringLength compare;
    std::sort(m_standalones.begin(), m_standalones.end(), compare);
}

bool Tokenizer::loadDocument(const std::string & filename)
{
    // Open file
    std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
    if (!in) {
        // Could not open file
        return false;
    }

    // Reset document
    m_document = "";

    // Read file
    in.seekg(0, std::ios::end);
    m_document.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&m_document[0], m_document.size());
    in.close();

    // Set document
    const char * begin = m_document.c_str();
    const char * end   = begin + m_document .size();
    setDocument(begin, end);

    // Success
    return true;
}

void Tokenizer::setDocument(const std::string & document)
{
    m_document = document;

    const char * begin = m_document.c_str();
    const char * end   = begin + m_document .size();

    setDocument(begin, end);
}

void Tokenizer::setDocument(const char * beginDoc, const char * endDoc)
{
    // Begin parsing the document
    m_begin   = beginDoc;
    m_end     = endDoc;
    m_current = m_begin;
    m_line    = 1;
    m_column  = 1;
    m_lastCR  = false;
}

Tokenizer::Token Tokenizer::parseToken()
{
    // Read next token
    Token token = readToken();

    // Skip comments (if wanted)
    if (!hasOption(OptionIncludeComments))
    {
        while (token.type == TokenComment)
        {
           token = readToken();
        }
    }

    // Save parsed token as string
    token.content = std::string(token.begin, token.end - token.begin);

    // Convert token of specific types
    if (hasOption(OptionParseNumber) && token.type == TokenNumber)
    {
        token.value = decodeNumber(token);
    }

    else if (hasOption(OptionParseStrings) && token.type == TokenString)
    {
        token.value = Variant(decodeString(token));
    }

    else if (hasOption(OptionParseBoolean) && token.content == "true")
    {
        token.type = TokenBoolean;
        token.value = Variant(true);
    }

    else if (hasOption(OptionParseBoolean) && token.content == "false")
    {
        token.type = TokenBoolean;
        token.value = Variant(false);
    }

    else if (hasOption(OptionParseNull) && token.content == "null")
    {
        token.type = TokenNull;
        token.value = Variant();
    }

    else
    {
        token.value = Variant(token.content);
    }

    // Return parsed token
    return token;
}

Tokenizer::Token Tokenizer::readToken()
{
    // Find beginning of next token
    skipWhitespace();

    // Interpret next token
    Lookahead lookahead = lookAheadTokenType();

    // Prepare token
    Token token;
    token.begin  = m_current;
    token.line   = m_line;
    token.column = m_column;
    token.type   = lookahead.type;

    // Comment
    if (lookahead.type == TokenComment)
    {
        if (lookahead.hint == "/*") {
            // C-style comment
            readCStyleComment();
        } else if (lookahead.hint == "//") {
            // CPP-style comment
            readCppStyleComment();
        } else if (lookahead.hint == "#") {
            // Shell-style comment
            readShellStyleComment();
        }
    }

    // Standalone string
    else if (lookahead.type == TokenStandalone)
    {
        readStandaloneString(lookahead.hint);
    }

    // String
    else if (lookahead.type == TokenString)
    {
        char endChar = nextChar();
        readString(endChar);
    }

    // Number
    else if (lookahead.type == TokenNumber)
    {
        readNumber();
    }

    // Single character
    else if (lookahead.type == TokenSingleChar)
    {
        readChar();
    }

    // Arbitrary token
    else if (lookahead.type == TokenWord)
    {
        readWord();
    }

    token.end = m_current;

    return token;
}

void Tokenizer::readCStyleComment()
{
    // Skip '/*'
    readChar();
    readChar();

    // Read until end of comment
    while (m_current != m_end)
    {
        char c = readChar();

        if (c == '*' && nextChar() == '/') {
            readChar();
            break;
        }
    }
}

void Tokenizer::readCppStyleComment()
{
    // Skip '//'
    readChar();
    readChar();

    // Read until end of comment
    while (m_current != m_end)
    {
        char c = readChar();

        if (c == '\r' || c == '\n') {
            break;
        }
    }
}

void Tokenizer::readShellStyleComment()
{
    // Skip '#'
    readChar();

    // Read until end of comment
    while (m_current != m_end)
    {
        char c = readChar();

        if (c == '\r' || c == '\n') {
            break;
        }
    }
}

void Tokenizer::readStandaloneString(const std::string & standalone)
{
    // Skip size of string
    for (size_t i=0; i<standalone.size(); i++)
    {
        readChar();
    }
}

void Tokenizer::readString(char endChar)
{
    // Skip first character (already read)
    readChar();

    // Read until end character is found
    char c = 0;
    while (m_current != m_end)
    {
        c = readChar();

        if (c == '\\') {
            readChar();
        } else if (c == endChar) {
            break;
        }
    }
}

void Tokenizer::readNumber()
{
    // [TODO] Use a proper regex!

    // Skip first character (already read)
    readChar();

    // Read until end of number
    while (m_current != m_end)
    {
        if (!(nextChar() >= '0' && nextChar() <= '9') && !charIn(nextChar(), ".eE+-")) {
            break;
        }

        readChar();
    }
}

void Tokenizer::readWord()
{
    // Skip first character (already read)
    readChar();

    // Read until word ends
    while (m_current != m_end && lookAheadTokenType().type == TokenWord)
    {
        readChar();
    }
}

void Tokenizer::skipWhitespace()
{
    // Skip until non-whitespace character is found
    while (m_current != m_end)
    {
        char c = nextChar();
 
        if (charIn(c, m_whitespace)) {
            readChar();
        } else {
            break;
        }
    }
}

char Tokenizer::readChar()
{
    // Check end-of-stream
    if (m_current == m_end) {
        return 0;
    }

    // Read current character
    char c = *m_current;

    // Count line and column
    if (c == '\r') { // CR
        m_line++;
        m_column = 1;
        m_lastCR = true;
    }

    else if (c == '\n' && !m_lastCR) { // LF
        m_line++;
        m_column = 1;
        m_lastCR = false;
    }

    else { // Anything else
        m_column++;
        m_lastCR = false;
    }

    // Return character, advance by one
    return *m_current++;
}

Tokenizer::Lookahead Tokenizer::lookAheadTokenType() const
{
    Lookahead lookahead;

    // Read ahead
    char         c = nextChar();    // Next character
    char        c2 = nextChar(1);   // Character after that
    std::string s2 = lookAhead(2);  // Next string of size 2
    std::string standalone = "";    // Standalone string if one was detected

    // End of stream
    if (c == 0)
    {
        lookahead.type = TokenEndOfStream;
    }

    // Whitespace
    else if (charIn(c, m_whitespace))
    {
        lookahead.type = TokenWhitespace;
    }

    // Comment
    else if (hasOption(OptionCStyleComments) && s2 == "/*")
    {
        lookahead.type = TokenComment;
        lookahead.hint = "/*";
    }
    else if (hasOption(OptionCppStyleComments) && s2 == "//")
    {
        lookahead.type = TokenComment;
        lookahead.hint = "//";
    }
    else if (hasOption(OptionShellStyleComments) && c == '#')
    {
        lookahead.type = TokenComment;
        lookahead.hint = "#";
    }

    // Standalone string
    else if ((standalone = matchStandaloneStrings()) != "")
    {
        lookahead.type = TokenStandalone;
        lookahead.hint = standalone;
    }

    // String
    else if (hasOption(OptionParseStrings) && charIn(c, m_quotationMarks))
    {
        lookahead.type = TokenString;
    }

    // Number
    else if (hasOption(OptionParseNumber) &&
             ((c >= '0' && c <= '9') || (c == '-' && c2 >= '0' && c2 <= '9')) )
    {
        lookahead.type = TokenNumber;
    }

    // Single character
    else if (charIn(c, m_singleCharacters))
    {
        lookahead.type = TokenSingleChar;
    }

    // Arbitrary token
    else {
        lookahead.type = TokenWord;
    }

    return lookahead;
}

char Tokenizer::nextChar(size_t index) const
{
    const char * pos = m_current + index;

    if (pos >= m_end) {
        return 0;
    }

    return *pos;
}

std::string Tokenizer::lookAhead(size_t length) const
{
    if (m_current >= m_end) {
        return "";
    }

    const char * begin = m_current;
    const char * end   = m_current + length;

    if (end > m_end) end = m_end;

    return std::string(begin, end);
}

std::string Tokenizer::matchStandaloneStrings() const
{
    // Check if any of the standalone strings matches at the current position
    for (size_t i=0; i<m_standalones.size(); i++)
    {
        std::string str = m_standalones[i];

        if (lookAhead(str.size()) == str)
        {
            return str;
        }
    }

    // No standalone string matches
    return "";
}

unsigned int Tokenizer::line() const
{
    return m_line;
}

unsigned int Tokenizer::column() const
{
    return m_column;
}

Variant Tokenizer::decodeNumber(const Token & token) const
{
    bool isDouble = false;

    for (const char * inspect = token.begin; inspect != token.end; ++inspect) {
        isDouble = isDouble ||
                   charIn(*inspect, ".eE+") ||
                   (*inspect == '-' && inspect != token.begin);
    }

    if (isDouble) {
        return Variant(decodeDouble(token));
    }

    const char * current = token.begin;
    bool isNegative = *current == '-';
    if (isNegative) {
        ++current;
    }

    unsigned int largest = (isNegative ? (unsigned)(MAX_INT) : MAX_UINT);
    unsigned int threshold = largest / 10;
    unsigned int value = 0;

    while (current < token.end)
    {
        char c = *current++;

        if (c < '0' || c > '9') {
            return error("'" + std::string(token.begin, token.end-token.begin) + "' is not a number.");
        }

        if (value >= threshold) {
            return Variant(decodeDouble(token));
        }

        value = value * 10 + unsigned(c - '0');
    }

    if (isNegative) {
        return Variant(-int(value));
    } else if (value <= unsigned(MAX_INT)) {
        return Variant(int(value));
    } else {
        return Variant(value);
    }
}
double Tokenizer::decodeDouble(const Token & token) const
{
    std::string buffer(token.begin, token.end-token.begin);
    return helper::fromString<double>(buffer);
}

std::string Tokenizer::decodeString(const Token & token) const
{
    std::string decoded;
    decoded.reserve(token.end - token.begin - 2);

    const char * current = token.begin + 1; // Do not include '"'
    const char * end     = token.end   - 1; // Do not include '"'

    while (current != end)
    {
        char c = *current++;

        if (c == '"')
        {
            break;
        }

        if (c == '\\')
        {
            if (current == end)
            {
                error("Empty escape sequence in string");
                return decoded;
            }

            char escape = *current++;
            switch (escape)
            {
                case '"':  decoded += '"';  break;
                case '/':  decoded += '/';  break;
                case '\\': decoded += '\\'; break;
                case 'b':  decoded += '\b'; break;
                case 'f':  decoded += '\f'; break;
                case 'n':  decoded += '\n'; break;
                case 'r':  decoded += '\r'; break;
                case 't':  decoded += '\t'; break;
                case 'u':
                {
                    unsigned int unicode;

                    if (!decodeUnicodeCodePoint(current, end, unicode)) {
                        return "";
                    }

                    decoded += codePointToUTF8(unicode);
                    break;
                }

                default:
                {
                    error("Bad escape sequence in string");
                    return "";
                }
            }
        }
        else
        {
            decoded += c;
        }
    }

    return decoded;
}

bool Tokenizer::decodeUnicodeCodePoint(const char * & current, const char * end, unsigned int & unicode) const
{
    if (!decodeUnicodeEscapeSequence(current, end, unicode))
    {
        return false;
    }

    if (unicode >= 0xD800 && unicode <= 0xDBFF)
    {
        // surrogate pairs
        if (end - current < 6) {
            return error( "additional six characters expected to parse unicode surrogate pair.");
        }

        unsigned int surrogatePair;

        if (*(current++) == '\\' && *(current++)== 'u') {
            if (decodeUnicodeEscapeSequence(current, end, surrogatePair)) {
                unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
            } else {
                return false;
            }
        } else {
            return error("expecting another \\u token to begin the second half of a unicode surrogate pair");
        }
    }

    return true;
}

bool Tokenizer::decodeUnicodeEscapeSequence(const char * & current, const char * end, unsigned int & unicode) const
{
    if (end - current < 4)
    {
        return error( "Bad unicode escape sequence in string: four digits expected.");
    }

    unicode = 0;

    for (int index=0; index<4; ++index)
    {
        char c = *current++;
        unicode *= 16;

        if (c >= '0' && c <= '9') {
            unicode += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            unicode += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            unicode += c - 'A' + 10;
        } else {
            return error("Bad unicode escape sequence in string: hexadecimal digit expected.");
        }
    }

    return true;
}

std::string Tokenizer::codePointToUTF8(unsigned int cp) const
{
    std::string result;

    // based on description from http://en.wikipedia.org/wiki/UTF-8

    if (cp <= 0x7f) {
        result.resize(1);
        result[0] = static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        result.resize(2);
        result[1] = static_cast<char>(0x80 | (0x3f & cp));
        result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
    } else if (cp <= 0xFFFF) {
        result.resize(3);
        result[2] = static_cast<char>(0x80 | (0x3f & cp));
        result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
        result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
    } else if (cp <= 0x10FFFF) {
        result.resize(4);
        result[3] = static_cast<char>(0x80 | (0x3f & cp));
        result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
        result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
        result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
    }

    return result;
}

bool Tokenizer::charIn(char c, const std::string & chars) const
{
    return chars.find(c) != std::string::npos;
}

bool Tokenizer::error(const std::string &) const
{
    // [TODO]

    return false;
}

bool Tokenizer::recoverFromError(TokenType skipUntilToken)
{
    // [TODO]

    while (true)
    {
        Token skip = readToken();

        if (skip.type == skipUntilToken || skip.type == TokenEndOfStream) {
            break;
        }
    }

    return false;
}


} // namespace cppexpose
