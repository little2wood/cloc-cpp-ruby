// rawstrings.cpp —— C++11 原始字符串（含编码前缀）词法测试
const char*      a = u8R"(// not a comment /* either */)";
const wchar_t*   b = LR"(/* still not */)";
const char16_t*  c = uR"(multi
line raw)";
const char32_t*  d = UR"(/* nope */)";
const char*      e = R"(plain
raw
string)";
const char*      f = "plain // string";
int total = 1; // tail comment
