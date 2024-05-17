/**
 * A Doc generator for C++ code. Similar to Doxygen.
 * Markdown is recommended, but you can really do any format you want since it is all just text.
 * Uses special "commands" to generate the docs.
 * For example, the command `DOC` will make the rest of the comment a part of the documentation (unless an END is found).
 * There is other commands to assist in the generation of the documentation. like `NEXT_LINE` and `NEXT_IDENTIFIER`. which can both take arguments to do more than 1 line, or other amounts or offsets.
 * A command is prefixed with a '@' symbol, and can be argumented or non-argumented. The argumented commands are followed by a '(' and then the arguments seperated by commas, and then a ')'.
 * The starting parenthesis must be the character directly after the command identifier.
 * Usage: docgen <output dir (defaults to docs/)> // Requires a .docgen file
 * For example, instead of just having everything in the file be right after each other, we can define "Sections" that can be selected with the `SECTION` command.
 * This allows multiple sources to be documented in the same output file, and allows for more organization.
 * The `SECTION` command can take an argument, which is the name of the section.
 * If you run the section command with no arguments, it will end the current section and start writing to the end of the doc file
 * The .docgen file is a markdown file with extra commands that are used to generate the documentation.
 * It specifies where the sections go, and the general format of the output
 * It can also (WIP) create custom commands that can be used in the documentation
 * It's commands are a different set than the ones used in the source code
 */

#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <regex>
#include "glob.hpp"

// cross platform dynamic library loading
#ifdef _WIN32
#include <windows.h>
#define LIB_HANDLE HINSTANCE
#define LIB_LOAD(path) LoadLibrary(path)
#define LIB_GET_FUNC(lib, name) GetProcAddress(lib, name)
#define LIB_CLOSE(lib) FreeLibrary(lib)
#define PATH_SEP "\\"
#else
#include <dlfcn.h>
#define LIB_HANDLE void*
#define LIB_LOAD(path) dlopen(path, RTLD_LAZY)
#define LIB_GET_FUNC(lib, name) dlsym(lib, name)
#define LIB_CLOSE(lib) dlclose(lib)
#define PATH_SEP "/"
#endif

namespace fs = std::filesystem;

struct DocContext {
    std::unordered_map<std::string, std::string> sections;
    std::string mainSection;
    fs::path outputDir;
    std::string inputDocgen;
    std::string currentSection;
    std::string output;
    std::unordered_map<std::string, std::string> aliases;
};

struct CommentData {
    size_t index;
    size_t end_index;
    std::string comment;
};

std::string strip(const std::string& s) {
    // remove leading and trailing whitespace
    size_t start = 0;
    size_t end = s.size();
    while (start < s.size() && std::isspace(s[start])) {
        start++;
    }
    while (end > 0 && std::isspace(s[end-1])) {
        end--;
    }
    return s.substr(start, end-start);
}

std::string simplify_whitespace(const std::string& s) {
    // strip, then convert all whitespace that is in a row into a single space
    // no newlines or tabs or anything else
    std::ostringstream out;
    size_t i = 0;
    while (i < s.size()) {
        if (std::isspace(s[i])) {
            i++;
            while (std::isspace(s[i])) {
                i++;
            }
            out << ' ';
        } else {
            out << s[i++];
        }
    }

    return strip(out.str());
}

std::vector<std::string> parse_args(std::string& src, size_t& index) {
    size_t lastPos = index;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    std::vector<std::string> args;
    for (size_t i = index+1; i < src.size(); i++) {
        if (src[i] == '"') {
            inQuote = !inQuote;
        }
        if (inQuote) {
            continue;
        }
        if (src[i] == '(') {
            parenDepth++;
        } else if (src[i] == ')') {
            parenDepth--;
        } else if (src[i] == '[') {
            bracketDepth++;
        } else if (src[i] == ']') {
            bracketDepth--;
        } else if (src[i] == '{') {
            braceDepth++;
        } else if (src[i] == '}') {
            braceDepth--;
        } else if (src[i] == ',' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            args.push_back(strip(src.substr(lastPos+1, i-lastPos-1)));
            lastPos = i;
        }
        if (parenDepth == -1) {
            size_t poss = i;
            while (poss < src.size() && src[poss] != ')') {
                poss++;
            }
            args.push_back(strip(src.substr(lastPos+1, poss-lastPos-1)));
            index = poss;
            return args;
        }
    }
    args.push_back(strip(src.substr(lastPos+1, src.size()-lastPos-2)));
    return args;
}

void process_char(char c, DocContext& context) {
    if (context.currentSection.empty()) {
        context.mainSection += c;
    } else {
        context.sections[context.currentSection] += c;
    }
}

void process_string(const std::string& s, DocContext& context) {
    for (char c: s) {
        process_char(c, context);
    }
}

void process_source(const std::string& src, DocContext& context, bool realSource, const std::string& filename);

void process_src_command(const std::string& command_, const std::vector<std::string>& args, DocContext& context, const CommentData& comment, const std::string& src, bool simplify, const std::string& filename) {
    std::string command = strip(command_);
    if (command[0] == 'S' && command[1] == '_') {
        command = command.substr(2);
        simplify = true;
    }
    auto process_str = [simplify, &context] (const std::string& s) {
        if (simplify) process_string(simplify_whitespace(s), context);
        else process_string(s, context);
    };
    if (command == "SECTION") {
        if (args.empty()) {
            context.currentSection = "";
        } else {
            context.currentSection = args[0];
        }
    } else if (command == "NEXT_LINE") {
        // find next line after end of comment
        size_t end = comment.end_index+1;
        while (end < src.size() && src[end] != '\n') {
            end++;
        }
        std::string a = strip(src.substr(comment.end_index, end-comment.end_index));
        process_str(a);
    } else if (command == "FUNC_NAME") {
        // find the next identifier before a '('
        size_t end = comment.end_index+1;
        while (end < src.size() && src[end] != '(') {
            end++;
        }
        while (end > 0 && !std::isalnum(src[end-1]) && src[end - 1] != '_') {
            end--;
        }
        size_t start = end;
        while (start > 0 && (std::isalnum(src[start-1]) || src[start-1] == '_')) {
            start--;
        }
        std::string a = strip(src.substr(start, end-start));
        if (a == "operator") {
            // expand the end until the '('
            while (end < src.size() && src[end] != '(') {
                end++;
            }

            a = strip(src.substr(start, end-start));
        }
        process_str(a);
    } else if (command == "NEXT_DECL") {
        // everything after comment until a ';', '=', or '{'
        size_t end = comment.end_index+1;
        while (end < src.size() && src[end] != ';' && src[end] != '=' && src[end] != '{') {
            end++;
        }
        std::string a = strip(src.substr(comment.end_index+1, end-comment.end_index-1));

        a += ";";
        process_str(a);
    } else if (command == "FUNC_RET") {
        // everything before the function name
        size_t end = comment.end_index+1;
        size_t start = end;
        while (end < src.size() && src[end] != '(') {
            end++;
        }
        while (end > 0 && !std::isspace(src[end-1])) {
            end--;
        }
        std::string a = strip(src.substr(start, end-start));
        process_str(a);
    } else if (command == "FUNC_ARGS") {
        // find the arg list
        size_t start = comment.end_index+1;
        while (start < src.size() && src[start] != '(') {
            start++;
        }
        size_t end = start;
        int parenDepth = 0;
        while (end < src.size()) {
            if (src[end] == '(') {
                parenDepth++;
            } else if (src[end] == ')') {
                parenDepth--;
            }
            if (parenDepth == 0) {
                break;
            }
            end++;
        }
        std::string a = strip(src.substr(start+1, end-start-1));
        process_str(a);
    } else if (command == "FUNC_ARG") {
        if (args.size() != 1) {
            std::cerr << "Error: FUNC_ARG requires 1 argument\n";
            return;
        }
        // find all args first
        size_t start = comment.end_index+1;
        while (start < src.size() && src[start] != '(') {
            start++;
        }
        size_t end = start;
        int parenDepth = 0;
        while (end < src.size()) {
            if (src[end] == '(') {
                parenDepth++;
            } else if (src[end] == ')') {
                parenDepth--;
            }
            if (parenDepth == 0) {
                break;
            }
            end++;
        }
        std::string argList = src.substr(start, end-start+1);
        size_t index = 0;
        std::vector<std::string> argss = parse_args(argList, index);
        int argNum = std::stoi(args[0]);
        if (argNum < 0) {
            argNum = argss.size() + argNum;
        }
        if (argNum < 0 || argNum >= argss.size()) {
            std::cerr << "Error: Argument " << args[0] << " not found\n";
            return;
        }
        process_str(argss[argNum]);
    }
    else if (command == "CLASS_NAME") {
        size_t end = comment.end_index+1;
        // last identifier before '{' or ':' or ';'
        // ':' has higher precedence than '{' or ';'
        while (end < src.size() && src[end] != '{' && src[end] != ':' && src[end] != ';') {
            end++;
        }
        while (end > 0 && std::isspace(src[end-1])) {
            end--;
        }
        size_t start = end;
        while (start > 0 && (std::isalnum(src[start-1])) || src[start-1] == '_') {
            start--;
        }
        std::string a = strip(src.substr(start, end-start));
        process_str(a);
    } else if (command == "NEXT_MACRO") {
        // given #define ABC sdfsdfsf
        // return #define ABC
        // given #define ABC(a, b, ...) sdfsdfsf
        // return #define ABC(a, b, ...)
        size_t start = comment.end_index+1;
        while (start < src.size() && src[start] != '#') {
            start++;
        }
        size_t end = start;
        while (end < src.size() && src[end] != ')') {
            end++;
        }
        std::string a = strip(src.substr(start, end-start)) + ')';
        process_str(a);
    } else if (command == "FILE_NAME") {
        // just the filename, no path
        fs::path p(filename);
        process_str(p.filename().string());
    }


    else if (command == "SIMPLIFY" || command == "S") {
        if (args.size() == 1) {
            process_src_command(args[0], {}, context, comment, src, true, filename);
        } else if (args.size() > 1) {
            std::vector<std::string> new_args;
            new_args.reserve(args.size()-1);
            for (size_t i = 1; i < args.size(); i++) {
                new_args.push_back(args[i]);
            }
            process_src_command(args[0], new_args, context, comment, src, true, filename);

        } else {
            std::cerr << "Wrong number of arguments in SIMPLIFY!" << std::endl;
        }
    } else if (context.aliases.find(command) != context.aliases.end()) {
        // the command should be replaced with the alias, and reprocessed
        // but we need to have the source code after to make sure commands work properly
//        std::string next_scr = src.substr(comment.end_index+1);

        size_t start = comment.end_index+1;
        // find the beginning of the next comment and stop before it
        size_t end = src.find("/*", start);
        size_t end2 = src.find("//", start);
        if (end == std::string::npos) {
            end = src.size();
        }
        if (end2 == std::string::npos) {
            end2 = src.size();
        }
        if (end2 < end) {
            end = end2;
        }
        std::string next_scr = src.substr(start, end-start);
        process_source("/* @DOC\n" + context.aliases[command] + "\n@END\n*/\n" + next_scr, context, false, filename);
    }

    else {
        if (fs::exists(context.outputDir / "commands" / (command + ".so"))) {
//            std::cout << "Found command " << command << '\n';
            // load
            LIB_HANDLE lib = LIB_LOAD((context.outputDir / "commands" / (command + ".so")).string().c_str());
            if (!lib) {
                std::cerr << "Error: Could not load command " << command << '\n';
                return;
            }
            // get function
            std::string (*func)(const std::string&, const std::vector<std::string>&);
            func = (std::string (*)(const std::string&, const std::vector<std::string>&))LIB_GET_FUNC(lib, ("_Z" + std::to_string(command.size()) + command + "RKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKSt6vectorIS4_SaIS4_EE").c_str());
            if (!func) {
                std::cerr << "Error: Could not find function " << command << '\n';
                return;
            }
            // call function
            // the function takes the code after the comment as an argument
            std::string code_after_comment = src.substr(comment.end_index+1);
            std::string result = func(code_after_comment, args);
            process_str(result);
            // close
            LIB_CLOSE(lib);


        } else {
            std::cerr << "Error: Unknown command " << command << '\n';
        }
    }
}

void process_source(const std::string& src, DocContext& context, bool realSource, const std::string& filename) {
    // for each comment in the source, create a comment data object
    std::vector<CommentData> comments;
    size_t index = 0;
    // two types of comments, single line and multi line
    // a single line comment can be inside of a multi line, and it will be ignored and be part of the multi line comment
    // same with the other way around
    while (index < src.size()) {
        if (src[index] == '/' && src[index+1] == '/') {
            size_t end = src.find('\n', index);
            if (end == std::string::npos) {
                end = src.size();
            }
//            comments.push_back({index, strip(src.substr(index+2, end-index))});
            comments.push_back({index, end, strip(src.substr(index+2, end-index-2))});
            index = end;
        } else if (src[index] == '/' && src[index+1] == '*') {
            size_t end = src.find("*/", index);
            if (end == std::string::npos) {
                end = src.size();
            }
//            comments.push_back({index, strip(src.substr(index+2, end-index-2))});
            comments.push_back({index, end+2, strip(src.substr(index+2, end-index-2))});
            index = end+2;
        } else {
            index++;
        }
    }

    // go through each comment and process it
    for (const CommentData& comment : comments) {
        bool doc = false;
        std::string cmt = comment.comment;
        // find commands inside the comment
        // a command is '@' followed by all caps, and then (optional) arguments
        size_t index = 0;
        while (index < cmt.size()) {
            if (cmt[index] == '@' && std::isupper(cmt[index + 1])) {
                // keep going as long as it is a letter, underscore, or number
                size_t end = index + 1;
                while (std::isalnum(cmt[end]) || cmt[end] == '_') {
                    end++;
                }
                std::string cmdName = cmt.substr(index + 1, end - index - 1);
                std::vector<std::string> args;
                // find the arguments
                index += 2 + end - index - 2;
                // if the next thing is a '(' then there is arguments
                if (cmt[index] == '(') {
                    args = parse_args(cmt, index);
                    index++;
                }

                if (cmdName == "DOC") {
                    doc = true;
                } else if (cmdName == "END") {
                    doc = false;
                } else {
                    if (doc) {
                        process_src_command(cmdName, args, context, comment, src, false, filename);

                    }
                }
                // if there is \( right after the command, then remove the \ a
                if (index + 1 < cmt.size() && cmt[index] == '\\' && cmt[index + 1] == '(') {
                    index++;
                }
            } else {
                if (doc) {
                    process_char(cmt[index], context);
                }
                index++;
            }
        }
        if (realSource)
            context.currentSection = "";
    }
}

std::string simplify_md(const std::string& s) {
    // only 1 empty line in a row is allowed
    // if we find \n\s*\n\s*\n then we replace it with \n\n
    std::regex re("\n[ \t]*\n[ \t]*\n");
    return std::regex_replace(s, re, "\n\n");
}

void process_md_command(const std::string& command_, DocContext& context, size_t startLine, size_t endLine, size_t startCol, size_t endCol) {
    std::string cmdName;
    std::string command = strip(command_);
    std::vector<std::string> args;
    size_t pos = command.find('(');
    if (pos != std::string::npos) {
        cmdName = strip(command.substr(0, pos));
        args = parse_args(command, pos);
    } else {
        cmdName = strip(command);
    }

    if (cmdName == "NEW_COMMAND") {
        if (args.size() != 2 && args.size() != 3) {
            std::cerr << "Error: NEW_COMMAND requires 2 arguments\n";
            return;
        }
        fs::path commandPath = context.outputDir / "commands" / (args[0] + ".cpp");
        if (!fs::exists(commandPath.parent_path())) {
            fs::create_directories(commandPath.parent_path());
        }
        std::string includes = "#include <string>\n#include<vector>\n";
        std::string code;
        if (args.size() == 3) {
            includes += args[1];
            code = args[2];
        } else {
            code = args[1];
        }
        std::ofstream commandFile(commandPath);
        commandFile << includes;
        commandFile << "\nstd::string " << args[0] << "(const std::string &code, const std::vector<std::string> &args) \n";
        commandFile << code;
        commandFile.close();
        // compile the command into a shared object
        std::string cmd = "g++ -shared -fPIC -o " + (context.outputDir / "commands" / (args[0] + ".so")).string() + " " + commandPath.string();
        system(cmd.c_str());
//        std::cout << cmd << '\n';

    } else if (cmdName == "PROCESS_SOURCES") {
        // glob
        std::vector<fs::path> sources = glob::rglob(args);
        for (const fs::path& source : sources) {
            std::ifstream sourceFile(source);
            std::string src((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
            std::cout << "Processing " << source << '\n';
            process_source(src, context, true, source.string());
        }
        if (sources.empty()) {
            std::cerr << "Error: No sources found\n";
            for (const std::string& s : args) {
                std::cerr << s << '\n';
            }
        }
    } else if (cmdName == "INSERT_SECTION") {
        if (args.size() != 1) {
            std::cerr << "Error: INSERT_SECTION requires 1 argument\n";
            return;
        }
        if (context.sections.find(args[0]) == context.sections.end()) {
            std::cerr << "Error: Section " << args[0] << " not found\n";
            return;
        }
        context.output += simplify_md(context.sections[args[0]]);
        context.output += "\n\n";
    } else if (cmdName == "NEW_ALIAS") {
        if (args.size() != 2) {
            std::cerr << "Error: NEW_ALIAS requires 2 arguments\n";
            return;
        }
        std::string s = strip(args[1]);
        // remove whatever was used to contain the alias data, () or {}
        if (s[0] == '(' || s[0] == '{' || s[0] == '[' || s[0] == '"') {
            s = s.substr(1, s.size()-2);
        }
        context.aliases[args[0]] = s;
    }

    else {
        std::cerr << "Error: Unknown command " << cmdName << '\n';
    }

}

int main() {
    fs::path p = fs::current_path();
    // check if .docgen file exists
    if (fs::exists(p / ".docgen")) {
        std::cout << "Generating docs...\n";
    } else {
        std::cout << "No .docgen file found\n";
        return 0;
    }
    // check if docs directory exists
    if (!fs::exists(p / "docs")) {
        fs::create_directory(p / "docs");
        std::cout << "Created docs directory\n";
    }
    // open the .docgen file
    std::ifstream docgenFile(p / ".docgen");
    std::string docgenSrc;
    // write entire file to string, then go back to beginning
    docgenFile.seekg(0, std::ios::end);
    docgenSrc.reserve(docgenFile.tellg());
    docgenFile.seekg(0, std::ios::beg);
    docgenSrc.assign((std::istreambuf_iterator<char>(docgenFile)), std::istreambuf_iterator<char>());
    docgenFile.seekg(0, std::ios::beg);

    // markdown unless @@ encountered on a new line
    std::string line;
    DocContext context;
    context.outputDir = p / "docs";
    context.inputDocgen = docgenSrc;
    size_t lineNum = 0;
    while (std::getline(docgenFile, line)) {
        lineNum++;
        if (line[0] == '@' && line[1] == '@') {
            size_t pos = line.find("@@", 2);
            if (pos != std::string::npos) {
                std::string command = line.substr(2, pos-2);
                process_md_command(command, context, lineNum, lineNum, 2, pos-2);
            } else {
                // the closing @@ is on a different line, if so, it MUST be at the beginning of the line, and the stuff after it is also part of the command
                std::string command = line.substr(2);
                size_t lineAdd = 0;
                while (std::getline(docgenFile, line)) {
                    lineAdd++;
                    if (line[0] == '@' && line[1] == '@') {
                        command += line.substr(2);
                        break;
                    } else {
                        command += line + '\n';
                    }
                }
                process_md_command(command, context, lineNum, lineNum + lineAdd, 2, command.size());
                lineNum += lineAdd;
            }
        } else {
            context.output += line + '\n';
        }
    }

    context.output += simplify_md(context.mainSection);

    context.output = strip(simplify_md(context.output));

    std::ofstream output(context.outputDir / "index.md");
    output << context.output;
    return 0;
}
