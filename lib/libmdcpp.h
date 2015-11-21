#ifndef LIBMDCPP_H_INCLUDED
#define LIBMDCPP_H_INCLUDED

#include <string>

using std::string;

class SyntaxHighlighter {
public:
    SyntaxHighlighter() {}
    virtual ~SyntaxHighlighter() {}
    virtual void highlight(const string& code, const string lang, std::ostream& out) {
        out << code;
    }
};

class Dokumento {
public:
    Dokumento() = default;
    virtual bool read(const string&) { return true; };
    virtual bool read(std::istream&) { return true; };
    virtual void write(std::ostream&) {};
};

class Procesoro {
public:
    Procesoro(SyntaxHighlighter *highlighter, const string type);
    ~Procesoro();
    
    bool read(const string& aString);
    bool read(std::istream& aIstream);
    void write(std::ostream& aOstream);
    
private:
    Dokumento *mDocument;
};

#endif
