// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "nha_test_case.h"

/**
 * @brief Test validates that a minimal valid query (header only) is accepted and validates every field.
 */
TEST_F(NHATestCase, ValidQuery_header_only_is_valid_and_header_is_correct)
{
    const auto response = send_query("{H}");

    // check all fields of the response header
    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "38")
        << "header should be 22 + MD5 size";

    EXPECT_EQ(response.at("0x00"), "0x4e 0x48 0x41 0x31")
        << "Magic TLV is incorrect";

    EXPECT_EQ(response.at("0x01"), "0x00 0x00 0x00 0x01")
        << "Version TLV is incorrect";

    EXPECT_EQ(response.at("0x02"), "0x01")
        << "Type TLV is incorrect";

    EXPECT_EQ(response.at("0x03"), "0x00")
        << "Flags TLV is incorrect";

    EXPECT_EQ(response.at("received"), "cae824810c258a53c798025a484f3426")
        << "received MD5 is incorrect";

    EXPECT_EQ(response.at("calculated"), "cae824810c258a53c798025a484f3426")
        << "calculated MD5 is incorrect";

    EXPECT_EQ(response.at("xored"), "00000000000000000000000000000000")
        << "xored MD5 is incorrect";
}

/**
 * @brief Test validates that a query that requests DeviceId, RX and TX is complete
 */
TEST_F(NHATestCase, ValidQuery_all_fields)
{
    const auto response = send_query("{H d rx tx l}");

    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "86");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
    // check the four expected TLVs are present
    EXPECT_NE(response.find("0x11"), response.end());
    EXPECT_NE(response.find("0x12"), response.end());
    EXPECT_NE(response.find("0x13"), response.end());
    EXPECT_NE(response.find("0x14"), response.end());
}

/**
 * @brief Test validates the MD5 of a response that was requested with XORing with DeviceId
 */
TEST_F(NHATestCase, ValidQuery_check_XOR)
{
    const auto response = send_query("{H(0x02) d}");

    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "56");
    // check md5 - they should mismatch
    EXPECT_NE(response.at("received"), response.at("calculated"));
    EXPECT_EQ(response.at("0x03"), "0x01")
        << "Flags TLV is incorrect";
    // check that "xored" equals DeviceID
    EXPECT_NE(response.find("0x11"), response.end());
    // convert the bytes
    std::vector<uint8_t> device_id_from_bytes;
    std::stringstream ss(response.at("0x11"));
    std::string token;
    while (ss >> token)
    {
        device_id_from_bytes.emplace_back(std::stoul(token.substr(2), nullptr, 16));
    }
    // convert the decoded DeviceID
    std::vector<uint8_t> device_id_from_decoded;
    std::string decoded_string = response.at("0x11_decoded").substr(11, 32);
    for (int i = 0; i < decoded_string.size(); i += 2)
    {
        device_id_from_decoded.emplace_back(std::stoul(decoded_string.substr(i, 2), nullptr, 16));
    }
    EXPECT_EQ(device_id_from_bytes, device_id_from_decoded);
}

/**
 * @brief Test validates that 'userData' is returned.
 */
TEST_F(NHATestCase, ValidQuery_user_data_returned)
{
    const auto response = send_query("{H u(578437695752307201L)}");

    EXPECT_EQ(response.at("completeSizeOfMsgInBytes"), "48");
    // check md5
    EXPECT_EQ(response.at("received"), response.at("calculated"));
    // check if returned userData is the expected
    EXPECT_EQ(response.at("0x05"), "0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08");
}

/**
 * @brief Test validates that two requests that immediately follow each other have different timer values.
 */
TEST_F(NHATestCase, ValidQuery_timer_is_monotonic)
{
    const auto response1 = send_query("{H rx}");
    const auto response2 = send_query("{H rx tx}");

    EXPECT_NE(response1.at("0x12"), response2.at("0x12"));
    EXPECT_NE(response2.at("0x12"), response2.at("0x13"));
}

/**
 * @brief Test validates that the device ID is consistent across two consecutive requests.
 */
TEST_F(NHATestCase, ValidQuery_device_id_consistency)
{
    const auto response1 = send_query("{H d}");
    const auto response2 = send_query("{H d}");

    EXPECT_EQ(response1.at("0x11"), response2.at("0x11"))
        << "DeviceID should remain consistent across consecutive requests";
}


