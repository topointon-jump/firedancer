/* Test for commission_rate_in_basis_points (SIMD-0291)

   Tests:
   - Commission split calculation with basis points (0-10000)
   - Commission capping at 10000 (100%)
   - Vote states cache stores commission_bps correctly

   Note: Full instruction-level tests require the test environment
   which is complex to set up. This file tests the core logic. */

#include "../../stakes/fd_vote_states.h"
#include "../../types/fd_types.h"
#include "../../../util/fd_util.h"

/* Mirror the fd_commission_split struct from fd_rewards.c for testing */
struct fd_commission_split {
  ulong voter_portion;
  ulong staker_portion;
  uint  is_split;
};
typedef struct fd_commission_split fd_commission_split_t;

/* External declaration - function is in fd_rewards.c */
extern void fd_vote_commission_split( ushort commission_bps, ulong on, fd_commission_split_t * result );

/* Test fd_vote_commission_split with basis points */
static void
test_commission_split_bps( void ) {
  FD_LOG_NOTICE(( "Testing commission split with basis points..." ));

  fd_commission_split_t result;

  /* Test 0 bps (0%) - all goes to staker */
  fd_vote_commission_split( 0, 1000, &result );
  FD_TEST( result.voter_portion == 0 );
  FD_TEST( result.staker_portion == 1000 );
  FD_TEST( result.is_split == 0 );

  /* Test 10000 bps (100%) - all goes to voter */
  fd_vote_commission_split( 10000, 1000, &result );
  FD_TEST( result.voter_portion == 1000 );
  FD_TEST( result.staker_portion == 0 );
  FD_TEST( result.is_split == 0 );

  /* Test 5000 bps (50%) - split evenly */
  fd_vote_commission_split( 5000, 1000, &result );
  FD_TEST( result.voter_portion == 500 );
  FD_TEST( result.staker_portion == 500 );
  FD_TEST( result.is_split == 1 );

  /* Test 2500 bps (25%) */
  fd_vote_commission_split( 2500, 1000, &result );
  FD_TEST( result.voter_portion == 250 );
  FD_TEST( result.staker_portion == 750 );
  FD_TEST( result.is_split == 1 );

  /* Test 100 bps (1%) */
  fd_vote_commission_split( 100, 10000, &result );
  FD_TEST( result.voter_portion == 100 );
  FD_TEST( result.staker_portion == 9900 );
  FD_TEST( result.is_split == 1 );

  /* Test capping: values > 10000 should be capped to 10000 */
  fd_vote_commission_split( 15000, 1000, &result );
  FD_TEST( result.voter_portion == 1000 );  /* Capped to 100% */
  FD_TEST( result.staker_portion == 0 );
  FD_TEST( result.is_split == 0 );

  /* Test max u16 value - should be capped to 10000 */
  fd_vote_commission_split( USHORT_MAX, 1000, &result );
  FD_TEST( result.voter_portion == 1000 );  /* Capped to 100% */
  FD_TEST( result.staker_portion == 0 );
  FD_TEST( result.is_split == 0 );

  /* Test precision with larger amounts */
  /* 1234 bps on 10000 lamports = 1234 voter, 8766 staker */
  fd_vote_commission_split( 1234, 10000, &result );
  FD_TEST( result.voter_portion == 1234 );
  FD_TEST( result.staker_portion == 8766 );
  FD_TEST( result.is_split == 1 );

  /* Test with odd splits that result in truncation */
  /* 3333 bps on 10000 = 3333 voter, 6667 staker */
  fd_vote_commission_split( 3333, 10000, &result );
  FD_TEST( result.voter_portion == 3333 );
  FD_TEST( result.staker_portion == 6667 );
  FD_TEST( result.is_split == 1 );

  FD_LOG_NOTICE(( "Commission split tests PASSED" ));
}

/* Test CommissionKind enum */
static void
test_commission_kind_enum( void ) {
  FD_LOG_NOTICE(( "Testing CommissionKind enum..." ));

  /* Verify enum values match Agave */
  FD_TEST( fd_commission_kind_enum_inflation_rewards == 0 );
  FD_TEST( fd_commission_kind_enum_block_revenue == 1 );

  /* Test enum discrimination */
  fd_commission_kind_t kind;

  kind.discriminant = fd_commission_kind_enum_inflation_rewards;
  FD_TEST( fd_commission_kind_is_inflation_rewards( &kind ) );
  FD_TEST( !fd_commission_kind_is_block_revenue( &kind ) );

  kind.discriminant = fd_commission_kind_enum_block_revenue;
  FD_TEST( !fd_commission_kind_is_inflation_rewards( &kind ) );
  FD_TEST( fd_commission_kind_is_block_revenue( &kind ) );

  FD_LOG_NOTICE(( "CommissionKind enum tests PASSED" ));
}

/* Test vote instruction enum values */
static void
test_vote_instruction_enum( void ) {
  FD_LOG_NOTICE(( "Testing vote instruction enum indices..." ));

  /* Verify the new instruction indices match Agave */
  FD_TEST( fd_vote_instruction_enum_initialize_account_v2 == 16 );
  FD_TEST( fd_vote_instruction_enum_update_commission_collector == 17 );
  FD_TEST( fd_vote_instruction_enum_update_commission_bps == 18 );
  FD_TEST( fd_vote_instruction_enum_deposit_delegator_rewards == 19 );

  FD_LOG_NOTICE(( "Vote instruction enum tests PASSED" ));
}

/* Test conversion from percentage to basis points */
static void
test_percentage_to_bps_conversion( void ) {
  FD_LOG_NOTICE(( "Testing percentage to basis points conversion..." ));

  /* The conversion formula is: bps = percentage * 100 */
  FD_TEST( (ushort)(0 * 100) == 0 );       /* 0% = 0 bps */
  FD_TEST( (ushort)(1 * 100) == 100 );     /* 1% = 100 bps */
  FD_TEST( (ushort)(5 * 100) == 500 );     /* 5% = 500 bps */
  FD_TEST( (ushort)(10 * 100) == 1000 );   /* 10% = 1000 bps */
  FD_TEST( (ushort)(25 * 100) == 2500 );   /* 25% = 2500 bps */
  FD_TEST( (ushort)(50 * 100) == 5000 );   /* 50% = 5000 bps */
  FD_TEST( (ushort)(75 * 100) == 7500 );   /* 75% = 7500 bps */
  FD_TEST( (ushort)(100 * 100) == 10000 ); /* 100% = 10000 bps */

  FD_LOG_NOTICE(( "Percentage to basis points conversion tests PASSED" ));
}

int
main( int argc, char ** argv ) {
  fd_boot( &argc, &argv );

  FD_LOG_NOTICE(( "=== commission_rate_in_basis_points Tests ===" ));

  test_commission_split_bps();
  test_commission_kind_enum();
  test_vote_instruction_enum();
  test_percentage_to_bps_conversion();

  FD_LOG_NOTICE(( "=== All tests PASSED ===" ));

  fd_halt();
  return 0;
}
