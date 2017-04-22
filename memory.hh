namespace matan {
  template <typename T, typename... Args>
  inline void place(T* loc, Args&&... args) {
    ::new (loc) T(args...);
  }

  template <typename T, typename... Args>
  inline void replace(T* p, Args&&... args) {
    p->~T();
    ::new (p) T(args...);
  }
}
