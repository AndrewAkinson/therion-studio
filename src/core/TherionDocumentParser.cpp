#include "TherionDocumentParser.h"

#include "TherionTokenRules.h"

namespace TherionStudio
{
bool TherionParsedSourceLine::hasTokens() const
{
    return !parsed.tokens.isEmpty();
}

bool TherionParsedSourceLine::isBlank() const
{
    return text.trimmed().isEmpty();
}

bool TherionParsedSourceLine::isCommentOnly() const
{
    return parsed.tokens.isEmpty() && parsed.commentStart >= 0;
}

int TherionParsedSourceLine::absoluteTokenStart(const TherionParsedToken &token) const
{
    return startOffset + token.start;
}

int TherionParsedSourceLine::absoluteTokenEnd(const TherionParsedToken &token) const
{
    return absoluteTokenStart(token) + token.length;
}

QString TherionParsedSourceDocument::toText() const
{
    QString contents;
    for (const TherionParsedSourceLine &line : lines) {
        contents += line.text;
        contents += line.lineEnding;
    }
    return contents;
}

QVector<TherionParsedLine> TherionParsedSourceDocument::tokenLines() const
{
    QVector<TherionParsedLine> parsedLines;
    for (const TherionParsedSourceLine &line : lines) {
        if (line.parsed.tokens.isEmpty()) {
            continue;
        }
        parsedLines.append(line.parsed);
    }
    return parsedLines;
}

namespace
{
// Index of a trailing line-continuation backslash, or -1.
int continuationMarkerIndex(const QString &line)
{
    int end = line.size();
    while (end > 0 && line.at(end - 1).isSpace()) {
        --end;
    }
    int backslashes = 0;
    while (backslashes < end && line.at(end - 1 - backslashes) == QLatin1Char('\\')) {
        ++backslashes;
    }
    return backslashes % 2 == 1 ? end - 1 : -1;
}
}

TherionParsedLine TherionDocumentParser::parseLine(const QString &line, int lineNumber)
{
    TherionParsedLine parsedLine;
    parsedLine.lineNumber = lineNumber;
    parsedLine.rawText = line;

    QString currentToken;
    int currentTokenStart = -1;
    QString quotedToken;
    int quotedTokenStart = -1;
    bool inSingleQuotes = false;
    bool inDoubleQuotes = false;
    bool escaped = false;
    // A bracketed value is one token that keeps its brackets, as Therion's thsplit_args reads it.
    int bracketTokenStart = -1;
    int bracketDepth = 0;
    const int continuationIndex = continuationMarkerIndex(line);

    auto flushToken = [&parsedLine, &currentToken, &currentTokenStart]() {
        if (currentToken.isEmpty()) {
            currentTokenStart = -1;
            return;
        }

        parsedLine.tokens.append(currentToken);
        TherionParsedToken tokenSpan;
        tokenSpan.start = currentTokenStart;
        tokenSpan.length = currentToken.length();
        tokenSpan.text = currentToken;
        parsedLine.tokenSpans.append(tokenSpan);
        currentToken.clear();
        currentTokenStart = -1;
    };

    auto flushQuotedToken = [&parsedLine, &quotedToken, &quotedTokenStart](int endIndex) {
        if (quotedTokenStart < 0) {
            return;
        }

        parsedLine.tokens.append(quotedToken);
        TherionParsedToken tokenSpan;
        tokenSpan.start = quotedTokenStart;
        tokenSpan.length = qMax(endIndex - quotedTokenStart, 0);
        tokenSpan.text = quotedToken;
        tokenSpan.type = TherionTokenType::QuotedString;
        parsedLine.tokenSpans.append(tokenSpan);
        quotedToken.clear();
        quotedTokenStart = -1;
    };

    auto flushBracketToken = [&parsedLine, &line, &bracketTokenStart, &bracketDepth](int endIndex) {
        if (bracketTokenStart < 0) {
            return;
        }

        // An unclosed value runs to the end of the line, without trailing spaces.
        int end = endIndex;
        while (end > bracketTokenStart && line.at(end - 1).isSpace()) {
            --end;
        }
        const QString text = line.mid(bracketTokenStart, end - bracketTokenStart);
        parsedLine.tokens.append(text);
        TherionParsedToken tokenSpan;
        tokenSpan.start = bracketTokenStart;
        tokenSpan.length = text.length();
        tokenSpan.text = text;
        parsedLine.tokenSpans.append(tokenSpan);
        bracketTokenStart = -1;
        bracketDepth = 0;
    };

    for (int index = 0; index < line.length(); ++index) {
        const QChar character = line.at(index);

        if (bracketTokenStart >= 0) {
            if (index != continuationIndex) {
                if (character == QLatin1Char('[')) {
                    ++bracketDepth;
                } else if (character == QLatin1Char(']') && --bracketDepth == 0) {
                    flushBracketToken(index + 1);
                }
                continue;
            }
            flushBracketToken(index);
        }

        if (escaped) {
            if (inSingleQuotes || inDoubleQuotes) {
                quotedToken.append(character);
            } else {
                if (currentTokenStart < 0) {
                    currentTokenStart = index - 1;
                }
                currentToken.append(character);
            }

            escaped = false;
            continue;
        }

        if (character == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }

        if (!inSingleQuotes && !inDoubleQuotes && (character == QLatin1Char('#') || character == QLatin1Char('%'))) {
            flushQuotedToken(index);
            flushToken();

            parsedLine.commentStart = index;
            parsedLine.commentText = line.mid(index + 1);

            TherionParsedToken commentSpan;
            commentSpan.start = index;
            commentSpan.length = line.length() - index;
            commentSpan.text = line.mid(index);
            commentSpan.type = TherionTokenType::Comment;
            parsedLine.tokenSpans.append(commentSpan);
            break;
        }

        if (!inDoubleQuotes && character == QLatin1Char('\'')) {
            if (inSingleQuotes) {
                flushQuotedToken(index + 1);
                inSingleQuotes = false;
            } else {
                flushToken();
                inSingleQuotes = true;
                quotedTokenStart = index;
            }
            continue;
        }

        if (!inSingleQuotes && character == QLatin1Char('"')) {
            if (inDoubleQuotes) {
                flushQuotedToken(index + 1);
                inDoubleQuotes = false;
            } else {
                flushToken();
                inDoubleQuotes = true;
                quotedTokenStart = index;
            }
            continue;
        }

        if (inSingleQuotes || inDoubleQuotes) {
            quotedToken.append(character);
            continue;
        }

        if (character.isSpace()) {
            flushToken();
            continue;
        }

        if (character == QLatin1Char('[') && currentTokenStart < 0 && index != continuationIndex) {
            bracketTokenStart = index;
            bracketDepth = 1;
            continue;
        }

        if (currentTokenStart < 0) {
            currentTokenStart = index;
        }
        currentToken.append(character);
    }

    if (escaped) {
        if (inSingleQuotes || inDoubleQuotes) {
            quotedToken.append(QLatin1Char('\\'));
        } else {
            if (currentTokenStart < 0) {
                currentTokenStart = line.length() - 1;
            }
            currentToken.append(QLatin1Char('\\'));
        }
    }

    flushBracketToken(line.length());
    flushQuotedToken(line.length());
    flushToken();

    if (!parsedLine.tokens.isEmpty()) {
        parsedLine.directive = parsedLine.tokens.first().toLower();
    }

    if (!parsedLine.tokenSpans.isEmpty()) {
        for (int index = 0; index < parsedLine.tokenSpans.size(); ++index) {
            TherionParsedToken &tokenSpan = parsedLine.tokenSpans[index];
            if (tokenSpan.type == TherionTokenType::QuotedString || tokenSpan.type == TherionTokenType::Comment) {
                continue;
            }

            if (index == 0) {
                tokenSpan.type = TherionTokenType::Other;
                continue;
            }

            if (TherionTokenRules::isNumericToken(tokenSpan.text)) {
                tokenSpan.type = TherionTokenType::Other;
            }
        }
    }

    return parsedLine;
}

QVector<TherionParsedLine> TherionDocumentParser::parseText(const QString &text)
{
    QVector<TherionParsedLine> parsedLines;
    const QStringList lines = text.split(QLatin1Char('\n'));

    int lineNumber = 0;
    for (QString line : lines) {
        ++lineNumber;
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const TherionParsedLine parsedLine = parseLine(line, lineNumber);
        if (parsedLine.tokens.isEmpty()) {
            continue;
        }

        parsedLines.append(parsedLine);
    }

    return parsedLines;
}

QVector<TherionParsedLine> TherionDocumentParser::parseTokenLines(const QString &text)
{
    return parseSourceDocument(text).tokenLines();
}

TherionParsedSourceDocument TherionDocumentParser::parseSourceDocument(const QString &text)
{
    return parseSourceDocument(TherionSourceText::fromText(text));
}

TherionParsedSourceDocument TherionDocumentParser::parseSourceDocument(const TherionSourceText &sourceText)
{
    TherionParsedSourceDocument document;
    const QVector<TherionSourceLine> &physicalLines = sourceText.physicalLines();
    const QVector<TherionSourceLineSpan> lineSpans = sourceText.lineSpans();
    document.lines.reserve(physicalLines.size());

    for (int index = 0; index < physicalLines.size(); ++index) {
        const TherionSourceLine &sourceLine = physicalLines.at(index);
        const TherionSourceLineSpan &lineSpan = lineSpans.at(index);
        document.lines.append(TherionParsedSourceLine{
            lineSpan.lineNumber,
            lineSpan.startOffset,
            lineSpan.textLength,
            lineSpan.lineEndingLength,
            lineSpan.endOffset,
            sourceLine.text,
            sourceLine.lineEnding,
            parseLine(sourceLine.text, lineSpan.lineNumber)
        });
    }

    return document;
}

QStringList TherionDocumentParser::tokenizeLine(const QString &line)
{
    return parseLine(line).tokens;
}

QVector<TherionParsedToken> TherionDocumentParser::bracketedValueSpans(const TherionParsedToken &token)
{
    if (!token.text.startsWith(QLatin1Char('['))) {
        return {token};
    }

    const int contentEnd = token.text.endsWith(QLatin1Char(']')) ? token.text.size() - 1 : token.text.size();
    QVector<TherionParsedToken> spans;
    int wordStart = -1;
    for (int index = 1; index <= contentEnd; ++index) {
        const bool wordCharacter = index < contentEnd && !token.text.at(index).isSpace();
        if (wordCharacter && wordStart < 0) {
            wordStart = index;
        } else if (!wordCharacter && wordStart >= 0) {
            TherionParsedToken span;
            span.start = token.start + wordStart;
            span.length = index - wordStart;
            span.text = token.text.mid(wordStart, span.length);
            spans.append(span);
            wordStart = -1;
        }
    }
    return spans;
}

QStringList TherionDocumentParser::bracketedValueParts(const QString &token)
{
    TherionParsedToken span;
    span.text = token.trimmed();
    QStringList parts;
    for (const TherionParsedToken &part : bracketedValueSpans(span)) {
        parts.append(part.text);
    }
    return parts;
}
}
