#include "testkit.h"

#include "common/sha256.h"

#include <string>

// Test values from FIPS 180-4 examples and RFC 4231.
TEST(sha256, digests_match_the_published_values)
{
    using sha256::digest;
    using sha256::hex;
    CHECK_EQ(hex(digest("")), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(hex(digest("abc")), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(hex(digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    CHECK_EQ(hex(digest(std::string(1000000, 'a'))),
             std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    // 55, 56 and 64 bytes: the padding edges.
    CHECK_EQ(hex(digest(std::string(55, 'a'))),
             std::string("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
    CHECK_EQ(hex(digest(std::string(56, 'a'))),
             std::string("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
    CHECK_EQ(hex(digest(std::string(64, 'a'))),
             std::string("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
}

TEST(sha256, hmac_matches_rfc_4231)
{
    using sha256::hex;
    using sha256::hmac;
    CHECK_EQ(hex(hmac(std::string(20, '\x0b'), "Hi There")),
             std::string("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    CHECK_EQ(hex(hmac("Jefe", "what do ya want for nothing?")),
             std::string("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    // A key longer than a block is hashed first.
    CHECK_EQ(hex(hmac(std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First")),
             std::string("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
}

TEST(sha256, same_bytes)
{
    CHECK(sha256::sameBytes("abc", "abc"));
    CHECK(!sha256::sameBytes("abc", "abd"));
    CHECK(!sha256::sameBytes("abc", "ab"));
}
