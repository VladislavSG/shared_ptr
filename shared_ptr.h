#pragma once

template<typename T>
struct weak_ptr;

template<typename T>
struct shared_ptr;

// Абстрактный класс
struct control_block {
    size_t strong_counter = 1;
    size_t weak_counter = 0;

    virtual void delete_object() = 0;
    virtual ~control_block() = default;
};

// Блок, хранящий ссылку на контролируемый объект
template<typename T, typename Deleter>
struct control_block_ptr : control_block {
    control_block_ptr(T *ptr, Deleter del) : control_object(ptr), deleter(del) {}

    void delete_object() override {
        deleter(control_object);
    }

private:
    T *control_object;
    Deleter deleter;
};

// Блок, создаваемый make_shared
template<typename T>
struct control_block_t : control_block {
    template<typename ...Args>
    explicit control_block_t(Args &&...args) {
        new(get_ptr()) T(std::forward<Args>(args)...);
    }

    void delete_object() override {
        get_ptr()->~T();
    }

private:
    std::aligned_storage<sizeof(T), alignof(T)> data;

    T *get_ptr() {
        return reinterpret_cast<T *>(&data);
    }

    template<typename U, typename... Args>
    friend
    shared_ptr<U> make_shared(Args &&... args);
};



template<typename T>
struct shared_ptr {
    // Секция конструкторов
    // Обычные
    constexpr shared_ptr() noexcept
            : shared_block(nullptr),
              focused_object(nullptr) {}

    constexpr explicit shared_ptr(std::nullptr_t) noexcept
            : shared_ptr() {}

    template<typename Y, typename Deleter>
    shared_ptr(Y *ptr, Deleter del)
            : shared_block(new control_block_ptr<Y, Deleter>(ptr, del)),
              focused_object(ptr) {}

    template<typename Y>
    explicit shared_ptr(Y *ptr)
            : shared_ptr(ptr, std::default_delete<Y>()) {}

    template<typename Deleter>
    shared_ptr(std::nullptr_t ptr, Deleter del)
            : shared_ptr(ptr, del) {}

    // Aliasing constructor
    template<typename Y>
    shared_ptr(const shared_ptr<Y> &other, T *ptr) noexcept
            : shared_block(other.shared_block),
              focused_object(ptr) {
        add_strong(); // лишний переход по ссылке => медленнее следующего конструктора
    }

    template<typename Y>
    shared_ptr(shared_ptr<Y> &&other, T *ptr) noexcept : shared_ptr() {
        shared_block = other.shared_block;
        focused_object = ptr;
        other.shared_block = nullptr;
        other.focused_object = nullptr;
    }

    // Конструктор копирования
    shared_ptr(const shared_ptr &other) noexcept
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_strong();
    }

    template<typename Y>
    shared_ptr(const shared_ptr<Y> &other) noexcept
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_strong();
    }

    // Move конструктор
    shared_ptr(shared_ptr &&other) noexcept
            : shared_ptr() {
        swap(other);
    }

    template<typename Y>
    shared_ptr(shared_ptr<Y> &&other) noexcept
            : shared_ptr() {
        swap(other);
    }

    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y> &other)
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_strong();
    }

    ~shared_ptr() {
        release_strong();
    }

    // Секция operator=
    shared_ptr &operator=(const shared_ptr &other) noexcept {
        if (*this != other) {
            release_strong();
            shared_block = other.shared_block;
            focused_object = other.focused_object;
            add_strong();
        }
        return *this;
    }

    template<typename Y>
    shared_ptr &operator=(const shared_ptr<Y> &other) noexcept {
        release_strong();
        shared_block = other.shared_block;
        focused_object = other.focused_object;
        add_strong();
        return *this;
    }

    // Move operator=
    shared_ptr &operator=(shared_ptr &&other) noexcept {
        shared_ptr(std::move(other)).swap(*this);
        return *this;
    }

    template<typename Y>
    shared_ptr &operator=(shared_ptr<Y> &&other) noexcept {
        shared_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // Секция modifiers
    void swap(shared_ptr &r) noexcept {
        std::swap(shared_block, r.shared_block);
        std::swap(focused_object, r.focused_object);
    }

    void reset() noexcept {
        release_strong();
        shared_block = nullptr;
        focused_object = nullptr;
    }

    template<typename Y>
    void reset(Y *ptr) {
        reset(ptr, std::default_delete<Y>());
    }

    template<typename Y, typename Deleter>
    void reset(Y *ptr, Deleter del) {
        release_strong();
        shared_block = new control_block_ptr<Y, Deleter>(ptr, del);
        focused_object = ptr;
    }

    // Секция observers
    T *get() const noexcept {
        return focused_object;
    }

    T &operator*() const noexcept {
        return *focused_object;
    }

    T *operator->() const noexcept {
        return focused_object;
    }

    size_t use_count() const noexcept {
        return shared_block != nullptr ? shared_block->strong_counter : 0;
    }

    explicit operator bool() const noexcept {
        return focused_object != nullptr;
    }

private:
    void add_strong() {
        if (shared_block != nullptr) {
            shared_block->strong_counter++;
        }
    }

    // После вызова ссылки на shared_block и focused_object - undefined
    void release_strong() {
        if (shared_block != nullptr && --shared_block->strong_counter == 0) {
            shared_block->delete_object();
            if (shared_block->weak_counter == 0) {
                delete shared_block;
            }
        }
    }


    template<typename Y>
    friend
    struct weak_ptr;

    template<typename Y>
    friend
    struct shared_ptr;

    template<typename U, typename... Args>
    friend
    shared_ptr<U> make_shared(Args &&... args);


    control_block *shared_block;
    T *focused_object;
};

// Секция non-member functions
template<typename T, typename... Args>
shared_ptr<T> make_shared(Args &&... args) {
    shared_ptr<T> ret;
    auto temp = new control_block_t<T>(std::forward<Args>(args)...);
    ret.shared_block = temp;
    ret.focused_object = temp->get_ptr();
    return ret;
}

template<typename L, typename R>
bool operator==(const shared_ptr<L> &lhs, const shared_ptr<R> &rhs) noexcept {
    return lhs.get() == rhs.get();
}

template<typename L, typename R>
bool operator!=(const shared_ptr<L> &lhs, const shared_ptr<R> &rhs) noexcept {
    return !(lhs == rhs);
}

template<typename L>
bool operator==(const shared_ptr<L> &lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
}

template<typename R>
bool operator==(std::nullptr_t, const shared_ptr<R> &rhs) noexcept {
    return rhs.get() == nullptr;
}

template<typename L>
bool operator!=(const shared_ptr<L> &lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
}

template<typename R>
bool operator!=(std::nullptr_t, const shared_ptr<R> &rhs) noexcept {
    return rhs.get() != nullptr;
}


template<typename T>
struct weak_ptr {
    // Секция конструкторов
    // Обычный
    constexpr weak_ptr() noexcept
            : shared_block(nullptr),
              focused_object(nullptr) {}
    // Конструктор копирования
    weak_ptr(const weak_ptr &other) noexcept
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_weak();
    }

    template<typename Y>
    weak_ptr(const weak_ptr<Y> &other) noexcept
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_weak();
    }

    template<typename Y>
    weak_ptr(const shared_ptr<Y> &other) noexcept
            : shared_block(other.shared_block),
              focused_object(other.focused_object) {
        add_weak();
    }
    // Move конструктор
    weak_ptr(weak_ptr &&other) noexcept
            : weak_ptr() {
        swap(other);
    }

    template<typename Y>
    weak_ptr(weak_ptr<Y> &&other) noexcept
            : weak_ptr() {
        swap(other);
    }

    ~weak_ptr() {
        release_weak();
    }

    // Секция operator=
    weak_ptr &operator=(const weak_ptr &other) noexcept {
        if (this != &other) {
            release_weak();
            shared_block = other.shared_block;
            focused_object = other.focused_object;
            add_weak();
        }
        return *this;
    }

    template<typename Y>
    weak_ptr &operator=(const weak_ptr<Y> &other) noexcept {
        release_weak();
        shared_block = other.shared_block;
        focused_object = other.focused_object;
        add_weak();
        return *this;
    }

    template<typename Y>
    weak_ptr &operator=(const shared_ptr<Y> &other) noexcept {
        release_weak();
        shared_block = other.shared_block;
        focused_object = other.focused_object;
        add_weak();
        return *this;
    }
    // Move operator=
    weak_ptr &operator=(weak_ptr &&other) noexcept {
        weak_ptr<T>(std::move(other)).swap(*this);
        return *this;
    }

    template<typename Y>
    weak_ptr &operator=(weak_ptr<Y> &&other) noexcept {
        weak_ptr<T>(std::move(other)).swap(*this);
        return *this;
    }

    // Секция modifiers
    void reset() noexcept {
        release_weak();
        shared_block = nullptr;
        focused_object = nullptr;
    }

    void swap(weak_ptr &other) noexcept {
        std::swap(shared_block, other.shared_block);
        std::swap(focused_object, other.focused_object);
    }

    // Секция observers
    size_t use_count() const noexcept {
        return shared_block != nullptr ? shared_block->strong_counter : 0;
    }

    bool expired() const noexcept {
        return use_count() == 0;
    }

    shared_ptr<T> lock() const noexcept {
        return expired() ? shared_ptr<T>() : shared_ptr<T>(*this);
    }

private:
    control_block *shared_block;
    T *focused_object;

    void add_weak() {
        if (shared_block != nullptr) {
            shared_block->weak_counter++;
        }
    }

    void release_weak() {
        if (shared_block != nullptr &&
                --shared_block->weak_counter == 0 &&
                shared_block->strong_counter == 0) {
            delete shared_block;
        }
    }

    template<typename Y>
    friend
    struct weak_ptr;

    template<typename Y>
    friend
    struct shared_ptr;
};