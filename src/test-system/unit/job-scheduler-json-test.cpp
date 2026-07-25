#include <catch2/catch_test_macros.hpp>

#include "../../mcp/job-scheduler.h"

using QaplaTester::Mcp::JobScheduler;
using QaplaTester::Mcp::QueueJob;
using QaplaTester::Mcp::QueueJobType;

// The scheduler's worker thread is never started in this test, so an
// enqueued job stays queued (not picked up) until clearQueuedJobs() removes
// it — this exercises queueStatusJson()'s JSON shape without any threading.
TEST_CASE("job_scheduler_queue_status_json_reports_queued_job_shape", "[json-migration][job-scheduler]") {
    auto& scheduler = JobScheduler::instance();
    scheduler.clearQueuedJobs();

    QueueJob job;
    job.jobType = QueueJobType::Sprt;
    job.toolName = "sprt";
    job.jobIntent = "unit-test-intent";
    job.reportBaseName = "sprt-report";

    const auto jobId = scheduler.enqueue(std::move(job));

    const auto status = scheduler.queueStatusJson();
    REQUIRE(status.is_object());
    CHECK_FALSE(status.at("running").as_boolean());
    CHECK(status.at("queue_length").as_number() == 1.0);

    const auto& queuedJobs = status.at("queued_jobs");
    REQUIRE(queuedJobs.is_array());
    REQUIRE(queuedJobs.size() == 1U);
    CHECK(queuedJobs.at(0U).at("job_id").as_string() == jobId);
    CHECK(queuedJobs.at(0U).at("type").as_string() == "sprt");
    CHECK(queuedJobs.at(0U).at("job_intent").as_string() == "unit-test-intent");
    CHECK(queuedJobs.at(0U).at("state").as_string() == "queued");

    // Must be valid, self-consistent JSON (no internal round trip left).
    const auto reparsed = QaplaTester::Json::JsonValue::parse(status.stringify());
    CHECK(reparsed.at("queued_jobs").size() == 1U);

    const auto clearedCount = scheduler.clearQueuedJobs();
    CHECK(clearedCount == 1U);
}

TEST_CASE("job_scheduler_finished_results_json_reports_count_and_results_shape", "[json-migration][job-scheduler]") {
    auto& scheduler = JobScheduler::instance();
    scheduler.clearQueuedJobs();
    scheduler.clearFinishedResults();

    QueueJob job;
    job.jobType = QueueJobType::Test;
    job.toolName = "test";
    job.jobIntent = "unit-test-intent";
    scheduler.enqueue(std::move(job));
    scheduler.clearQueuedJobs(); // moves the queued job into finishedJobs_ as canceled

    const auto results = scheduler.finishedResultsJson();
    REQUIRE(results.is_object());
    CHECK(results.at("count").as_number() == 1.0);
    REQUIRE(results.at("results").is_array());
    CHECK(results.at("results").at(0U).at("state").as_string() == "canceled");

    scheduler.clearFinishedResults();
}
