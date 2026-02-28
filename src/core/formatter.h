/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * Neutron Code Formatter
 */

#ifndef NEUTRON_FORMATTER_H
#define NEUTRON_FORMATTER_H

#include <string>
#include <vector>
#include <sstream>

namespace neutron {

class Formatter {
public:
    struct Options {
        int indentSize;
        bool useSpaces;
        bool trimTrailingWhitespace;
        bool insertFinalNewline;
        int maxLineLength;
        
        Options() : indentSize(4), useSpaces(true), trimTrailingWhitespace(true), 
                    insertFinalNewline(true), maxLineLength(100) {}
    };
    
    static std::string format(const std::string& source, const Options& options = Options());
    static bool formatFile(const std::string& path, const Options& options = Options());
    static bool checkFormat(const std::string& path, const Options& options = Options());
    
private:
    static std::string getIndent(int level, const Options& options);
    static std::string trimRight(const std::string& str);
    static bool isBlockOpener(const std::string& line);
    static bool isBlockCloser(const std::string& line);
    static bool isControlFlow(const std::string& line);
    static int countBraces(const std::string& line, char open, char close);
};

} // namespace neutron

#endif // NEUTRON_FORMATTER_H
