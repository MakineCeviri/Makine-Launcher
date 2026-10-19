// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The "xdelta" install step's decoder, against deltas made by the legacy
// installer's own c1.exe (xdelta3 3.0.4) — the tool whose output it replaces
// for AC Odyssey. Getting this wrong would rewrite a multi-gigabyte .forge
// archive with garbage, so the deltas below are not hand-made:
//
//   c1.exe -e -f -S djw -W 32768    -s src.bin tgt.bin multi.vcdiff
//   c1.exe -e -f -S djw -W 32768 -n -s src.bin tgt.bin nocksum.vcdiff
//
// src/tgt are generated below. Six windows, a DJW ("static Huffman") data
// section and an application header: the same shape as the legacy .aco files.

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "xdeltaapply.h"

using namespace makine::xdelta;

namespace {

std::string bytes(const char* hex)
{
    std::string out;
    for (std::size_t i = 0; hex[i] && hex[i + 1]; i += 2)
        out.push_back(char(std::stoi(std::string(hex + i, 2), nullptr, 16)));
    return out;
}

// glibc-style LCG; the fixture generator used the same one.
std::string lcg(std::uint32_t x, std::size_t n)
{
    std::string out(n, '\0');
    for (std::size_t i = 0; i < n; ++i) {
        x = x * 1103515245u + 12345u;
        out[i] = char((x >> 16) & 0xFF);
    }
    return out;
}

// Low-entropy text, so the delta's data section is worth compressing.
std::string text(std::uint32_t seed, std::size_t n)
{
    static const char kAlphabet[] = "aeiklmnorstuyz .";
    std::string out = lcg(seed, n);
    for (char& c : out) c = kAlphabet[std::uint8_t(c) & 15];
    return out;
}

const std::string kSource = lcg(1, 200000);
const std::string kTarget = kSource.substr(0, 60000) + text(7, 1500)
                          + kSource.substr(60000, 90000) + kSource.substr(160000)
                          + text(2, 2500);

// c1.exe output, verbatim.
const char* kMultiWindowHex =
    "d6c3c40005011766785f7467742e62696e2f2f66785f7372632e62696e2f0582800000108280000000040178bfefa213"
    "828000000581f424828000863582800001856c281559f374f48b3800240200083ebde8fbfe6c6e2738305117607e0579"
    "e9003bcd794a9e709031d52a4bb807001a66fc6510b578f032a61a3d27bb1e3257dea5f2b286ecf6ec86228a94763cf1"
    "5bb40759be3b7404d9ad3d478fc4e8d95973443e1629dd4a41f4c90533d338fedeb09e47049ba72b20f8eb1dd0a95a52"
    "1658a93fcd2d476d34e899fd3d0cc5f76e63e1ddf03f01d7b3a26543897342be2e42393014bd8b3e28eb4c88ddfe70c3"
    "c9d996c73497c386cb86543e64484870fd415a71b7f3a125184e05cf606bfcecd764407d1e80939e6f1d21e1107ea624"
    "a4ddf3417bd3bf76baa463e2fe9faf5086378e572f4b1a93a9a41ce9fddb53731f97bf0d3f1d5b4b05b4a4240ec2ca31"
    "ae11ed57a073ef4f1c4c29ebb9cb55df6b7f527d83a299de0cdecd5c4264b49b157b3cd44d68adcb0007c0d25300d6e2"
    "a7e9d24e4edfa934da99ddaece9d3dded7dd1810b32ca3239bc8366214042b04551e9980b5a909e61105e2083f2287b6"
    "1a01dd16c05455e4fb8c5e43c31ccf36310c87e00248e4cd1d32e1d3db88af8e40d4695ca2dc8f6a0cfdd934e5924798"
    "1445a929658a0fb30b57b1e03b5470b614a42981cae25578711c8c194e6b616da6d847ffe04bb5508945f3312fb7038c"
    "98643d9e878253ff45b80a6a8c793534ae5e4bf832030c354f2c13e19c5d9aeee4a06ec5bb42f382cc96388ae9407dab"
    "9e09fd6d9ef3ce0f558e6efc89df9739a774de20032b8fb4acd0e0d990e4aba97e9d264af467bcd7d6f01e629c537093"
    "dc205552b5e58a02def5baae73c96394b2c11c9a94a49e1dc00f7fc9576105a78308abc9a6e7703a1a732d9bb17066ee"
    "89038639926809d20abf07ae39ab0eb3d99a58ea0c3f734b98d12b9c1061f52a75d52b731fe7c6d1840ca7ae0a7d38c7"
    "7d280be1bb2efe97280e5a0e2238c9a896d81614f136bc19796700613ede427006336001d8ffd5158dc144d358871afc"
    "59be8adfdd10da694b546b8474dd7a7ecfb046ce3953b6f3fbedd86156b266be828406ce7f58acfc5a7afef77f13b9d0"
    "cd175877001381d46001833824016a2401812d2401564401811c64011c2401810024017a240171540170139f4400831b"
    "8148813c81308114862b813a4e833881d4600582800083f4241082800000000401e72addb513828000000582800085f4"
    "2410828000000004014d15fa5913828000000582ce1087f424168280000000070484fb3fc413819f4c13e0340081ed5c"
    "0581d80c8ac2348a5e81eb500389256a43e3b4db6f922a00240200083ebde8fbfe6c6e273846f9ac0675bdd5d3083c0f"
    "7a1a47f104d3d31521e623ed7b7c83d3cb2445f6c039c6a3a6446f62d272d42719f54756cbe51a5d73e88052b8cc128a"
    "c6e3cbcc46bcd9ec4f882a4d22efd2dbf9fe206880b71fd6457c81f5af36c7a97d45e7e37ea0e5e6f0aaabf0bfa37ef1"
    "08269249d39ef5dc4079c08396f08d1da14f7cd2e82c27692b0f9a9c745c65c0b4874dbf0c526ed5f4ea95271ee84ebd"
    "9fbe1d3994b36ce62043874cb8eb40eeafed78e80c61ca9863a6caeb20166679e8fe68fde855bd2ac0b0353f263b8995"
    "94458d0433944214ff9932008e44f96d1d89068f6cc7d10d596dd694a8db146442d8a7c8626aeb918894f08036a52b62"
    "7a8ee279c38e7d91317c505b15b34dc75f439d22b9a48cfd2051b196bda0ad34600097aeb95fc199e2eb4e7915a8abda"
    "603f2bb4d8dfe16800b1af01208d6ab395c13d053c9cf474d681cdc5887fa4f9638fea185cb42ae218c4270fbb2afc9b"
    "f9419f6b894704799dc2ad5f696469d747b4a9173251fb64598c6f88f402dc25805b5c7d792f44f789b95b80ed2bd86c"
    "d5816f63188c3a67532ef0f878ad12f670d1fd2e90f25af828b047682e33cd56baae86baf0e6a14c923f95f2d65fe389"
    "a4dff2f026b0242ff43bb3547e32b835e74fc68d4d27176465d5f30a7da250730d76706c23df0328f94e058bba6861e4"
    "4982d144dbc990648ef15b217e2d56e16c91e0b1486ff4fb0c85db1842be5c0969aab14db332a1996a490c3bdd6c9fbe"
    "11d25a4cec9439826839293cf90aec73a5a2286f20b5efb92cc1ea45c476800b0a899343302642f3e558ca45c35f1c25"
    "1bd28f4d7881a047070d0602074642af3e5d83dd1088f297376d09b68def92b676cb746358c4f38b853ba8ecc322fcfa"
    "fb05a217e4d56b96e431a185afb91a9a680cda2d2ca7c9652c46e3ef9dc8cc112e8996a85fcc29e6d95a5a4f6a81b62f"
    "796fe8065ece00e0731933b01df2f4769dc628e14c6d2925bc0d92653670239429389e275fd75ef1ac2b2e23a86eafdf"
    "958507fb4db8d9edf6351881f214558e45665e6cd0d4659a435a62ca018ad7db8ede9cfac8a461913bd87ea7c7f510c3"
    "bedcb196a10643ea8c1c5e0754ac38286abce21f4fec485676dd75fb9c1bc423cbd8f003dea1374da30bd8b5b1c4fba0"
    "69a80a5d86c7c4921d800503907c3257b005269185223f206595d2bdc6192378decd7cd13c955716659306b2074a2eb1"
    "2ec1883a32615f4413a252c4801fef6f04ded5f22e9bd16fdf9a090fd32d4e4daa559379930e2ac6e0fefef2df6ab7f6"
    "cb2dcf320db7a244ace054b188e426e94b16b41785b84c792bf779cca6428a8b26c4260bd3222a941177263e06e5b354"
    "b4dc3ca5e20dc343273c7748accfb794bef6583540947144d5f22de6d81400bed650921c920cd403fda81b54b6b4e363"
    "a703d4591b19d61731f533e33f6be5b9b4f4e7fe4b6519f0a6d95c750c367e63453b70afa6d3304f0c124874880db89d"
    "ca1bf6521a7e2b30fa4803d0493da7a0c1b92598b415bdbf348e22992e3bd6cfb574ca209b5e394d5bfff90ea3fdd43e"
    "349781291c72c8471fa91cfcf90f9c40dd3e4a7338035e30f67fdccb57778f6560c6581b32f70f55880feeaa0cdc2060"
    "a31bb0e56d0e1b6f8e026d50226271750077fec1e3318ae5bb19511cd5efc6659bdbbe7104978ba5be96967c3ea3a410"
    "d642a5a4544b2d9514bf8af74944233a55997889d8d8ceb818b5e84472e26c4c33a373709eda3a0fe39a456ba5b378cf"
    "2a1a91c4d07fbedc558a7166c97f8e62ef45e606006883718427075f8317826e815767871981103333827b82218c041e"
    "852a14823d40822281388a22168b388c2f835d8e2185118e7c86428736814b825a8d7b84218159";

const char* kNoChecksumHex =
    "d6c3c40005011766785f7467742e62696e2f2f66785f7372632e62696e2f01828000000c828000000004011382800000"
    "0181f424828000863182800001856c28158b3800240200083ebde8fbfe6c6e2738305117607e0579e9003bcd794a9e70"
    "9031d52a4bb807001a66fc6510b578f032a61a3d27bb1e3257dea5f2b286ecf6ec86228a94763cf15bb40759be3b7404"
    "d9ad3d478fc4e8d95973443e1629dd4a41f4c90533d338fedeb09e47049ba72b20f8eb1dd0a95a521658a93fcd2d476d"
    "34e899fd3d0cc5f76e63e1ddf03f01d7b3a26543897342be2e42393014bd8b3e28eb4c88ddfe70c3c9d996c73497c386"
    "cb86543e64484870fd415a71b7f3a125184e05cf606bfcecd764407d1e80939e6f1d21e1107ea624a4ddf3417bd3bf76"
    "baa463e2fe9faf5086378e572f4b1a93a9a41ce9fddb53731f97bf0d3f1d5b4b05b4a4240ec2ca31ae11ed57a073ef4f"
    "1c4c29ebb9cb55df6b7f527d83a299de0cdecd5c4264b49b157b3cd44d68adcb0007c0d25300d6e2a7e9d24e4edfa934"
    "da99ddaece9d3dded7dd1810b32ca3239bc8366214042b04551e9980b5a909e61105e2083f2287b61a01dd16c05455e4"
    "fb8c5e43c31ccf36310c87e00248e4cd1d32e1d3db88af8e40d4695ca2dc8f6a0cfdd934e59247981445a929658a0fb3"
    "0b57b1e03b5470b614a42981cae25578711c8c194e6b616da6d847ffe04bb5508945f3312fb7038c98643d9e878253ff"
    "45b80a6a8c793534ae5e4bf832030c354f2c13e19c5d9aeee4a06ec5bb42f382cc96388ae9407dab9e09fd6d9ef3ce0f"
    "558e6efc89df9739a774de20032b8fb4acd0e0d990e4aba97e9d264af467bcd7d6f01e629c537093dc205552b5e58a02"
    "def5baae73c96394b2c11c9a94a49e1dc00f7fc9576105a78308abc9a6e7703a1a732d9bb17066ee89038639926809d2"
    "0abf07ae39ab0eb3d99a58ea0c3f734b98d12b9c1061f52a75d52b731fe7c6d1840ca7ae0a7d38c77d280be1bb2efe97"
    "280e5a0e2238c9a896d81614f136bc19796700613ede427006336001d8ffd5158dc144d358871afc59be8adfdd10da69"
    "4b546b8474dd7a7ecfb046ce3953b6f3fbedd86156b266be828406ce7f58acfc5a7afef77f13b9d0cd175877001381d4"
    "6001833824016a2401812d2401564401811c64011c2401810024017a240171540170139f4400831b8148813c81308114"
    "862b813a4e833881d4600182800083f4240c8280000000040113828000000182800085f4240c82800000000401138280"
    "00000182ce1087f424128280000000070413819f4c13e0340081ed5c0181d80c8ac2348a5a81eb500389256a43922a00"
    "240200083ebde8fbfe6c6e273846f9ac0675bdd5d3083c0f7a1a47f104d3d31521e623ed7b7c83d3cb2445f6c039c6a3"
    "a6446f62d272d42719f54756cbe51a5d73e88052b8cc128ac6e3cbcc46bcd9ec4f882a4d22efd2dbf9fe206880b71fd6"
    "457c81f5af36c7a97d45e7e37ea0e5e6f0aaabf0bfa37ef108269249d39ef5dc4079c08396f08d1da14f7cd2e82c2769"
    "2b0f9a9c745c65c0b4874dbf0c526ed5f4ea95271ee84ebd9fbe1d3994b36ce62043874cb8eb40eeafed78e80c61ca98"
    "63a6caeb20166679e8fe68fde855bd2ac0b0353f263b899594458d0433944214ff9932008e44f96d1d89068f6cc7d10d"
    "596dd694a8db146442d8a7c8626aeb918894f08036a52b627a8ee279c38e7d91317c505b15b34dc75f439d22b9a48cfd"
    "2051b196bda0ad34600097aeb95fc199e2eb4e7915a8abda603f2bb4d8dfe16800b1af01208d6ab395c13d053c9cf474"
    "d681cdc5887fa4f9638fea185cb42ae218c4270fbb2afc9bf9419f6b894704799dc2ad5f696469d747b4a9173251fb64"
    "598c6f88f402dc25805b5c7d792f44f789b95b80ed2bd86cd5816f63188c3a67532ef0f878ad12f670d1fd2e90f25af8"
    "28b047682e33cd56baae86baf0e6a14c923f95f2d65fe389a4dff2f026b0242ff43bb3547e32b835e74fc68d4d271764"
    "65d5f30a7da250730d76706c23df0328f94e058bba6861e44982d144dbc990648ef15b217e2d56e16c91e0b1486ff4fb"
    "0c85db1842be5c0969aab14db332a1996a490c3bdd6c9fbe11d25a4cec9439826839293cf90aec73a5a2286f20b5efb9"
    "2cc1ea45c476800b0a899343302642f3e558ca45c35f1c251bd28f4d7881a047070d0602074642af3e5d83dd1088f297"
    "376d09b68def92b676cb746358c4f38b853ba8ecc322fcfafb05a217e4d56b96e431a185afb91a9a680cda2d2ca7c965"
    "2c46e3ef9dc8cc112e8996a85fcc29e6d95a5a4f6a81b62f796fe8065ece00e0731933b01df2f4769dc628e14c6d2925"
    "bc0d92653670239429389e275fd75ef1ac2b2e23a86eafdf958507fb4db8d9edf6351881f214558e45665e6cd0d4659a"
    "435a62ca018ad7db8ede9cfac8a461913bd87ea7c7f510c3bedcb196a10643ea8c1c5e0754ac38286abce21f4fec4856"
    "76dd75fb9c1bc423cbd8f003dea1374da30bd8b5b1c4fba069a80a5d86c7c4921d800503907c3257b005269185223f20"
    "6595d2bdc6192378decd7cd13c955716659306b2074a2eb12ec1883a32615f4413a252c4801fef6f04ded5f22e9bd16f"
    "df9a090fd32d4e4daa559379930e2ac6e0fefef2df6ab7f6cb2dcf320db7a244ace054b188e426e94b16b41785b84c79"
    "2bf779cca6428a8b26c4260bd3222a941177263e06e5b354b4dc3ca5e20dc343273c7748accfb794bef6583540947144"
    "d5f22de6d81400bed650921c920cd403fda81b54b6b4e363a703d4591b19d61731f533e33f6be5b9b4f4e7fe4b6519f0"
    "a6d95c750c367e63453b70afa6d3304f0c124874880db89dca1bf6521a7e2b30fa4803d0493da7a0c1b92598b415bdbf"
    "348e22992e3bd6cfb574ca209b5e394d5bfff90ea3fdd43e349781291c72c8471fa91cfcf90f9c40dd3e4a7338035e30"
    "f67fdccb57778f6560c6581b32f70f55880feeaa0cdc2060a31bb0e56d0e1b6f8e026d50226271750077fec1e3318ae5"
    "bb19511cd5efc6659bdbbe7104978ba5be96967c3ea3a410d642a5a4544b2d9514bf8af74944233a55997889d8d8ceb8"
    "18b5e84472e26c4c33a373709eda3a0fe39a456ba5b378cf2a1a91c4d07fbedc558a7166c97f8e62ef45e60600688371"
    "8427075f8317826e815767871981103333827b82218c041e852a14823d40822281388a22168b388c2f835d8e2185118e"
    "7c86428736814b825a8d7b84218159";

struct Decoded {
    Result result;
    std::string out;
};

Decoded run(const std::string& delta, const std::string& source)
{
    Decoded r;
    r.result = apply(reinterpret_cast<const std::uint8_t*>(delta.data()), delta.size(),
                     reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
                     [&r](const std::uint8_t* d, std::size_t n) {
                         r.out.append(reinterpret_cast<const char*>(d), n);
                         return true;
                     });
    return r;
}

} // namespace

TEST(XdeltaApply, RebuildsTheTargetTheLegacyToolWouldHave)
{
    ASSERT_EQ(kTarget.size(), 194000u);
    const Decoded r = run(bytes(kMultiWindowHex), kSource);
    EXPECT_EQ(r.result, Result::Success);
    EXPECT_TRUE(r.out == kTarget);
}

TEST(XdeltaApply, AnotherArchiveIsNoMatch)
{
    // One byte the delta copies from (60000..150000 lands in the target).
    std::string other = kSource;
    other[70000] = char(other[70000] ^ 0x5A);
    EXPECT_EQ(run(bytes(kMultiWindowHex), other).result, Result::NoMatch);
    // Shorter than the windows the delta copies from.
    EXPECT_EQ(run(bytes(kMultiWindowHex), kSource.substr(0, 100000)).result, Result::NoMatch);
}

TEST(XdeltaApply, AnAlreadyPatchedArchiveIsUpToDate)
{
    // A re-install or an update runs the step over its own result.
    const Decoded r = run(bytes(kMultiWindowHex), kTarget);
    EXPECT_EQ(r.result, Result::UpToDate);
}

TEST(XdeltaApply, AlmostTheTargetIsNotUpToDate)
{
    std::string almost = kTarget;
    almost[100000] = char(almost[100000] ^ 1);
    EXPECT_EQ(run(bytes(kMultiWindowHex), almost).result, Result::NoMatch);
    EXPECT_EQ(run(bytes(kMultiWindowHex), kTarget + "x").result, Result::NoMatch);
}

TEST(XdeltaApply, ADeltaWithoutChecksumsIsRefused)
{
    // It would decode against any archive of the right size; nothing could
    // tell its output from a translation.
    EXPECT_EQ(run(bytes(kNoChecksumHex), kSource).result, Result::Corrupt);
}

TEST(XdeltaApply, ABrokenDeltaIsCorrupt)
{
    const std::string delta = bytes(kMultiWindowHex);
    EXPECT_EQ(run(delta.substr(0, delta.size() - 10), kSource).result, Result::Corrupt);
    EXPECT_EQ(run(std::string("not a delta at all"), kSource).result, Result::Corrupt);
    EXPECT_EQ(run(std::string(), kSource).result, Result::Corrupt);
}

TEST(XdeltaApply, AnEmptySourceIsNoMatch)
{
    EXPECT_EQ(run(bytes(kMultiWindowHex), std::string()).result, Result::NoMatch);
}

TEST(XdeltaApply, ASinkThatRefusesStopsTheDecode)
{
    const std::string delta = bytes(kMultiWindowHex);
    const Result r = apply(reinterpret_cast<const std::uint8_t*>(delta.data()), delta.size(),
                           reinterpret_cast<const std::uint8_t*>(kSource.data()), kSource.size(),
                           [](const std::uint8_t*, std::size_t) { return false; });
    EXPECT_EQ(r, Result::WriteError);
}
