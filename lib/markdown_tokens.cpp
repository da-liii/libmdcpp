
/*
	Copyright (c) 2009 by Chad Nelson
	Released under the MIT License.
	See the provided LICENSE.TXT file for details.
*/

#include "markdown_tokens.h"

#include <stack>
#include <algorithm>
#include <unordered_set>
#include <cctype>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

using std::cerr;
using std::endl;
using boost::regex;
using boost::smatch;
using boost::regex_search;
using boost::regex_match;
using std::unordered_set;
using std::isalnum;

namespace markdown {
namespace token {

namespace {

const string cEscapedCharacters("\\`*_{}[]()#+-.!>");

optional<size_t> isEscapedCharacter(char c) {
    string::const_iterator i=std::find(cEscapedCharacters.begin(),
                                       cEscapedCharacters.end(), c);
    if (i!=cEscapedCharacters.end())
        return std::distance(cEscapedCharacters.begin(), i);
    else return none;
}

char escapedCharacter(size_t index) {
    return cEscapedCharacters[index];
}

string encodeString(const string& src, int encodingFlags) {
    bool amps=(encodingFlags & cAmps)!=0,
         doubleAmps=(encodingFlags & cDoubleAmps)!=0,
         angleBrackets=(encodingFlags & cAngles)!=0,
         quotes=(encodingFlags & cQuotes)!=0;

    string tgt;
    for (auto i=src.cbegin(), ie=src.cend(); i!=ie; ++i) {
        if (*i=='&' && amps) {
            static const regex cIgnore("^(&amp;)|(&#[0-9]{1,3};)|(&#[xX][0-9a-fA-F]{1,2};)");
            if (regex_search(i, ie, cIgnore)) {
                tgt.push_back(*i);
            } else {
                tgt+="&amp;";
            }
        }
        else if (*i=='&' && doubleAmps) tgt+="&amp;";
        else if (*i=='<' && angleBrackets) tgt+="&lt;";
        else if (*i=='>' && angleBrackets) tgt+="&gt;";
        else if (*i=='\"' && quotes) tgt+="&quot;";
        else tgt.push_back(*i);
    }
    return tgt;
}

bool looksLikeUrl(const string& str) {
    const char *schemes[]= { "http://", "https://", "ftp://", "ftps://",
                             "file://", "www.", "ftp.", 0
                           };
    for (size_t x=0; schemes[x]!=0; ++x) {
        const char *s=str.c_str(), *t=schemes[x];
        while (*s!=0 && *t!=0 && *s==*t) {
            ++s;
            ++t;
        }
        if (*t==0) return true;
    }
    return false;
}

bool notValidNameCharacter(char c) {
    return !(isalnum(c) || c=='.' || c=='_' || c=='%' || c=='-' || c=='+');
}

bool notValidSiteCharacter(char c) {
    // NOTE: Kludge alert! The official spec for site characters is only
    // "a-zA-Z._%-". However, MDTest supports "international domain names,"
    // which use characters other than that; I'm kind of cheating here, handling
    // those by allowing all utf8-encoded characters too.
    return !(isalnum(c) || c=='.' || c=='_' || c=='%' || c=='-' || (c & 0x80));
}

bool isNotAlpha(char c) {
    return !isalpha(c);
}

string emailEncode(const string& src) {
    std::ostringstream out;
    bool inHex=false;
    for (auto i=src.cbegin(), ie=src.cend(); i!=ie;
            ++i)
    {
        if (*i & 0x80) out << *i;
        else if (inHex) {
            out << "&#x" << std::hex << static_cast<int>(*i) << ';';
        } else {
            out << "&#" << std::dec << static_cast<int>(*i) << ';';
        }
        inHex=!inHex;
    }
    return out.str();
}

bool looksLikeEmailAddress(const string& str) {
    typedef string::const_iterator Iter;
    typedef string::const_reverse_iterator RIter;
    Iter i=std::find_if(str.begin(), str.end(), notValidNameCharacter);
    if (i!=str.cend() && *i=='@' && i!=str.cbegin()) {
        // The name part is valid.
        i=std::find_if(i+1, str.cend(), notValidSiteCharacter);
        if (i==str.end()) {
            // The site part doesn't contain any invalid characters.
            RIter ri=std::find_if(str.crbegin(), str.crend(), isNotAlpha);
            if (ri!=str.crend() && *ri=='.') {
                // It ends with a dot and only alphabetic characters.
                size_t d=std::distance(ri.base(), str.cend());
                if (d>=2 && d<=4) {
                    // There are two-to-four of them. It's valid.
                    return true;
                }
            }
        }
    }
    return false;
}

// From <http://en.wikipedia.org/wiki/HTML_element>

const char *cOtherTagInit[]= {
    // Header tags
    "title/", "link", "script/", "style/",
    "object/", "meta",

    // Inline tags
    "em/", "strong/", "q/", "cite/", "dfn/", "abbr/", "acronym/",
    "code/", "samp/", "kbd/", "var/", "sub/", "sup/", "del/", "ins/",
    "isindex", "a/", "img", "br", "map/", "area", "object/", "param",
    "applet/", "span/",

    0
};

const char *cBlockTagInit[]= { "address/", "article/", "aside/", "base", "basefont", "blockquote/",
                               "body/", "caption/", "center/", "col", "colgroup/", "dd/", "details",
                               "dir/", "div/", "dl/", "dt/", "fieldset/", "figcaption", "figure",
                               "footer", "form/", "frame/", "frameset/", "h1/", "h2/", "h3/", "h4/",
                               "h5/", "h6/", "ul/", "head", "header", "hr", "html/", "iframe/",
                               "legend", "li/", "link", "main/", "menu/", "menuitem", "meta", "nav",
                               "noframes/", "ol/", "optgroup", "option", "p/", "param", "section",
                               "source", "summary", "table/", "tbody/", "tr/", "th/", "td/", "thead/",
                               "tfoot/", "title", "track", "ul/"
                             };

// Other official ones (not presently in use in this code)
//"!doctype", "bdo", "body", "button", "fieldset", "head", "html",
//"legend", "noscript", "optgroup", "xmp",

unordered_set<string> otherTags, blockTags;

void initTag(unordered_set<string> &set, const char *init[]) {
    for (size_t x=0; init[x]!=0; ++x) {
        string str=init[x];
        if (*str.rbegin()=='/') {
            // Means it can have a closing tag
            str=str.substr(0, str.length()-1);
        }
        set.insert(str);
    }
}

string cleanTextLinkRef(const string& ref) {
    string r;
    for (auto i=ref.cbegin(), ie=ref.cend(); i!=ie;
            ++i)
    {
        if (*i==' ') {
            if (r.empty() || *r.rbegin()!=' ') r.push_back(' ');
        } else r.push_back(*i);
    }
    return r;
}

} // namespace



size_t isValidTag(const string& tag, bool nonBlockFirst) {
    if (blockTags.empty()) {
        initTag(otherTags, cOtherTagInit);
        initTag(blockTags, cBlockTagInit);
    }

    string TAG(tag);
    transform(TAG.begin(), TAG.end(), TAG.begin(), tolower);
    if (nonBlockFirst) {
        if (otherTags.find(TAG)!=otherTags.end()) return 1;
        if (blockTags.find(TAG)!=blockTags.end()) return 2;
    } else {
        if (blockTags.find(TAG)!=blockTags.end()) return 2;
        if (otherTags.find(TAG)!=otherTags.end()) return 1;
    }
    return 0;
}



void TextHolder::writeAsHtml(std::ostream& out) const {
    preWrite(out);
    if (mEncodingFlags!=0) {
        out << encodeString(mText, mEncodingFlags);
    } else {
        out << mText;
    }
    postWrite(out);
}

optional<TokenGroup> RawText::processSpanElements(const LinkIds& idTable) {
    if (!canContainMarkup()) return none;

    ReplacementTable replacements;
    string str=_processHtmlTagAttributes(*text(), replacements);
    str=_processCodeSpans(str, replacements);
    str=_processEscapedCharacters(str);
    str=_processLinksImagesAndTags(str, replacements, idTable);
    return _processBoldAndItalicSpans(str, replacements);
}

string RawText::_processHtmlTagAttributes(string src, ReplacementTable&
        replacements)
{
    // Because "Attribute Content Is Not A Code Span"
    string tgt;
    auto prev=src.cbegin(), end=src.cend();
    while (true) {
        static const regex cHtmlToken("<((/?)([a-zA-Z0-9]+)(?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\5))+? */? *))>");
        smatch m;
        if (regex_search(prev, end, m, cHtmlToken)) {
            // NOTE: Kludge alert! The `isValidTag` test is a cheat, only here
            // to handle some edge cases between the Markdown test suite and the
            // PHP-Markdown one, which seem to conflict.
            if (isValidTag(m[3])) {
                tgt+=string(prev, m[0].first);

                string fulltag=m[0], tgttag;
                auto prevtag=fulltag.cbegin(), endtag=fulltag.cend();
                while (1) {
                    static const regex cAttributeStrings("= ?(\"|').*?\\1");
                    smatch mtag;
                    if (regex_search(prevtag, endtag, mtag, cAttributeStrings)) {
                        tgttag+=string(prevtag, mtag[0].first);
                        tgttag+="\x01@"+boost::lexical_cast<string>(replacements.size())+"@htmlTagAttr\x01";
                        prevtag=mtag[0].second;

                        replacements.push_back(TokenPtr(new TextHolder(string(mtag[0]), false, cAmps|cAngles)));
                    } else {
                        tgttag+=string(prevtag, endtag);
                        break;
                    }
                }
                tgt+=tgttag;
                prev=m[0].second;
            } else {
                tgt+=string(prev, m[0].second);
                prev=m[0].second;
            }
        } else {
            tgt+=string(prev, end);
            break;
        }
    }

    return tgt;
}

string RawText::_processCodeSpans(string src, ReplacementTable&
                                  replacements)
{
    static const regex cCodeSpan = regex("(?<!`)(`+)(?!`) *(.*?[^ ]) *(?<!`)\\1(?!`)");
    
    string tgt;
    auto prev=src.cbegin(), end=src.cend();
    while (true) {
        smatch m;
        if (regex_search(prev, end, m, cCodeSpan)) {
            tgt += string(prev, m[0].first);
            tgt += "\x01@"+boost::lexical_cast<string>(replacements.size())+"@codeSpan\x01";
            prev = m[0].second;
            replacements.push_back(TokenPtr(new CodeSpan(_restoreProcessedItems(m.str(2), replacements))));
        } else {
            tgt += string(prev, end);
            break;
        }
    }
    src.swap(tgt);
    tgt.clear();
        
    return src;
}

string RawText::_processEscapedCharacters(const string& src) {
    string tgt;
    auto prev=src.cbegin(), end=src.cend();
    while (true) {
        string::const_iterator i=std::find(prev, end, '\\');
        if (i!=end) {
            tgt+=string(prev, i);
            ++i;
            if (i!=end) {
                optional<size_t> e=isEscapedCharacter(*i);
                if (e) tgt+="\x01@#"+boost::lexical_cast<string>(*e)+"@escaped\x01";
                else tgt=tgt+'\\'+*i;
                prev=i+1;
            } else {
                tgt+='\\';
                break;
            }
        } else {
            tgt+=string(prev, end);
            break;
        }
    }
    return tgt;
}

string RawText::_processSpaceBracketedGroupings(const string &src,
        ReplacementTable& replacements)
{
    static const regex cRemove("(?:(?: \\*+ )|(?: _+ ))");

    string tgt;
    auto prev=src.cbegin(), end=src.cend();
    while (1) {
        smatch m;
        if (regex_search(prev, end, m, cRemove)) {
            tgt+=string(prev, m[0].first);
            tgt+="\x01@"+boost::lexical_cast<string>(replacements.size())+"@spaceBracketed\x01";
            replacements.push_back(TokenPtr(new RawText(m[0])));
            prev=m[0].second;
        } else {
            tgt+=string(prev, end);
            break;
        }
    }
    return tgt;
}

string RawText::_processLinksImagesAndTags(const string &src,
        ReplacementTable& replacements, const LinkIds& idTable)
{
    // NOTE: Kludge alert! The "inline link or image" regex should be...
    //
    //   "(?:(!?)\\[(.+?)\\] *\\((.*?)\\))"
    //
    // ...but that fails on the 'Images' test because it includes a "stupid URL"
    // that has parentheses within it. The proper way to deal with this would be
    // to match any nested parentheses, but regular expressions can't handle an
    // unknown number of nested items, so I'm cheating -- the regex for it
    // allows for one (and *only* one) pair of matched parentheses within the
    // URL. It makes the regex hard to follow (it was even harder to get right),
    // but it allows it to pass the test.
    //
    // The "reference link or image" one has a similar problem; it should be...
    //
    //   "|(?:(!?)\\[(.+?)\\](?: *\\[(.*?)\\])?)"
    //
    static const regex cExpression(
        "(?:(!?)\\[(.*)\\]\\(([^\\(]*(?:\\(.*?\\).*?)*?)\\))" // Inline link or image
        "|(?:(!?)\\[((?:[^]]*?\\[.*?\\].*?)|(?:.+?))\\](?: *\\[(.*?)\\])?)" // Reference link or image
        "|(?:<(/?([a-zA-Z0-9]+).*?)>)" // potential HTML tag or auto-link
    );
    // Important captures: 1/4=image indicator, 2/5=contents/alttext,
    // 3=URL/title, 6=optional link ID, 7=potential HTML tag or auto-link
    // contents, 8=actual tag from 7.

    string tgt;
    auto prev=src.cbegin(), end=src.cend();
    while (1) {
        smatch m;
        if (regex_search(prev, end, m, cExpression)) {
            assert(m[0].matched);
            assert(m[0].length()!=0);

            tgt+=string(prev, m[0].first);
            tgt+="\x01@"+boost::lexical_cast<string>(replacements.size())+"@links&Images1\x01";
            prev=m[0].second;

            bool isImage=false, isLink=false, isReference=false;
            if (m[4].matched && m[4].length()) isImage=isReference=true;
            else if (m[1].matched && m[1].length()) isImage=true;
            else if (m[5].matched) isLink=isReference=true;
            else if (m[2].matched) isLink=true;

            if (isImage || isLink) {
                string contentsOrAlttext, url, title;
                bool resolved=false;
                if (isReference) {
                    contentsOrAlttext=m[5];
                    string linkId=(m[6].matched ? string(m[6]) : string());
                    if (linkId.empty()) linkId=cleanTextLinkRef(contentsOrAlttext);

                    optional<markdown::LinkIds::Target> target=idTable.find(linkId);
                    if (target) {
                        url=target->url;
                        title=target->title;
                        resolved=true;
                    };
                } else {
                    static const regex cReference("^<?([^ >]*)>?(?: *(?:('|\")(.*)\\2)|(?:\\((.*)\\)))? *$");
                    // Useful captures: 1=url, 3/4=title
                    contentsOrAlttext=m[2];
                    string urlAndTitle=m[3];
                    smatch mm;
                    if (regex_match(urlAndTitle, mm, cReference)) {
                        url=mm[1];
                        if (mm[3].matched) title=mm[3];
                        else if (mm[4].matched) title=mm[4];
                        resolved=true;
                    }
                }

                if (!resolved) {
                    // Just encode the first character as-is, and continue
                    // searching after it.
                    prev=m[0].first+1;
                    replacements.push_back(TokenPtr(new RawText(string(m[0].first, prev))));
                } else if (isImage) {
                    replacements.push_back(TokenPtr(new Image(contentsOrAlttext,
                                                    url, title)));
                } else {
                    replacements.push_back(TokenPtr(new HtmlAnchorTag(url, title)));
                    tgt+=contentsOrAlttext;
                    tgt+="\x01@"+boost::lexical_cast<string>(replacements.size())+"@links&Images2\x01";
                    replacements.push_back(TokenPtr(new HtmlTag("/a")));
                }
            } else {
                // Otherwise it's an HTML tag or auto-link.
                string contents=m[7];

//				cerr << "Evaluating potential HTML or auto-link: " << contents << endl;
//				cerr << "m[8]=" << m[8] << endl;

                if (looksLikeUrl(contents)) {
                    TokenGroup subgroup;
                    subgroup.push_back(TokenPtr(new HtmlAnchorTag(contents)));
                    subgroup.push_back(TokenPtr(new RawText(contents, false)));
                    subgroup.push_back(TokenPtr(new HtmlTag("/a")));
                    replacements.push_back(TokenPtr(new Container(subgroup)));
                } else if (looksLikeEmailAddress(contents)) {
                    TokenGroup subgroup;
                    subgroup.push_back(TokenPtr(new HtmlAnchorTag(emailEncode("mailto:"+contents))));
                    subgroup.push_back(TokenPtr(new RawText(emailEncode(contents), false)));
                    subgroup.push_back(TokenPtr(new HtmlTag("/a")));
                    replacements.push_back(TokenPtr(new Container(subgroup)));
                } else if (isValidTag(m[8])) {
                    replacements.push_back(TokenPtr(new HtmlTag(_restoreProcessedItems(contents, replacements))));
                } else {
                    // Just encode it as-is
                    replacements.push_back(TokenPtr(new RawText(m[0])));
                }
            }
        } else {
            tgt+=string(prev, end);
            break;
        }
    }
    return tgt;
}

TokenGroup RawText::_processBoldAndItalicSpans(const string& src,
        ReplacementTable& replacements)
{
    static const regex cEmphasisExpression(
        "((?:(?<= |[[:punct:]])\\*{1,3}(?! |$))|"                            // Open
        "(?:\\*{1,3}(?! |$|[[:punct:]])))|"
        "((?:_{1,3}(?! |$|[[:punct:]]))|"
        "(?:(?<![[:punct:]])(?<= )_{1,3}(?! |$)(?![[:punct:]])))|"                                  
        "((?:(?<! )\\*{1,3}(?=$| |[[:punct:]]))|"                                 // Close
        "(?:(?<! |[[:punct:]])\\*{1,3}))|"
        "((?<! |[[:punct:]])_{1,3}|"
        "(?<! )(?<=[[:punct:]])_{1,3}(?= |$)(?![[:punct:]]))"
    );

    TokenGroup tgt;
    auto i = src.cbegin(), end = src.cend(), prev = i;

    while (true) {
        smatch m;
        if (regex_search(prev, end, m, cEmphasisExpression)) {
            if (prev != m[0].first)
                tgt.push_back(TokenPtr(new RawText(string(prev, m[0].first))));
            
            if (m[1].matched) {
                string token = m[1];
                tgt.push_back(TokenPtr(new BoldOrItalicMarker(true, token[0],
                                       token.length())));
            } else if (m[2].matched) {
                string token = m[2];
                if (m[0].first != i && m[0].second != end &&
                    isalnum(*(m[0].first-1)) && isalnum(*m[0].second))
                    tgt.push_back(TokenPtr(new RawText(token)));
                else
                    tgt.push_back(TokenPtr(new BoldOrItalicMarker(true, token[0],
                                            token.length())));
            } else if (m[3].matched) {
                string token = m[3];
                tgt.push_back(TokenPtr(new BoldOrItalicMarker(false, token[0],
                                       token.length())));
            } else if (m[4].matched) {
                string token = m[4];
                if (m[0].first != i && m[0].second != end &&
                    isalnum(*(m[0].first-1)) && isalnum(*m[0].second))
                    tgt.push_back(TokenPtr(new RawText(token)));
                else
                    tgt.push_back(TokenPtr(new BoldOrItalicMarker(false, token[0],
                                            token.length())));
            }
            prev = m[0].second;
        } else {
            if (prev != end)
                tgt.push_back(TokenPtr(new RawText(string(prev, end))));
            break;
        }
    }

    int id=0;
    for (auto ii=tgt.begin(), iie=tgt.end(); ii!=iie; ++ii) {
        if ((*ii)->isUnmatchedOpenMarker()) {
            BoldOrItalicMarker *openToken=dynamic_cast<BoldOrItalicMarker*>(ii->get());

            // Find a matching close-marker, if it's there
            auto iii=ii;
            for (++iii; iii!=iie; ++iii) {
                if ((*iii)->isUnmatchedCloseMarker()) {
                    BoldOrItalicMarker *closeToken=dynamic_cast<BoldOrItalicMarker*>(iii->get());
                    if (closeToken->size()==3 && openToken->size()!=3) {
                        // Split the close-token into a match for the open-token
                        // and a second for the leftovers.
                        closeToken->disable();
                        TokenGroup g;
                        g.push_back(TokenPtr(new BoldOrItalicMarker(false,
                                             closeToken->tokenCharacter(), closeToken->size()-
                                             openToken->size())));
                        g.push_back(TokenPtr(new BoldOrItalicMarker(false,
                                             closeToken->tokenCharacter(), openToken->size())));
                        auto after=iii;
                        ++after;
                        tgt.splice(after, g);
                        continue;
                    }

                    if (closeToken->tokenCharacter()==openToken->tokenCharacter()
                            && closeToken->size()==openToken->size())
                    {
                        openToken->matched(closeToken, id);
                        closeToken->matched(openToken, id);
                        ++id;
                        break;
                    } else if (openToken->size()==3) {
                        // Split the open-token into a match for the close-token
                        // and a second for the leftovers.
                        openToken->disable();
                        TokenGroup g;
                        g.push_back(TokenPtr(new BoldOrItalicMarker(true,
                                             openToken->tokenCharacter(), openToken->size()-
                                             closeToken->size())));
                        g.push_back(TokenPtr(new BoldOrItalicMarker(true,
                                             openToken->tokenCharacter(), closeToken->size())));
                        auto after=ii;
                        ++after;
                        tgt.splice(after, g);
                        break;
                    }
                }
            }
        }
    }

    // "Unmatch" invalidly-nested matches.
    std::stack<BoldOrItalicMarker*> openMatches;
    for (auto ii=tgt.begin(), iie=tgt.end(); ii!=iie; ++ii) {
        if ((*ii)->isMatchedOpenMarker()) {
            BoldOrItalicMarker *open=dynamic_cast<BoldOrItalicMarker*>(ii->get());
            openMatches.push(open);
        } else if ((*ii)->isMatchedCloseMarker()) {
            BoldOrItalicMarker *close=dynamic_cast<BoldOrItalicMarker*>(ii->get());

            if (close->id() != openMatches.top()->id()) {
                close->matchedTo()->matched(0);
                close->matched(0);
            } else {
                openMatches.pop();
                while (!openMatches.empty() && openMatches.top()->matchedTo()==0)
                    openMatches.pop();
            }
        }
    }

    TokenGroup r;
    for (auto ii=tgt.begin(), iie=tgt.end(); ii!=iie; ++ii) {
        if ((*ii)->text() && (*ii)->canContainMarkup()) {
            TokenGroup t=_encodeProcessedItems(*(*ii)->text(), replacements);
            r.splice(r.end(), t);
        } else r.push_back(*ii);
    }

    return r;
}

TokenGroup RawText::_encodeProcessedItems(const string &src,
        ReplacementTable& replacements)
{
    static const regex cReplaced("\x01@(#?[0-9]*)@.+?\x01");

    TokenGroup r;
    auto prev=src.cbegin();
    while (1) {
        smatch m;
        if (regex_search(prev, src.cend(), m, cReplaced)) {
            string pre=string(prev, m[0].first);
            if (!pre.empty()) r.push_back(TokenPtr(new RawText(pre)));
            prev=m[0].second;

            string ref=m[1];
            if (ref[0]=='#') {
                size_t n=boost::lexical_cast<size_t>(ref.substr(1));
                r.push_back(TokenPtr(new EscapedCharacter(escapedCharacter(n))));
            } else if (!ref.empty()) {
                size_t n=boost::lexical_cast<size_t>(ref);

                assert(n<replacements.size());
                r.push_back(replacements[n]);
            } // Otherwise just eat it
        } else {
            string pre=string(prev, src.end());
            if (!pre.empty())
                r.push_back(TokenPtr(new RawText(pre)));
            break;
        }
    }
    return r;
}

string RawText::_restoreProcessedItems(const string &src,
                                       ReplacementTable& replacements)
{
    static const regex cReplaced("\x01@(#?[0-9]*)@.+?\x01");

    std::ostringstream r;
    auto prev=src.cbegin();
    while (1) {
        smatch m;
        if (regex_search(prev, src.cend(), m, cReplaced)) {
            string pre=string(prev, m[0].first);
            if (!pre.empty()) r << pre;
            prev=m[0].second;

            string ref=m[1];
            if (ref[0]=='#') {
                size_t n=boost::lexical_cast<size_t>(ref.substr(1));
                r << '\\' << escapedCharacter(n);
            } else if (!ref.empty()) {
                size_t n=boost::lexical_cast<size_t>(ref);

                assert(n<replacements.size());
                replacements[n]->writeAsOriginal(r);
            } // Otherwise just eat it
        } else {
            string pre=string(prev, src.end());
            if (!pre.empty()) r << pre;
            break;
        }
    }
    return r.str();
}

HtmlAnchorTag::HtmlAnchorTag(const string& url, const string& title):
    TextHolder("<a href=\""+encodeString(url, cQuotes|cAmps)+"\""
               +(title.empty() ? string() : " title=\""+encodeString(title, cQuotes|cAmps)+"\"")
               +">", false, 0)
{
    // This space deliberately blank. ;-)
}

void CodeBlock::writeAsHtml(std::ostream& out) const {
    out << "<pre><code>";
    TextHolder::writeAsHtml(out);
    out << "</code></pre>\n";
}

void FencedCodeBlock::writeAsHtml(std::ostream& out) const
{
    if (mInfoString.empty()) {
        out << "<pre><code>";
        TextHolder::writeAsHtml(out);
    } else {
        auto si = mInfoString.begin(), sie = mInfoString.end();
        while (si!=sie && *si==' ') si++;
        auto sii=si;
        while (sii!=sie && *sii!=' ') sii++;
        out << "<pre><code class=\"language-"+ string(si, sii) + "\">";
        mHighlighter->highlight(*text(), string(si, sii), out);
    }

    out << "</code></pre>\n\n";
}


void CodeSpan::writeAsHtml(std::ostream& out) const {
    out << "<code>";
    TextHolder::writeAsHtml(out);
    out << "</code>";
}

void CodeSpan::writeAsOriginal(std::ostream& out) const {
    out << '`' << *text() << '`';
}



void Container::writeAsHtml(std::ostream& out) const {
    preWrite(out);
    for (auto i=mSubTokens.cbegin(), ie=mSubTokens.cend(); i!=ie; ++i)
        (*i)->writeAsHtml(out);
    postWrite(out);
}

void Container::writeToken(size_t indent, std::ostream& out) const {
    out << string(indent*2, ' ') << containerName() << endl;
    for (auto ii=mSubTokens.cbegin(), iie=mSubTokens.cend(); ii!=iie;
            ++ii)
        (*ii)->writeToken(indent+1, out);
}

optional<TokenGroup> Container::processSpanElements(const LinkIds& idTable) {
    TokenGroup t;
    for (auto ii=mSubTokens.cbegin(), iie=mSubTokens.cend(); ii!=iie;
            ++ii)
    {
        if ((*ii)->text()) {
            optional<TokenGroup> subt=(*ii)->processSpanElements(idTable);
            if (subt) {
                if (subt->size()>1) t.push_back(TokenPtr(new Container(*subt)));
                else if (!subt->empty()) t.push_back(*subt->begin());
            } else t.push_back(*ii);
        } else {
            optional<TokenGroup> subt=(*ii)->processSpanElements(idTable);
            if (subt) {
                const Container *c=dynamic_cast<const Container*>((*ii).get());
                assert(c!=0);
                t.push_back(c->clone(*subt));
            } else t.push_back(*ii);
        }
    }
    swapSubtokens(t);
    return none;
}

UnorderedList::UnorderedList(const TokenGroup& contents, bool paragraphMode) {
    if (paragraphMode) {
        // Change each of the text items into paragraphs
        for (auto i=contents.cbegin(), ie=contents.cend(); i!=ie; ++i) {
            token::ListItem *item=dynamic_cast<token::ListItem*>((*i).get());
            assert(item!=0);
            item->inhibitParagraphs(false);
            mSubTokens.push_back(*i);
        }
    } else mSubTokens=contents;
}

void Paragraph::writeAsHtml(std::ostream& out) const {
    preWrite(out);
    for (auto i=mSubTokens.cbegin(), ie=mSubTokens.cend(); i!=ie;) {
        (*i)->writeAsHtml(out);
        ++i;
        if (i!=ie && ((*i)->isRawText() || (*i)->isUnmatchedOpenMarker() 
            || (*i)->isUnmatchedCloseMarker()))
            out << "\n";
    }
    postWrite(out);
}


void BoldOrItalicMarker::writeAsHtml(std::ostream& out) const {
    if (!mDisabled) {
        if (mMatch!=0) {
            assert(mSize>=1 && mSize<=3);
            if (mOpenMarker) {
                out << (mSize==1 ? "<em>" : mSize==2 ? "<strong>" : "<strong><em>");
            } else {
                out << (mSize==1 ? "</em>" : mSize==2 ? "</strong>" : "</em></strong>");
            }
        } else out << string(mSize, mTokenCharacter);
    }
}

void BoldOrItalicMarker::writeToken(std::ostream& out) const {
    if (!mDisabled) {
        if (mMatch!=0) {
            string type=(mSize==1 ? "italic" : mSize==2 ? "bold" : "italic&bold");
            if (mOpenMarker) {
                out << "Matched open-" << type << " marker" << endl;
            } else {
                out << "Matched close-" << type << " marker" << endl;
            }
        } else {
            if (mOpenMarker) out << "Unmatched bold/italic open marker: " <<
                                     string(mSize, mTokenCharacter) << endl;
            else out << "Unmatched bold/italic close marker: " <<
                         string(mSize, mTokenCharacter) << endl;
        }
    }
}

void Image::writeAsHtml(std::ostream& out) const {
    out << "<img src=\"" << mUrl << "\" alt=\"" << mAltText << "\"";
    if (!mTitle.empty()) out << " title=\"" << mTitle << "\"";
    out << "/>";
}

} // namespace token
} // namespace markdown
