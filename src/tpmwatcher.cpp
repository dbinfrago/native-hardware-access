// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include "tpmwatcher.h"

#include <iostream>
#include <string.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_rc.h>
#include <tss2/tss2_tctildr.h>


TPMWatcher::TPMWatcher(unsigned intervalInMs, const MonotonicTimer &monotonicTimer)
    : intervalInMs(intervalInMs),
      monotonicTimer(monotonicTimer),
      deviceID{0},
      lastUpdated(0)
{
}

void TPMWatcher::get(uint8_t *deviceID)
{
    const std::lock_guard<std::mutex> lock(deviceIDLastUpdatedMutex);
    memcpy(deviceID, this->deviceID, deviceIDLength);
}

uint64_t TPMWatcher::getLastUpdated()
{
    const std::lock_guard<std::mutex> lock(deviceIDLastUpdatedMutex);
    return lastUpdated;
}

bool TPMWatcher::start()
{
    // Update once in the main thread so that immediately after return, get() can be called
    if (false == update())
    {
        // If the first attempt to update fails, do not start the timer.
        // Error already logged by update().
        return false;
    }

    timer.start(std::bind(&TPMWatcher::update, this), std::chrono::milliseconds(intervalInMs));
    return true;
}

// Forward declarations of some helper functions used by update()
static void logTpmError(TSS2_RC rc, const char *location);

#define FREE_TPM_RESOURCE(resource) \
if ((resource) != nullptr) \
{ \
    Esys_Free((resource)); \
    (resource) = nullptr; \
}

bool TPMWatcher::update()
{
    ESYS_CONTEXT *esysContext = nullptr;
    TSS2_TCTI_CONTEXT *tctiContext = nullptr;
    TSS2_RC rc = Tss2_TctiLdr_Initialize("device:/dev/tpmrm0", &tctiContext);
    if (rc != TSS2_RC_SUCCESS)
    {
        logTpmError(rc, "Tss2_TctiLdr_Initialize(device:/dev/tpmrm0)");
        return false;
    }

    rc = Esys_Initialize(&esysContext, tctiContext, nullptr);
    if (rc != TSS2_RC_SUCCESS)
    {
        logTpmError(rc, "Esys_Initialize");
        Tss2_TctiLdr_Finalize(&tctiContext);
        return false;
    }

    const TPM2B_SENSITIVE_CREATE inSensitive = {};
    TPM2B_PUBLIC inPublic = {};
    
    // Initialize the public key template
    inPublic.size = 0;
    inPublic.publicArea.type = TPM2_ALG_RSA;
    inPublic.publicArea.nameAlg = TPM2_ALG_SHA256;
    inPublic.publicArea.objectAttributes = (TPMA_OBJECT_FIXEDTPM |
                                          TPMA_OBJECT_FIXEDPARENT |
                                          TPMA_OBJECT_SENSITIVEDATAORIGIN |
                                          TPMA_OBJECT_ADMINWITHPOLICY |
                                          TPMA_OBJECT_RESTRICTED |
                                          TPMA_OBJECT_DECRYPT);
    
    // Set auth policy
    // This is the well-known SHA-256 policy digest for TPM2_PolicySecret(TPM_RH_ENDORSEMENT),
    // as specified in the TCG EK Credential Profile for TPM Family 2.0, Annex B.3.3
    // "Template L-1: RSA 2048 (Storage)".
    // See: https://trustedcomputinggroup.org/resource/tcg-ek-credential-profile-for-tpm-family-2-0/
    inPublic.publicArea.authPolicy.size = 32;
    const UINT8 authPolicyBuffer[] = {
        0x83, 0x71, 0x97, 0x67, 0x44, 0x84, 0xb3, 0xf8,
        0x1a, 0x90, 0xcc, 0x8d, 0x46, 0xa5, 0xd7, 0x24,
        0xfd, 0x52, 0xd7, 0x6e, 0x06, 0x52, 0x0b, 0x64,
        0xf2, 0xa1, 0xda, 0x1b, 0x33, 0x14, 0x69, 0xaa
    };
    for (size_t i = 0; i < 32; ++i)
    {
        inPublic.publicArea.authPolicy.buffer[i] = authPolicyBuffer[i];
    }
    
    // Set RSA parameters
    inPublic.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
    inPublic.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
    inPublic.publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
    inPublic.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
    inPublic.publicArea.parameters.rsaDetail.keyBits = 2048;
    inPublic.publicArea.parameters.rsaDetail.exponent = 0;
    
    // Set unique RSA field
    inPublic.publicArea.unique.rsa.size = 256;
    for (size_t i = 0; i < 256; ++i)
    {
        inPublic.publicArea.unique.rsa.buffer[i] = 0;
    }

    const TPM2B_DATA        outsideInfo    = {};
    const TPML_PCR_SELECTION creationPCR  = {};
    ESYS_TR           ekHandle      = ESYS_TR_NONE;
    TPM2B_PUBLIC      *outPublic    = nullptr;
    TPM2B_CREATION_DATA *creationData   = nullptr;
    TPM2B_DIGEST        *creationHash   = nullptr;
    TPMT_TK_CREATION    *creationTicket = nullptr;

    // Create the primary key in the endorsement hierarchy
    rc = Esys_CreatePrimary(
        esysContext,
        ESYS_TR_RH_ENDORSEMENT,
        ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE,
        &inSensitive, &inPublic, &outsideInfo, &creationPCR,
        &ekHandle,
        &outPublic, &creationData, &creationHash, &creationTicket
    );
    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_CreatePrimary");

        // cleanup TPM resources allocated before returning
        Esys_Finalize(&esysContext);
        Tss2_TctiLdr_Finalize(&tctiContext);
        
        return false;
    }

    // Read the Endorsement Key's TPM2B_NAME
    TPM2B_NAME *ekName = nullptr;
    TPM2B_NAME *ekQualifiedName = nullptr;
    
    rc = Esys_ReadPublic(esysContext, ekHandle, 
                        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                        nullptr, &ekName, &ekQualifiedName);
    
    if (rc != TSS2_RC_SUCCESS)
    {
        logTpmError(rc, "Esys_ReadPublic");
        
        // cleanup TPM resources allocated before returning
        FREE_TPM_RESOURCE(ekName)
        FREE_TPM_RESOURCE(ekQualifiedName)
        Esys_FlushContext(esysContext, ekHandle);
        FREE_TPM_RESOURCE(outPublic)
        FREE_TPM_RESOURCE(creationData)
        FREE_TPM_RESOURCE(creationHash)
        FREE_TPM_RESOURCE(creationTicket)
        Esys_Finalize(&esysContext);
        Tss2_TctiLdr_Finalize(&tctiContext);
        
        return false;
    }

    // Process the TPM2B_NAME according to specification
    const UINT8* const nameData = ekName->name;
    const size_t nameSize = ekName->size;
    size_t dataOffset = 0;
    
    // Check if first two bytes are 0x00 and 0x0B, if so skip them
    if ((nameSize >= 2) && ((nameData[0] == 0x00) && (nameData[1] == 0x0B)))
    {
        dataOffset = 2;
    }
    
    // Check if we have at least 32 bytes remaining after potential offset
    if ((nameSize - dataOffset) < 32)
    {
        logTpmError(TSS2_ESYS_RC_GENERAL_FAILURE, "Insufficient data in TPM2B_NAME for UUID generation");
        
        // cleanup TPM resources allocated before returning
        FREE_TPM_RESOURCE(ekName)
        FREE_TPM_RESOURCE(ekQualifiedName)
        Esys_FlushContext(esysContext, ekHandle);
        FREE_TPM_RESOURCE(outPublic)
        FREE_TPM_RESOURCE(creationData)
        FREE_TPM_RESOURCE(creationHash)
        FREE_TPM_RESOURCE(creationTicket)
        Esys_Finalize(&esysContext);
        Tss2_TctiLdr_Finalize(&tctiContext);

        return false;
    }

    // Copy result to cache and update last modified time
    {
        const std::lock_guard<std::mutex> lock(deviceIDLastUpdatedMutex);

        // Extract 32 bytes and create UUID by XORing first 16 with second 16
        const UINT8* const sourceData = &nameData[dataOffset];
    
        // XOR bytes 0-15 with bytes 16-31 to create the UUID
        for (size_t i = 0; i < 16; ++i)
        {
            deviceID[i] = sourceData[i] ^ sourceData[i + 16];
        }

        lastUpdated = monotonicTimer.get();
    }
    
    // cleanup TPM resources allocated before returning
    FREE_TPM_RESOURCE(ekName)
    FREE_TPM_RESOURCE(ekQualifiedName)
    Esys_FlushContext(esysContext, ekHandle);
    FREE_TPM_RESOURCE(outPublic)
    FREE_TPM_RESOURCE(creationData)
    FREE_TPM_RESOURCE(creationHash)
    FREE_TPM_RESOURCE(creationTicket)
    Esys_Finalize(&esysContext);
    Tss2_TctiLdr_Finalize(&tctiContext);


    return true;
}

static void logTpmError(TSS2_RC rc, const char *location)
{
    std::cerr << "ERROR: TPMWatcher::update(): error in " << location << std::endl;
    const char *decoded = Tss2_RC_Decode(rc);
    if (decoded == nullptr)
    {
        decoded = "(decode failed)";
    }
    std::cerr << "Error code: ";
    const auto oldFlags = std::cerr.setf(std::ios::hex, std::ios::basefield);
    const auto oldFill = std::cerr.fill('0');
    const auto oldWidth = std::cerr.width(8);
    std::cerr << rc;
    std::cerr.flags(oldFlags);
    std::cerr.fill(oldFill);
    std::cerr.width(oldWidth);
    std::cerr << ", error message: " << decoded << std::endl;
}
