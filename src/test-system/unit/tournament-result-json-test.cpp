#include <catch2/catch_test_macros.hpp>

#include "../../game-manager/tournament-result.h"

using QaplaTester::EngineDuelResult;
using QaplaTester::TournamentResult;
using QaplaTester::Json::JsonValue;

TEST_CASE("tournament_result_outcome_json_escapes_special_characters_in_names", "[json-migration][tournament-result]") {
    TournamentResult result;
    EngineDuelResult duel("Engine \"A\"\\", "EngineB");
    duel.winsEngineA = 3;
    duel.winsEngineB = 1;
    duel.draws = 1;
    result.push_back(duel);

    const auto outcome = result.getOutcome();
    REQUIRE(outcome.is_object());
    CHECK(outcome.at("type").as_string() == "outcome");

    // Behavior fix: previously hand-built via std::format with no escaping,
    // so a name containing '"' or '\' produced invalid JSON.
    const auto reparsed = JsonValue::parse(outcome.stringify());

    const auto& data = reparsed.at("data");
    REQUIRE(data.is_array());
    bool foundEscapedName = false;
    for (const auto& row : data.as_array()) {
        if (row.at("name").as_string() == "Engine \"A\"\\") {
            foundEscapedName = true;
            CHECK(row.at("wins").as_number() == 3.0);
            CHECK(row.at("losses").as_number() == 1.0);
            CHECK(row.at("draws").as_number() == 1.0);
            CHECK(row.at("total").as_number() == 5.0);
        }
    }
    CHECK(foundEscapedName);
}

TEST_CASE("tournament_result_rating_table_json_has_documented_fields_and_is_valid", "[json-migration][tournament-result]") {
    TournamentResult result;
    EngineDuelResult duel("EngineA", "EngineB");
    duel.winsEngineA = 6;
    duel.winsEngineB = 2;
    duel.draws = 2;
    result.push_back(duel);

    const auto rating = result.getRatingTable(2600);
    REQUIRE(rating.is_object());
    CHECK(rating.at("type").as_string() == "ratingTable");

    const auto& data = rating.at("data");
    REQUIRE(data.is_array());
    REQUIRE(data.size() == 2U);
    for (const auto& row : data.as_array()) {
        CHECK(row.contains("rank"));
        CHECK(row.contains("name"));
        CHECK(row.contains("elo"));
        CHECK(row.contains("error"));
        CHECK(row.contains("games"));
        CHECK(row.contains("score"));
        CHECK(row.contains("drawPct"));
        CHECK(row.at("games").as_number() == 10.0);
    }

    const auto reparsed = JsonValue::parse(rating.stringify());
    CHECK(reparsed.at("data").size() == 2U);
}
