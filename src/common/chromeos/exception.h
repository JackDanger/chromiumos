// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_EXCEPTION_H_
#define CHROMEOS_EXCEPTION_H_

#include <base/logging.h>

#include <algorithm>
#include <exception>
#include <string>
#include <typeinfo>

/******************************************************************************/

namespace chromeos {
using std::swap;

/******************************************************************************/

// \brief AnyException is a runtime polymophic type which can hold any
// std::exception derived exception.
//
// AnyException supports a safe bool cast, return true iff it contains an
// exception (a default constructed AnyException contains no exception.
//
// AnyException is useful for error handling which doesn't use try/catch - but
// may be converted to use try/catch in the future. I can also be used to
// marshal an exception across a non exception safe boundary.
//
// \example
// AnyException error;
// int x = SomeFunction(error);
// if (error) return;
//
// // ...
//
// int SomeFunction(AnyException& error) {
//   // ...
//   if (something_failed) {
//      error = std::logic_error("Something Failed.");
//      return 0;
//   }
//   // ...
// \end_example

class AnyException : public std::exception {
 public:
  AnyException()
      : object_(NULL) {
  }

  template <typename T>  // T is derived from std::exception
  explicit AnyException(const T& x)
      : object_(new Model<T>(x)) {
  }

  AnyException(const AnyException& x)
      : object_(x.object_ ? x.object_->Copy() : NULL) {
  }

  ~AnyException() throw() {
    delete object_;
  }

  AnyException& operator=(AnyException x) {
    swap(*this, x);
  }

  template <typename T>  // T is derived from std::exception
  AnyException& operator=(const T& x) {
    *this = AnyException(x);
  }

  inline friend void swap(AnyException& x, AnyException& y) {
    swap(x.object_, y.object_);
  }

  const char* what() throw() {
    return object_ ? object_->what() : "empty AnyException";
  }

  operator bool() const {
    return object_;
  }

 private:
  operator int() const;  // for safe bool cast.

  class Concept : public std::exception {
   public:
    virtual Concept* Copy() const = 0;
  };

  template <class T>  // T is derived from std::exception
  class Model : public Concept {
   public:
    explicit Model(const T& x)
        : object_(x) {
    }
    Concept* Copy() const {
      return new Model(object_);
    }
    const char* what() throw() {
      return object_.what();
    }

   private:
    T object_;
  };

  Concept* object_;
};

/******************************************************************************/

// \brief BadCast is an std::bad_cast which report the \param from and \param to
// name of types for better error reporting.
//
// \note Consider moving bad cast into a typeinfo.h file.
//
// \example
// template <typaname R,
//           typename T>
// R Cast(const T& x, AnyException& error) {
//   if (!Compatible<T, R>()) {
//     error = BadCast(typeid(T).name(), typeid(R).name());
//     return R();
//   }
//   // ...
// \end_example

class BadCast : public std::bad_cast {
 public:
  BadCast(const char* from, const char* to)
      : what_("BadCast from '") {
    what_ += from;
    what_ += "' to '";
    what_ += to;
    what_ += "'.";
  }

  ~BadCast() throw() {
  }

  const char* what() const throw() {
    return what_.c_str();
  }

 private:
  std::string what_;
};

/******************************************************************************/

// \brief SquelchError logs a warning noting the x.what().
//
// In a system using exception handling, SquelchError would become a throw.

template <typename T>  // T is derived from std::exception
void SquelchError(const T& x) {
  LOG(WARNING) << "error squelched:" << x.what();
}

/******************************************************************************/

// \brief TerminalError logs a fatal error causing the process to terminate.
//
// x.what() is noted in the log. In a system using exceptino handling,
// TerminalError would become a throw.

template <typename T>  // T is derived from std::exception
void TerminalError(const T& x) {
  LOG(FATAL) << "terminal error:" << x.what();
}

/******************************************************************************/

}  // namespace chromeos

/******************************************************************************/

#endif  // CHROMEOS_EXCEPTION_H_

