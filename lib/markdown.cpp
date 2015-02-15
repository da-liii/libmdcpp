
/*
	Copyright (c) 2009 by Chad Nelson
	Released under the MIT License.
	See the provided LICENSE.TXT file for details.
*/

#include "markdown.h"
#include "markdown_tokens.h"

#include <sstream>
#include <cassert>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>

using std::cerr;
using std::endl;
using boost::regex;
using boost::smatch;
using boost::regex_match;
using boost::regex_search;

using boost::optional;
using boost::none;
using markdown::TokenPtr;
using markdown::CTokenGroupIter;

namespace {

struct HtmlTagInfo {
    string tagName, extra;
    bool isClosingTag;
    size_t lengthOfToken; // In original string
};

const string cHtmlTokenSource("<((/?)([a-zA-Z0-9]+)(?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\5))*? */? *))>");
const regex cHtmlTokenExpression(cHtmlTokenSource),
      cStartHtmlTokenExpression("^"+cHtmlTokenSource),
      cOneHtmlTokenExpression("^"+cHtmlTokenSource+"$");

enum ParseHtmlTagFlags { cAlone, cStarts };

optional<HtmlTagInfo> parseHtmlTag(string::const_iterator begin,
                                   string::const_iterator end, ParseHtmlTagFlags flags)
{
    smatch m;
    if (regex_search(begin, end, m, (flags==cAlone ?
                                     cOneHtmlTokenExpression : cStartHtmlTokenExpression)))
    {
        HtmlTagInfo r;
        r.tagName=m[3];
        if (m[4].matched) r.extra=m[4];
        r.isClosingTag=(m[2].length()>0);
        r.lengthOfToken=m[0].length();
        return r;
    }
    return none;
}

markdown::TokenGroup parseInlineHtmlText(const string& src) {
    markdown::TokenGroup r;
    string::const_iterator prev=src.begin(), end=src.end();
    while (1) {
        smatch m;
        if (regex_search(prev, end, m, cHtmlTokenExpression)) {
            if (prev!=m[0].first) {
                //cerr << "  Non-tag (" << std::distance(prev, m[0].first) << "): " << string(prev, m[0].first) << endl;
                r.push_back(TokenPtr(new markdown::token::InlineHtmlContents(string(prev, m[0].first))));
            }
            //cerr << "  Tag: " << m[1] << endl;
            r.push_back(TokenPtr(new markdown::token::HtmlTag(m[1])));
            prev=m[0].second;
        } else {
            string eol;
            if (prev!=end) {
                eol=string(prev, end);
                //cerr << "  Non-tag: " << eol << endl;
            }
            eol+='\n';
            r.push_back(TokenPtr(new markdown::token::InlineHtmlContents(eol)));
            break;
        }
    }
    return r;
}

bool isHtmlCommentStart(string::const_iterator begin,
                        string::const_iterator end)
{
    // It can't be a single-line comment, those will already have been parsed
    // by isBlankLine.
    static const regex cExpression("^<!--");
    return regex_search(begin, end, cExpression);
}

bool isHtmlCommentEnd(string::const_iterator begin,
                      string::const_iterator end)
{
    static const regex cExpression(".*-- *>$");
    return regex_match(begin, end, cExpression);
}

bool isBlankLine(const string& line) {
    static const regex cExpression(" {0,3}(<--(.*)-- *> *)* *");
    return regex_match(line, cExpression);
}

optional<TokenPtr> parseInlineHtml(CTokenGroupIter& i, CTokenGroupIter end) {
    // Preconditions: Previous line was blank, or this is the first line.
    if ((*i)->text()) {
        const string& line(*(*i)->text());

        bool tag=false, comment=false;
        optional<HtmlTagInfo> tagInfo=parseHtmlTag(line.cbegin(), line.cend(), cStarts);
        if (tagInfo && markdown::token::isValidTag(tagInfo->tagName)>1) {
            tag=true;
        } else if (isHtmlCommentStart(line.begin(), line.end())) {
            comment=true;
        }

        if (tag) {
            // Block continues until an HTML tag (alone) on a line followed by a
            // blank line.
            markdown::TokenGroup contents;
            auto firstLine=i, prevLine=i;
            size_t lines=0;

            bool done=false;
            do {
                // We encode HTML tags so that their contents gets properly
                // handled -- i.e. "<div style=">"/>" becomes <div style="&gt;"/>
                if ((*i)->text()) {
                    markdown::TokenGroup t=parseInlineHtmlText(*(*i)->text());
                    contents.splice(contents.end(), t);
                } else contents.push_back(*i);

                prevLine=i;
                ++i;
                ++lines;

                if (i!=end && (*i)->isBlankLine() && (*prevLine)->text()) {
                    if (prevLine==firstLine) {
                        done=true;
                    } else {
                        const string& text(*(*prevLine)->text());
                        if (parseHtmlTag(text.cbegin(), text.cend(), cAlone)) done=true;
                    }
                }
            } while (i!=end && !done);

            if (lines>1 || markdown::token::isValidTag(tagInfo->tagName, true)>1) {
                i=prevLine;
                return TokenPtr(new markdown::token::InlineHtmlBlock(contents));
            } else {
                // Single-line HTML "blocks" whose initial tags are span-tags
                // don't qualify as inline HTML.
                i=firstLine;
                return none;
            }
        } else if (comment) {
            // Comment continues until a closing tag is found; at present, it
            // also has to be the last thing on the line, and has to be
            // immediately followed by a blank line too.
            markdown::TokenGroup contents;
            auto firstLine=i, prevLine=i;

            bool done=false;
            do {
                if ((*i)->text()) contents.push_back(TokenPtr(new markdown::token::InlineHtmlComment(*(*i)->text()+'\n')));
                else contents.push_back(*i);

                prevLine=i;
                ++i;

                if (i!=end && (*i)->isBlankLine() && (*prevLine)->text()) {
                    if (prevLine==firstLine) {
                        done=true;
                    } else {
                        const string& text(*(*prevLine)->text());
                        if (isHtmlCommentEnd(text.begin(), text.end())) done=true;
                    }
                }
            } while (i!=end && !done);
            i=prevLine;
            return TokenPtr(new markdown::token::InlineHtmlBlock(contents));
        }
    }

    return none;
}

optional<string> isCodeBlockLine(CTokenGroupIter& i, CTokenGroupIter end) {
    if ((*i)->isBlankLine()) {
        // If we get here, we're already in a code block.
        ++i;
        if (i!=end) {
            optional<string> r=isCodeBlockLine(i, end);
            if (r) return string("\n"+*r);
        }
        --i;
    } else if ((*i)->text() && (*i)->canContainMarkup()) {
        const string& line(*(*i)->text());
        if (line.length()>=4) {
            auto si=line.begin(), sie=si+4;
            while (si!=sie && *si==' ') ++si;
            if (si==sie) {
                ++i;
                return string(si, line.end());
            }
        }
    }
    return none;
}

optional<TokenPtr> parseCodeBlock(CTokenGroupIter& i, CTokenGroupIter end) {
    if (!(*i)->isBlankLine()) {
        optional<string> contents=isCodeBlockLine(i, end);
        if (contents) {
            std::ostringstream out;
            out << *contents << '\n';
            while (i!=end) {
                contents=isCodeBlockLine(i, end);
                if (contents) out << *contents << '\n';
                else break;
            }
            i--;
            return TokenPtr(new markdown::token::CodeBlock(out.str()));
        }
    }
    return none;
}

bool isCodeFenceBeginLine(const string& line, int& indent, int& length, char& fence, string& info) {
    indent = 0;
    auto si = line.begin(), sie = line.end();
    while (si!=sie && *si==' ') {
        si++;
        indent++;
    }
    if (indent > 3)
        return false;


    length = 0;
    fence = *si;
    if (fence!='`' && fence!='~')
        return false;
    while (si!=sie && *si==fence) {
        si++;
        length++;
    }
    if (length < 3)
        return false;
    if (si==sie)
        return true;

    // info string cannot contain backsticks
    info = string(si, sie);
    while (si != sie) {
        if (*si == '`')
            return false;
        si++;
    }

    return true;
}

bool isCodeFenceEndLine(const string& line, int indent, int openLen, char fence, std::ostringstream& out) {
    int maxIndent = 0;
    auto si = line.begin(), sie = line.end();
    while (si!=sie && *si==' ' && indent) {
        si++;
        indent--;
        maxIndent++;
    }
    auto sii = si;
    while (si!=sie && *si==' ' && maxIndent < 4) {
        si++;
        maxIndent++;
    }
    if (maxIndent > 3) {
        out << string(sii, sie) << "\n";
        return false;
    }

    int closeLen = 0;
    while (si!=sie && *si==fence) {
        si++;
        closeLen++;
    }
    if (closeLen < openLen) {
        out << string(sii, sie) << "\n";
        return false;
    }

    // closing fence cannot contain info string
    while (si!=sie && (*si==' ' || *si=='\t')) si++;
    if (si != sie) {
        out << string(sii, sie) << "\n";
        return false;
    }

    return true;
}

size_t countQuoteLevel(const string& prefixString) {
    size_t r=0;
    for (auto qi=prefixString.cbegin(),
            qie=prefixString.cend(); qi!=qie; ++qi)
        if (*qi=='>') ++r;
    return r;
}

bool parseBlockQuote(markdown::TokenGroup& subTokens,CTokenGroupIter& i, CTokenGroupIter end) {
    static const regex cBlockQuoteExpression("^((?: {0,3}>)+) ?(.*)$");
    // Useful captures: 1=prefix, 2=content

    if (!(*i)->isBlankLine() && (*i)->text() && (*i)->canContainMarkup()) {
        const string& line(*(*i)->text());
        smatch m;
        if (regex_match(line, m, cBlockQuoteExpression)) {
            size_t quoteLevel=countQuoteLevel(m[1]);
            regex continuationExpression=regex("^((?: {0,3}>){"+boost::lexical_cast<string>(quoteLevel)+"}) ?(.*)$");

            if (!isBlankLine(m[2]))
                subTokens.push_back(TokenPtr(new markdown::token::RawText(m[2])));
            else
                subTokens.push_back(TokenPtr(new markdown::token::BlankLine(m[2])));
            
            ++i;
            while (i!=end) {
                const string& line(*(*i)->text());
                if (regex_match(line, m, continuationExpression)) {
                    assert(m[2].matched);
                    if (!isBlankLine(m[2]))
                        subTokens.push_back(TokenPtr(new markdown::token::RawText(m[2])));
                    else
                        subTokens.push_back(TokenPtr(new markdown::token::BlankLine(m[2])));
                    ++i;
                } else {
                    //--i;
                    break;
                }
            }

            return true;
        }
    }
    return false;
}

optional<TokenPtr> parseListBlock(CTokenGroupIter& i, CTokenGroupIter end, bool sub=false) {
    static const regex cUnorderedListExpression("^( *)([*+-]) +([^*-].*)$");
    static const regex cOrderedListExpression("^( *)([0-9]+)\\. +(.*)$");

    enum ListType { cNone, cUnordered, cOrdered };
    ListType type=cNone;
    if (!(*i)->isBlankLine() && (*i)->text() && (*i)->canContainMarkup()) {
        regex nextItemExpression, startSublistExpression;
        size_t indent=0;

        const string& line(*(*i)->text());

        //cerr << "IsList? " << line << endl;

        markdown::TokenGroup subTokens, subItemTokens;

        smatch m;
        if (regex_match(line, m, cUnorderedListExpression)) {
            indent=m[1].length();
            if (sub || indent<4) {
                type=cUnordered;
                char startChar=*m[2].first;
                subItemTokens.push_back(TokenPtr(new markdown::token::RawText(m[3])));

                std::ostringstream next;
                next << "^" << string(indent, ' ') << "\\" << startChar << " +([^*-].*)$";
                nextItemExpression=next.str();
            }
        } else if (regex_match(line, m, cOrderedListExpression)) {
            indent=m[1].length();
            if (sub || indent<4) {
                type=cOrdered;
                subItemTokens.push_back(TokenPtr(new markdown::token::RawText(m[3])));

                std::ostringstream next;
                next << "^" << string(indent, ' ') << "[0-9]+\\. +(.*)$";
                nextItemExpression=next.str();
            }
        }

        if (type!=cNone) {
            auto originalI=i;
            size_t itemCount=1;
            std::ostringstream sub;
            sub << "^" << string(indent, ' ') << " +(([*+-])|([0-9]+\\.)) +.*$";
            startSublistExpression=sub.str();

            // There are several options for the next line. It's another item in
            // this list (in which case this one is done); it's a continuation
            // of this line (collect it and keep going); it's the first item in
            // a sub-list (call this function recursively to collect it), it's
            // the next item in the parent list (this one is ended); or it's
            // blank.
            //
            // A blank line requires looking ahead. If the next line is an item
            // for this list, then switch this list into paragraph-items mode
            // and continue processing. If it's indented by four or more spaces
            // (more than the list itself), then it's another continuation of
            // the current item. Otherwise it's either a new paragraph (and this
            // list is ended) or the beginning of a sub-list.
            static const regex cContinuedItemExpression("^ *([^ ].*)$");

            regex continuedAfterBlankLineExpression("^ {"+
                                                    boost::lexical_cast<string>(indent+4)+"}([^ ].*)$");
            regex codeBlockAfterBlankLineExpression("^ {"+
                                                    boost::lexical_cast<string>(indent+8)+"}(.*)$");

            enum NextItemType { cUnknown, cEndOfList, cAnotherItem };
            NextItemType nextItem=cUnknown;
            bool setParagraphMode=false;

            ++i;
            while (i!=end) {
                if ((*i)->isBlankLine()) {
                    auto ii=i;
                    ++ii;
                    if (ii==end) {
                        i=ii;
                        nextItem=cEndOfList;
                    } else if ((*ii)->text()) {
                        const string& line(*(*ii)->text());
                        if (regex_match(line, startSublistExpression)) {
                            setParagraphMode=true;
                            ++itemCount;
                            i=ii;
                            optional<TokenPtr> p=parseListBlock(i, end, true);
                            assert(p);
                            subItemTokens.push_back(*p);
                            continue;
                        } else if (regex_match(line, m, nextItemExpression)) {
                            setParagraphMode=true;
                            i=ii;
                            nextItem=cAnotherItem;
                        } else if (regex_match(line, m, continuedAfterBlankLineExpression)) {
                            assert(m[1].matched);
                            subItemTokens.push_back(TokenPtr(new markdown::token::BlankLine()));
                            subItemTokens.push_back(TokenPtr(new markdown::token::RawText(m[1])));
                            i=++ii;
                            continue;
                        } else if (regex_match(line, m, codeBlockAfterBlankLineExpression)) {
                            setParagraphMode=true;
                            ++itemCount;
                            assert(m[1].matched);
                            subItemTokens.push_back(TokenPtr(new markdown::token::BlankLine()));

                            string codeBlock = m.str(1)+'\n';
                            ++ii;
                            while (ii!=end) {
                                if ((*ii)->isBlankLine()) {
                                    auto iii=ii;
                                    ++iii;
                                    const string& nextLine(*(*iii)->text());
                                    if (regex_match(nextLine, m, codeBlockAfterBlankLineExpression)) {
                                        codeBlock += '\n'+m.str(1)+'\n';
                                        ii=iii;
                                    } else break;
                                } else if ((*ii)->text()) {
                                    const string& line(*(*ii)->text());
                                    if (regex_match(line, m, codeBlockAfterBlankLineExpression)) {
                                        codeBlock += m.str(1)+'\n';
                                    } else break;
                                } else break;
                                ++ii;
                            }

                            subItemTokens.push_back(TokenPtr(new markdown::token::CodeBlock(codeBlock)));
                            i=ii;
                            continue;
                        } else {
                            nextItem=cEndOfList;
                        }
                    } else break;
                } else if ((*i)->text()) {
                    const string& line(*(*i)->text());
                    if (regex_match(line, startSublistExpression)) {
                        ++itemCount;
                        optional<TokenPtr> p=parseListBlock(i, end, true);
                        assert(p);
                        subItemTokens.push_back(*p);
                        continue;
                    } else if (regex_match(line, m, nextItemExpression)) {
                        nextItem=cAnotherItem;
                    } else {
                        if (regex_match(line, m, cUnorderedListExpression)
                                || regex_match(line, m, cOrderedListExpression))
                        {
                            // Belongs to the parent list
                            nextItem=cEndOfList;
                        } else {
                            regex_match(line, m, cContinuedItemExpression);
                            assert(m[1].matched);
                            subItemTokens.push_back(TokenPtr(new markdown::token::RawText(m[1])));
                            ++i;
                            continue;
                        }
                    }
                } else nextItem=cEndOfList;

                if (!subItemTokens.empty()) {
                    subTokens.push_back(TokenPtr(new markdown::token::ListItem(subItemTokens)));
                    subItemTokens.clear();
                }

                assert(nextItem!=cUnknown);
                if (nextItem==cAnotherItem) {
                    subItemTokens.push_back(TokenPtr(new markdown::token::RawText(m[1])));
                    ++itemCount;
                    ++i;
                } else { // nextItem==cEndOfList
                    break;
                }
            }

            // In case we hit the end with an unterminated item...
            if (!subItemTokens.empty()) {
                subTokens.push_back(TokenPtr(new markdown::token::ListItem(subItemTokens)));
                subItemTokens.clear();
            }

            if (itemCount>1 || indent!=0) {
                if (type==cUnordered) {
                    return TokenPtr(new markdown::token::UnorderedList(subTokens, setParagraphMode));
                } else {
                    return TokenPtr(new markdown::token::OrderedList(subTokens, setParagraphMode));
                }
            } else {
                // It looked like a list, but turned out to be a false alarm.
                i=originalI;
                return none;
            }
        }
    }
    return none;
}

bool parseReference(CTokenGroupIter& i, CTokenGroupIter end, markdown::LinkIds &idTable) {
    if ((*i)->text()) {
        static const regex cReference("^ {0,3}\\[(.+)\\]: +<?([^ >]+)>?(?: *(?:('|\")(.*)\\3)|(?:\\((.*)\\)))?$");
        // Useful captures: 1=id, 2=url, 4/5=title

        const string& line1(*(*i)->text());
        smatch m;
        if (regex_match(line1, m, cReference)) {
            string id(m[1]), url(m[2]), title;
            if (m[4].matched) title=m[4];
            else if (m[5].matched) title=m[5];
            else {
                auto ii=i;
                ++ii;
                if (ii!=end && (*ii)->text()) {
                    // It could be on the next line
                    static const regex cSeparateTitle("^ *(?:(?:('|\")(.*)\\1)|(?:\\((.*)\\))) *$");
                    // Useful Captures: 2/3=title

                    const string& line2(*(*ii)->text());
                    if (regex_match(line2, m, cSeparateTitle)) {
                        ++i;
                        title=(m[2].matched ? m[2] : m[3]);
                    }
                }
            }

            idTable.add(id, url, title);
            return true;
        }
    }
    return false;
}

void flushParagraph(markdown::TokenGroup& paragraphTokens,
                    markdown::TokenGroup& finalTokens, bool noParagraphs)
{
    if (!paragraphTokens.empty()) {
        if (noParagraphs) {
            if (paragraphTokens.size()>1) {
                finalTokens.push_back(TokenPtr(new markdown::token::Container(paragraphTokens)));
            } else
                finalTokens.push_back(*paragraphTokens.begin());
        } else
            finalTokens.push_back(TokenPtr(new markdown::token::Paragraph(paragraphTokens)));
        paragraphTokens.clear();
    }
}

optional<TokenPtr> parseHeader(CTokenGroupIter& i, CTokenGroupIter end) {
    if (!(*i)->isBlankLine() && (*i)->text() && (*i)->canContainMarkup()) {
        // Hash-mark type
        static const regex cHashHeaders("^ {0,3}(#{1,6}) +(.*?)( +#* *)?$");
        const string& line=*(*i)->text();
        smatch m1, m2;
        if (regex_match(line, m1, cHashHeaders)) {
            markdown::TokenGroup g;
            g.push_back(TokenPtr(new markdown::token::RawText(m1[2])));
            return TokenPtr(new markdown::token::Header(m1[1].length(), g));
        }

        // Underlined type
        auto ii=i;
        ++ii;
        if (ii!=end && !(*ii)->isBlankLine() && (*ii)->text() && (*ii)->canContainMarkup()) {
            static const regex cUnderlinedHeaders("^ {0,3}([-=])\\1* *$");
            static const regex titleWithSpaces("^ {0,3}(.*[^ ]) *$");
            const string& line=*(*ii)->text();
            const string& title=*(*i)->text();
            if (regex_match(line, m1, cUnderlinedHeaders)) {
                char typeChar = m1.str(1)[0];
                regex_match(title, m2, titleWithSpaces);
                markdown::TokenGroup g;
                g.push_back(TokenPtr(new markdown::token::RawText(m2.str(1))));
                TokenPtr p=TokenPtr(new markdown::token::Header((typeChar=='='? 1 : 2), g));
                i=ii;
                return p;
            }
        }
    }
    return none;
}

optional<TokenPtr> parseHorizontalRule(CTokenGroupIter& i, CTokenGroupIter end) {
    if (!(*i)->isBlankLine() && (*i)->text() && (*i)->canContainMarkup()) {
        static const regex cHorizontalRules("^ {0,3}((\\* *){3,}|(- *){3,}|(_ *){3,})$");
        const string& line=*(*i)->text();
        if (regex_match(line, cHorizontalRules)) {
            return TokenPtr(new markdown::token::HtmlTag("hr /"));
        }
    }
    return none;
}

} // namespace



namespace markdown {

optional<LinkIds::Target> LinkIds::find(const string& id) const {
    Table::const_iterator i=mTable.find(_scrubKey(id));
    if (i!=mTable.end()) return i->second;
    else return none;
}

void LinkIds::add(const string& id, const string& url, const
                  string& title)
{
    mTable.insert(std::make_pair(_scrubKey(id), Target(url, title)));
}

string LinkIds::_scrubKey(string str) {
    boost::algorithm::to_lower(str);
    return str;
}



const size_t Document::cSpacesPerInitialTab=4; // Required by Markdown format
const size_t Document::cDefaultSpacesPerTab=cSpacesPerInitialTab;

Document::Document(SyntaxHighlighter *highlighter, size_t spacesPerTab)
    : cSpacesPerTab(spacesPerTab),
      mTokenContainer(new token::Container), mIdTable(new LinkIds),
      mProcessed(false), mHighlighter(highlighter)
{
    // This space deliberately blank ;-)
}

Document::Document(std::istream& in, SyntaxHighlighter *highlighter, size_t spacesPerTab)
    : cSpacesPerTab(spacesPerTab), mTokenContainer(new token::Container),
      mIdTable(new LinkIds), mProcessed(false), mHighlighter(highlighter)
{
    read(in);
}

Document::~Document() {
    delete mIdTable;
}

bool Document::read(const string& src) {
    std::istringstream in(src);
    return read(in);
}

bool Document::_getline(std::istream& in, string& line) {
    // Handles \n, \r, and \r\n (and even \n\r) on any system. Also does tab-
    // expansion, since this is the most efficient place for it.
    line.clear();

    bool initialWhitespace=true;
    char c;
    while (in.get(c)) {
        if (c=='\r') {
            if ((in.get(c)) && c!='\n') in.unget();
            return true;
        } else if (c=='\n') {
            if ((in.get(c)) && c!='\r') in.unget();
            return true;
        } else if (c=='\t') {
            size_t convert=(initialWhitespace ? cSpacesPerInitialTab :
                            cSpacesPerTab);
            line+=string(convert-(line.length()%convert), ' ');
        } else {
            line.push_back(c);
            if (c!=' ') initialWhitespace=false;
        }
    }
    return !line.empty();
}

bool Document::read(std::istream& in) {
    if (mProcessed) return false;

    token::Container *tokens=dynamic_cast<token::Container*>(mTokenContainer.get());
    assert(tokens!=0);

    string line;
    TokenGroup tgt;
    while (_getline(in, line)) {
        if (isBlankLine(line)) {
            tgt.push_back(TokenPtr(new token::BlankLine(line)));
        } else {
            tgt.push_back(TokenPtr(new token::RawText(line)));
        }
    }
    tokens->appendSubtokens(tgt);

    return true;
}

void Document::write(std::ostream& out) {
    _process();
    mTokenContainer->writeAsHtml(out);
}

void Document::writeTokens(std::ostream& out) {
    _process();
    mTokenContainer->writeToken(0, out);
}

void Document::_process() {
    if (!mProcessed) {
        _processFencedBlocks();
        _mergeMultilineHtmlTags();
        _processInlineHtmlAndReferences();
        _processBlocksItems(mTokenContainer);
        _processParagraphLines(mTokenContainer);
        mTokenContainer->processSpanElements(*mIdTable);
        mProcessed=true;
    }
}

optional<TokenPtr> Document::parseFencedCodeBlock(CTokenGroupIter& i, CTokenGroupIter end) {
    if (!(*i)->isBlankLine() && (*i)->text() && (*i)->canContainMarkup()) {
        string info("");
        std::ostringstream out;
        int indent, length;
        char fence;
        if (!isCodeFenceBeginLine(*((*i)->text()), indent, length, fence, info))
            return none;
        ++i;
        while (i!=end && !isCodeFenceEndLine(*((*i)->text()), indent, length, fence, out))
            ++i;
        // Unclosed code blocks are closed by the end of the document
        if (i == end)
            --i;
        return TokenPtr(new markdown::token::FencedCodeBlock(out.str(), info, mHighlighter));
    }
    return none;
}

void Document::_processFencedBlocks() {
    token::Container *tokens=dynamic_cast<token::Container*>(mTokenContainer.get());
    assert(tokens!=0);

    TokenGroup processed;

    for (auto ii=tokens->subTokens().begin(),
            iie=tokens->subTokens().end(); ii!=iie; ++ii)
    {
        optional<TokenPtr> subitem;
        subitem=parseFencedCodeBlock(ii, iie);

        if (subitem) {
            processed.push_back(*subitem);
            continue;
        } else
            processed.push_back(*ii);
    }
    tokens->swapSubtokens(processed);
}

void Document::_mergeMultilineHtmlTags() {
    static const regex cHtmlTokenStart("<((/?)([a-zA-Z0-9]+)(?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\5))*? */? *))$");
    static const regex cHtmlTokenEnd("^ *((?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\3))*? */? *))>");

    TokenGroup processed;

    token::Container *tokens=dynamic_cast<token::Container*>(mTokenContainer.get());
    assert(tokens!=0);

    for (auto i=tokens->subTokens().cbegin(),
            ie=tokens->subTokens().cend(); i!=ie; ++i)
    {
        if ((*i)->text() && regex_match(*(*i)->text(), cHtmlTokenStart)) {
            auto i2=i;
            ++i2;
            if (i2!=tokens->subTokens().end() && (*i2)->text() &&
                    regex_match(*(*i2)->text(), cHtmlTokenEnd))
            {
                processed.push_back(TokenPtr(new markdown::token::RawText(*(*i)->text()+' '+*(*i2)->text())));
                ++i;
                continue;
            }
        }
        processed.push_back(*i);
    }
    tokens->swapSubtokens(processed);
}

void Document::_processInlineHtmlAndReferences() {
    TokenGroup processed;

    token::Container *tokens=dynamic_cast<token::Container*>(mTokenContainer.get());
    assert(tokens!=0);

    for (auto ii=tokens->subTokens().begin(),
            iie=tokens->subTokens().end(); ii!=iie; ++ii)
    {
        if ((*ii)->text()) {
            if (processed.empty() || processed.back()->isBlankLine()) {
                optional<TokenPtr> inlineHtml=parseInlineHtml(ii, iie);
                if (inlineHtml) {
                    processed.push_back(*inlineHtml);
                    if (ii==iie) break;
                    continue;
                }
            }

            if (parseReference(ii, iie, *mIdTable)) {
                if (ii==iie) break;
                continue;
            }

            // If it gets down here, just store it in its current (raw text)
            // form. We'll group the raw text lines into paragraphs in a
            // later pass, since we can't easily tell where paragraphs
            // end until then.
        }
        processed.push_back(*ii);
    }
    tokens->swapSubtokens(processed);
}

void Document::_processBlocksItems(TokenPtr inTokenContainer) {
    if (!inTokenContainer->isContainer()) return;

    token::Container *tokens=dynamic_cast<token::Container*>(inTokenContainer.get());
    assert(tokens!=0);

    TokenGroup processed;
    TokenGroup accu;
    bool isPrevParagraph = false;
    bool isBlockQuote = false;
    bool isPrevBlockQuote = false;
    bool isPrevBlankLine = false;

    for (auto ii=tokens->subTokens().cbegin(),
            iie=tokens->subTokens().cend(); ii!=iie; ++ii)
    {
        int status(0);
        optional<TokenPtr> subitem, blockQuoteToken;
        if ((*ii)->text()) {
            isBlockQuote = parseBlockQuote(accu, ii, iie);
            
            if (ii != iie) {
                subitem=parseHorizontalRule(ii, iie);
                if (!subitem) subitem=parseListBlock(ii, iie);
                if (!subitem) subitem=parseHeader(ii, iie);
                if (!subitem && !isPrevParagraph)
                    subitem=parseCodeBlock(ii, iie);
            }

            if (isBlockQuote) {
                if (subitem) status = 1;
                else if (ii != iie) status = 2;
                else status = 3;
            } else {
                if (subitem) status = 4;
                else status = 5;
            }
        } else if ((*ii)->isContainer())
            status = 6;
        
        switch (status) {
            case 1:
                blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                _processBlocksItems(*blockQuoteToken);
                processed.push_back(*blockQuoteToken);
                accu.clear();
                
                _processBlocksItems(*subitem);
                processed.push_back(*subitem);
                isPrevBlockQuote = false;
                isPrevParagraph = false;
                break;
                
            case 2:
                isPrevBlankLine = (*ii)->isBlankLine();
                if (!isPrevBlankLine)
                    accu.push_back(*ii);
                ++ii;
                if (isPrevBlankLine || ii == iie) {
                    blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                    _processBlocksItems(*blockQuoteToken);
                    processed.push_back(*blockQuoteToken);
                    accu.clear();
                }
                --ii;
                isPrevBlockQuote = !isPrevBlankLine;
                isPrevParagraph = false;
                break;
                
            case 3:
                blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                _processBlocksItems(*blockQuoteToken);
                processed.push_back(*blockQuoteToken);
                assert(ii==iie);
                break;
                
            case 4:
                if (isPrevBlockQuote) {
                    blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                    _processBlocksItems(*blockQuoteToken);
                    processed.push_back(*blockQuoteToken);
                    accu.clear();
                }
                _processBlocksItems(*subitem);
                processed.push_back(*subitem);
                isPrevBlockQuote = false;
                isPrevParagraph = false;
                break;
                
            case 5:
                isPrevBlankLine = (*ii)->isBlankLine();
                if (isPrevBlockQuote) {
                    if (!isPrevBlankLine)
                        accu.push_back(*ii);
                    ++ii;
                    if (isPrevBlankLine || ii == iie) {
                        blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                        _processBlocksItems(*blockQuoteToken);
                        processed.push_back(*blockQuoteToken);
                        accu.clear();
                    }
                    --ii;
                    isPrevBlockQuote = !isPrevBlankLine;
                    isPrevParagraph = false;
                } else {
                    processed.push_back(*ii);
                    isPrevBlockQuote = false;
                    isPrevParagraph = !isPrevBlankLine;
                }
                break;
                
            case 6:
                if (isPrevBlockQuote) {
                    blockQuoteToken = TokenPtr(new markdown::token::BlockQuote(accu));
                    _processBlocksItems(*blockQuoteToken);
                    processed.push_back(*blockQuoteToken);
                    accu.clear();
                }
                _processBlocksItems(*ii);
                processed.push_back(*ii);
                isPrevParagraph = false;
                isPrevBlockQuote = false;
                break;
            default:
                break;
        } // end of switch
        if (ii == iie)
            break;
    } // end of for loop
    tokens->swapSubtokens(processed);
}

void Document::_processParagraphLines(TokenPtr inTokenContainer) {
    token::Container *tokens=dynamic_cast<token::Container*>(inTokenContainer.get());
    assert(tokens!=0);

    bool noPara=tokens->inhibitParagraphs();
    for (auto ii=tokens->subTokens().cbegin(),
            iie=tokens->subTokens().cend(); ii!=iie; ++ii)
        if ((*ii)->isContainer()) _processParagraphLines(*ii);

    TokenGroup processed;
    string paragraphText;
    TokenGroup paragraphTokens;
    for (auto ii=tokens->subTokens().cbegin(),
            iie=tokens->subTokens().cend(); ii!=iie; ++ii)
    {
        if ((*ii)->text() && (*ii)->canContainMarkup() && !(*ii)->inhibitParagraphs()) {
            static const regex cExpression("^ *(.*?)(  +)?$");
            smatch m;
            regex_match(*(*ii)->text(), m, cExpression);
            paragraphTokens.push_back(TokenPtr(new markdown::token::RawText(m.str(1))));
            auto after = ii;
            if (m[2].matched && ++after != iie)
                paragraphTokens.push_back(TokenPtr(new markdown::token::HtmlTag("br /")));
        } else {
            flushParagraph(paragraphTokens, processed, noPara);
            processed.push_back(*ii);
        }
    }

    // Make sure the last paragraph is properly flushed too.
    flushParagraph(paragraphTokens, processed, noPara);

    tokens->swapSubtokens(processed);
}

} // namespace markdown
