// 
// Copyright 2017 The Abseil Authors. 
// 
// Licensed under the Apache License, Version 2.0 (the "License"); 
// you may not use this file except in compliance with the License. 
// You may obtain a copy of the License at 
// 
//      https://www.apache.org/licenses/LICENSE-2.0 
// 
// Unless required by applicable law or agreed to in writing, software 
// distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and 
// limitations under the License. 
// 
// ----------------------------------------------------------------------------- 
// File: escaping.h 
// ----------------------------------------------------------------------------- 
// 
// This header file contains string utilities involved in escaping and 
// unescaping strings in various ways. 
 
#ifndef ABSL_STRINGS_ESCAPING_H_ 
#define ABSL_STRINGS_ESCAPING_H_ 
 
#include <cstddef> 
#include <util/generic/string.h>
#include <vector> 
 
#include "y_absl/base/macros.h"
#include "y_absl/strings/ascii.h"
#include "y_absl/strings/str_join.h"
#include "y_absl/strings/string_view.h"
 
namespace y_absl {
ABSL_NAMESPACE_BEGIN
 
// CUnescape() 
// 
// Unescapes a `source` string and copies it into `dest`, rewriting C-style 
// escape sequences (https://en.cppreference.com/w/cpp/language/escape) into 
// their proper code point equivalents, returning `true` if successful. 
// 
// The following unescape sequences can be handled: 
// 
//   * ASCII escape sequences ('\n','\r','\\', etc.) to their ASCII equivalents 
//   * Octal escape sequences ('\nnn') to byte nnn. The unescaped value must 
//     resolve to a single byte or an error will occur. E.g. values greater than 
//     0xff will produce an error. 
//   * Hexadecimal escape sequences ('\xnn') to byte nn. While an arbitrary 
//     number of following digits are allowed, the unescaped value must resolve 
//     to a single byte or an error will occur. E.g. '\x0045' is equivalent to 
//     '\x45', but '\x1234' will produce an error. 
//   * Unicode escape sequences ('\unnnn' for exactly four hex digits or 
//     '\Unnnnnnnn' for exactly eight hex digits, which will be encoded in 
//     UTF-8. (E.g., `\u2019` unescapes to the three bytes 0xE2, 0x80, and 
//     0x99). 
// 
// If any errors are encountered, this function returns `false`, leaving the 
// `dest` output parameter in an unspecified state, and stores the first 
// encountered error in `error`. To disable error reporting, set `error` to 
// `nullptr` or use the overload with no error reporting below. 
// 
// Example: 
// 
//   TString s = "foo\\rbar\\nbaz\\t";
//   TString unescaped_s;
//   if (!y_absl::CUnescape(s, &unescaped_s) {
//     ... 
//   } 
//   EXPECT_EQ(unescaped_s, "foo\rbar\nbaz\t"); 
bool CUnescape(y_absl::string_view source, TString* dest, TString* error);
 
// Overload of `CUnescape()` with no error reporting. 
inline bool CUnescape(y_absl::string_view source, TString* dest) {
  return CUnescape(source, dest, nullptr); 
} 
 
// CEscape() 
// 
// Escapes a 'src' string using C-style escapes sequences 
// (https://en.cppreference.com/w/cpp/language/escape), escaping other 
// non-printable/non-whitespace bytes as octal sequences (e.g. "\377"). 
// 
// Example: 
// 
//   TString s = "foo\rbar\tbaz\010\011\012\013\014\x0d\n";
//   TString escaped_s = y_absl::CEscape(s);
//   EXPECT_EQ(escaped_s, "foo\\rbar\\tbaz\\010\\t\\n\\013\\014\\r\\n"); 
TString CEscape(y_absl::string_view src);
 
// CHexEscape() 
// 
// Escapes a 'src' string using C-style escape sequences, escaping 
// other non-printable/non-whitespace bytes as hexadecimal sequences (e.g. 
// "\xFF"). 
// 
// Example: 
// 
//   TString s = "foo\rbar\tbaz\010\011\012\013\014\x0d\n";
//   TString escaped_s = y_absl::CHexEscape(s);
//   EXPECT_EQ(escaped_s, "foo\\rbar\\tbaz\\x08\\t\\n\\x0b\\x0c\\r\\n"); 
TString CHexEscape(y_absl::string_view src);
 
// Utf8SafeCEscape() 
// 
// Escapes a 'src' string using C-style escape sequences, escaping bytes as 
// octal sequences, and passing through UTF-8 characters without conversion. 
// I.e., when encountering any bytes with their high bit set, this function 
// will not escape those values, whether or not they are valid UTF-8. 
TString Utf8SafeCEscape(y_absl::string_view src);
 
// Utf8SafeCHexEscape() 
// 
// Escapes a 'src' string using C-style escape sequences, escaping bytes as 
// hexadecimal sequences, and passing through UTF-8 characters without 
// conversion. 
TString Utf8SafeCHexEscape(y_absl::string_view src);
 
// Base64Unescape() 
// 
// Converts a `src` string encoded in Base64 to its binary equivalent, writing 
// it to a `dest` buffer, returning `true` on success. If `src` contains invalid 
// characters, `dest` is cleared and returns `false`. 
bool Base64Unescape(y_absl::string_view src, TString* dest);
 
// WebSafeBase64Unescape() 
// 
// Converts a `src` string encoded in Base64 to its binary equivalent, writing 
// it to a `dest` buffer, but using '-' instead of '+', and '_' instead of '/'. 
// If `src` contains invalid characters, `dest` is cleared and returns `false`. 
bool WebSafeBase64Unescape(y_absl::string_view src, TString* dest);
 
// Base64Escape() 
// 
// Encodes a `src` string into a base64-encoded string, with padding characters. 
// This function conforms with RFC 4648 section 4 (base64). 
void Base64Escape(y_absl::string_view src, TString* dest);
TString Base64Escape(y_absl::string_view src);
 
// WebSafeBase64Escape() 
// 
// Encodes a `src` string into a base64-like string, using '-' instead of '+' 
// and '_' instead of '/', and without padding. This function conforms with RFC 
// 4648 section 5 (base64url). 
void WebSafeBase64Escape(y_absl::string_view src, TString* dest);
TString WebSafeBase64Escape(y_absl::string_view src);
 
// HexStringToBytes() 
// 
// Converts an ASCII hex string into bytes, returning binary data of length 
// `from.size()/2`. 
TString HexStringToBytes(y_absl::string_view from);
 
// BytesToHexString() 
// 
// Converts binary data into an ASCII text string, returning a string of size 
// `2*from.size()`. 
TString BytesToHexString(y_absl::string_view from);
 
ABSL_NAMESPACE_END
}  // namespace y_absl
 
#endif  // ABSL_STRINGS_ESCAPING_H_ 
