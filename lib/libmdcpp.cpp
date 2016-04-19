#include "libmdcpp.h"
#include "markdown.h"

Procesoro::Procesoro(SyntaxHighlighter *highlighter, const string type)
{
    if (type == "markdown") {
        mDocument = new markdown::Document(highlighter);
    } else {
        // TODO: should handle others
    }
}
  
bool Procesoro::read(const string& aString)
{
    return mDocument->read(aString);
}

bool Procesoro::read(std::istream& aIstream)
{
    return mDocument->read(aIstream);
}

void Procesoro::write(std::ostream& aOstream)
{
    mDocument->write(aOstream);
}


Procesoro::~Procesoro() {
    delete mDocument;
}
