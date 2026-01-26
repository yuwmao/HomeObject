#include "homeobj_fixture.hpp"
#include <homestore/replication_service.hpp>

// Test reconcile_membership on recovery when membership is inconsistent
TEST_F(HomeObjectFixture, ReconcileMembershipOnRecovery) {
    LOGINFO("ReconcileMembershipOnRecovery test started, replica={}", g_helper->replica_num());

    // Step 1: Create a PG with normal flow
    pg_id_t pg_id{1};
    create_pg(pg_id);

    // Step 2: Simulate membership inconsistency by manually modifying pg_info_.members
    // This simulates the bug scenario where START_REPLACE modified membership but rollback didn't
    run_on_pg_leader(pg_id, [&]() {
        auto hs_pg = const_cast<HSHomeObject::HS_PG*>(_obj_inst->get_hs_pg(pg_id));
        ASSERT_NE(hs_pg, nullptr);

        // Record original membership size
        auto original_member_count = hs_pg->pg_info_.members.size();
        LOGINFO("Original member count: {}", original_member_count);

        // Add a fake member to simulate inconsistency (this member is not in raft config)
        auto fake_member_id = boost::uuids::random_generator()();
        PGMember fake_member{fake_member_id, "fake_member", 0};

        // Directly modify in-memory membership to simulate the bug
        hs_pg->pg_info_.members.insert(fake_member);
        LOGINFO("Added fake member, new count: {}", hs_pg->pg_info_.members.size());
        EXPECT_EQ(hs_pg->pg_info_.members.size(), original_member_count + 1);

        // Call reconcile_membership to fix the inconsistency
        hs_pg->reconcile_membership();

        // Verify membership is corrected
        LOGINFO("After reconcile, member count: {}", hs_pg->pg_info_.members.size());
        EXPECT_EQ(hs_pg->pg_info_.members.size(), original_member_count);

        // Verify the fake member is removed
        auto it = hs_pg->pg_info_.members.find(fake_member);
        EXPECT_EQ(it, hs_pg->pg_info_.members.end());

        // Verify all members in pg_info are actually in raft config
        auto repl_status = hs_pg->repl_dev_->get_replication_status();
        for (const auto& member : hs_pg->pg_info_.members) {
            bool found = false;
            for (const auto& r : repl_status) {
                if (r.id_ == member.id && r.can_vote_) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Member " << boost::uuids::to_string(member.id)
                               << " in pg_info but not in raft voting members";
        }

        LOGINFO("Membership reconciliation verified successfully");
    });
}

// Test reconcile_membership preserves existing member info (name, priority)
TEST_F(HomeObjectFixture, ReconcileMembershipPreservesInfo) {
    LOGINFO("ReconcileMembershipPreservesInfo test started, replica={}", g_helper->replica_num());

    pg_id_t pg_id{1};
    create_pg(pg_id);

    run_on_pg_leader(pg_id, [&]() {
        auto hs_pg = const_cast<HSHomeObject::HS_PG*>(_obj_inst->get_hs_pg(pg_id));
        ASSERT_NE(hs_pg, nullptr);

        // Record original member info
        std::map<peer_id_t, std::pair<std::string, int32_t>> original_info;
        for (const auto& m : hs_pg->pg_info_.members) {
            original_info[m.id] = {m.name, m.priority};
            LOGINFO("Original member: id={}, name={}, priority={}",
                    boost::uuids::to_string(m.id), m.name, m.priority);
        }

        // Call reconcile_membership
        hs_pg->reconcile_membership();

        // Verify all original members still have their name and priority
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

        LOGINFO("Member info preservation verified successfully");
    });
}

// Test reconcile_membership on actual recovery restart
TEST_F(HomeObjectFixture, ReconcileMembershipOnRestart) {
    LOGINFO("ReconcileMembershipOnRestart test started, replica={}", g_helper->replica_num());

    pg_id_t pg_id{1};
    create_pg(pg_id);

    // Record membership before restart
    std::set<peer_id_t> expected_members;
    run_on_pg_leader(pg_id, [&]() {
        auto hs_pg = _obj_inst->get_hs_pg(pg_id);
        ASSERT_NE(hs_pg, nullptr);

        for (const auto& m : hs_pg->pg_info_.members) {
            expected_members.insert(m.id);
            LOGINFO("Expected member: {}", boost::uuids::to_string(m.id));
        }
    });

    // Restart to trigger recovery
    restart();

    // Verify membership is reconciled correctly on recovery
    run_on_pg_leader(pg_id, [&]() {
        auto hs_pg = const_cast<HSHomeObject::HS_PG*>(_obj_inst->get_hs_pg(pg_id));
        ASSERT_NE(hs_pg, nullptr);

        LOGINFO("After restart, member count: {}", hs_pg->pg_info_.members.size());
        EXPECT_EQ(hs_pg->pg_info_.members.size(), expected_members.size());

        for (const auto& m : hs_pg->pg_info_.members) {
            EXPECT_TRUE(expected_members.contains(m.id))
                << "Unexpected member after restart: " << boost::uuids::to_string(m.id);
            LOGINFO("Verified member after restart: {}", boost::uuids::to_string(m.id));
        }

        LOGINFO("Membership reconciliation on restart verified successfully");
    });
}
