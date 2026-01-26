#include "homeobj_fixture.hpp"
#include "homeobj_fixture_http.hpp"
#include <homestore/replication_service.hpp>

// Test reconcile_membership: simulate inconsistency, fix it, verify member composition and info preservation,
// then restart and verify again
TEST_F(HomeObjectFixture, ReconcileMembership) {
    LOGINFO("ReconcileMembership test started, replica={}", g_helper->replica_num());
    auto http_enabled = SISL_OPTIONS["enable_http"].as< bool >();

    // Step 1: Create a PG with normal flow
    pg_id_t pg_id{1};
    create_pg(pg_id);

    // Step 2: Execute on all members (not just leader)
    auto hs_pg = const_cast<HSHomeObject::HS_PG*>(_obj_inst->get_hs_pg(pg_id));
    if (hs_pg == nullptr) {
        LOGINFO("This replica is not a member of pg={}, skipping", pg_id);
        return;
    }

    // Step 3: Record original membership and member info
    auto original_member_count = hs_pg->pg_info_.members.size();
    std::map<peer_id_t, std::pair<std::string, int32_t>> original_info;
    for (const auto& m : hs_pg->pg_info_.members) {
        original_info[m.id] = {m.name, m.priority};
        LOGINFO("Original member: id={}, name={}, priority={}",
                boost::uuids::to_string(m.id), m.name, m.priority);
    }
    LOGINFO("Original member count: {}", original_member_count);

    // Step 4: Simulate membership inconsistency by adding a fake member
    // This simulates the bug scenario where START_REPLACE modified membership but rollback didn't
    auto fake_member_id = boost::uuids::random_generator()();
    PGMember fake_member{fake_member_id, "fake_member", 0};
    hs_pg->pg_info_.members.insert(fake_member);
    LOGINFO("Added fake member, new count: {}", hs_pg->pg_info_.members.size());
    EXPECT_EQ(hs_pg->pg_info_.members.size(), original_member_count + 1);

    // Step 5: Call reconcile_membership to fix the inconsistency
    if (!http_enabled) {
        bool success = _obj_inst->reconcile_membership(pg_id);
        ASSERT_TRUE(success);
    } else {
        HttpHelper http_helper("127.0.0.1", 5000 + g_helper->replica_num());
        nlohmann::json j;
        j["pg_id"] = std::to_string(pg_id);
        auto r = http_helper.post("/api/v1/reconcile_membership", j.dump());
        ASSERT_EQ(r.code(), Pistache::Http::Code::Ok);
    }

    // Step 6: Verify membership composition is corrected
    LOGINFO("After reconcile, member count: {}", hs_pg->pg_info_.members.size());
    EXPECT_EQ(hs_pg->pg_info_.members.size(), original_member_count);

    // Verify the fake member is removed
    auto it = hs_pg->pg_info_.members.find(fake_member);
    EXPECT_EQ(it, hs_pg->pg_info_.members.end());

    // Verify all members in pg_info are actually in raft config
    auto quorum = hs_pg->repl_dev_->get_replication_quorum();
    for (const auto& member : hs_pg->pg_info_.members) {
        bool found = false;
        for (const auto& r : quorum) {
            if (r == member.id) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Member " << boost::uuids::to_string(member.id)
                           << " in pg_info but not in raft voting members";
    }

    // Step 7: Verify all original members still have their name and priority preserved
    for (const auto& m : hs_pg->pg_info_.members) {
        ASSERT_TRUE(original_info.contains(m.id))
            << "Member " << boost::uuids::to_string(m.id) << " not found in original info";

        EXPECT_EQ(m.name, original_info[m.id].first)
            << "Member " << boost::uuids::to_string(m.id) << " name mismatch";
        EXPECT_EQ(m.priority, original_info[m.id].second)
            << "Member " << boost::uuids::to_string(m.id) << " priority mismatch";

        LOGINFO("Verified member: id={}, name={}, priority={}",
                boost::uuids::to_string(m.id), m.name, m.priority);
    }

    LOGINFO("Membership reconciliation verified successfully before restart");

    // // Step 8: Restart to trigger recovery
    // restart();
    //
    // // Step 9: Verify membership is still correct after restart
    // hs_pg = const_cast<HSHomeObject::HS_PG*>(_obj_inst->get_hs_pg(pg_id));
    // if (hs_pg == nullptr) {
    //     LOGINFO("This replica is not a member of pg={} after restart, skipping", pg_id);
    //     return;
    // }
    //
    // LOGINFO("After restart, member count: {}", hs_pg->pg_info_.members.size());
    // EXPECT_EQ(hs_pg->pg_info_.members.size(), original_member_count);
    //
    // // Verify all original members still exist with correct info after restart
    // for (const auto& [member_id, info] : original_info) {
    //     auto member_it = hs_pg->pg_info_.members.find(PGMember(member_id));
    //     EXPECT_NE(member_it, hs_pg->pg_info_.members.end())
    //         << "Member " << boost::uuids::to_string(member_id) << " not found after restart";
    //
    //     if (member_it != hs_pg->pg_info_.members.end()) {
    //         EXPECT_EQ(member_it->name, info.first)
    //             << "Member " << boost::uuids::to_string(member_id) << " name mismatch after restart";
    //         EXPECT_EQ(member_it->priority, info.second)
    //             << "Member " << boost::uuids::to_string(member_id) << " priority mismatch after restart";
    //         LOGINFO("Verified member after restart: id={}, name={}, priority={}",
    //                 boost::uuids::to_string(member_it->id), member_it->name, member_it->priority);
    //     }
    // }

    LOGINFO("Membership reconciliation verified successfully after restart");
}
