// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef MESSAGEBUILDER_H
#define MESSAGEBUILDER_H

#include "tpmwatcher.h"
#include "monotonictimer.h"

#include <vector>

class MessageBuilder
{
public:
    /**
     * @brief Constructor
     * @param[in]  tpmWatcher   Reference to TPMWatcher instance that is used for getting device ID.
     *                          Must outlive this object!
     */
    MessageBuilder(TPMWatcher &tpmWatcher, MonotonicTimer &monotonicTimer);

    /**
     * @brief Create a response to a given query.
     * @param[in]  query            Buffer containing the query
     * @param[in]  querySize        Size of the query in bytes
     * @param[out] responseBuffer   Buffer where the response is to be stored
     * @param[in]  maxResponseSize  Maximum allowed size of response (e.g. size of the \p response buffer) in bytes
     * @return                      Size of the response in bytes, or -1 if an error occurred
     */
    int createResponse(const char *query, int querySize, char *responseBuffer, uint32_t maxResponseSize);

private:
    /**
     * @brief converter
     * @param[in]  array  4 element unsigned byte array
     * @return            An unsigned 32 bits integer          
     */
    static uint32_t fromLE32(const uint8_t *array);

    /**
     * @brief converter
     * @param[in]  num  An unsigned 64 bits integer   
     * @param[out] array  8 element unsigned byte array       
     */
    static void toLE64(uint64_t num, uint8_t *array);

    /**
     * @brief md5 checksum calculator
     * @param[in]  data    Pointer to the input data you want to hash 
     * @param[in]  length  Length of the input data in bytes        
     * @param[out] digest  Buffer to store the computed digest       
     */
    static bool computeMD5(const void* data, size_t length, std::vector<uint8_t>& digest);

    TPMWatcher     & tpmWatcher;
    MonotonicTimer & monotonicTimer;
};

#endif
