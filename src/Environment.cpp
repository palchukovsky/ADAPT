#include "Environment.hpp"

#include "Keyword.hpp"

#include <regex>

using namespace adapt;

Environment::Entity::Entity(std::string name,
                            std::shared_ptr<const Keyword> runtime)
    : m_name(std::move(name)), m_runtime(std::move(runtime)) {}

const std::string &Environment::Entity::GetName() const { return m_name; }

const Keyword &Environment::Entity::GetRuntime() const { return *m_runtime; }

bool Environment::RegisterEntity(const std::string &name,
                                 std::string path,
                                 std::shared_ptr<const Keyword> runtime) {
  static const std::regex nameRule(R"([a-z][a-z\d]*)",
                                   std::regex_constants::icase);
  if (!std::regex_match(name, nameRule)) {
    throw BadLanguageException(
        runtime->GetCodeSource(),
        "declaration \"" + name + R"(" has invalid format)");
  }
  auto entity = std::make_shared<Entity>(path, std::move(runtime));
  return m_scope.emplace(std::move(path), std::move(entity)).second;
}

std::shared_ptr<const Environment::Entity> Environment::FindEntity(
    const std::string &name) const {
  const auto &result = m_scope.find(name);
  if (result == m_scope.cend()) {
    return {};
  }
  return result->second;
}

void Environment::PrintLn(std::string line) {
  m_output.push_back(std::move(line));
}

void Environment::FlushOutput(std::ostream &outStream) {
  for (const auto &line : m_output) {
    outStream << line << std::endl;
  }
  m_output.clear();
  m_output.shrink_to_fit();
}
