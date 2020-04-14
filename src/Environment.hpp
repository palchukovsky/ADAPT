#pragma once

#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace adapt {

class Keyword;

class Environment {
 public:
  class Entity {
   public:
    explicit Entity(std::string name, std::shared_ptr<const Keyword>);
    Entity(Entity &&) = default;
    Entity(const Entity &) = delete;
    Entity &operator=(Entity &&) = default;
    ~Entity() = default;

    const std::string &GetName() const;
    const Keyword &GetRuntime() const;

   private:
    const std::string m_name;
    const std::shared_ptr<const Keyword> m_runtime;
  };

 public:
  explicit Environment(std::ostream &outStream);
  Environment(Environment &&) = default;
  Environment(const Environment &) = delete;
  Environment &operator=(Environment &&) = default;
  ~Environment() = default;

  bool RegisterEntity(const std::string &name,
                      std::string path,
                      std::shared_ptr<const Keyword>);

  std::shared_ptr<const Entity> FindEntity(const std::string &name) const;

  void PrintLn(const std::string &);

 private:
  std::unordered_map<std::string, std::shared_ptr<Entity>> m_scope;
  std::ostream &m_outStream;
};

}  // namespace adapt