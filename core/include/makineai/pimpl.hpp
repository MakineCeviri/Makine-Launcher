/**
 * @file pimpl.hpp
 * @brief Pointer-to-Implementation (Pimpl) idiom helpers
 * @copyright (c) 2026 MakineAI Team
 *
 * Provides a type-safe, exception-safe pimpl implementation
 * for ABI stability and compile-time firewall.
 *
 * Usage:
 * @code
 * // header.hpp
 * class Foo {
 * public:
 *     Foo();
 *     ~Foo();
 *     Foo(Foo&&) noexcept;
 *     Foo& operator=(Foo&&) noexcept;
 *
 *     void doSomething();
 *
 * private:
 *     struct Impl;
 *     Pimpl<Impl> impl_;
 * };
 *
 * // source.cpp
 * struct Foo::Impl {
 *     int internalData;
 *     void internalMethod() { }
 * };
 *
 * Foo::Foo() : impl_(makePimpl<Impl>()) {}
 * Foo::~Foo() = default;  // Must be in .cpp where Impl is complete
 * Foo::Foo(Foo&&) noexcept = default;
 * Foo& Foo::operator=(Foo&&) noexcept = default;
 *
 * void Foo::doSomething() {
 *     impl_->internalMethod();
 * }
 * @endcode
 */

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace makineai {

/**
 * @brief Custom deleter for Pimpl that can be defined later
 *
 * This allows the destructor to be called even when Impl
 * is only forward-declared in the header.
 */
template <typename T>
struct PimplDeleter {
    void operator()(T* ptr) const noexcept {
        // Static assert ensures T is complete when delete is called
        static_assert(
            sizeof(T) > 0,
            "Cannot delete pointer to incomplete type. "
            "Define destructor in .cpp where Impl is complete."
        );
        delete ptr;
    }
};

/**
 * @brief Pimpl smart pointer wrapper
 *
 * A unique_ptr wrapper specifically designed for the pimpl idiom.
 * Provides:
 * - Automatic memory management
 * - Move semantics
 * - const-propagation (operator-> returns const Impl* for const Pimpl)
 * - Static checks for complete type at destruction
 *
 * @tparam T The implementation type (usually a nested struct called Impl)
 */
template <typename T>
class Pimpl {
public:
    /**
     * @brief Default constructor - creates null pimpl
     */
    Pimpl() noexcept = default;

    /**
     * @brief Construct from raw pointer (takes ownership)
     */
    explicit Pimpl(T* ptr) noexcept : ptr_(ptr) {}

    /**
     * @brief Construct from unique_ptr
     */
    explicit Pimpl(std::unique_ptr<T, PimplDeleter<T>> ptr) noexcept
        : ptr_(std::move(ptr)) {}

    /**
     * @brief Destructor - must be defined where T is complete
     */
    ~Pimpl() = default;

    /**
     * @brief Move constructor
     */
    Pimpl(Pimpl&&) noexcept = default;

    /**
     * @brief Move assignment
     */
    Pimpl& operator=(Pimpl&&) noexcept = default;

    // Non-copyable
    Pimpl(const Pimpl&) = delete;
    Pimpl& operator=(const Pimpl&) = delete;

    /**
     * @brief Access implementation (non-const)
     * @return Pointer to implementation
     */
    T* operator->() noexcept {
        return ptr_.get();
    }

    /**
     * @brief Access implementation (const)
     * @return Const pointer to implementation
     */
    const T* operator->() const noexcept {
        return ptr_.get();
    }

    /**
     * @brief Dereference (non-const)
     * @return Reference to implementation
     */
    T& operator*() noexcept {
        return *ptr_;
    }

    /**
     * @brief Dereference (const)
     * @return Const reference to implementation
     */
    const T& operator*() const noexcept {
        return *ptr_;
    }

    /**
     * @brief Get raw pointer
     */
    T* get() noexcept {
        return ptr_.get();
    }

    /**
     * @brief Get raw pointer (const)
     */
    const T* get() const noexcept {
        return ptr_.get();
    }

    /**
     * @brief Check if valid (not null)
     */
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    /**
     * @brief Reset to new pointer
     */
    void reset(T* ptr = nullptr) noexcept {
        ptr_.reset(ptr);
    }

    /**
     * @brief Release ownership
     */
    T* release() noexcept {
        return ptr_.release();
    }

    /**
     * @brief Swap with another Pimpl
     */
    void swap(Pimpl& other) noexcept {
        ptr_.swap(other.ptr_);
    }

private:
    std::unique_ptr<T, PimplDeleter<T>> ptr_;
};

/**
 * @brief Create a Pimpl with forwarded constructor arguments
 *
 * @tparam T The implementation type
 * @tparam Args Constructor argument types
 * @param args Arguments to forward to T's constructor
 * @return Pimpl<T> containing the new object
 */
template <typename T, typename... Args>
Pimpl<T> makePimpl(Args&&... args) {
    return Pimpl<T>(new T(std::forward<Args>(args)...));
}

/**
 * @brief RAII helper for implementing copy-on-write semantics
 *
 * Use this when you need deep copy support with pimpl.
 *
 * @tparam T The implementation type (must be copy-constructible)
 */
template <typename T>
class CopyablePimpl {
public:
    CopyablePimpl() noexcept = default;

    explicit CopyablePimpl(T* ptr) noexcept : ptr_(ptr) {}

    ~CopyablePimpl() = default;

    // Move semantics
    CopyablePimpl(CopyablePimpl&&) noexcept = default;
    CopyablePimpl& operator=(CopyablePimpl&&) noexcept = default;

    // Copy semantics (deep copy)
    CopyablePimpl(const CopyablePimpl& other)
        : ptr_(other.ptr_ ? new T(*other.ptr_) : nullptr) {}

    CopyablePimpl& operator=(const CopyablePimpl& other) {
        if (this != &other) {
            ptr_.reset(other.ptr_ ? new T(*other.ptr_) : nullptr);
        }
        return *this;
    }

    T* operator->() noexcept { return ptr_.get(); }
    const T* operator->() const noexcept { return ptr_.get(); }
    T& operator*() noexcept { return *ptr_; }
    const T& operator*() const noexcept { return *ptr_; }
    T* get() noexcept { return ptr_.get(); }
    const T* get() const noexcept { return ptr_.get(); }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    std::unique_ptr<T, PimplDeleter<T>> ptr_;
};

/**
 * @brief Create a CopyablePimpl with forwarded constructor arguments
 */
template <typename T, typename... Args>
CopyablePimpl<T> makeCopyablePimpl(Args&&... args) {
    return CopyablePimpl<T>(new T(std::forward<Args>(args)...));
}

/**
 * @brief Shared pimpl for reference-counted implementations
 *
 * Use when multiple objects should share the same implementation.
 * Useful for immutable value types.
 *
 * @tparam T The implementation type
 */
template <typename T>
class SharedPimpl {
public:
    SharedPimpl() noexcept = default;

    explicit SharedPimpl(T* ptr) noexcept : ptr_(ptr) {}

    explicit SharedPimpl(std::shared_ptr<T> ptr) noexcept
        : ptr_(std::move(ptr)) {}

    ~SharedPimpl() = default;

    // All default - shared_ptr handles everything
    SharedPimpl(const SharedPimpl&) = default;
    SharedPimpl& operator=(const SharedPimpl&) = default;
    SharedPimpl(SharedPimpl&&) noexcept = default;
    SharedPimpl& operator=(SharedPimpl&&) noexcept = default;

    T* operator->() noexcept { return ptr_.get(); }
    const T* operator->() const noexcept { return ptr_.get(); }
    T& operator*() noexcept { return *ptr_; }
    const T& operator*() const noexcept { return *ptr_; }
    T* get() noexcept { return ptr_.get(); }
    const T* get() const noexcept { return ptr_.get(); }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    /**
     * @brief Get reference count
     */
    long useCount() const noexcept {
        return ptr_.use_count();
    }

    /**
     * @brief Check if this is the only reference
     */
    bool unique() const noexcept {
        return ptr_.use_count() == 1;
    }

private:
    std::shared_ptr<T> ptr_;
};

/**
 * @brief Create a SharedPimpl with forwarded constructor arguments
 */
template <typename T, typename... Args>
SharedPimpl<T> makeSharedPimpl(Args&&... args) {
    return SharedPimpl<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

} // namespace makineai
