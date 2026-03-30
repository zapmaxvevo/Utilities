#include "ExtraArgsParser.h"

ExtraArgsParseResult ExtraArgsParser::Parse(const std::string& input)
{
    ExtraArgsParseResult result;

    enum class State { Normal, InDoubleQuote, InSingleQuote };
    State       state    = State::Normal;
    std::string current;
    bool        inToken  = false;

    for (char c : input) {
        switch (state) {
        case State::Normal:
            if (c == '"') {
                state   = State::InDoubleQuote;
                inToken = true;
            } else if (c == '\'') {
                state   = State::InSingleQuote;
                inToken = true;
            } else if (c == ' ' || c == '\t') {
                if (inToken) {
                    result.args.push_back(current);
                    current.clear();
                    inToken = false;
                }
            } else {
                current += c;
                inToken  = true;
            }
            break;

        case State::InDoubleQuote:
            if (c == '"')
                state = State::Normal;
            else
                current += c;
            break;

        case State::InSingleQuote:
            if (c == '\'')
                state = State::Normal;
            else
                current += c;
            break;
        }
    }

    // Detect unbalanced quotes
    if (state == State::InDoubleQuote)
        result.errors.push_back("Unbalanced double quote.");
    else if (state == State::InSingleQuote)
        result.errors.push_back("Unbalanced single quote.");
    else if (inToken)
        result.args.push_back(current);

    return result;
}
