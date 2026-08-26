<!--
SPDX-FileCopyrightText: Copyright DB InfraGO AG
SPDX-License-Identifier: Apache-2.0
-->

# Purpose

This document describes the communication protocol that serves as the
interface between the Native Hardware Access component and its partner.

[[_TOC_]]

# Technical contents

## Introduction

The NHA TLV protocol is used for query-response communication. Messages
are based on SIMPLE-TLV according to ISO/IEC 7816-4, with two
differences:

-   Each message starts with its total size in bytes

-   Each message ends with a checksum

The byte-order of messages is little-endian, unless explicitly defined
otherwise on a per-attribute basis (e.g. UUIDs in RFC4122).

The communication between the NHA and its clients is assumed to use a connection-oriented protocol for the ISO/OSI transport layer, e.g. TCP according to RFC 9293.
A connection and its identification are defined by the protocol. At least TCP must be supported.

A message is composed of the following sections in the following order:

-   The mandatory static header

-   A sequence of Tag-Length-Value (**TLV**) records

-   The mandatory checksum

The device identification in this protocol shall refer to an identification of the physical device, on which the NHA is executed.
If two physical items are driven by the same time sources (e.g., quartz oscillators or phase-locked loops (PLLs)), then they are parts of the same device.
The device identification shall be obtained from a physical trusted platform module (TPM). A physical device shall not have more than one TPM.
A software-based TPM (aka. soft-TPM, virtual TPM, or vTPM) is not permitted to be used to obtain an device identification.

## Header specification

Each message shall start with the following mandatory common header, in
this exact order:

<table width="100%">
<colgroup>
<col style="width: 14%" />
<col style="width: 18%" />
<col style="width: 20%" />
<col style="width: 46%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td colspan="4" align="center"><strong><em>NHAHeader</em> (Little Endian) – 22
Bytes</strong></td>
</tr>
</thead>
<tbody>
<tr class="odd" bgcolor="lightgrey">
<td><strong>Byte index</strong></td>
<td><strong>Tag (1 byte)</strong></td>
<td><strong>Length (1 byte)</strong></td>
<td><strong>Value (Length bytes)</strong></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>0</strong></td>
<td colspan="3" align="center"><em>completeSizeOfMsgInBytes</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>4</strong></td>
<td><em>0x00</em></td>
<td><em>4</em></td>
<td>Magic string ‘N’ ‘H’ ‘A’ ‘1’</td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>10</strong></td>
<td><em>0x01</em></td>
<td><em>4</em></td>
<td><em>Version</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>16</strong></td>
<td><em>0x02</em></td>
<td><em>1</em></td>
<td><em>Message type</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>19</strong></td>
<td><em>0x03</em></td>
<td><em>1</em></td>
<td><em>Message flags</em></td>
</tr>
</tbody>
</table>

<center>Table 1 Mandatory common header</center>

### Attribute completeSizeOfMsgInBytes

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Size of the complete message including the size of this variable
itself.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT32</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>32 bits</td>
</tr>
<tr class="odd">
<td><strong>Possible values</strong></td>
<td>38 ≤ n ≤ 32768</td>
</tr>
<tr class="even">
<td><strong>Recommendation</strong></td>
<td>For performance aspects it is recommended not to exceed 1.2 kb per
message, to be sure not to exceed the maximum size of an Ethernet frame.
With this limit, the message does not need to be split and being sent in
different low-level-frames.</td>
</tr>
</tbody>
</table>

### Attribute Magic string

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>String literal “NHA1” in ASCII encoding.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8 [4]</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>32 bits</td>
</tr>
<tr class="odd">
<td><strong>Byte value</strong></td>
<td>0x4e 0x48 0x41 0x31</td>
</tr>
</tbody>
</table>

### Attribute Version

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Version of the protocol.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT32</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>32 bits</td>
</tr>
<tr class="odd">
<td><strong>Usage</strong></td>
<td><p>The 4 bytes of this value can be interpreted as a subset of
Semantic Versioning 2.0.0. The Major version is the most-significant
byte. The Minor version is the second most significant byte. The Patch
value is the two least significant bytes.</p>
<p>Example: 0x00 0x00 0x02 0x01 = 0x01020000 for v1.2.0. </p></td>
</tr>
</tbody>
</table>

### Attribute Message type

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Designated type of the message.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>8 bits</td>
</tr>
<tr class="odd">
<td><strong>Currently defined values</strong></td>
<td><table width="100%">
<colgroup>
<col style="width: 13%" />
<col style="width: 86%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td>Value</td>
<td>Meaning</td>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0x00</td>
<td>Forbidden.</td>
</tr>
<td>0x01</td>
<td>Standard message.</td>
</tr>
<tr class="even">
<td>0x02-0xFF</td>
<td>Reserved for future use, currently unused.</td>
</tr>
</tbody>
</table></td>
</tr>
</tbody>
</table>

### Attribute Message flags

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Flags that apply to this message</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>8 bits</td>
</tr>
<tr class="odd">
<td><strong>Currently defined values</strong></td>
<td><table width="100%">
<colgroup>
<col style="width: 2%" />
<col style="width: 49%" />
<col style="width: 47%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td>Value</td>
<td>Symbol</td>
<td>Meaning</td>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>0x00</td>
<td>None</td>
<td>No flags.</td>
</tr>
<tr class="even">
<td>0x01</td>
<td>HAS_CHECKSUM_XOR_DEVICE_ID</td>
<td>Indicates that this message has its checksum XOR’ed with the “Device
ID”.</td>
</tr>
<tr class="odd">
<td>0x02</td>
<td>REPLY_CHECKSUM_XOR_DEVICE_ID</td>
<td>Requests that the reply will be created with
HAS_CHECKSUM_XOR_DEVICE_ID.</td>
</tr>
</tbody>
</table></td>
</tr>
</tbody>
</table>

## Message body

The above header is followed by a sequence of Tag-Length-Value records.
The following rules apply:

-   TLV records are processed in order, as records may depend on each
    other

-   TLV records that are a response to a query shall be present in the
    response first and have the same order as they were in the query.
    Other TLV records may appear in any order afterwards.

-   Multiple TLV records with the same tag are not permitted within one
    message

-   Unsupported TLV records can simply be ignored by the receiver

As a convention, a tag with the most significant bit (value 0x80) set is
a query for information, while a cleared bit indicates a response. A
query typically has the length set to zero, unless there are query
details to be given.

In case a participant cannot fulfil a request, it must still respond
with a message containing at least the header and the MD5. In case of an
individual TLV record, if for whatever reason the data cannot be
provided, then the entire TLV record will be omitted from the response.

<table width="100%">
<colgroup>
<col style="width: 16%" />
<col style="width: 18%" />
<col style="width: 21%" />
<col style="width: 44%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td><strong>Must be supported</strong></td>
<td><strong>Tag (1 byte)<br />
(Query tag)</strong></td>
<td><strong>Length (1 byte)</strong></td>
<td><strong>Value (Length bytes)</strong></td>
</tr>
</thead>
<tbody>
<tr class="odd">
<td>Yes</td>
<td><em>0x04</em></td>
<td><em>1..255</em></td>
<td><em>User data</em></td>
</tr>
<tr class="even">
<td>Yes</td>
<td><em>0x05</em></td>
<td><em>1..255</em></td>
<td><em>User data mirrored</em></td>
</tr>
<tr class="odd">
<td>No</td>
<td><em>0x10 (0x90)</em></td>
<td><em>8</em></td>
<td><em>Capabilities</em></td>
</tr>
<tr class="even">
<td>Yes</td>
<td><em>0x11 (0x91)</em></td>
<td><em>16</em></td>
<td><em>Device-ID</em></td>
</tr>
<tr class="odd">
<td>Yes</td>
<td><em>0x12 (0x92)</em></td>
<td><em>8</em></td>
<td><em>Monotonic timer RX</em></td>
</tr>
<tr class="even">
<td>Yes</td>
<td><em>0x13 (0x93)</em></td>
<td><em>8</em></td>
<td><em>Monotonic timer TX</em></td>
</tr>
<tr class="odd">
<td>Yes</td>
<td><em>0x14 (0x94)</em></td>
<td><em>8</em></td>
<td><em>Device ID last update</em></td>
</tr>
<tr class="even">
<td>No</td>
<td><em>0x20 (0xA0)</em></td>
<td><em>1..255</em></td>
<td><em>Temperatures (example)</em></td>
</tr>
</tbody>
</table>

<center>Table 2 Possible TLV</center>
records in message body

### Attribute User data

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Arbitrary user data.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8 [ ]</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>8..2040 bits</td>
</tr>
<tr class="odd">
<td><strong>Usage</strong></td>
<td>This data is to be mirrored in the reply to this message in the
“User data mirrored” value field.</td>
</tr>
</tbody>
</table>

### Attribute User data mirrored

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Arbitrary user data, mirrored from the original request.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8 [ ]</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>8..2040 bits</td>
</tr>
<tr class="odd">
<td><strong>Usage</strong></td>
<td>When constructing a reply message to a previous request that
contained the “User data” attribute, the value of said record must be
copied to this “User data mirrored” value field.</td>
</tr>
</tbody>
</table>

### Attribute Capabilities

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>A bitmask that specifies whether pre-defined flags are true (1) or false (0). Defaults to 0, meaning all flags are false.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT64</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>64 bits</td>
</tr>
<tr class="odd">
<td><strong>Possible flags</strong></td>
<td>No flags are defined as of yet.</td>
</tr>
</tbody>
</table>

### Attribute Device-ID

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Identification of the physical device.</td>
</tr>
<tr class="odd">
<td><strong>Data Type</strong></td>
<td>UINT8 [16]</td>
</tr>
<tr class="even">
<td><strong>Width</strong></td>
<td>128 bits</td>
</tr>
<tr class="odd">
<td><strong>Usage</strong></td>
<td><p>The Device-ID shall be obtained from the Trusted Platform Module.
The Device-ID shall be derived from the TPM Name of the endorsement
primary key as follows:</p>
<ol>
<li><p>Create the endorsement primary key in the TPM endorsement
hierarchy using the standard RSA 2048 EK template.</p></li>
<li><p>Read the TPM Name of this key using the SHA-256 hash algorithm.
The TPM Name is the hash of the key's public area, prefixed with a
2-byte algorithm identifier (e.g. 0x000B for SHA-256).</p></li>
<li><p>Strip the 2-byte algorithm identifier prefix if present
(0x00 0x0B for SHA-256), leaving 32 bytes of hash data.</p></li>
<li><p>XOR the first 16 bytes with the second 16 bytes to produce
the 16-byte Device-ID.</p></li>
</ol>
Note: The correct implementation can manually be verified by calling <code>sudo tpm2_readpublic -c 0x81010001</code> and processing the value of the output's "name:"-line as done in step 3 and 4 of the list above.
</td>
</tr>
</tbody>
</table>

### Attribute Monotonic timer RX

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Timestamp of message receipt, in nanosecond resolution and at least
millisecond precision. The timer is strictly monotonic increasing,
its start value is undefined, and the start value may change from connection to connection.</td>
</tr>
<tr class="odd">
<td><strong>Constraints</strong></td>
<td>The timer shall not be affected by adjustments, such as leap
seconds.</td>
</tr>
<tr class="even">
<td><strong>Data Type</strong></td>
<td>UINT64</td>
</tr>
<tr class="odd">
<td><strong>Width</strong></td>
<td>64 bits</td>
</tr>
</tbody>
</table>

### Attribute Monotonic timer TX

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Timestamp of the last possible moment before message transmission,
in nanosecond resolution and at least millisecond precision. The timer
is strictly monotonic increasing, its start value is undefined, and the start value may change from connection to connection.</td>
</tr>
<tr class="odd">
<td><strong>Constraints</strong></td>
<td>The timer shall not be affected by adjustments, such as leap
seconds.</td>
</tr>
<tr class="even">
<td><strong>Data Type</strong></td>
<td>UINT64</td>
</tr>
<tr class="odd">
<td><strong>Width</strong></td>
<td>64 bits</td>
</tr>
<tr class="even">
<td><strong>Recommendation</strong></td>
<td>Since the final message will require a checksum calculation before
it can be transmitted, this record will not contain the real
transmission time. To minimize the difference, take this timestamp
directly before checksum calculation.</td>
</tr>
</tbody>
</table>

### Attribute Device ID last update

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>Timestamp of the device ID last update in nanoseconds, queried from the same monotonic timer that provides for all clients.</td>
</tr>
<tr class="odd">
<td><strong>Constraints</strong></td>
<td>The timer shall not be affected by adjustments, such as leap
seconds.</td>
</tr>
<tr class="even">
<td><strong>Data Type</strong></td>
<td>UINT64</td>
</tr>
<tr class="odd">
<td><strong>Width</strong></td>
<td>64 bits</td>
</tr>
</tbody>
</table>

### Attribute Temperatures (example)

<table width="100%">
<colgroup>
<col style="width: 23%" />
<col style="width: 76%" />
</colgroup>
<tbody>
<tr class="even">
<td><strong>Meaning</strong></td>
<td>This is an example record to illustrate how extensions to the
available definitions can look like.</td>
</tr>
<tr class="odd">
<td><strong>Width</strong></td>
<td>8..2040 bits</td>
</tr>
</tbody>
</table>

## Checksum

To protect against corruption, the TLV records shall be followed by an
MD5 checksum of the message bytes.

If requested via the appropriate message flag, the bit pattern of the
response checksum MD5 is XORed with the Device-ID of the physical device
on which the responding software is executed. This way, the requester
can check if the responder has the expected Device-ID. Otherwise, the
MD5 will be unusable.

Note that the choice of MD5 is deliberate, since fast computation of this
hash is possible on modern processors and it has a better quality than e.g.
a CRC. The checksum is not intended as a security measure. There is no
authentication in this protocol and any attacker being able to modify
exchanged data can do so, independent of the algorithm chosen to protect
against data corruption.

## Example communication

The following is an example query-response message pair.

<table width="100%">
<colgroup>
<col style="width: 14%" />
<col style="width: 38%" />
<col style="width: 47%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td><strong>Byte index</strong></td>
<td><strong>Bytes</strong></td>
<td><strong>Decoded</strong></td>
</tr>
</thead>
<tbody>
<tr class="odd">
<td bgcolor="lightgrey"><strong>0</strong></td>
<td><em>0x30</em> <em>0x00 0x00 0x00</em></td>
<td><em>completeSizeOfMsg=48</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>4</strong></td>
<td><em>0x00 0x04 0x4e 0x48 0x41 0x31</em></td>
<td><em>“NHA1”</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>10</strong></td>
<td><em>0x01 0x04</em> <em>0x00 0x00 0x00 0x01</em></td>
<td><em>version=1.0.0</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>16</strong></td>
<td><em>0x02 0x01 0x01</em></td>
<td><em>standard message</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>19</strong></td>
<td><em>0x03 0x01 0x02</em></td>
<td><em>flags: REPLY_CHECKSUM_XOR_DEVICE_ID</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>22</strong></td>
<td><em>0x04 0x02 0xAB 0xCD</em></td>
<td><em>user data = 0xAB 0xCD</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>26</strong></td>
<td><em>0x91 0x00</em></td>
<td><em>Query: Device-ID</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>28</strong></td>
<td><em>0x92 0x00</em></td>
<td><em>Query: Monotonic timer RX</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>30</strong></td>
<td><em>0x93 0x00</em></td>
<td><em>Query: Monotonic timer TX</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>32</strong></td>
<td><em>0x94 0x00</em></td>
<td><em>Query: Device ID last updated</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>34</strong></td>
<td><em>&lt;16 bytes&gt;</em></td>
<td><em>MD5</em></td>
</tr>
</tbody>
</table>

<center>Table 3 Example request</center>

<table width="100%">
<colgroup>
<col style="width: 14%" />
<col style="width: 38%" />
<col style="width: 47%" />
</colgroup>
<thead>
<tr class="header" bgcolor="lightgrey">
<td><strong>Byte index</strong></td>
<td><strong>Bytes</strong></td>
<td><strong>Decoded</strong></td>
</tr>
</thead>
<tbody>
<tr class="odd">
<td bgcolor="lightgrey"><strong>0</strong></td>
<td><em>0x50</em> <em>0x00 0x00</em> <em>0x00</em></td>
<td><em>completeSizeOfMsg=80</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>4</strong></td>
<td><em>0x00 0x04 0x4e 0x48 0x41 0x31</em></td>
<td><em>“NHA1”</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>10</strong></td>
<td><em>0x01 0x04</em> <em>0x00 0x00 0x00 0x01</em></td>
<td><em>version=1.0.0</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>16</strong></td>
<td><em>0x02 0x01 0x01</em></td>
<td><em>standard message</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>19</strong></td>
<td><em>0x03 0x01 0x01</em></td>
<td><em>flags: HAS_CHECKSUM_XOR_DEVICE_ID</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>22</strong></td>
<td><em>0x05 0x02 0xAB 0xCD</em></td>
<td><em>mirrored user data = 0xAB 0xCD</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>26</strong></td>
<td><em>0x11 0x10 0xA0 0xA1 0xA2 0xA3 0xA4 0xA5 0xA6 0xA7 0xA8 0xA9 0xAA
0xAB 0xAC 0xAD 0xAE 0xAF</em></td>
<td><em>Device-ID=0xA0A1A2A3A4A5…</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>44</strong></td>
<td><em>0x12 0x08 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08</em></td>
<td><em>Monotonic timer RX=0x807060504030201</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>54</strong></td>
<td><em>0x13 0x08 0x02 0x02 0x03 0x04 0x05 0x06 0x07 0x08</em></td>
<td><em>Monotonic timer TX=0x807060504030202</em></td>
</tr>
<tr class="even">
<td bgcolor="lightgrey"><strong>64</strong></td>
<td><em>0x14 0x08 0x02 0x02 0x03 0x04 0x05 0x06 0x07 0x08</em></td>
<td><em>Device ID last updated=0x807060504030200</em></td>
</tr>
<tr class="odd">
<td bgcolor="lightgrey"><strong>74</strong></td>
<td><em>&lt;16 bytes&gt;</em></td>
<td><em>MD5</em> <em>XOR Device-ID</em></td>
</tr>
</tbody>
</table>

<center>Table 4 Example response</center>
