# Every C++ pin, with its download and SHA-256 (11.C4). A new version needs its new hash.

set(FUSION_PUGIXML_VERSION 1.16)
set(FUSION_PUGIXML_URL "https://github.com/zeux/pugixml/releases/download/v1.16/pugixml-1.16.tar.gz")
set(FUSION_PUGIXML_SHA256 4cee1ca4aad395170f4c7a07824f3bdd41f28316c6e1e1090a1425b278ec0b4b)

set(FUSION_JSON_VERSION 3.12.0)
set(FUSION_JSON_URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz")
set(FUSION_JSON_SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)

set(FUSION_FAST_FLOAT_VERSION 8.3.3)
set(FUSION_FAST_FLOAT_URL "https://github.com/fastfloat/fast_float/archive/refs/tags/v8.3.3.tar.gz")
set(FUSION_FAST_FLOAT_SHA256 4dd695f051893c8218faf2ed9871fa4a4990aa6aee2d3190ed7bfa2e83092245)

set(FUSION_CATCH2_VERSION 3.16.1)
set(FUSION_CATCH2_URL "https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz")
set(FUSION_CATCH2_SHA256 d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d)

# 11.C2 pins Z3 5.3.3, gated on experiment X11. Until X11 has run, the build links the
# system Z3 4.8.12, the version FuSION 1 links (decision 0039).
set(FUSION_Z3_VERSION 4.8.12)
