#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "../cli/settings-manager.h"

namespace QaplaTester::Mcp {

class SettingsReporter {
public:
    enum class Column {
        Name,
        FullName,
        Value,
        Default,
        Required,
        Description
    };

    /**
     * @brief Generates a markdown report of the current settings.
     * 
     * @param groups Optional list of group names to include. If nullopt, all groups are included.
     * @param columns Optional list of columns to include in the table. If nullopt, all columns are included.
     * @return std::string Markdown formatted report.
     */
    static std::string generateReport(
        std::optional<std::vector<std::string>> groups = std::nullopt,
        std::optional<std::vector<Column>> columns = std::nullopt
    );

private:
   static void appendGlobalSettingsReport(std::string& report, const std::vector<Column>& columns);
   static void appendGroupSettingsReport(std::string& report, const std::vector<std::string>& groupFilter, const std::vector<Column>& columns);
   static std::string formatSettingValue(const Settings::Value& v);
   static std::string getColumnHeader(Column col);
   static std::string getColumnValue(Column col, const std::string& name, const std::string& fullName, 
                                     const std::string& value, const std::string& defaultValue, 
                                     bool required, const std::string& description);
   
   // Helpers
   static void processGroupSection(
        const std::string& groupName,
        QaplaTester::Settings::Manager& manager,
        std::string& report,
        const std::vector<Column>& columns,
        const std::function<std::string(const std::optional<QaplaTester::Settings::Value>&)>& formatDefValueLoc
   );
   
   static std::string getGlobalValue(
       QaplaTester::Settings::Manager& manager, 
       const std::string& name, 
       const QaplaTester::Settings::ParameterDefinition& def
    );
};

} // namespace QaplaTester::Mcp
