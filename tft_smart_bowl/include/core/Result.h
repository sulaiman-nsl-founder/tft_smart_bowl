#ifndef CORE_RESULT_H
#define CORE_RESULT_H

#include "core/Errors.h"
#include <utility>

namespace Core {

template <typename T, typename E = Error>
class Result {
public:
    Result(const T& val) : _hasValue(true), _value(val), _error() {}
    Result(T&& val) : _hasValue(true), _value(std::move(val)), _error() {}
    Result(const E& err) : _hasValue(false), _value(), _error(err) {}
    Result(E&& err) : _hasValue(false), _value(), _error(std::move(err)) {}

    bool isOk() const { return _hasValue; }
    bool isError() const { return !_hasValue; }

    const T& value() const {
        return _value;
    }

    T& value() {
        return _value;
    }

    const E& error() const {
        return _error;
    }

    E& error() {
        return _error;
    }

    explicit operator bool() const { return _hasValue; }

private:
    bool _hasValue;
    T _value;
    E _error;
};

// Specialization for void
template <typename E>
class Result<void, E> {
public:
    Result() : _hasValue(true), _error() {}
    Result(const E& err) : _hasValue(false), _error(err) {}
    Result(E&& err) : _hasValue(false), _error(std::move(err)) {}

    bool isOk() const { return _hasValue; }
    bool isError() const { return !_hasValue; }

    const E& error() const {
        return _error;
    }

    E& error() {
        return _error;
    }

    explicit operator bool() const { return _hasValue; }

private:
    bool _hasValue;
    E _error;
};

} // namespace Core

#endif // CORE_RESULT_H
