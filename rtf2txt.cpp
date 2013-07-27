/*
rtf2txt - command line converter

Usage: rtf2txt file.rtf

Output sent to stdout
*/

/*
The MIT License (MIT)

Copyright (c) 2013 PapaCharlie9

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define _CRT_SECURE_NO_WARNINGS 1

#include <cstdio>
#include <string>


static std::string* Line = nullptr;
const size_t MY_MAX_LINE = 78;

void lineWrap(const std::string& text) {
    if (text == "\n") {
        Line->append(text);
        ::fputs(Line->c_str(), stdout);
        ::fflush(stdout);
        Line->clear();
        return;
    }
    Line->append(text);
    while (Line->size() > MY_MAX_LINE) {
        size_t i = 0;
        size_t blank = MY_MAX_LINE-1;

        // Find whitespace closet to max line length
        while (i < MY_MAX_LINE) {
            if (::isspace(Line->at(i))) blank = i;
            ++i;
        }

        std::string part = Line->substr(0, blank);
        ::fputs(part.c_str(), stdout);
        ::fputs("\n", stdout);
        Line->erase(0, blank+1);
    }
}

bool getToken(FILE* ip, std::string& token, std::string& terminal) {
    int i = 0;
    int specialCase = 0;
    std::string special;

    token.clear();
    terminal.clear();

    int ic = 0;
    while ((ic = ::fgetc(ip)) != EOF) {
        char c = static_cast<char>(ic);
        switch (c) {
            case '\\':
            case '{':
            case '}':
            case ' ':
            case '\t':
            case '\n':
                if (specialCase > 0) {
                    // False alarm
                    token += special;
                    special.empty();
                    specialCase = 0;
                }
                terminal += c;
                return false;
                break;
            case '\'':
                if (specialCase > 0) {
                    // False alarm
                    token += special;
                    special.empty();
                    specialCase = 0;
                } else {
                    specialCase = 1;
                    special += c;
                    continue;
                }
                break;
            case 'a':
            case 'A':
                if (specialCase == 1) {
                    special += c;
                    specialCase = 2;
                    continue;
                } else if (specialCase > 0) {
                    // False alarm
                    token += special;
                    special.empty();
                    specialCase = 0;
                }
                break;
            case '0':
                if (specialCase == 2) {
                    /* Found complete 'A0 sequence, elide it */
                    special.empty();
                    specialCase = 0;
                    continue;
                } else if (specialCase > 0) {
                    // False alarm
                    token += special;
                    special.empty();
                    specialCase = 0;
                }
                break;
            default:;
        }
        token += c;
    }
    return true;
}

typedef enum {
    s_init_1 = 0,
    s_init_2,
    s_init_3,
    s_block,
    s_tag,
    s_text,
    s_esc
} State;

char* Name[7] = {
    "s_init_1",
    "s_init_2",
    "s_init_3",
    "s_block ",
    "s_tag   ",
    "s_text  ",
    "s_esc   "
};

int main(int argc, char** argv) {
    FILE *ip = nullptr;

    --argc,++argv;
    if (argc != 1) {
        fprintf(stderr, "Usage: rtf2txt file.rtf\n");
        return -1;
    }
    if ((ip = fopen(*argv, "r")) == nullptr) {
        fprintf(stderr, "Problem file file: %s\n", *argv);
        return -1;
    }

    Line = new std::string;
    std::string token;
    std::string terminal;
    State state = s_init_1;

    while (!getToken(ip, token, terminal)) {
        //printf("\n");
        //printf("[%s] [%s] [%s] ", Name[state], token.c_str(), terminal.c_str());
        //fflush(stdout);
        switch (state) {
            case s_init_1:
                // Looking for {\rtf
                if (token.empty() && terminal == "{") {
                    state = s_init_2;
                    continue;
                }
                break;
            case s_init_2:
                // Looking for \rtf
                if (token.empty() && terminal == "\\") {
                    state = s_init_3;
                    continue;
                }
                state = s_init_1;
                break;
            case s_init_3:
                // Looking for rtf
                if (token.find("rtf", 0) == std::string::npos) {
                    state = s_init_1;
                    continue;
                }
                // found it, dispatch on terminal
                if (terminal == "\\") {
                    state = s_esc;
                } else if (terminal == "{") {
                    state = s_block;
                } else {
                    state = s_text;
                }
                break;
            case s_block: {
                // dispatch on terminal, ignore token
                if (terminal == "\\") {
                    state = s_esc;
                } else if (terminal == "{" || terminal == "}") {
                    state = s_block;
                } else {
                    state = s_text;
                }
            }
            case s_tag: {
                // dispatch on terminal, ignore token
                if (terminal == "\\") {
                    state = s_esc;
                } else if (terminal == "{" || terminal == "}") {
                    state = s_block;
                } else {
                    state = s_text;
                }
            }
            case s_text: {
                if (!token.empty()) {
                    lineWrap(token);
                }
                if (terminal == " " || terminal == "\t" || terminal == "\n") {
                    lineWrap(terminal);
                    state = s_text;
                } else {
                    if (terminal == "\\") {
                        state = s_esc;
                    } else if (terminal == "{") {
                        state = s_block;
                    } else if (terminal == "}") {
                        state = s_text;
                    }
                }
                break;
            }
            case s_esc:
                // looking for \} or \}
                if (token.empty() && (terminal == "{" || terminal == "}")) {
                    lineWrap(terminal);
                    state = s_text;
                    continue;
                }
                // dispatch on terminal
                if (terminal == "\\") {
                    state = s_esc;
                } else if (terminal == "{") {
                    state = s_block;
                } else {
                    state = s_text;
                }
                break;
            default:;
        }
    }
    lineWrap("\n");

    return 0;
}