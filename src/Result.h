//
// Created by Will Gulian on 11/26/20.
//

#ifndef TRACEVIEWER2_RESULT_H
#define TRACEVIEWER2_RESULT_H

#include <variant>
#include <utility>

template<typename T, typename E>
class Result {
  std::variant<T, E> var;

public:

  static Result<T, E> with_value(const T &value) {
    return Result(std::variant<T, E>(std::in_place_index<0>, value));
  }

  static Result<T, E> with_value(T &&value) {
    return Result(std::variant<T, E>(std::in_place_index<0>, std::move(value)));
  }

  static Result<T, E> with_error(const E &value) {
    return Result(std::variant<T, E>(std::in_place_index<1>, value));
  }

  static Result<T, E> with_error(E &&value) {
    return Result(std::variant<T, E>(std::in_place_index<1>, std::move(value)));
  }

  explicit Result(std::variant<T, E> var) : var(std::move(var)) {}

  bool is_ok() {
    return var.index() == 0;
  }

  T &as_value() {
    return std::get<0>(var);
  }

  E &as_error() {
    return std::get<1>(var);
  }

  T &&into_value() && {
    return std::get<0>(std::move(var));
  }

  E &&into_error() && {
    return std::get<1>(std::move(var));
  }

};


#endif //TRACEVIEWER2_RESULT_H
