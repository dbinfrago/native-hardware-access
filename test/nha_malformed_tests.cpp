// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "nha_test_case.h"

/**
 * @brief Test validates that an empty query results in an empty response.
 */
TEST_F(NHATestCase, MalformedQuery_empty_query)
{
    // send empty query, expecting error
    const auto response = send_query("{}", 1);
    // response should be empty
    EXPECT_TRUE(response.empty());
}

/**
 * @brief Test validates that a corrup header results in an empty response.
 */
TEST_F(NHATestCase, MalformedQuery_corrupt_header)
{
    // send wrong magic, expecting error
    const auto response = send_query("{m(\"NHA9\") v(0x00 0x00 0x00 0x01) t(0x01) f(0x00)}", 1);
    // response should be empty
    EXPECT_TRUE(response.empty());
}

/**
 * @brief Test validates that a query TLV with unexpected value results in a header-only response
 */
TEST_F(NHATestCase, MalformedQuery_TLV_query_with_unexpected_data)
{
    // send DeviceID query with unexpected data
    const auto response = send_query("{H d(0x01 0x02 0x03)}");
    // response should be only a header
    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "38");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
}

/**
 * @brief Test validates that a query which actually contains a response-specific TLV results in a header-only response
 */
TEST_F(NHATestCase, MalformedQuery_sending_response_TLV)
{
    // send DeviceID response, which is unexpected
    const auto response = send_query("{H 0x12(0x12 0x08 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08)}");
    // response should be only a header
    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "38");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
}

/**
 * @brief Test validates that a query with a duplicate TLV results in a header-only response
 */
TEST_F(NHATestCase, MalformedQuery_duplicate_TLV)
{
    // query DeviceID twice
    const auto response = send_query("{H d d}");
    // response should be only a header
    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "38");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
}
