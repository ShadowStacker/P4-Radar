#pragma once

#include <cstdint>
#include <map>
#include <string>

/** Small helpers for the radar's own settings web page (see app/web.cpp). */
namespace core::web {

/** Decodes %XX escapes (and '+' as a space, as forms send it). */
std::string urlDecode(const std::string& s);
/** Splits "a=1&b=two" (a query string or form body) into decoded pairs. */
std::map<std::string, std::string> parseForm(const std::string& s);
/** The value of one cookie in a Cookie header ("" if absent). */
std::string cookie(const std::string& header, const char* name);
/** Escapes a string for inside JSON quotes. */
std::string jsonEscape(const std::string& s);
/** "\"...\"" */
std::string jsonString(const std::string& s);

/** The whole phone page: HTML, style and script in one. */
extern const char kPage[];

}  // namespace core::web
