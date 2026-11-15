#ifndef USV_REPLAY_LAB_CASE_IO_H_
#define USV_REPLAY_LAB_CASE_IO_H_

#include "replay.h"

#include <iosfwd>
#include <string>

namespace usv_replay_lab::case_io
{
    struct CaseSpec
    {
        std::string id_;
        ReplayConfig config_;
        std::vector<LinkEvent> expected_event_vector_;
    };

    CaseSpec readCase(const std::string &_path);
    std::vector<std::uint64_t> readHeartbeats(const std::string &_path);
    void writeReport(std::ostream &_output, const CaseSpec &_case_spec, Subject _subject,
        const ReplayResult &_result, bool _passed);
} // namespace usv_replay_lab::case_io

#endif // USV_REPLAY_LAB_CASE_IO_H_
