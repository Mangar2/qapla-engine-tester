#pragma once

#include "../sprt/sprt-tournament-file.h"
#include "../tournament/tournament-file.h"
#include "../epd/epd-file.h"
#include "../spsa/spsa-file.h"
#include "../clop/clop-file.h"
#include <string>

namespace QaplaTester::Cli {

    enum class TaskType {
        Sprt,
        Tournament,
        Epd,
        Spsa,
        Clop,
        SystemTest,
        Test,
        Perft,
        All,
        None
    };

    inline std::string getTaskId(TaskType type) {
        switch(type) {
            case TaskType::Sprt: return SprtTournamentFile::id;
            case TaskType::Tournament: return TournamentFile::id;
            case TaskType::Epd: return EpdFile::id;
            case TaskType::Spsa: return SpsaFile::id;
            case TaskType::Clop: return ClopFile::id;
            case TaskType::SystemTest: return "systemtest";
            case TaskType::Test: return "test";
            case TaskType::Perft: return "perft";
            case TaskType::All: return "all";
            default: return "none";
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
        if (name == "clop") {
            return TaskType::Clop;
        }
        if (name == "systemtest") {
            return TaskType::SystemTest;
        }
        if (name == "test") {
            return TaskType::Test;
        }
        if (name == "perft") {
            return TaskType::Perft;
        }
        if (name == "all") {
            return TaskType::All;
        }
        return TaskType::None;
    }
}
