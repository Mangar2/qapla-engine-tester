/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#pragma once

#include <stdexcept>
#include <string>
#include <algorithm>
#include <limits>

enum class AppReturnCode {
    NoError = 0,
    GeneralError = 1,
    InvalidParameters = 2,
    EngineError = 10,
    EngineMissbehaviour = 11,
    EngineNote = 12,
    MissedTarget=13,
    H1Accepted = 14,
    H0Accepted = 15,
    UndefinedResult = 16
};

 /**
  * Represents a structured application error with user and system context.
  */
class AppError : public std::runtime_error {
public:


    int getInternalCode() const noexcept { return internalCode; }
    AppReturnCode getReturnCode() const noexcept { return returnCode; }
    const std::string& getUserHint() const noexcept { return userHint; }
    const std::string& getInternalDetail() const noexcept { return internalDetail; }

	/**
	 * Creates an AppError with a default internal code and return code of 1.
	 * @param externalText The error message to display to the user.
	 */
    static AppError make(const std::string& externalText) {
        return AppError(0, AppReturnCode::GeneralError, externalText, {}, {});
    }

	/**
	 * Creates an AppError with a specific internal code and a default return code of 1.
	 * @param internalCode The internal error code.
	 * @param externalText The error message to display to the user.
	 */
    static AppError make(int internalCode, const std::string& externalText) {
        return AppError(internalCode, AppReturnCode::GeneralError, externalText, {}, {});
    }

	/**
	 * Creates an AppError with specific internal and return codes.
	 * @param internalCode The internal error code.
	 * @param returnCode The return code for the application.
	 * @param externalText The error message to display to the user.
	 */
    static AppError make(int internalCode, AppReturnCode returnCode, const std::string& externalText) {
        return AppError(internalCode, returnCode, externalText, {}, {});
    }

	/** 
	 * Creates an AppError with all details provided.
	 * @param internalCode The internal error code.
	 * @param returnCode The return code for the application.
	 * @param externalText The error message to display to the user.
	 * @param userHint A hint for the user on how to resolve the issue.
	 * @param internalDetail Additional internal details for debugging.
     */
    static AppError make(int internalCode, AppReturnCode returnCode, const std::string& externalText,
        const std::string& userHint, const std::string& internalDetail) {
        return AppError(internalCode, returnCode, externalText, userHint, internalDetail);
    }

    /**
     * Creates an AppError indicating invalid or missing parameters.
     * @param externalText The error message to display to the user.
     * @return An AppError with internal code 0 and return code 2.
     */
    static AppError makeInvalidParameters(const std::string& externalText) {
        return AppError(0, AppReturnCode::InvalidParameters, externalText, 
            "Use --help to display all supported parameters.", {});
    }

    /**
     * Throws an AppError if the given option is not in the list of allowed options.
     * Suggests a similar option if available.
     * @param allowedOptions List of valid options.
     * @param givenOption The provided option to validate.
     * @param contextText The context or description for the error message.
     */
    static void throwOnInvalidOption(const std::vector<std::string>& allowedOptions, 
        const std::string& givenOption, const std::string& contextText) {

        if (std::find(allowedOptions.begin(), allowedOptions.end(), givenOption) != allowedOptions.end()) return;

        auto levenshtein = [](const std::string& a, const std::string& b) {
            const size_t m = a.size(), n = b.size();
            std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));
            for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
            for (size_t j = 0; j <= n; ++j) dp[0][j] = j;
            for (size_t i = 1; i <= m; ++i)
                for (size_t j = 1; j <= n; ++j)
                    dp[i][j] = std::min({ dp[i - 1][j - 1] + (a[i - 1] != b[j - 1]), dp[i - 1][j] + 1, dp[i][j - 1] + 1 });
            return dp[m][n];
            };

        std::string suggestion;
        size_t minDistance = std::numeric_limits<int>::max();
        for (const auto& option : allowedOptions) {
            size_t dist = levenshtein(givenOption, option);
            if (dist < minDistance && dist <= 3) {
                minDistance = dist;
                suggestion = option;
            }
        }

        std::string hint = "\nValid options: ";
        for (const auto& opt : allowedOptions) hint += opt + ", ";
        hint.pop_back(); hint.pop_back(); // remove trailing comma and space

        if (!suggestion.empty()) hint += ".\nDid you mean '" + suggestion + "'?";

        throw makeInvalidParameters("\n" + contextText + ": '" + givenOption + "' " + hint);
    }


private:

    AppError(int internalCode, AppReturnCode returnCode, const std::string& externalText,
        const std::string& userHint, const std::string& internalDetail)
        : std::runtime_error(userHint.empty() ? externalText : externalText + "\nHint: " + userHint),
        internalCode(internalCode),
        returnCode(returnCode),
        userHint(userHint),
        internalDetail(internalDetail) {
    }
    int internalCode;
    AppReturnCode returnCode;
    std::string userHint;
    std::string internalDetail;
};


