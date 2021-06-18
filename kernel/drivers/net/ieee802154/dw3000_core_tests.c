#include <kunit/test.h>
#include "dw3000_core.h"

/* Define the test cases. */

static void dw3000_ktime_to_dtu_test_basic(struct kunit *test)
{
	struct dw3000 *dw = kunit_kzalloc(test, sizeof(*dw), GFP_KERNEL);
	/* Ensure allocation succeeded. */
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dw);
	/* Tests with time_zero_ns == 0 */
	KUNIT_EXPECT_EQ(test, 0u, dw3000_ktime_to_dtu(dw, 0));
	KUNIT_EXPECT_EQ(test, 15u, dw3000_ktime_to_dtu(dw, 1000));
	KUNIT_EXPECT_EQ(test, 156u, dw3000_ktime_to_dtu(dw, 10000));
	KUNIT_EXPECT_EQ(test, 312u, dw3000_ktime_to_dtu(dw, 20000));
	/* Tests with time_zero_ns == 1000000 */
	dw->time_zero_ns = 1000000;
	KUNIT_EXPECT_EQ(test, 0u, dw3000_ktime_to_dtu(dw, 1000000));
	KUNIT_EXPECT_EQ(test, 15u, dw3000_ktime_to_dtu(dw, 1001000));
	KUNIT_EXPECT_EQ(test, (u32)-15600, dw3000_ktime_to_dtu(dw, 0));
	KUNIT_EXPECT_EQ(test, (u32)-15584, dw3000_ktime_to_dtu(dw, 1000));
}

static void dw3000_dtu_to_ktime_test_basic(struct kunit *test)
{
	struct dw3000 *dw = kunit_kzalloc(test, sizeof(*dw), GFP_KERNEL);
	/* Ensure allocation succeeded. */
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dw);
	/* Tests with time_zero_ns == 0 */
	KUNIT_EXPECT_EQ(test, 0ll, dw3000_dtu_to_ktime(dw, 0));
	KUNIT_EXPECT_EQ(test, 64102ll, dw3000_dtu_to_ktime(dw, 1000));
	/* Tests with time_zero_ns == 1000000 */
	dw->time_zero_ns = 1000000;
	KUNIT_EXPECT_EQ(test, 1000000ll, dw3000_dtu_to_ktime(dw, 0));
	KUNIT_EXPECT_EQ(test, 1064102ll, dw3000_dtu_to_ktime(dw, 1000));
}

static void dw3000_dtu_to_sys_time_test_basic(struct kunit *test)
{
	struct dw3000 *dw = kunit_kzalloc(test, sizeof(*dw), GFP_KERNEL);
	/* Ensure allocation succeeded. */
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dw);
	/* Tests with dtu_sync == 0 & sys_time_sync == 0 */
	KUNIT_EXPECT_EQ(test, 0u, dw3000_dtu_to_sys_time(dw, 0));
	KUNIT_EXPECT_EQ(test, 1u << 4, dw3000_dtu_to_sys_time(dw, 1));
	/* Tests with dtu_sync == 10000 & sys_time_sync == 0 */
	dw->dtu_sync = 10000;
	KUNIT_EXPECT_EQ(test, 0u, dw3000_dtu_to_sys_time(dw, 10000));
	KUNIT_EXPECT_EQ(test, 1u << 4, dw3000_dtu_to_sys_time(dw, 10001));
	/* Tests with dtu_sync == 0 & sys_time_sync == 0 */
	dw->sys_time_sync = 1000000;
	KUNIT_EXPECT_EQ(test, 1000000u, dw3000_dtu_to_sys_time(dw, 10000));
	KUNIT_EXPECT_EQ(test, 1000016u, dw3000_dtu_to_sys_time(dw, 10001));
}

static void dw3000_sys_time_to_dtu_test_basic(struct kunit *test)
{
	u32 dtu_near = 0;
	struct dw3000 *dw = kunit_kzalloc(test, sizeof(*dw), GFP_KERNEL);
	/* Ensure allocation succeeded. */
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dw);
	/* Tests with dtu_sync == 0 & sys_time_sync == 0 */
	KUNIT_EXPECT_EQ(test, 0u, dw3000_sys_time_to_dtu(dw, 0, dtu_near));
	KUNIT_EXPECT_EQ(test, 1u, dw3000_sys_time_to_dtu(dw, 1 << 4, dtu_near));
	/* Tests with dtu_near == 10000 */
	dtu_near = 10000;
	KUNIT_EXPECT_EQ(test, 1005u,
			dw3000_sys_time_to_dtu(dw, 1005 << 4, dtu_near));
	/* Tests with dtu_sync == 1000000/16 & sys_time_sync == 1000000 */
	dw->sys_time_sync = 1000000;
	dw->dtu_sync = 1000000 >> 4;
	KUNIT_EXPECT_EQ(test, 10000u,
			dw3000_sys_time_to_dtu(dw, 10000 << 4, dtu_near));
	/* Tests with real values from traces
	   wakeup: dtu_near: 0x773a2f1, dtu_sync: 0x852a9ff, sys_time_sync: 0x293a6
	   rx_enable: timestamp_dtu=0x085776e8
	   read_rx_timestamp: value 1336512593 -> 0x4fa99051 -> sys_time 0x13ea64
	   get_rx_frame: timestamp_dtu=0x185776e6 timestamp_rctu=0xffffffffc3fee418
	*/
	dtu_near = 0x773a2f1u;
	dw->dtu_sync = 0x852a9ffu;
	dw->sys_time_sync = 0x293a6u;
	KUNIT_EXPECT_EQ(test, 0x853bf6au,
			dw3000_sys_time_to_dtu(dw, 0x13ea64u, dtu_near));
}

static struct kunit_case dw3000_core_test_cases[] = {
	KUNIT_CASE(dw3000_ktime_to_dtu_test_basic),
	KUNIT_CASE(dw3000_dtu_to_ktime_test_basic),
	KUNIT_CASE(dw3000_dtu_to_sys_time_test_basic),
	KUNIT_CASE(dw3000_sys_time_to_dtu_test_basic),
	{}
};

static struct kunit_suite dw3000_core_test_suite = {
	.name = "dw3000-core",
	.test_cases = dw3000_core_test_cases,
};
kunit_test_suite(dw3000_core_test_suite);

MODULE_LICENSE("GPL v2");
