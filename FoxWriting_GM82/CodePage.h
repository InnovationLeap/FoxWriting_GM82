#pragma once

struct CodePageEntry {
    UINT code_page;
    LPCSTR name;
};

static CodePageEntry g_codePages[] = {
    {   936, "GB2312" },
    {   936, "gb2312" },
    {   936, "GBK" },
    {   936, "gbk" },
    { 54936, "GB18030" },
    { 54936, "gb18030" },
    {   950, "BIG5" },
    {   950, "big5" },
    {   932, "SHIFTJIS" },
    {   932, "shiftjis" },
    {   949, "EUCKR" },
    {   949, "euckr" },
    {  65001, "UTF8" },
    {  65001, "utf8" },
    {  65001, "UTF-8" },
    {  65001, "utf-8" },
    {      0, NULL }
};
