## Limit Order Book

A limit order book (LOB) is a dynamic, high performance system that records and organizes all outstanding orders for a financial instrument, such as a stock, cryptocurrency or futures contract. 


This is a C++ implementation of a high performance, low latency limit order book.


---

### Tests
This implementation uses `cmake` as its build system.

- Make the build directory:
```bash
rm -rf build/
mkdir build
```

- Set the `dev` preset
```bash
cmake --preset dev
```
- Build
```bash
cmake --build build/dev
```

- Run Tests
```bash
ctest --preset dev
```

---


### LICENSE
This project is licensed under the Boost Software License (BSL 1). Check the [LICENSE](./LICENSE) file or visit the official [Boost Software License](https://www.boost.org/LICENSE_1_0.txt) page.