#pragma once

#include "Environment.hpp"
#include "Exception.hpp"

#include <memory>
#include <ostream>
#include <vector>

namespace adapt {

namespace Details {

class AccessInaccessibleException : public Exception {
 public:
  AccessInaccessibleException() : Exception("") {}
  ~AccessInaccessibleException() override = default;

  const char *what() const noexcept override {
    return "ENTITY IS INACCESSIBLE";
  }
};

}  // namespace Details

class Keyword {
 public:
  explicit Keyword(const CodeSource &codeSource) : m_codeSource(codeSource) {}
  Keyword(Keyword &&) = default;
  Keyword(const Keyword &) = delete;
  Keyword &operator=(Keyword &&) = default;
  virtual ~Keyword() = default;

  const CodeSource &GetCodeSource() const { return m_codeSource; }

  virtual void Execute(Environment &) const = 0;
  virtual void Access(Environment &, const Keyword &accesser) const = 0;

 private:
  const CodeSource m_codeSource;
};

class EnvironmentEntityKeyword : public Keyword,
                                 public std::enable_shared_from_this<Keyword> {
 public:
  explicit EnvironmentEntityKeyword(std::string arg,
                                    std::string name,
                                    const CodeSource &codeSource)
      : Keyword(codeSource), m_arg(std::move(arg)), m_name(std::move(name)) {}
  ~EnvironmentEntityKeyword() override = default;

  void Execute(Environment &env) const override {
    if (!env.RegisterEntity(m_arg, m_name, shared_from_this())) {
      throw BadLanguageException(GetCodeSource(),
                                 "declaration \"" + m_arg +
                                     R"("is not unique and conflicts with ")" +
                                     m_name + "\"");
    }
  }

  void Access(Environment &, const Keyword &) const override {
    throw Details::AccessInaccessibleException();
  }

 protected:
  const std::string &GetArg() const { return m_arg; }
  const std::string &GetName() const { return m_name; }

 private:
  const std::string m_arg;
  const std::string m_name;
};

class DeclareKeyword : public EnvironmentEntityKeyword {
 public:
  using EnvironmentEntityKeyword::EnvironmentEntityKeyword;
  ~DeclareKeyword() override = default;

  void Access(Environment &env, const Keyword &accesser) const override {
    std::ostringstream out;
    out << "LINE " << accesser.GetCodeSource().line << " ACCESS " << GetName();
    env.PrintLn(out.str());
  }
};

class AccessKeyword : public Keyword {
 public:
  explicit AccessKeyword(std::string arg,
                         std::vector<std::string> directNames,
                         std::vector<std::string> altNames,
                         const CodeSource &codeSource)
      : Keyword(codeSource),
        m_arg(std::move(arg)),
        m_directNames(std::move(directNames)),
        m_altNames(std::move(altNames)) {}
  ~AccessKeyword() override = default;

  void Execute(Environment &env) const override {
    std::shared_ptr<const Environment::Entity> target;
    for (auto it = m_directNames.rbegin(); it != m_directNames.crend(); ++it) {
      target = env.FindEntity(*it);
      if (target) {
        break;
      }
    }
    for (auto it = m_altNames.rbegin(); it != m_altNames.crend(); ++it) {
      auto entity = env.FindEntity(*it);
      if (!entity) {
        continue;
      }
      if (target) {
        // alt-name conflicts with direct name, ambiguous names
        throw BadLanguageException(
            GetCodeSource(),
            "declaration \"" + m_arg +
                R"("is ambiguous by USING statement, could be ")" +
                target->GetName() + R"(" or ")" + entity->GetName() + "\"");
      }
      target = std::move(entity);
    }

    if (!target) {
      throw BadLanguageException(
          GetCodeSource(), R"(declaration ")" + m_arg + R"(" is not existent)");
    }

    try {
      target->GetRuntime().Access(env, *this);
    } catch (const Details::AccessInaccessibleException &) {
      throw BadLanguageException(
          GetCodeSource(),
          R"(attempt to access incaccessble item with name ")" +
              target->GetName() + "\"");
    }
  }

  void Access(Environment &, const Keyword &) const override {
    throw Details::AccessInaccessibleException();
  }

 private:
  const std::string m_arg;
  const std::vector<std::string> m_directNames;
  const std::vector<std::string> m_altNames;
};  // namespace adapt

}  // namespace adapt