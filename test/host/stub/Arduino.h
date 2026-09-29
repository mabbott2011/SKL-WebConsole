#pragma once
// Minimal host stand-ins for the Arduino APIs SKLWebConsole uses.
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#define PROGMEM
class __FlashStringHelper;
class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  explicit String(uint32_t v) : s(std::to_string(v)) {}
  explicit String(int v) : s(std::to_string(v)) {}
  size_t length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
  char operator[](size_t i) const { return s[i]; }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const char* o) const { return s == o; }
  bool endsWith(const char* e) const { size_t n = strlen(e); return s.size() >= n && s.compare(s.size() - n, n, e) == 0; }
  void remove(size_t i) { s.erase(i); }
  void reserve(size_t n) { s.reserve(n); }
};
inline String operator+(const String& a, const char* b) { return String(a.s + b); }
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
inline String operator+(const String& a, const String& b) { return String(a.s + b.s); }
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const char* s) { return write(s); }
  size_t print(const String& s) { return write(s.c_str()); }
  size_t print(int v) { return write(std::to_string(v).c_str()); }
  size_t println() { return write("\r\n"); }
  size_t println(const char* s) { size_t n = print(s); return n + println(); }
  size_t println(const String& s) { size_t n = print(s); return n + println(); }
  size_t println(int v) { size_t n = print(v); return n + println(); }
  size_t printf(const char* f, ...) { char b[512]; va_list a; va_start(a, f); int n = vsnprintf(b, sizeof b, f, a); va_end(a); return write((const uint8_t*)b, n); }
};
unsigned long millis();
