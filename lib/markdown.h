/*
	Copyright (c) 2009 by Chad Nelson
	Released under the MIT License.
	See the provided LICENSE.TXT file for details.
*/

#ifndef LIBMDCPP_H_INCLUDED
#define LIBMDCPP_H_INCLUDED

#include <iostream>
#include <string>
#include <list>
#include <memory>
#include <unordered_map>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

using std::string;

namespace markdown {

using boost::optional;
using boost::none;

// Forward references.
class Token;
class LinkIds;

typedef std::shared_ptr<Token> TokenPtr;
typedef std::list<TokenPtr> TokenGroup;
typedef TokenGroup::const_iterator CTokenGroupIter;

class SyntaxHighlighter {
public:
    SyntaxHighlighter() {}
    virtual ~SyntaxHighlighter() {}
    virtual void highlight(const string& code, const string lang, std::ostream& out) {
        out << code;
    }
};

class Document: private boost::noncopyable {
public:
    Document(SyntaxHighlighter* highlighter, size_t spacesPerTab=cDefaultSpacesPerTab);
    Document(std::istream& in, SyntaxHighlighter* highlighter, size_t spacesPerTab=cDefaultSpacesPerTab);
    ~Document();

    // You can call read() functions multiple times before writing if
    // desirable. Once the document has been processed for writing, it can't
    // accept any more input.
    bool read(const string&);
    bool read(std::istream&);
    void write(std::ostream&);
    void writeTokens(std::ostream&); // For debugging

    // The class is marked noncopyable because it uses reference-counted
    // links to things that get changed during processing. If you want to
    // copy it, use the `copy` function to explicitly say that.
    Document copy() const; // TODO: Copy function not yet written.

private:
    bool _getline(std::istream& in, string& line);
    void _process();
    void _processFencedBlocks();
    optional<TokenPtr> parseFencedCodeBlock(CTokenGroupIter& i, CTokenGroupIter end);
    void _mergeMultilineHtmlTags();
    void _processInlineHtmlAndReferences();
    void _processBlocksItems(TokenPtr inTokenContainer);
    void _processParagraphLines(TokenPtr inTokenContainer);

    static const size_t cSpacesPerInitialTab, cDefaultSpacesPerTab;

    const size_t cSpacesPerTab;
    TokenPtr mTokenContainer;
    LinkIds *mIdTable;
    bool mProcessed;
    SyntaxHighlighter *mHighlighter;
};

} // namespace markdown

#endif
