// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "nha_test_case.h"

/**
 * @brief Test validates that an unsupported TLV is simply ignored
 */
TEST_F(NHATestCase, UnsupportedQuery_unknown_TLV)
{
    // send query that includes an unknown Tag
    const auto response = send_query("{H d 0x54(0x00 0x01 0x02) rx}");
    // size must be header + d + rx + checksum, but nothing for the unknown "T=0x54"
    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "66");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
    // check if DeviceID and RX are present
    EXPECT_NE(response.find("0x11"), response.end());
    EXPECT_NE(response.find("0x12"), response.end());
}

/**
 * @brief Test validates that a query with an unknown Flag value is considered malformed.
 */
TEST_F(NHATestCase, UnsupportedQuery_unknown_flag)
{
    // send header that includes an unknown Flag
    const auto response = send_query("{m(\"NHA1\") v(0x00 0x00 0x00 0x01) t(0x01) f(0x54) d}", 1);
    EXPECT_TRUE(response.empty());
}

/**
 * @brief Test validates that a query with an unknown Type value is considered malformed.
 */
TEST_F(NHATestCase, UnsupportedQuery_unknown_type)
{
    // send header that includes an unknown Type
    const auto response = send_query("{m(\"NHA1\") v(0x00 0x00 0x00 0x01) t(0x54) f(0x00) d}", 1);
    EXPECT_TRUE(response.empty());
}

/**
 * @brief Test validates that a query with an unknown Version value is considered malformed.
 */
TEST_F(NHATestCase, UnsupportedQuery_unknown_version)
{
    // send header that includes an unknown Version
    const auto response = send_query("{m(\"NHA1\") v(0x54 0x00 0x00 0x54) t(0x01) f(0x00) d}", 1);
    EXPECT_TRUE(response.empty());
}
