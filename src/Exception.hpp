
#pragma once

#include "Types.hpp"

#include <exception>
#include <sstream>

namespace adapt {

namespace Details {
inline std::string FormatExceptionDetails(const CodeSource& source,
                                          std::string reason) {
  std::ostringstream os;
  os << reason << " at " << source.line << ':' << source.column;
  return os.str();
}
}  // namespace Details

class Exception : public std::exception {
 public:
  explicit Exception(std::string details) noexcept
      : m_details(std::move(details)) {}
  ~Exception() override = default;

  const std::string& GetDetails() const noexcept { return m_details; }

 private:
  const std::string m_details;
};

class BadLanguageException : public Exception {
 public:
  explicit BadLanguageException(const CodeSource& source, std::string reason)
      : Exception(Details::FormatExceptionDetails(source, reason)),
        m_what(FormatWhat(source)) {}
  ~BadLanguageException() override = default;

  const char* what() const noexcept override { return m_what.c_str(); }

 private:
  static std::string FormatWhat(const CodeSource& source) {
    std::ostringstream os;
    os << "ERROR " << source.line;
    return os.str();
  }

 private:
  const std::string m_what;
};
}  // namespace adapt
