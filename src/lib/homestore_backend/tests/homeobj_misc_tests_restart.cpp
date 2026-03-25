#include "homeobj_fixture.hpp"

// #ifdef _PRERELEASE
// TEST_F(HomeObjectFixture, Restart_With_Lookup_Peer_Failure) {
//     LOGINFO("HomeObject replica={} setup completed", g_helper->replica_num());
//     g_helper->sync();
//
//     // Step 1: Create a PG and write some data to ensure raft group is active
//     constexpr pg_id_t pg_id = 1;
//     create_pg(pg_id);
//     auto shard_info = create_shard(pg_id, 64 * Mi, "test shard");
//     LOGINFO("pg={} shard {} created", pg_id, shard_info.id);
//
//     // Write some blobs
//     std::map< pg_id_t, std::vector< shard_id_t > > pg_shard_id_vec;
//     pg_shard_id_vec[pg_id].push_back(shard_info.id);
//     std::map< pg_id_t, blob_id_t > pg_blob_id;
//     pg_blob_id[pg_id] = 0;
//     put_blobs(pg_shard_id_vec, 5 /* num_blobs_per_shard */, pg_blob_id);
//
//     g_helper->sync();
//     LOGINFO("Validate all data written");
//     verify_get_blob(pg_shard_id_vec, 5);
//     g_helper->sync();
//
//     // Step 2: Inject flip on replica 1 to simulate lookup_peer failure during restart
//     if (g_helper->replica_num() == 1) {
//         LOGINFO("Set flip to fake lookup_peer failure on replica=1");
//         // Set flip to trigger multiple times to ensure join_group attempts fail initially
//         set_basic_flip("fake_lookup_peer_failure", 10000, 100);
//     }
//
//     g_helper->sync();
//
//     // Step 3: Restart replica 1 (follower)
//     if (g_helper->replica_num() == 1) {
//         LOGINFO("Restart follower replica=1 with lookup_peer flip enabled");
//         restart();
//     }
//
//     g_helper->sync();
//
//     // Step 4: Verify system behavior when lookup_peer fails
//     // The member should eventually start successfully after flip expires
//     LOGINFO("Waiting for replica to stabilize after lookup_peer failures...");
//     std::this_thread::sleep_for(std::chrono::seconds(10));
//
//     g_helper->sync();
//
//     // Step 5: Verify data is still accessible after restart
//     LOGINFO("Validate all data after restart");
//     verify_get_blob(pg_shard_id_vec, 5);
//
//     g_helper->sync();
//     LOGINFO("Test completed successfully");
// }
// #endif
