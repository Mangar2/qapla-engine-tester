#pragma once

#include "../sprt/sprt-tournament-file.h"
#include "../tournament/tournament-file.h"
#include "../epd/epd-file.h"
#include "../spsa/spsa-file.h"
#include <string>
#include <stdexcept>

namespace QaplaTester::Cli {

    enum class TaskType {
        Sprt,
        Tournament,
        Epd,
        Spsa,
        Test
    };

    inline std::string getTaskId(TaskType type) {
        switch(type) {
            case TaskType::Sprt: return SprtTournamentFile::id;
            case TaskType::Tournament: return TournamentFile::id;
            case TaskType::Epd: return EpdFile::id;
            case TaskType::Spsa: return SpsaFile::id;
            case TaskType::Test: return "test";
            default: throw std::runtime_error("Unknown task type");
        }
    }

    inline TaskType getTaskType(const std::string& name) {
        if (name == "sprt") {
            return TaskType::Sprt;
        } 
        if (name == "tournament") {
            return TaskType::Tournament;
        } 
        if (name == "epd") {
            return TaskType::Epd;
        } 
        if (name == "spsa") {
            return TaskType::Spsa;
        }
        return TaskType::Test;
    }
}
