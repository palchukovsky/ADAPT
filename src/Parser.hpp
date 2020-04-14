
#pragma once

#include "Exception.hpp"
#include "Keyword.hpp"
#include "Types.hpp"

#include <functional>
#include <istream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace adapt {
namespace Details {

inline bool IsNewLine(const char ch) { return ch == '\r' || ch == '\n'; }
inline bool IsLineCommentStart(const char ch) { return ch == '/'; }
inline bool IsScopeBegin(const char ch) { return ch == '{'; }
inline bool IsScopeEnd(const char ch) { return ch == '}'; }
inline bool IsKeywordEnd(const char ch) { return ch == ';'; }
inline bool IsKeywordPathDel(const char ch) { return ch == ':'; }

template <typename Char>
struct NamesPolicy {};

template <>
struct NamesPolicy<char> {
  static std::string GetUsingKeyword() { return "USING"; }
  static std::string GetScopeKeyword() { return "SCOPE"; }
  static std::string GetDeclareKeyword() { return "DECLARE"; }
  static std::string GetAccessKeyword() { return "ACCESS"; }
  static std::string GetScopePathDel() { return "::"; }
};

class SyntaxError : public Exception {
 public:
  explicit SyntaxError(const CodeSource &source, std::string reason)
      : Exception(FormatExceptionDetails(source, reason)) {}
  ~SyntaxError() override = default;

  const char *what() const noexcept override { return "SYNTAX ERROR"; }
};

template <typename Char>
class ParserSession {
 public:
  using SourceStream = std::basic_istream<Char>;

 private:
  using String = std::basic_string<Char>;
  using Names = NamesPolicy<Char>;

 public:
  explicit ParserSession(SourceStream &source,
                         std::vector<std::shared_ptr<Keyword>> &resultRef,
                         std::function<void(const Exception &)> handleError)
      : m_source(source),
        m_handleError(std::move(handleError)),
        m_result(resultRef) {}
  ParserSession(ParserSession &&) = default;
  ParserSession(const ParserSession &) = delete;
  ParserSession &operator=(ParserSession &&) = default;
  ~ParserSession() = default;

  void Parse() {
    Char ch;
    while (m_source.get(ch)) {
      ++m_codeSource.column;
      if (CheckNewLine(ch)) {
        continue;
      }
      if (IsComment()) {
        continue;
      }
      if (CheckCommentStart(ch)) {
        continue;
      }
      CheckKeyword(ch);
    }
  }

 private:
  bool CheckNewLine(const Char &ch) {
    if (!IsNewLine(ch)) {
      return false;
    }
    if (!IsComment() && !m_keywordName.empty()) {
      throw SyntaxError(m_codeSource, "keyword is not finished");
    }
    ++m_codeSource.line;
    m_codeSource.column = 0;
    return true;
  }

  bool CheckCommentStart(const Char &ch) {
    if (!IsLineCommentStart(ch)) {
      if (m_commentStartsNo) {
        // math is not supported, so this is syntax error
        std::basic_ostringstream<Char> os;
        os << "unexpected symbol '" << ch << "'";
        throw SyntaxError(m_codeSource, os.str());
      }
      return false;
    }
    if (!m_keywordName.empty()) {
      throw SyntaxError(m_codeSource,
                        "keyword is not finished, but comment started");
    }
    if (++m_commentStartsNo == 2) {
      // comment start finished
      m_commentStartLineNo = m_codeSource.line;
      m_commentStartsNo = 0;
    }
    return true;
  }

  void CheckKeyword(const Char &ch) {
    if (std::isspace(ch)) {
      if (m_keywordName.empty()) {
        // just spaces before any keywords
        return;
      }
      // keyword already has been read
      if (!m_keywordArgs.empty() && m_keywordArgs.back().empty()) {
        // more then one space between arguments between
        return;
      }
      // new argument (possible)
      m_keywordArgs.push_back(String{});
      return;
    }

    if (IsKeywordEnd(ch) || IsScopeBegin(ch)) {
      CreateKeyword(ch);
      return;
    }

    if (IsScopeEnd(ch)) {
      if (m_scope.size() < 2) {
        // 1-st is constant, this is the root
        throw SyntaxError(
            m_codeSource,
            "number of scope ends is not the same as number of scope starts");
      }
      m_scope.pop_back();
      return;
    }

    if (!m_keywordArgs.empty()) {
      // continue keyword argument
      m_keywordArgs.back() += ch;
    } else {
      // start or continue keyword name
      m_keywordName += ch;
    }
  }

  void CreateKeyword(const Char &ch) {
    const auto &factory = m_factories.find(m_keywordName);
    if (factory == m_factories.cend()) {
      throw BadLanguageException(m_codeSource,
                                 R"(unknown keyword ")" + m_keywordName + "\"");
    }
    factory->second(ch);
    m_keywordName.clear();
    m_keywordArgs.clear();
  }

  void CreateUsingKeyword(const Char &ch) {
    ValidateKeyword<1, false>(ch);
    // new using usage rests previous using
    m_using = std::move(m_keywordArgs[0]);
  }

  void CreateScopeKeyword(const Char &ch) {
    ValidateKeyword<1, true>(ch);
    auto name = m_scope.front() + m_keywordArgs[0];
    // caches all possible scopes, prepares it for future declarations and
    // accessors
    m_scope.push_back(name + Names::GetScopePathDel());
    // holds entity name in envelopment
    // creating keyword with full path
    m_result.emplace_back(std::make_shared<EnvironmentEntityKeyword>(
        std::move(m_keywordArgs[0]), std::move(name), m_codeSource));
  }

  void CreateDeclareKeyword(const Char &ch) {
    ValidateKeyword<1, false>(ch);
    // creating keyword with full path
    m_result.emplace_back(std::make_shared<DeclareKeyword>(
        std::move(m_keywordArgs[0]), m_scope.back() + m_keywordArgs[0],
        m_codeSource));
  }

  void CreateAccessKeyword(const Char &ch) {
    ValidateKeyword<1, false>(ch);
    const auto &delimeter = Names::GetScopePathDel();
    if (m_keywordArgs[0].size() >= delimeter.size() &&
        std::equal(m_keywordArgs[0].cbegin(),
                   m_keywordArgs[0].cbegin() + delimeter.size(),
                   delimeter.cbegin(), delimeter.cend())) {
      // has only one variant as in the path provided as an absolute path from
      // root
      m_result.emplace_back(std::make_shared<AccessKeyword>(
          std::move(m_keywordArgs[0]),
          std::vector<std::string>{m_keywordArgs[0]},
          std::vector<std::string>{}, m_codeSource));
      return;
    }
    // combinign all possible names for this accessing, starting from the root
    auto directNames = m_scope;
    for (auto &level : directNames) {
      level += m_keywordArgs[0];
    }
    std::vector<String> altNames;
    if (!m_using.empty()) {
      altNames = m_scope;
      for (auto &level : altNames) {
        level += m_using + delimeter + m_keywordArgs[0];
      }
    }
    m_result.emplace_back(std::make_shared<AccessKeyword>(
        std::move(m_keywordArgs[0]), std::move(directNames),
        std::move(altNames), m_codeSource));
  }

  bool IsComment() const { return m_commentStartLineNo == m_codeSource.line; }

  template <size_t argsNoReq, bool isScope>
  void ValidateKeyword(const Char &ch) const {
    auto argsNo = m_keywordArgs.size();
    if (argsNo && m_keywordArgs.back().empty()) {
      --argsNo;
    }
    if (argsNo != argsNoReq) {
      throw BadLanguageException(
          m_codeSource,
          "number of keyword keyword arguments is not the same as expected");
    }
    if (!(isScope ? IsScopeBegin(ch) : IsKeywordEnd(ch))) {
      throw SyntaxError(m_codeSource, "unexpected end of keyword");
    }
  }

 private:
  SourceStream &m_source;
  const std::function<void(const Exception &)> m_handleError;

  String m_using;
  std::vector<String> m_scope{Names::GetScopePathDel()};

  String m_keywordName;
  std::vector<String> m_keywordArgs;

  const std::unordered_map<String, std::function<void(const Char &)>>
      m_factories{{Names::GetAccessKeyword(),
                   [this](const Char &ch) { CreateAccessKeyword(ch); }},
                  {Names::GetScopeKeyword(),
                   [this](const Char &ch) { CreateScopeKeyword(ch); }},
                  {Names::GetUsingKeyword(),
                   [this](const Char &ch) { CreateUsingKeyword(ch); }},
                  {Names::GetDeclareKeyword(),
                   [this](const Char &ch) { CreateDeclareKeyword(ch); }}};

  CodeSource m_codeSource{1, 1};

  size_t m_commentStartLineNo = 0;
  size_t m_commentStartsNo = 0;

  std::vector<std::shared_ptr<Keyword>> &m_result;
};  // namespace Details

}  // namespace Details

template <typename Char, typename ErrorHandeler>
std::vector<std::shared_ptr<Keyword>> Parse(std::basic_istream<Char> &source,
                                            const ErrorHandeler &handleError) {
  std::vector<std::shared_ptr<Keyword>> result;
  bool hasErrors = false;
  Details::ParserSession<Char>(source, result,
                               [&handleError, &hasErrors](const Exception &ex) {
                                 hasErrors = true;
                                 handleError(ex);
                               })
      .Parse();
  if (hasErrors) {
    return {};
  }
  return result;
}
}  // namespace adapt
