#include "json_pretty_print.h"

namespace mqtt::json {

namespace {

void append_indent(std::string& outputText, std::size_t depth, std::size_t indentWidth) {
    outputText.append(depth * indentWidth, ' ');
}

void append_pretty(const JsonValue& inputValue, std::string& outputText, std::size_t depth, std::size_t indentWidth) {
    if (inputValue.is_object()) {
        const auto& objectValue = inputValue.as_object();
        if (objectValue.empty()) {
            outputText += "{}";
            return;
        }
        outputText += "{\n";
        std::size_t written = 0;
        for (const auto& [keyName, memberValue] : objectValue) {
            append_indent(outputText, depth + 1, indentWidth);
            outputText += JsonValue(keyName).stringify();
            outputText += ": ";
            append_pretty(memberValue, outputText, depth + 1, indentWidth);
            ++written;
            if (written < objectValue.size()) {
                outputText += ',';
            }
            outputText += '\n';
        }
        append_indent(outputText, depth, indentWidth);
        outputText += '}';
        return;
    }

    if (inputValue.is_array()) {
        const auto& arrayValue = inputValue.as_array();
        if (arrayValue.empty()) {
            outputText += "[]";
            return;
        }
        outputText += "[\n";
        for (std::size_t index = 0; index < arrayValue.size(); ++index) {
            append_indent(outputText, depth + 1, indentWidth);
            append_pretty(arrayValue[index], outputText, depth + 1, indentWidth);
            if (index + 1 < arrayValue.size()) {
                outputText += ',';
            }
            outputText += '\n';
        }
        append_indent(outputText, depth, indentWidth);
        outputText += ']';
        return;
    }

    // Leaf value (string/number/boolean/null) -- reuse stringify()'s already-tested escaping
    // and number formatting instead of duplicating it here.
    outputText += inputValue.stringify();
}

} // namespace

std::string stringify_pretty(const JsonValue& inputValue, std::size_t indentWidth) {
    std::string outputText;
    append_pretty(inputValue, outputText, 0U, indentWidth);
    return outputText;
}

} // namespace mqtt::json
