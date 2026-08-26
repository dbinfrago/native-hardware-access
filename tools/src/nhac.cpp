// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#include <vector>
#include <map>
#include <cerrno>
#include <cstdint>
#include <string>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <optional>
#include <array>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>


extern "C"
{
#include "picohash.h"
}

class TLV
{
    unsigned char tag;
    std::vector<unsigned char> value;
public:
    size_t serialize(unsigned char *t, size_t n) const;
    size_t deserialize(const unsigned char *t, size_t n);
    std::string decode() const;
    unsigned char getTag() const { return tag; }
    unsigned char getLength() const { return value.size(); }
    const std::vector<unsigned char> & getValue() const { return value; }
    TLV() {}
    TLV(unsigned char tag, std::vector<unsigned char> && value)
        : tag(tag), value(value) {}
};

class Message
{
    std::vector<TLV> tlvs;
    typedef std::array<unsigned char, 16> md5_t;
    std::optional<md5_t> receivedMd5, calculatedMd5, xoredMd5;
public:
    uint32_t getCompleteSizeOfMsgInBytes() const;
    size_t serialize(unsigned char *t, size_t n);
    size_t deserialize(const unsigned char *t, size_t n);
    void print(std::ostream &os, const std::string &title) const;
    void add(TLV && tlv) { tlvs.push_back(tlv); }
    void clear() { tlvs.clear(); receivedMd5.reset(); calculatedMd5.reset(); xoredMd5.reset(); }
    static void md5(const unsigned char *in, size_t n, unsigned char *out);
};

std::string toHex(unsigned char x, bool withPrefix = true)
{
    static constexpr char xDigits[] = "0123456789abcdef";
    return std::string{withPrefix ? "0x" : ""} + xDigits[(x>>4)&0xf] + xDigits[x&0xf];
}

void toLE32(uint32_t x, unsigned char *t)
{
    t[0] = x         & 0xFF;
    t[1] = (x >>  8) & 0xFF;
    t[2] = (x >> 16) & 0xFF;
    t[3] = (x >> 24) & 0xFF;
}

uint32_t fromLE32(const unsigned char *t)
{
    return t[0] | (t[1] << 8) | (t[2] << 16) | (t[3] << 24);
}

std::string TLV::decode() const
{
    if (tag == 0x00 && value.size() == 4 &&
        value[0] == 'N' && value[1] == 'H' && value[2] == 'A' && value[3] == '1')
    {
        return "magic";
    }
    if (tag == 0x01 && value.size() == 4)
    {
        std::ostringstream oss;
        oss << "version " << static_cast<unsigned>(value[3]) << "." << static_cast<unsigned>(value[2]) << ".";
        oss << ((static_cast<unsigned>(value[1]) << 8) | static_cast<unsigned>(value[0]));
        return oss.str();
    }
    if (tag == 0x02 && value.size() == 1)
    {
        return std::string{"type "} + (value[0] == 0x01 ? "Standard" : "SPECIAL");
    }
    if (tag == 0x03 && value.size() == 1)
    {
        std::ostringstream oss;
        oss << "flags ";
        if (value[0] == 0x00) oss << "None";
        else if (value[0] == 0x01) oss << "HAS_CHECKSUM_XOR_DEVICE_ID";
        else if (value[0] == 0x02) oss << "REPLY_CHECKSUM_XOR_DEVICE_ID";
        else oss << "??";
        return oss.str();
    }
    if (tag == 0x04 || tag == 0x05)
    {
        std::ostringstream oss;
        oss << "user_data";
        if (tag == 0x05) oss << "_mirrored";
        if (!value.empty()) oss << " ";
        for (const unsigned char c: value) oss << toHex(c, false);
        return oss.str();
    }
    if ((tag & 0x7f) == 0x10 || (tag & 0x7f) == 0x20)
    {
        std::ostringstream oss;
        oss << ( ((tag & 0x7f) == 0x10) ? "capabilities" : "temperatures" );
        if ((tag & 0x80) == 0x80 && value.size() == 0) oss << " query";
        else if ((tag & 0x80) == 0x00)
        {
            if (!value.empty()) oss << " ";
            for (const unsigned char c: value) oss << toHex(c, false);
        }
        else return "";
        return oss.str();
    }
    if ((tag & 0x7f) == 0x11)
    {
        std::ostringstream oss;
        oss << "device_id";
        if ((tag & 0x80) == 0x80 && value.size() == 0) oss << " query";
        else if ((tag & 0x80) == 0x00 && value.size() == 16)
        {
            oss << " ";
            for (const unsigned char c: value) oss << toHex(c, false);
        }
        else return "";
        return oss.str();
    }
    if ((tag & 0x7f) == 0x12 || (tag & 0x7f) == 0x13)
    {
        std::ostringstream oss;
        oss << "monotonic_timer_"  << ( ((tag & 0x7f) == 0x12) ? "rx" : "tx" );
        if ((tag & 0x80) == 0x80 && value.size() == 0) oss << " query";
        else if ((tag & 0x80) == 0x00 && value.size() == 8)
        {
            oss << " ";
            const uint64_t t = (static_cast<uint64_t>(fromLE32(value.data() + 4)) << 32) |
                                static_cast<uint64_t>(fromLE32(value.data() + 0));
            oss << t;
        }
        else return "";
        return oss.str();
    }
    if ((tag & 0x7f) == 0x14)
    {
        std::ostringstream oss;
        oss << "device_id_last_updated";
        if ((tag & 0x80) == 0x80 && value.size() == 0) oss << " query";
        else if ((tag & 0x80) == 0x00 && value.size() == 8)
        {
            oss << " ";
            const uint64_t t = (static_cast<uint64_t>(fromLE32(value.data() + 4)) << 32) |
                                static_cast<uint64_t>(fromLE32(value.data() + 0));
            oss << t;
        }
        else return "";
        return oss.str();
    }
    return "";
}

uint32_t Message::getCompleteSizeOfMsgInBytes() const
{
    uint32_t completeSizeOfMsgInBytes = 0;
    completeSizeOfMsgInBytes += 4;  // completeSizeOfMsgInBytes
    for (const auto &tlv : tlvs)
    {
        completeSizeOfMsgInBytes += 2 + tlv.getLength();
    }
    completeSizeOfMsgInBytes += 16;  // MD5
    return completeSizeOfMsgInBytes;
}

size_t Message::serialize(unsigned char *t, size_t n)
{
    // get size
    const uint32_t completeSizeOfMsgInBytes = getCompleteSizeOfMsgInBytes();

    if (completeSizeOfMsgInBytes > n)
    {
        throw std::runtime_error(std::string{"Message::serialize: message size "} +
                                 std::to_string(completeSizeOfMsgInBytes) +
                                 " is too big, maximum " + std::to_string(n) + " bytes are allowed");
    }

    size_t pos = 0;

    // serialize size
    toLE32(completeSizeOfMsgInBytes, t + pos);
    pos += 4;

    // serialize TLVs
    for (const auto &tlv : tlvs)
    {
        try
        {
            pos += tlv.serialize(t + pos, n - pos);
        }
        catch (std::runtime_error &e)
        {
            throw std::runtime_error(std::string{"Message::serialize: could not serialize TLV at byte "} +
                                     std::to_string(pos) + " (completeSizeOfMessage=" +
                                     std::to_string(completeSizeOfMsgInBytes) + ") : " + e.what());
        }
    }

    // serialize MD5, and also store it for later printing
    md5(t, pos, t + pos);
    calculatedMd5.emplace();
    std::copy(t + pos, t + pos + 16, calculatedMd5.value().begin());
    receivedMd5.reset();
    xoredMd5.reset();

    pos += 16;

    return pos;
}

void Message::md5(const unsigned char *in, size_t n, unsigned char *out)
{
    assert(PICOHASH_MD5_DIGEST_LENGTH == 16);
    picohash_ctx_t ctx;
    picohash_init_md5(&ctx);
    picohash_update(&ctx, in, n);
    picohash_final(&ctx, out);
}

size_t TLV::serialize(unsigned char *t, size_t n) const
{
    if (n < 2 + value.size())
    {
        throw std::runtime_error(std::string{"TLV::serialize: output area with size "} + std::to_string(n) +
                                 " bytes is insufficient to serialize TLV with tag=" + std::to_string(tag) +
                                 " length=" + std::to_string(value.size()));
    }

    size_t pos = 0;

    t[pos++] = tag;
    t[pos++] = static_cast<unsigned char>(value.size());
    for (const auto c : value)
    {
        t[pos++] = c;
    }

    return pos;
}

size_t Message::deserialize(const unsigned char *t, size_t n)
{
    if (n < 4 + 16)  // completeSizeOfMsgInBytes + MD5
    {
        throw std::runtime_error("Message::deserialize: input of " + std::to_string(n) +
                                 " bytes is too small for a message (minimum message size is 20 bytes)");
    }

    size_t pos = 0;

    const uint32_t completeSizeOfMsgInBytes = fromLE32(t + pos);
    pos += 4;

    if (completeSizeOfMsgInBytes > n)
    {
        throw std::runtime_error(std::string{"Message::deserialize: message size is "} +
                                 std::to_string(completeSizeOfMsgInBytes) +
                                 " but only " + std::to_string(n) + " received");
    }

    if (completeSizeOfMsgInBytes < 4 + 16)  // completeSizeOfMsgInBytes + MD5
    {
        throw std::runtime_error(std::string{"Message::deserialize: invalid message size "} +
                                 std::to_string(completeSizeOfMsgInBytes) +
                                 ", minimum 20 required");
    }

    std::vector<TLV> newTlvs;

    const size_t endPos = completeSizeOfMsgInBytes - 16;
    while (pos < endPos)
    {
        try
        {
            TLV tlv;
            pos += tlv.deserialize(t + pos, endPos - pos);
            newTlvs.push_back(std::move(tlv));
        }
        catch (std::runtime_error &e)
        {
            throw std::runtime_error(std::string{"Message::deserialize: could not deserialize TLV at byte "} +
                            std::to_string(pos) + " (completeSizeOfMessage=" +
                            std::to_string(completeSizeOfMsgInBytes) + ") : " + e.what());
        }
    }

    // Store MD5 from message, calculate MD5, xor them
    receivedMd5.emplace();
    std::copy(t + pos, t + pos + 16, receivedMd5.value().begin() );
    calculatedMd5.emplace();
    md5(t, pos, calculatedMd5.value().data());
    xoredMd5.emplace();
    for (size_t i = 0; i < 16; ++i) xoredMd5.value()[i] = receivedMd5.value()[i] ^ calculatedMd5.value()[i];

    pos += 16;

    tlvs = std::move(newTlvs);
    return pos;
}

void Message::print(std::ostream &os, const std::string &title) const
{
    os << "Message=" << title;
    os << " completeSizeOfMsgInBytes=" << getCompleteSizeOfMsgInBytes() << std::endl;
    for (const TLV &tlv : tlvs)
    {
        os << "  T=" << toHex(tlv.getTag());
        os << " L="  << std::setw(3) << std::left << static_cast<unsigned>(tlv.getLength());
        os << " V=";
        bool first = true;   // to omit separator before first item
        for (const unsigned char c : tlv.getValue())
        {
            if (!first) os << " ";
            os << toHex(c);
            first = false;
        }
        os << std::endl;
        const std::string decodedTlv = tlv.decode();
        if (!decodedTlv.empty())
        {
            os << "    (" << decodedTlv << ")" << std::endl;
        }
    }

    if (receivedMd5.has_value())
    {
        os << "received   MD5=";
        for (const unsigned char c : receivedMd5.value()) os << toHex(c, false);
        os << std::endl;
    }

    if (calculatedMd5.has_value())
    {
        os << "calculated MD5=";
        for (const unsigned char c : calculatedMd5.value()) os << toHex(c, false);
        os << std::endl;
    }

    if (xoredMd5.has_value())
    {
        os << "xored      MD5=";
        for (const unsigned char c : xoredMd5.value()) os << toHex(c, false);
        os << std::endl;
    }

    os << std::endl;
}

size_t TLV::deserialize(const unsigned char *t, size_t n)
{
    if (n < 2)  // tag+length do not fit
    {
        throw std::runtime_error(std::string{"TLV::deserialize: input area with size "} + std::to_string(n) +
                                 " bytes is insufficient to store TLV tag and length");
    }

    const unsigned char tag = t[0];
    const unsigned char length = t[1];

    if (n < 2 + length)  // value does not fit
    {
        throw std::runtime_error(std::string{"TLV::deserialize: input area with size "} + std::to_string(n) +
                                 " bytes is insufficient to store value of TLV with tag=" +
                                 std::to_string(tag) + ", length=" +std::to_string(length));
    }

    this->tag = tag;
    this->value = std::vector<unsigned char>(t + 2, t + 2 + length);

    return 2 + length;
}

/* Functions for parsing message descriptions in  format
   { tag1(value1 value2 ...) tag2... }  */

std::string nextToken(std::istream &is)
{
    std::istreambuf_iterator<char> it(is);
    const std::istreambuf_iterator<char> end;

    while (true)
    {
        // skip whitespace
        it = std::find_if(it, end, [](char c) {return !isspace(c) && c != ',';} );
        if (it == end) return std::string();

        // skip comment
        if (*it == '#')
        {
            it = std::find(it, end, '\n');
            if (it == end) return std::string();
        }
        else
        {
            break;
        }
    }

    if (*it == '"')
    {
        // string token up to closing quotes
        std::string token{*it++};
        while (it != end && *it != '"' && *it != '\n') token.push_back(*it++);
        // also include closing quotes
        if (it != end && *it == '"') token.push_back(*it++);
        return token;
    }
    if (isalnum(*it))
    {
        // value or identifier token
        std::string token{*it++};
        while (it != end && isalnum(*it)) token.push_back(*it++);
        return token;
    }
    else
    {
        // single character token: parenthesis etc.
        return std::string{*it++};
    }
}

bool parse(Message &msg, std::istream &is)
{
    static const std::map<std::string, unsigned char> tagAliases {
        { "m",                      0x00 },
        { "magic",                  0x00 },
        { "v",                      0x01 },
        { "version",                0x01 },
        { "t",                      0x02 },
        { "message_type",           0x02 },
        { "f",                      0x03 },
        { "message_flags",          0x03 },
        { "u",                      0x04 },
        { "user_data",              0x04 },
        { "c",                      0x90 },
        { "capabilities",           0x90 },
        { "d",                      0x91 },
        { "device_id",              0x91 },
        { "rx",                     0x92 },
        { "monotonic_timer_rx",     0x92 },
        { "tx",                     0x93 },
        { "monotonic_timer_tx",     0x93 },
        { "l",                      0x94 },
        { "deviceid_last_updated",  0x94 },
        { "e",                      0xa0 },
        { "temperatures",           0xa0 }
    };

    msg.clear();

    std::string token = nextToken(is);

    if (token.empty())
    {
        return false;
    }

    if (token != "{")
    {
        throw std::runtime_error(std::string{"Syntax error: unexpected token '"}+token+"', expected: '{'");
    }

    token = nextToken(is);
    while (token != "}")
    {
        if (token.empty())
        {
            throw std::runtime_error("Syntax error: unexpected end of input, expected: tag identifier or '}'");
        }

        if (token == "{" || token == "(" || token == ")")
        {
            throw std::runtime_error(std::string{"Syntax error: unexpected token '"}+token+"', expected: tag identifier or '}'");
        }
        else
        {
            std::string tagId = token;
            std::vector<unsigned char> args;

            token = nextToken(is);
            if (token == "(")
            {
                token = nextToken(is);
                while (!token.empty() && token != ")")
                {
                    if (token.size() == 4 &&
                        token[0] == '0' && toupper(token[1]) == 'X' && isxdigit(token[2]) && isxdigit(token[3]))
                    {
                        args.push_back(static_cast<unsigned char>(std::stoi(token, nullptr, 16)));
                    }
                    else if (token.size() >= 1 && token[0] == '"')
                    {
                        if (token.size() < 2 || token.back() != '"')
                        {
                            throw std::runtime_error(std::string{"Syntax error: missing closing quotes for string "}+token);
                        }
                        for (const char &c: token.substr(1, token.size()-2))
                        {
                            args.push_back(static_cast<unsigned char>(c));
                        }
                    }
                    else if ((isdigit(token.back()) || toupper(token.back()) == 'L') &&
                             std::all_of(token.begin(), token.end()-1, isdigit))
                    {
                        int nBytes = (isdigit(token.back()) ? 4 : 8);
                        uint64_t num = std::stoull(token);
                        for (int i = 0; i < nBytes; i++)
                        {
                            args.push_back(static_cast<unsigned char>(num & 0xFF));
                            num = num >> 8;
                        }
                    }
                    else
                    {
                        throw std::runtime_error(std::string{"Syntax error: unexpected token '"}+token+"', expected: value or ')'");
                    }
                    token = nextToken(is);
                }
                if (token.empty())
                {
                    throw std::runtime_error("Syntax error: unexpected end of input, expected: value or ')'");
                }

                // next token after ')'
                token = nextToken(is);
            }

            if (tagId == "H" || tagId == "Header")
            {
                if (args.empty()) args = {0x00};
                // Construct header
                msg.add({0x00, {0x4e, 0x48, 0x41, 0x31}});
                msg.add({0x01, {0x00, 0x00, 0x00, 0x01}});
                msg.add({0x02, {0x01}});
                msg.add({0x03, std::move(args)});
            }
            else
            {
                unsigned char tag;

                // decode tag identifier
                if (tagId.size() == 4 &&
                    tagId[0] == '0' && toupper(tagId[1]) == 'X' && isxdigit(tagId[2]) && isxdigit(tagId[3]))
                {
                    tag = static_cast<unsigned char>(std::stoi(tagId, nullptr, 16));
                }
                else if (tagAliases.find(tagId) != tagAliases.end())
                {
                    tag = tagAliases.at(tagId);
                }
                else
                {
                    throw std::runtime_error(std::string{"Syntax error: unexpected token '"}+tagId+"', expected: tag identifier or '}'");
                }

                msg.add({tag, std::move(args)});
            }
        }
    }

    return true;
}

int connectToNha(const char *ip, const char *port)
{
    struct addrinfo serverAddr, *addrinfoList = nullptr;

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.ai_family = AF_UNSPEC;
    serverAddr.ai_socktype = SOCK_STREAM;

    const int addrinfoResult = getaddrinfo(ip, port, &serverAddr, &addrinfoList);
    if (addrinfoResult != 0)
        throw std::runtime_error(std::string{"connectToNha: getaddrinfo (ip="} + ip + ", port=" + port + "): " +
                                 gai_strerror(addrinfoResult));

    serverAddr = *addrinfoList;
    freeaddrinfo(addrinfoList);

    int sockfd = socket(serverAddr.ai_family, serverAddr.ai_socktype, serverAddr.ai_protocol);
    if (-1 == sockfd)
        throw std::runtime_error(std::string{"connectToNha: socket (ip="} + ip + ", port=" + port +"): " +
                                 strerror(errno));

    if (-1 == connect(sockfd, serverAddr.ai_addr, serverAddr.ai_addrlen))
        throw std::runtime_error(std::string{"connectToNha: connect: (ip="} + ip + ", port=" + port +"): " +
                                 strerror(errno));

    return sockfd;
}

void disconnectFromNha(int &sockfd)
{
    if (sockfd >= 0) close(sockfd);
    sockfd = -1;
}

void sendQuery(int sockfd, const unsigned char *t, size_t n)
{
    int res = send(sockfd, t, n, 0);
    while (res != -1 && res != n)
    {
        t += res;
        n -= res;
        res = send(sockfd, t, n, 0);
    }
    if (res == -1) throw std::runtime_error(std::string{"sendQuery: send: "} + strerror(errno));
}

size_t receiveResponse(int sockfd, unsigned char *t, size_t n)
{
    int res = recv(sockfd, t, 4, 0);
    if (res == -1) throw std::runtime_error(std::string{"receiveResponse: recv: "} + strerror(errno));
    else if (res != 4) throw std::runtime_error(std::string{"receiveResponse: recv: failed to receive size of message"});
    const size_t completeSizeOfMsgInBytes = fromLE32(t);
    n = std::min(n, completeSizeOfMsgInBytes) - 4;  // size already received
    t += 4;  // size already received
    res = recv(sockfd, t, n, 0);
    while (res != -1 && res != 0 && res != n)
    {
        t += res;
        n -= res;
        res = recv(sockfd, t, n, 0);
    }
    if (res == -1) throw std::runtime_error(std::string{"receiveResponse: recv: "} + strerror(errno));
    else if (res == 0) throw std::runtime_error(std::string{"receiveResponse: recv: connection closed"});
    return completeSizeOfMsgInBytes;
}

void usage()
{
    std::cerr << "usage: nhac [<options>] <ip> <port> [<query-description>]" << std::endl << std::endl;
    std::cerr << "Connect to NHA server at <ip> : <port>, send queries and print responses." << std::endl;
    std::cerr << "If <query-description> is missing, the query description will be read from" << std::endl;
    std::cerr << "standard input. For detailed description of the query description format," << std::endl;
    std::cerr << "use the switch -H." << std::endl << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -h    print this help and exit" << std::endl;
    std::cerr << "  -H    print help on query description format and exit" << std::endl;
    std::cerr << "  -q    quiet mode; do not print details of query" << std::endl;
    std::cerr << "  -j    output in JSON format (not implemented!)" << std::endl;
}

void formatUsage()
{
    std::cerr << "The query description consists of a series of messages," << std::endl;
    std::cerr << "where each message is a brace-enclosed expression in the following format:" << std::endl << std::endl;
    std::cerr << "    { <tag>(<value>) <tag>(<value>) ... }" << std::endl << std::endl;
    std::cerr << "A series of any number of whitespace and/or comma (',') characters" << std::endl;
    std::cerr << "delimit tokens. A number sign ('#') denotes a comment up to the end of line." << std::endl << std::endl;
    std::cerr << "The binary query message will contain the TLV records given as these" << std::endl;
    std::cerr << "<tag>(<value>) items, in the given order." << std::endl << std::endl;
    std::cerr << "<tag> is either the hexadecimal value of the tag byte as '0x' followed by two" << std::endl;
    std::cerr << "hex digits, case insensitive (e.g. 0x04 or 0XaB) or one of the mnemonics" << std::endl;
    std::cerr << "described below (see 'Tag Aliases:')." << std::endl << std::endl;
    std::cerr << "<value> is a series of value-parts, where each part is one of the following:" << std::endl;
    std::cerr << "  byte:           given as a two-digit hexadecimal in the same format as" << std::endl;
    std::cerr << "                  for the tag (see above), e.g. 0x02" << std::endl;
    std::cerr << "  4-byte" << std::endl;
    std::cerr << "  little-endian:  given as a decimal value, e.g. 64000" << std::endl;
    std::cerr << "  8-byte" << std::endl;
    std::cerr << "  little-endian:  given as a decimal value suffixed by 'l' or 'L'," << std::endl;
    std::cerr << "                  e.g. 578437695752307201L" << std::endl;
    std::cerr << "  string without" << std::endl;
    std::cerr << "  terminator:     given as a double-quote-delimited series of characters," << std::endl;
    std::cerr << "                  e.g. \"NHA1\"." << std::endl;
    std::cerr << "                  Note: no escaping is supported. To insert special characters," << std::endl;
    std::cerr << "                        you must split the string into multiple parts," << std::endl;
    std::cerr << "                        e.g. to insert a newline: \"hello\" 0x0a \"world\"" << std::endl << std::endl;
    std::cerr << "If no value is given, the parentheses '(' and ')' can be omitted." << std::endl << std::endl;
    std::cerr << "Tag Aliases:" << std::endl;
    std::cerr << "  m or magic                    equals 0x00" << std::endl;
    std::cerr << "  v or version                  equals 0x01" << std::endl;
    std::cerr << "  t or message_type             equals 0x02" << std::endl;
    std::cerr << "  f or message_flags            equals 0x03" << std::endl;
    std::cerr << "  u or user_data                equals 0x04" << std::endl;
    std::cerr << "  c or capabilities             equals 0x90" << std::endl;
    std::cerr << "  d or device_id                equals 0x91" << std::endl;
    std::cerr << "  rx or monotonic_timer_rx      equals 0x92" << std::endl;
    std::cerr << "  tx or monotonic_timer_tx      equals 0x93" << std::endl;
    std::cerr << "  l or last_updated             equals 0x94" << std::endl;
    std::cerr << "  e or temperatures             equals 0xa0" << std::endl;
    std::cerr << "  H or Header                   this is a special alias; instead of standing in" << std::endl;
    std::cerr << "                                for a single tag, it represents a series of" << std::endl;
    std::cerr << "                                tags constituting the header." << std::endl;
    std::cerr << "                                See 'Header Macro:'" << std::endl << std::endl;
    std::cerr << "Header Macro:" << std::endl;
    std::cerr << "  You can conveniently build a header for the query message by writing" << std::endl;
    std::cerr << "  H(<flags>) or Header(<flags>) in the brace-delimited message block." << std::endl;
    std::cerr << "  This will have the same meaning as" << std::endl;
    std::cerr << "    m(\"NHA1\") v(0x00 0x00 0x00 0x01) t(0x01) f(<flags>)" << std::endl;
    std::cerr << "  You can omit <flags>, in which case 0x00 will be used." << std::endl << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  a query for the device ID, followed by a query for the monotonic timer RX:" << std::endl;
    std::cerr << "    {H d} {H rx}" << std::endl;
    std::cerr << "  a query for all currently supported values, requesting to XOR the MD5:" << std::endl;
    std::cerr << "    {H(0x02) d rx tx}" << std::endl;
    std::cerr << "  a query containing 8 bytes of user data and a not yet defined TLV:" << std::endl;
    std::cerr << "    {H u(578437695752307201L) 0x99}" << std::endl;
    std::cerr << "  an invalid query, containing a query-type TLV with non-zero length:" << std::endl;
    std::cerr << "    {H rx(0x01 0x02)}" << std::endl;
}

int main(int argc, char **argv)
{
    // parse cli

    const char *ip = nullptr, *port = nullptr;
    bool quiet = false, jsonOutput = false;
    std::string queryString;

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1 ; argv[i][j] != '\0' ; ++j)
            {
                switch (argv[i][j])
                {
                    case 'h':
                        usage();
                        return 0;
                        break;
                    case 'H':
                        formatUsage();
                        return 0;
                        break;
                    case 'q':
                        quiet = true;
                        break;
                    case 'j':
                        jsonOutput = true;
                        break;
                    default:
                        std::cerr << "Unknown switch '" << argv[i][j] << "'\n";
                        usage();
                        return 1;
                        break;
                }
            }
        }
        else if (ip == nullptr)
        {
            ip = argv[i];
        }
        else if (port == nullptr)
        {
            port = argv[i];
        }
        else
        {
            queryString = queryString + " " + argv[i];
        }
    }

    if (ip == nullptr || port == nullptr)
    {
        usage();
        return 1;
    }

    std::istringstream iss(queryString);
    std::istream &input = (queryString.empty() ? std::cin : iss);

    constexpr size_t bufferSize = 65536;
    unsigned char buffer[bufferSize];
    int sockfd = -1;
    try
    {
        sockfd = connectToNha(ip, port);

        Message msg;
        int counter = 0;
        while (parse(msg, input))
        {
            std::ostringstream oss;
            oss << std::setw(3) << std::setfill('0') << counter;
            const std::string counterStr = oss.str();

            size_t sendSize = msg.serialize(buffer, bufferSize);
            if (!quiet) msg.print(std::cout, std::string{"query_"} + counterStr);

            sendQuery(sockfd, buffer, sendSize);
            size_t recvSize = receiveResponse(sockfd, buffer, bufferSize);

            msg.deserialize(buffer, recvSize);
            msg.print(std::cout, std::string{"response_"} + counterStr);

            ++counter;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        disconnectFromNha(sockfd);
        return 1;
    }

    return 0;
}
