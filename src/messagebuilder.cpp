// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "messagebuilder.h"

#include "auxiliary.h"

#include <ctype.h>
#include <cstring>
#include <iostream>
#include <openssl/evp.h>

constexpr uint32_t HEADER_SIZE = 22;
constexpr uint32_t MIN_MSG_SIZE = 38;
constexpr uint32_t MAX_MSG_SIZE = 32768;
constexpr uint32_t CHECKSUM_SIZE = 16;
constexpr uint32_t MIN_TLV_SIZE = 2;

// message Tags (TLV struct)
constexpr uint8_t MAGIC_TAG = 0x00;
constexpr uint8_t VERSION_TAG = 0x01;
constexpr uint8_t MESSAGE_TYPE_TAG = 0x02;
constexpr uint8_t MESSAGE_FLAGS_TAG = 0x03;

constexpr uint8_t USER_DATA_QTAG = 0x04;
constexpr uint8_t DEVICEID_QTAG = 0x91;
constexpr uint8_t TIMER_RX_QTAG = 0x92;
constexpr uint8_t TIMER_TX_QTAG = 0x93;
constexpr uint8_t DEVICEID_LAST_UPDATED_QTAG = 0x94;
constexpr uint8_t CAPABILITIES_QTAG = 0x90;
constexpr uint8_t TEMPERATURES_QTAG = 0xA0;

constexpr uint8_t USER_DATA_RTAG = 0x05;
constexpr uint8_t DEVICEID_RTAG = 0x11;
constexpr uint8_t TIMER_RX_RTAG = 0x12;
constexpr uint8_t TIMER_TX_RTAG = 0x13;
constexpr uint8_t DEVICEID_LAST_UPDATED_RTAG = 0x14;
constexpr uint8_t CAPABILITIES_RTAG = 0x10;
constexpr uint8_t TEMPERATURES_RTAG = 0x20;

// message Lengths (of values) (TLV struct)
constexpr uint8_t MAGIC_LENGTH = 0x04;
constexpr uint8_t VERSION_LENGTH = 0x04;
constexpr uint8_t MESSAGE_TYPE_LENGTH = 0x01;
constexpr uint8_t MESSAGE_FLAGS_LENGTH = 0x01;

// message values (TLV struct)
constexpr uint8_t MAGIC_BYTES[4] = {0x4E, 0x48, 0x41, 0x31};
constexpr uint8_t VERSION_BYTES[4] = {0x00, 0x00, 0x00, 0x01};
constexpr uint8_t MESSAGE_TYPE_STANDARD_MESSAGE = 0x01;



enum MsgFlags {
    None = 0x00,
    HAS_CHECKSUM_XOR_DEVICE_ID = 0x01,
    REPLY_CHECKSUM_XOR_DEVICE_ID = 0x02
};

MessageBuilder::MessageBuilder(TPMWatcher &tpmWatcher,  MonotonicTimer &monotonicTimer)
    : tpmWatcher(tpmWatcher),
      monotonicTimer(monotonicTimer)
{
}

uint32_t MessageBuilder::fromLE32(const uint8_t *array)
{
    return static_cast<uint32_t>(array[0]) | (static_cast<uint32_t>(array[1]) << 8) | (static_cast<uint32_t>(array[2]) << 16) | (static_cast<uint32_t>(array[3]) << 24);
}

void MessageBuilder::toLE64(uint64_t num, uint8_t *array)
{
    for (int i = 0; i < 8; ++i)
    {
        array[i] = static_cast<uint8_t>((num >> (8 * i)) & 0xff);
    }
}

bool MessageBuilder::computeMD5(const void* data, size_t length, std::vector<uint8_t>& digest)
{
    size_t md_len = 0;
    digest.resize(EVP_MAX_MD_SIZE);

    //checksum calculation
    if (EVP_Q_digest(NULL, "MD5", NULL, data, length, static_cast<unsigned char *>(digest.data()), &md_len) != 1)
    {
        return false;
    }
    if (CHECKSUM_SIZE != md_len)
    {
        return false;
    }
    digest.resize(md_len);
    return true;
}

int MessageBuilder::createResponse(const char * const query, const int querySize, char * const responseBuffer,
                                   const uint32_t maxResponseSize)
{
    // Get the RX time as early as possible
    const uint64_t timerRx = monotonicTimer.get();

    // Get the device ID from the TPMWatcher
    std::vector<uint8_t> devID(TPMWatcher::deviceIDLength);
    tpmWatcher.get(&devID[0]);

    // Check arguments
    if ((nullptr == query) || (nullptr == responseBuffer) || (querySize < HEADER_SIZE) || (maxResponseSize < HEADER_SIZE))
    {
        return -1;
    }

    // Process the header (completeSizeOfMsgInBytes, Magic string, Version, Message type, Message flags) of query
    uint8_t const * const headerQ = reinterpret_cast<const uint8_t *>(query);

    // Get header size and validate
    uint32_t totalSize = fromLE32(headerQ);
    if (totalSize != querySize || totalSize < MIN_MSG_SIZE || totalSize > MAX_MSG_SIZE)
    { return -1; }

    // We use static assert to make sure that HEADER_SIZE is correct.
    // If the header is changed, we want to know, since this code needs to be updated then.
    static_assert(HEADER_SIZE == 22, "HEADER_SIZE value is incorrect");


    // Check and validate magic bytes
    if (headerQ[4] != MAGIC_TAG || headerQ[5] != MAGIC_LENGTH)
    { return -1; }

    for (int i = 0; i < (sizeof(MAGIC_BYTES) / sizeof(uint8_t)); ++i)
    {
        if (headerQ[6 + i] != MAGIC_BYTES[i])
        { return -1; }
    }

    // Check format version
    if (  (headerQ[10] != VERSION_TAG)
       || (headerQ[11] != VERSION_LENGTH))
    { return -1; }

    for (int i = 0; i < (sizeof(VERSION_BYTES) / sizeof(uint8_t)); ++i)
    {
        if (headerQ[12 + i] != VERSION_BYTES[i])
        { return -1; }
    }

    // Check message type
    if ((headerQ[16] != MESSAGE_TYPE_TAG) || (headerQ[17] != MESSAGE_TYPE_LENGTH))
    { return -1; }

    uint8_t msgTypeValue = headerQ[18];

    if (msgTypeValue != MESSAGE_TYPE_STANDARD_MESSAGE)
    {
        return -1;
    }

    // Check message flags
    if ((headerQ[19] != MESSAGE_FLAGS_TAG) || (headerQ[20] != MESSAGE_FLAGS_LENGTH))
    { return -1; }

    uint8_t msgFlagsValue = headerQ[21];

    if ((msgFlagsValue != None) && (msgFlagsValue != REPLY_CHECKSUM_XOR_DEVICE_ID))
    {
        return -1;
    }

    // Remember the decision to XOR with device ID in the response
    uint8_t msgFlagsValueR = None;
    if ((msgFlagsValue & REPLY_CHECKSUM_XOR_DEVICE_ID) != 0)
    {
        msgFlagsValueR = HAS_CHECKSUM_XOR_DEVICE_ID;
    }

    // Calculate checksum on query
    std::vector<uint8_t> mdValue;
    const uint8_t *checksumQ = (const uint8_t *)(query + querySize - CHECKSUM_SIZE);
    if (!computeMD5(query, querySize - CHECKSUM_SIZE, mdValue))
    {
        return -1;
    }

    // Verify checksum
    for (int i = 0; i < CHECKSUM_SIZE; ++i)
    {
        if (checksumQ[i] != mdValue[i])
        {
            return -1;
        }
    }

    // The size of response which is created later. Initialize with header and checksum size.
    // TLV body size is added later, entry by entry.
    uint32_t sizeR = HEADER_SIZE + CHECKSUM_SIZE;

    // Defensive check. This would be a bug, since this would mean that the maxResponseSize is too small to even hold the header and checksum.
    if (sizeR > maxResponseSize)
    {
        return -1;
    }

    // Process the body (User data, Device-ID, Monotonic timer RX, Monotonic timer TX) of query.
    // For this, we create a convenience pointer and a remaining size variable.
    const uint8_t *bodyQ = reinterpret_cast<const uint8_t *>(query + HEADER_SIZE);
    int32_t remainingTLVDataFromQuery = querySize - HEADER_SIZE - CHECKSUM_SIZE;

    // Verify whether which query tag is in query
    std::vector<uint8_t> userData;
    bool wantDeviceID = false;
    bool wantTimeTx = false;
    bool wantTimeRx = false;
    bool wantLastUpdated = false;

    // In case of error in the query body the response is created without body.
    bool createResponseWithoutBody = false;

    // Process all TLV records from the query body
    while ((remainingTLVDataFromQuery >= MIN_TLV_SIZE) && (false == createResponseWithoutBody))
    {
        const uint8_t tag = bodyQ[0];
        const uint8_t length = bodyQ[1];

        if (remainingTLVDataFromQuery < (MIN_TLV_SIZE + length))
        {
            // Malformed query. We miss the space in the body for the value of this TLV record.
            // Note that we check below for remainingTLVDataFromQuery > 0 to catch this as well.
            break;
        }

        uint8_t const * const value = bodyQ + MIN_TLV_SIZE;
        switch(tag)
        {
            case USER_DATA_QTAG:
                // Check for querying user data twice or malformed TLV data
                if ((userData.size() != 0) || (length < 1) || (!try_add_unsigned(sizeR, static_cast<uint32_t>(MIN_TLV_SIZE + length), maxResponseSize)))
                {
                    createResponseWithoutBody = true;
                }
                else
                {
                    // By filling "userData", we remember that user data is requested in the response.
                    userData.resize(length);
                    for (int i = 0; i < length; ++i)
                    {
                        userData[i] = value[i];
                    }
                }
                break;
            case DEVICEID_QTAG:
                // Check for querying device ID twice or malformed TLV data
                if (wantDeviceID || (length != 0) || (!try_add_unsigned(sizeR, static_cast<uint32_t>(MIN_TLV_SIZE + TPMWatcher::deviceIDLength), maxResponseSize)))
                {
                    createResponseWithoutBody = true;
                }
                else
                {
                    wantDeviceID = true;
                }
                break;
            case TIMER_RX_QTAG:
                // Check for querying timer RX twice or malformed TLV data
                if (wantTimeRx || (length != 0) || (!try_add_unsigned(sizeR, static_cast<uint32_t>(MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()))), maxResponseSize)))
                {
                    createResponseWithoutBody = true;
                }
                else
                {
                    wantTimeRx = true;
                }
                break;
            case TIMER_TX_QTAG:
                // Check for querying timer TX twice or malformed TLV data
                if (wantTimeTx || (length != 0) || (!try_add_unsigned(sizeR, static_cast<uint32_t>(MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()))), maxResponseSize)))
                {
                    createResponseWithoutBody = true;
                }
                else
                {
                    wantTimeTx = true;
                }
                break;
            case DEVICEID_LAST_UPDATED_QTAG:
                if (wantLastUpdated || (length != 0) || (!try_add_unsigned(sizeR, static_cast<uint32_t>(MIN_TLV_SIZE + sizeof(decltype(tpmWatcher.getLastUpdated()))), maxResponseSize)))
                {
                    createResponseWithoutBody = true;
                }
                else
                {
                    wantLastUpdated = true;
                }
                break;
            default:
                // Not supported TLV record, skip
                break;
        }

        // Advance to next TLV record
        bodyQ                     += (MIN_TLV_SIZE + length);
        remainingTLVDataFromQuery -= (MIN_TLV_SIZE + length);
    }

    // If we didn't process all TLV data from the query body, then the query was malformed.
    // Therefore, we close the TCP connection, as we are likely out of sync.
    if (remainingTLVDataFromQuery > 0)
    {
        return -1;
    }

    // If we decide to create the response without body, adjust sizeR accordingly
    if (createResponseWithoutBody == true)
    {
        // Ignore the complete body of query and send response which consists of header and checksum
        sizeR = HEADER_SIZE + CHECKSUM_SIZE;
    }

    // Here we construct the reply message in the response buffer.
    uint8_t * const headerAndBodyR = reinterpret_cast<uint8_t *>(responseBuffer) ;

    // Create the header of response
    {
        for (int i = 0; i < 4; ++i)
        {
            headerAndBodyR[i] = static_cast<uint8_t>((sizeR >> (i * 8)) & 0xff);
        }
        headerAndBodyR[4] = MAGIC_TAG;
        headerAndBodyR[5] = MAGIC_LENGTH;
        for (int i = 6; i < 10; ++i)
        {
            headerAndBodyR[i] = MAGIC_BYTES[i-6];
        }
        headerAndBodyR[10] = VERSION_TAG;
        headerAndBodyR[11] = VERSION_LENGTH;
        for (int i = 12; i < 16; ++i)
        {
            headerAndBodyR[i] = VERSION_BYTES[i-12];
        }
        headerAndBodyR[16] = MESSAGE_TYPE_TAG;
        headerAndBodyR[17] = MESSAGE_TYPE_LENGTH;
        headerAndBodyR[18] = msgTypeValue;
        headerAndBodyR[19] = MESSAGE_FLAGS_TAG;
        headerAndBodyR[20] = MESSAGE_FLAGS_LENGTH;
        headerAndBodyR[21] = msgFlagsValueR;

        // We use static assert to make sure that HEADER_SIZE is correct.
        // If the header is changed, we want to know, since this code needs to be updated then.
        static_assert(HEADER_SIZE == 22, "HEADER_SIZE value is incorrect");
    }
    // We added the header so far. Record these bytes.
    uint16_t completeSizeR = HEADER_SIZE;

    //Create the body of response
    if (createResponseWithoutBody == false)
    {
        if ((userData.size() != 0) && ((completeSizeR + MIN_TLV_SIZE + userData.size()) < MAX_MSG_SIZE))
        {
            headerAndBodyR[completeSizeR] = USER_DATA_RTAG;
            headerAndBodyR[completeSizeR+1] = userData.size();
            for (int i = 0; i < userData.size(); ++i)
            {
                headerAndBodyR[completeSizeR+MIN_TLV_SIZE+i] = userData[i];
            }
            completeSizeR += MIN_TLV_SIZE + userData.size();
        }
        if (wantDeviceID && ((completeSizeR + MIN_TLV_SIZE + TPMWatcher::deviceIDLength) < MAX_MSG_SIZE))
        {
            headerAndBodyR[completeSizeR] = DEVICEID_RTAG;
            headerAndBodyR[completeSizeR+1] = TPMWatcher::deviceIDLength;
		    for (int i = 0; i < TPMWatcher::deviceIDLength; ++i)
            {
                headerAndBodyR[completeSizeR+MIN_TLV_SIZE+i] = devID[i];
            }
            completeSizeR += MIN_TLV_SIZE + TPMWatcher::deviceIDLength;
        }
        if (wantTimeRx && ((completeSizeR + MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()))) < MAX_MSG_SIZE))
        {
            headerAndBodyR[completeSizeR] = TIMER_RX_RTAG;
            headerAndBodyR[completeSizeR+1] = sizeof(decltype(monotonicTimer.get()));
            toLE64(timerRx, &headerAndBodyR[completeSizeR+MIN_TLV_SIZE]);
            completeSizeR += MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()));
        }
        if (wantTimeTx && ((completeSizeR + MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()))) < MAX_MSG_SIZE))
        {
            headerAndBodyR[completeSizeR] = TIMER_TX_RTAG;
            headerAndBodyR[completeSizeR+1] = sizeof(decltype(monotonicTimer.get()));
            const uint64_t timerTx = monotonicTimer.get();
            toLE64(timerTx, &headerAndBodyR[completeSizeR+MIN_TLV_SIZE]);
            completeSizeR += MIN_TLV_SIZE + sizeof(decltype(monotonicTimer.get()));
        }
        if (wantLastUpdated && ((completeSizeR + MIN_TLV_SIZE + sizeof(decltype(tpmWatcher.getLastUpdated()))) < MAX_MSG_SIZE))
        {
            headerAndBodyR[completeSizeR] = DEVICEID_LAST_UPDATED_RTAG;
            headerAndBodyR[completeSizeR+1] = sizeof(decltype(tpmWatcher.getLastUpdated()));
            toLE64(tpmWatcher.getLastUpdated(), &headerAndBodyR[completeSizeR+MIN_TLV_SIZE]);
            completeSizeR += MIN_TLV_SIZE + sizeof(decltype(tpmWatcher.getLastUpdated()));
        }
    }

    // Calculate checksum
    std::vector<uint8_t> mdValueR;
    if ((completeSizeR + CHECKSUM_SIZE) > MAX_MSG_SIZE) {
        // No space left for the checksum, this invalidates the whole message.
        return -1;
    }
    uint8_t *checksumR = (uint8_t *)(responseBuffer + completeSizeR);
    if(!computeMD5(responseBuffer, completeSizeR, mdValueR))
    {
        return -1;
    }

    // Copy checksum into response, possibly XORed with device ID
    for (int i = 0; i < CHECKSUM_SIZE; ++i)
    {
        if (msgFlagsValueR == HAS_CHECKSUM_XOR_DEVICE_ID)
        {
             checksumR[i] = mdValueR[i] ^ devID[i]; //XOR with deviceID
        }
        else
        {
             checksumR[i] = mdValueR[i];
        }
    }

    // Record that we added the checksum
    completeSizeR += CHECKSUM_SIZE;

    // Success
    return completeSizeR;
}
