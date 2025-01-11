#ifndef SINGLETON_HPP_
#define SINGLETON_HPP_


// ==================================
// Singlton
// ==================================
template <typename T>
class Singleton {
public:
    static T& Instance() {
        static T instance;
        return instance;
    }

protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton& operator=(Singleton&&) = delete;
    ~Singleton() = default;
};

#endif  // SINGLETON_HPP_