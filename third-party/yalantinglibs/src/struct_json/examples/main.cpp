#include <cassert>
#include <iguana/json_reader.hpp>
#include <iguana/json_writer.hpp>
#include <iostream>

#include "ylt/struct_json/json_reader.h"
#include "ylt/struct_json/json_writer.h"

void test_user_defined_struct();

struct person {
  std::string name;
  int age;
};

bool operator==(const person& a, const person& b) {
  return a.name == b.name && a.age == b.age;
}

YLT_REFL(person, name, age);

class some_object {
  int id;
  std::string name;

 public:
  some_object() = default;
  some_object(int i, std::string str) : id(i), name(str) {}
  int get_id() const { return id; }
  std::string get_name() const { return name; }
  YLT_REFL(some_object, id, name);
};

void test_inner_object() {
  some_object obj{20, "tom"};
  std::string str;
  iguana::to_json(obj, str);
  std::cout << str << "\n";

  some_object obj1;
  iguana::from_json(obj1, str);
  assert(obj1.get_id() == 20);
  assert(obj1.get_name() == "tom");
}

struct person1 {
  std::shared_ptr<std::string> name;
  std::unique_ptr<int64_t> age;
};
YLT_REFL(person1, name, age);

void use_smart_pointer() {
  person1 p{std::make_shared<std::string>("tom"),
            std::make_unique<int64_t>(42)};
  std::string str;
  iguana::to_json(p, str);

  person1 p1;
  iguana::from_json(p1, str);

  assert(*p1.name == "tom");
  assert(*p1.age == 42);
}

void test_escape_serialize() {
  person p{"老\t人", 20};
  std::string ss;
  struct_json::to_json(p, ss);
  std::cout << ss << std::endl;
  person p1;
  struct_json::from_json(p1, ss);
  assert(p1.name == p.name);
}

struct test_optstr_reader_null {
  std::optional<std::string> name;
};
YLT_REFL(test_optstr_reader_null, name);
void test_optional() {
  test_optstr_reader_null v;
  v.name = "name";  // optional<string> begin with 'n'
  std::string json;
  iguana::to_json(v, json);

  test_optstr_reader_null v1;
  iguana::from_json(v1, json);
  assert(v.name == v1.name);
}

int main() {
  person p{"tom", 20};
  std::string str;

  // struct to json
  struct_json::to_json(p, str);
  std::cout << str << "\n";

  person p1;
  // json to struct
  struct_json::from_json(p1, str);
  assert(p == p1);

  // dom parse, json to dom
  struct_json::jvalue val;
  struct_json::parse(val, str);
  assert(val.at<std::string>("name") == "tom");
  assert(val.at<int>("age") == 20);

  test_inner_object();
  use_smart_pointer();
  test_escape_serialize();
  test_user_defined_struct();
  test_optional();
}